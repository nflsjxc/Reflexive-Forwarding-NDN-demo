# Reflexive-Forwarding-NDN-demo
A demo for draft-oran-icnrg-reflexive-forwarding-05   
(https://www.ietf.org/archive/id/draft-oran-icnrg-reflexive-forwarding-05.html)


## What is reflexive forwarding?
Current Information-Centric Networking protocols such as CCNx and NDN have a wide range of useful applications in content retrieval and other scenarios that depend only on a robust two-way exchange in the form of a request and response (represented by an Interest-Data exchange in the case of the two protocols noted above). A number of important applications however, require placing large amounts of data in the Interest message, and/or more than one two-way handshake. 

While these can be accomplished using independent Interest-Data exchanges by reversing the roles of consumer and producer, such approaches can be both clumsy for applications and problematic from a state management, congestion control, or security standpoint. This specification proposes a Reflexive Forwarding extension to the CCNx and NDN protocol architectures that eliminates the problems inherent in using independent Interest-Data exchanges for such applications. It updates RFC8569 and RFC8609.

## Installation
+ The demo is developed based on Mini-NDN version 0.6.0  
+ Mini-NDN stores NDN software source code in `/mini-ndn/dl`. When installing please use `./install.sh --source` to install NDN software from source code.
+ After mini-ndn is installed, install modified ndn-cxx and NFD in `/Mods` to overwrite orignial ndn-cxx and NFD.
## Experiments
The experiment code is stored in `/Experiments`  
To start an experiment,
+ You need to first setup the mini-ndn environment. For example, ``
python3 interest_test.py  
``  
+ This will setup a simple topology containing 4 nodes. After you see the `mini-ndn>` CLI, open a terminal and `export HOME=/tmp/minindn/a`, then you can run NDN commands on `node a`.  
+ Choose one node as the producer and one node as the consumer, run `nlsrc advertise /example` on producer node, and use `nfdc route` to check if the network converges or not
+ If the network converges, run `producer` and `consumer` on the chosen nodes and you will see the results



## Procedure  
The demo's procedure is the following:
```text

Consumer sends, I1: /example/testApp/RN=1234, pit-token=2345
Forwarder forwards I1 to Producer,
Producer receives I1, sending RI1: /testApp/RN=9999, with pit-token from I1,
Forwarder forwards RI1 to Consumer based on the pit-token,
Consumer receives RI1, sending RD1 to Producer,
Producer sends RI1 to Consumer and receives RD2 following the same process, 
Producer gathers the necessary information and sends D1 to the Consumer.

```
During the procedure, the consumer remains anonymous to the producer.

## Major Code changes
### NFD:
``NFD/daemon/fw/best-route-strategy.cpp``
-  `BestRouteStrategy::afterReceiveInterest`: modify control logic for Reflexive Interest sending  

 ``NFD/daemon/fw/forwarder.cpp``:   
+ `Forwarder::onIncomingInterest`: modify control logic to track Reflexive Interest
+ `Forwarder::onSendingRI`: add control logic for Reflexive Interest from Producer
+ `Forwarder::onContentStoreMiss`: add control logic for pending Reflexive Interest upstream

``NFD/daemon/table/assist.hpp,cpp``:  
+ contain auxilary functions to set/read pit-tokens in interest/data packets
+ maintain a data structure using `std::map` to track and restore pit-tokens hop by hop

### ndn-cxx

``ndn-cxx/encoding/tlv.hpp``: 
+ add TLV type for `ReflexiveNameComponent` (currently setting it as 59)  

``ndn-cxx/impl/face-impl.hpp``:  
+ `expressInterest`: modify encoding for lpPacket to transmit pit-token for interests
+ `putData`: modify encoding for lpPacket to transmit pit-token for data  

``ndn-cxx/face.hpp``:  
+ `extractLpLocalFields`: modify decoding for lpPacket to transmit pit-token in interest/data packets  


``ndn-cxx/impl/name-component-types.hpp``:
+ `ReflexiveNameComponentType`: add Reflexive Name Component class (Reflexive Name Segment in the draft)

``ndn-cxx/name-component.hpp,cpp``:
+ `RNfromEscapedString`: add function to parse Reflexive Name Segment
+ `isReflexive`: add function to judge if the name component is Reflexive Name Component

``ndn-cxx/name.hpp,cpp``:
+ `Name`: add Name class contructor to generate Reflexive Names

``ndn-cxx/interest.hpp,cpp``:
+ `isReflexiveInterest`: add function to judge if the interest is Reflexive Interest
+ `isReflexiveInterestFromProducer`: add function to judge if the if the interest is Reflexive Interest from Producer (Currently reserve RN=9999 For this interest)

## Difference with the draft and possible improvements(?)  
### Reflexive Name segment
+ Currently Reflexive Name Component is the **suffix** instead of the **prefix** for the Reflexive Names. This is easier for prefix matching in NFD.

+ We could also try to reserve certain **pit-tokens** for reflexive messages so the names will not need a new TLV

### Processing of a Reflexive Interest
+ Currently we omit the RNP checking of Reflexive Interest with RNP in PIT entry because NFD now uses name-based PITs

+ Generally speaking there are 2 types of interests in the message flow:  
    * Reflexive Interest (Consumer -> Producer)  
    * Reflexive Interest  (Producer -> Consumer)  

 Currently the two types of Interests are identified by the Reflexive Name value, if RN=9999, then it is the Reflexive Interest from Producer to Consumer.  Maybe we can try to merge this in the **pit-tokens** in the future.

## License
See [COPYING.md](./COPYING.md)

 ## Contributor
 Xinchen Jin, Research Assistant of Professor Dirk Kutscher at HKUST(GZ)