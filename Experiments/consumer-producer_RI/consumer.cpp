#include<ndn-cxx/face.hpp>
#include<ndn-cxx/security/validator-config.hpp>
#include<iostream>
#include <ndn-cxx/lp/pit-token.hpp>
#include "assist.hpp"

using namespace ndn;

class Consumer
{
public:
    Consumer()
    {
    }
    void run()
    {
        auto cert = m_keyChain.getPib().getDefaultIdentity().getDefaultKey().getDefaultCertificate();
        m_certServeHandle = m_face.setInterestFilter(security::extractIdentityFromCertName(cert.getName()),
                                                 [this, cert] (auto&&...) {
                                                   m_face.put(cert);
                                                 },
                                                 std::bind(&Consumer::onRegisterFailed, this, _1, _2));

        std::cout<<"Consumer: I am ready\n";
        auto name=Name("/example/testApp/1234",true);
        // auto name=Name("/example/testApp/1234");
       //Create I1
        Interest interest(name);
        auto lpPitToken = setPitToken(2345);
        interest.setTag(make_shared<lp::PitToken>(lpPitToken));
        interest.setInterestLifetime(5_s); // 2 seconds
        interest.setMustBeFresh(true);
        std::cout<<interest<<'\n';

        std::cout<<"pit-token set= "<<readInterestPitToken(interest)<<'\n';
        // interest.setCanBePrefix()
        // interest.setIsReflexiveInterst(false);
        // std::cout<<interest.getIsReflexiveInterest()<<'\n';
        
        //The filter for RI1
        m_face.setInterestFilter(Name("/testApp"),
                             std::bind(&Consumer::onInterest, this, _2),
                             nullptr, // RegisterPrefixSuccessCallback is optional
                             std::bind(&Consumer::onRegisterFailed, this, _1, _2));

        m_face.expressInterest(interest,
                            bind(&Consumer::datacallback, this,  _1, _2),
                            bind(&Consumer::nackcallback, this, _1, _2),
                            bind(&Consumer::timeoutcallback, this, _1)
                            );

        std::cout << ">> I1: " << interest << std::endl;

        m_face.processEvents();
    }
private:
    KeyChain m_keyChain;
    ScopedRegisteredPrefixHandle m_certServeHandle;
    ndn::Face m_face;
    void datacallback(const Interest& interest, const Data& data)
    {
        std::cout << "Received Data " << data << std::endl;
    }
    void nackcallback(const Interest&, const lp::Nack& nack) const
    {
        std::cout << "Received Nack with reason " << nack.getReason() << std::endl;
    }
    void timeoutcallback(const Interest& interest) const
    {
        std::cout << "Timeout for " << interest << std::endl;
    }
    void RIcallback(const Interest& interest, const Interest& Rinterest)
    {
        std::cout << "Received Reflexive Interest " << Rinterest << std::endl;
    }
    void onRegisterFailed(const Name& prefix, const std::string& reason)
    {
        std::cerr << "ERROR: Failed to register prefix '" << prefix
                << "' with the local forwarder (" << reason << ")\n";
        m_face.shutdown();
    }
    void onInterest(const Interest& interest)
    {
        std::cout << ">> Incoming reflexive interest: " << interest << std::endl;
        auto lpPitToken = interest.getTag<lp::PitToken>();
        if(lpPitToken==nullptr)
        {
            std::cout<<"No pit token for interest\n\n";
        }
        std::cout << interest<<"\n\n";
        std::cout << "<< Outcomming Reflexive Data\n";

        //reflexive data
        uint32_t pitToken = readPitToken(*lpPitToken);
        std::cout<< "pit-token = " << pitToken << std::endl;
        auto data = std::make_shared<Data>();
        data->setName(interest.getName());
        data->setFreshnessPeriod(10_s);
        // data->setContent(pitToken);

        m_keyChain.sign(*data);
        std::cout << *data << std::endl;
        m_face.put(*data);
    }
};


int main()
{
    Consumer consumer;
    try
    {
        consumer.run();
    }
    catch(const std::exception& e)
    {
        std::cerr<< "ERROR: "<<e.what()<<'\n';
    }

    return 0;
}