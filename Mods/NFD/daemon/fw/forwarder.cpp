/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014-2023,  Regents of the University of California,
 *                           Arizona Board of Regents,
 *                           Colorado State University,
 *                           University Pierre & Marie Curie, Sorbonne University,
 *                           Washington University in St. Louis,
 *                           Beijing Institute of Technology,
 *                           The University of Memphis.
 *
 * This file is part of NFD (Named Data Networking Forwarding Daemon).
 * See AUTHORS.md for complete list of NFD authors and contributors.
 *
 * NFD is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * NFD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * NFD, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "forwarder.hpp"

#include "algorithm.hpp"
#include "best-route-strategy.hpp"
#include "scope-prefix.hpp"
#include "strategy.hpp"
#include "common/global.hpp"
#include "common/logger.hpp"
#include "table/cleanup.hpp"

#include <ndn-cxx/lp/pit-token.hpp>
#include <ndn-cxx/lp/tags.hpp>

#include<iostream>
namespace nfd {

NFD_LOG_INIT(Forwarder);

const std::string CFG_FORWARDER = "forwarder";

static Name
getDefaultStrategyName()
{
  return fw::BestRouteStrategy::getStrategyName();
}

Forwarder::Forwarder(FaceTable& faceTable)
  : m_faceTable(faceTable)
  , m_unsolicitedDataPolicy(make_unique<fw::DefaultUnsolicitedDataPolicy>())
  , m_fib(m_nameTree)
  , m_pit(m_nameTree)
  , m_measurements(m_nameTree)
  , m_strategyChoice(*this)
{
  m_faceTable.afterAdd.connect([this] (const Face& face) {
    face.afterReceiveInterest.connect(
      [this, &face] (const Interest& interest, const EndpointId& endpointId) {
        this->onIncomingInterest(interest, FaceEndpoint(const_cast<Face&>(face), endpointId));
      });
    face.afterReceiveData.connect(
      [this, &face] (const Data& data, const EndpointId& endpointId) {
        this->onIncomingData(data, FaceEndpoint(const_cast<Face&>(face), endpointId));
      });
    face.afterReceiveNack.connect(
      [this, &face] (const lp::Nack& nack, const EndpointId& endpointId) {
        this->onIncomingNack(nack, FaceEndpoint(const_cast<Face&>(face), endpointId));
      });
    face.onDroppedInterest.connect(
      [this, &face] (const Interest& interest) {
        this->onDroppedInterest(interest, const_cast<Face&>(face));
      });
  });

  m_faceTable.beforeRemove.connect([this] (const Face& face) {
    cleanupOnFaceRemoval(m_nameTree, m_fib, m_pit, face);
  });

  m_fib.afterNewNextHop.connect([this] (const Name& prefix, const fib::NextHop& nextHop) {
    this->onNewNextHop(prefix, nextHop);
  });

  m_strategyChoice.setDefaultStrategy(getDefaultStrategyName());
}

void
Forwarder::onIncomingInterest(const Interest& interest, const FaceEndpoint& ingress)
{
  NFD_LOG_DEBUG("onIncomingInterest in=" << ingress << " interest=" << interest <<" pit-token= "<<readInterestPitToken(interest));
  interest.setTag(make_shared<lp::IncomingFaceIdTag>(ingress.face.getId()));
  ++m_counters.nInInterests;

  // ensure the received Interest has a Nonce
  auto nonce = interest.getNonce();
  auto hopLimit = interest.getHopLimit();

  // drop if HopLimit zero, decrement otherwise (if present)
  if (hopLimit) {
    NFD_LOG_DEBUG("onIncomingInterest in=" << ingress << " interest=" << interest.getName()
                  << " nonce=" << nonce << " hop-limit=" << static_cast<unsigned>(*hopLimit));
    if (*hopLimit == 0) {
      ++ingress.face.getCounters().nInHopLimitZero;
      // drop
      return;
    }
    const_cast<Interest&>(interest).setHopLimit(*hopLimit - 1);
  }
  else {
    NFD_LOG_DEBUG("onIncomingInterest in=" << ingress << " interest=" << interest.getName()
                  << " nonce=" << nonce);
  }

  // /localhost scope control
  bool isViolatingLocalhost = ingress.face.getScope() == ndn::nfd::FACE_SCOPE_NON_LOCAL &&
                              scope_prefix::LOCALHOST.isPrefixOf(interest.getName());
  if (isViolatingLocalhost) {
    NFD_LOG_DEBUG("onIncomingInterest in=" << ingress << " interest=" << interest.getName()
                  << " nonce=" << nonce << " violates /localhost");
    // drop
    return;
  }

  // detect duplicate Nonce with Dead Nonce List
  bool hasDuplicateNonceInDnl = m_deadNonceList.has(interest.getName(), nonce);
  if (hasDuplicateNonceInDnl) {
    // goto Interest loop pipeline
    this->onInterestLoop(interest, ingress);
    return;
  }

  // strip forwarding hint if Interest has reached producer region
  if (!interest.getForwardingHint().empty() &&
      m_networkRegionTable.isInProducerRegion(interest.getForwardingHint())) {
    NFD_LOG_DEBUG("onIncomingInterest in=" << ingress << " interest=" << interest.getName()
                  << " nonce=" << nonce << " reaching-producer-region");
    const_cast<Interest&>(interest).setForwardingHint({});
  }

  // PIT insert
  shared_ptr<pit::Entry> pitEntry = m_pit.insert(interest).first;
  
  if(interest.isReflexiveInterestFromProducer() ) //This is the reflexive interest from producer
  {
    NFD_LOG_DEBUG("Sending RI: "<<interest<<'\n');
    this->onSendingRI(interest, ingress, pitEntry);
    return ;
  }

  // Pit token insert
  if(interest.isReflexiveInterest())
  {
    auto pitTokenTag = interest.getTag<lp::PitToken>();
    uint32_t prevToken = pitTokenTag == nullptr? 0 : readPitToken(*pitTokenTag);
    if(!m_pit_assist.existName(interest.getName()))
    {
      m_pit_assist.createName(interest.getName(), prevToken);
      auto res = m_pit_assist.nameToToken(interest.getName());
      uint32_t pitToken = res.first;
      NFD_LOG_DEBUG("at onincominginterest, generate pit-token: "<<pitToken);
    }
    //Loop should not detect here
    // else
    // {
    //   NFD_LOG_DEBUG("at onincominginterest, detect loop: "<<interest);
    //   this->onInterestLoop(interest, ingress);
    //   return ;
    // }
  }
 

  // detect duplicate Nonce in PIT entry
  int dnw = fw::findDuplicateNonce(*pitEntry, nonce, ingress.face);
  bool hasDuplicateNonceInPit = dnw != fw::DUPLICATE_NONCE_NONE;
  if (ingress.face.getLinkType() == ndn::nfd::LINK_TYPE_POINT_TO_POINT) {
    // for p2p face: duplicate Nonce from same incoming face is not loop
    hasDuplicateNonceInPit = hasDuplicateNonceInPit && !(dnw & fw::DUPLICATE_NONCE_IN_SAME);
  }
  if (hasDuplicateNonceInPit) {
    // goto Interest loop pipeline
    this->onInterestLoop(interest, ingress);
    return;
  }

  // is pending?
  if (!pitEntry->hasInRecords()) {
    m_cs.find(interest,
              [=] (const Interest& i, const Data& d) { onContentStoreHit(i, ingress, pitEntry, d); },
              [=] (const Interest& i) { onContentStoreMiss(i, ingress, pitEntry); });
  }
  else {
    this->onContentStoreMiss(interest, ingress, pitEntry);
  }
}

void
Forwarder::onInterestLoop(const Interest& interest, const FaceEndpoint& ingress)
{
  // if multi-access or ad hoc face, drop
  if (ingress.face.getLinkType() != ndn::nfd::LINK_TYPE_POINT_TO_POINT) {
    NFD_LOG_DEBUG("onInterestLoop in=" << ingress << " interest=" << interest.getName()
                  << " nonce=" << interest.getNonce() << " drop");
    return;
  }

  NFD_LOG_DEBUG("onInterestLoop in=" << ingress << " interest=" << interest.getName()
                << " nonce=" << interest.getNonce() << " nack");

  // send Nack with reason=DUPLICATE
  // note: Don't enter outgoing Nack pipeline because it needs an in-record.
  lp::Nack nack(interest);
  nack.setReason(lp::NackReason::DUPLICATE);
  ingress.face.sendNack(nack);
  ++m_counters.nOutNacks;
}

void Forwarder::onSendingRI(const Interest& interest, const FaceEndpoint& ingress,
                     const shared_ptr<pit::Entry>& pitEntry) //At last go to strategy?
{
    // Reflexive Interest processing part (producer -> consumer)
  NFD_LOG_DEBUG("At forwarder.cpp, onSendingRI Reflexive Interest= "<< interest);
  
  auto currentPitname=pitEntry->getName();
  NFD_LOG_DEBUG("on reflexive interest current pit name: " << currentPitname<<" "<<interest.getName());
  auto pitTokenTag = interest.getTag<lp::PitToken>();
  uint32_t pitToken = pitTokenTag == nullptr? 0 : readPitToken(*pitTokenTag);

  if(pitToken == 0)// Reflexive interest doesn't have pitToken
  {
    NFD_LOG_DEBUG("Reflexive interest without pitToken");
    lp::Nack nack(interest);
    nack.setReason(lp::NackReason::NONE);
    ingress.face.sendNack(nack);
    ++m_counters.nOutNacks;
    return ;
  }
  auto res = m_pit_assist.tokenToName(pitToken);
  if(!res.second)
  {
    NFD_LOG_DEBUG("Cannot find reflexive interest's corresponding original interst name, nack with NO_ROUTE!");
    lp::Nack nack(interest);
    nack.setReason(lp::NackReason::NO_ROUTE);
    ingress.face.sendNack(nack);
    ++m_counters.nOutNacks;
    return ;
  }

  //currently skip rnp checking...
  Name originalInterestname = res.first; //original interest name is used for reconstructing prev pit token
  NFD_LOG_DEBUG("restore original interest name: "<<originalInterestname);
  shared_ptr<pit::Entry> pitEntry_original = m_pit.findBasedOnName(originalInterestname);
  if(!pitEntry_original)
  {
    NFD_LOG_DEBUG("error finding original interstname's pit entry"); //This should not happen?
    return ;
  }

  auto res2 = m_pit_assist.nameToPrevToken(originalInterestname);
  if(!res2.second)
  {
    NFD_LOG_DEBUG("error finding original interstname's prev pit token"); //This should not happen?
    return ;
  }

  uint32_t prevPitToken = res2.first;

  //Restore pit token for previous hop (Reflexive Interest special case)
  auto lpPitToken = setPitToken(prevPitToken);
  interest.setTag(make_shared<lp::PitToken>(lpPitToken)); 

  // Other things treat like normal interest
  // attach HopLimit if configured and not present in Interest
  if (m_config.defaultHopLimit > 0 && !interest.getHopLimit()) {
    const_cast<Interest&>(interest).setHopLimit(m_config.defaultHopLimit);
  }

  // insert in-record
  pitEntry->insertOrUpdateInRecord(ingress.face, interest);

  // set PIT expiry timer to the time that the last PIT in-record expires
  auto lastExpiring = std::max_element(pitEntry->in_begin(), pitEntry->in_end(),
                                       [] (const auto& a, const auto& b) {
                                         return a.getExpiry() < b.getExpiry();
                                       });
  auto lastExpiryFromNow = lastExpiring->getExpiry() - time::steady_clock::now();
  this->setExpiryTimer(pitEntry, time::duration_cast<time::milliseconds>(lastExpiryFromNow));

  // has NextHopFaceId?
  // auto nextHopTag = interest.getTag<lp::NextHopFaceIdTag>();
  // if (nextHopTag != nullptr) {
  //   // chosen NextHop face exists?
  //   Face* nextHopFace = m_faceTable.get(*nextHopTag);
  //   if (nextHopFace != nullptr) {
  //     NFD_LOG_DEBUG("onContentStoreMiss interest=" << interest.getName()
  //                   << " nonce=" << interest.getNonce() << " nexthop-faceid=" << nextHopFace->getId());
  //     // go to outgoing Interest pipeline
  //     // scope control is unnecessary, because privileged app explicitly wants to forward
  //     this->onOutgoingInterest(interest, *nextHopFace, pitEntry);
  //   }
  //   return;
  // }


  //Suppose to go to best route strategy, here ingress is the producer face, egress is the consumer face
  m_strategyChoice.findEffectiveStrategy(*pitEntry)
  .afterReceiveInterest(interest, FaceEndpoint(ingress.face), pitEntry_original);
  return;
}

void
Forwarder::onContentStoreMiss(const Interest& interest, const FaceEndpoint& ingress,
                              const shared_ptr<pit::Entry>& pitEntry)
{
  NFD_LOG_DEBUG("onContentStoreMiss interest=" << interest.getName() << " nonce=" << interest.getNonce());
  ++m_counters.nCsMisses;

  // attach HopLimit if configured and not present in Interest
  if (m_config.defaultHopLimit > 0 && !interest.getHopLimit()) {
    const_cast<Interest&>(interest).setHopLimit(m_config.defaultHopLimit);
  }

  // insert in-record
  pitEntry->insertOrUpdateInRecord(ingress.face, interest);

  // set PIT expiry timer to the time that the last PIT in-record expires
  auto lastExpiring = std::max_element(pitEntry->in_begin(), pitEntry->in_end(),
                                       [] (const auto& a, const auto& b) {
                                         return a.getExpiry() < b.getExpiry();
                                       });
  auto lastExpiryFromNow = lastExpiring->getExpiry() - time::steady_clock::now();
  this->setExpiryTimer(pitEntry, time::duration_cast<time::milliseconds>(lastExpiryFromNow));

  // has NextHopFaceId?
  auto nextHopTag = interest.getTag<lp::NextHopFaceIdTag>();
  if (nextHopTag != nullptr) {
    // chosen NextHop face exists?
    Face* nextHopFace = m_faceTable.get(*nextHopTag);
    if (nextHopFace != nullptr) {
      NFD_LOG_DEBUG("onContentStoreMiss interest=" << interest.getName()
                    << " nonce=" << interest.getNonce() << " nexthop-faceid=" << nextHopFace->getId());
      // go to outgoing Interest pipeline
      // scope control is unnecessary, because privileged app explicitly wants to forward
      this->onOutgoingInterest(interest, *nextHopFace, pitEntry);
    }
    return;
  }

  
  if(interest.isReflexiveInterest())
  {
    //Set pitToken and it will be delivered to nexthop
    uint32_t pitToken = m_pit_assist.nameToToken(interest.getName()).first;
    auto lpPitToken = setPitToken(pitToken);
    interest.setTag(make_shared<lp::PitToken>(lpPitToken));
    NFD_LOG_DEBUG("CS miss reflexive interest set pit token: "<< interest<<" token= "<<pitToken);
  }

  // dispatch to strategy: after receive Interest
  m_strategyChoice.findEffectiveStrategy(*pitEntry)
    .afterReceiveInterest(interest, FaceEndpoint(ingress.face), pitEntry);
}

void
Forwarder::onContentStoreHit(const Interest& interest, const FaceEndpoint& ingress,
                             const shared_ptr<pit::Entry>& pitEntry, const Data& data)
{
  NFD_LOG_DEBUG("onContentStoreHit interest=" << interest.getName() << " nonce=" << interest.getNonce());
  ++m_counters.nCsHits;

  data.setTag(make_shared<lp::IncomingFaceIdTag>(face::FACEID_CONTENT_STORE));
  data.setTag(interest.getTag<lp::PitToken>());
  // FIXME Should we lookup PIT for other Interests that also match the data?

  pitEntry->isSatisfied = true;
  pitEntry->dataFreshnessPeriod = data.getFreshnessPeriod();

  // set PIT expiry timer to now
  this->setExpiryTimer(pitEntry, 0_ms);

  // dispatch to strategy: after Content Store hit
  m_strategyChoice.findEffectiveStrategy(*pitEntry).afterContentStoreHit(data, ingress, pitEntry);
}

pit::OutRecord*
Forwarder::onOutgoingInterest(const Interest& interest, Face& egress,
                              const shared_ptr<pit::Entry>& pitEntry)
{
  // drop if HopLimit == 0 but sending on non-local face
  if (interest.getHopLimit() == 0 && egress.getScope() == ndn::nfd::FACE_SCOPE_NON_LOCAL) {
    NFD_LOG_DEBUG("onOutgoingInterest out=" << egress.getId() << " interest=" << interest.getName()
                  << " nonce=" << interest.getNonce() << " non-local hop-limit=0");
    ++egress.getCounters().nOutHopLimitZero;
    return nullptr;
  }

  NFD_LOG_DEBUG("onOutgoingInterest out=" << egress.getId() << " interest=" << interest.getName()
                << " nonce=" << interest.getNonce() << " pit-token="<<readInterestPitToken(interest));

   if(interest.isReflexiveInterestFromProducer())
  {
    auto originalInterestname = pitEntry->getName();
    auto res2 = m_pit_assist.nameToPrevToken(originalInterestname);

    //pit token is removed in strategy::sendinterst, restore that
    auto lpPitToken = setPitToken(res2.first);
    interest.setTag(make_shared<lp::PitToken>(lpPitToken));

    auto pitEntry2 = m_pit.find(interest);
    if(pitEntry2 == nullptr)
    {
      NFD_LOG_DEBUG("At onOutgoingInterest Cannot find pitEntry for reflexive interetst from producer= "<<interest);
    }
    auto it = pitEntry2->insertOrUpdateOutRecord(egress, interest);
    BOOST_ASSERT(it != pitEntry->out_end());

    // send Interest
    egress.sendInterest(interest);
    ++m_counters.nOutInterests;

    return &*it;
  }

  //restore pit-token generated for next-hop in I1
  if(interest.isReflexiveInterest())
  {
    auto res = m_pit_assist.nameToToken(interest.getName());
    auto lpPitToken = setPitToken(res.first);
    interest.setTag(make_shared<lp::PitToken>(lpPitToken));
  }
  NFD_LOG_DEBUG("onOutgoingInterest out=" << egress.getId() << " interest=" << interest.getName()
                << " nonce=" << interest.getNonce() << " pit-token="<<readInterestPitToken(interest));

  // insert out-record
  auto it = pitEntry->insertOrUpdateOutRecord(egress, interest);
  BOOST_ASSERT(it != pitEntry->out_end());

  // auto lpPitToken = setPitToken(6666);
  // interest.setTag(make_shared<lp::PitToken>(lpPitToken));
  // send Interest
  egress.sendInterest(interest);
  ++m_counters.nOutInterests;

  return &*it;
}

void
Forwarder::onInterestFinalize(const shared_ptr<pit::Entry>& pitEntry)
{
  NFD_LOG_DEBUG("onInterestFinalize interest=" << pitEntry->getName()
                << (pitEntry->isSatisfied ? " satisfied" : " unsatisfied"));

  // Dead Nonce List insert if necessary
  this->insertDeadNonceList(*pitEntry, nullptr);

  // Increment satisfied/unsatisfied Interests counter
  if (pitEntry->isSatisfied) {
    ++m_counters.nSatisfiedInterests;
  }
  else {
    ++m_counters.nUnsatisfiedInterests;
  }

  // PIT delete
  pitEntry->expiryTimer.cancel();
  m_pit.erase(pitEntry.get());
}

void
Forwarder::onIncomingData(const Data& data, const FaceEndpoint& ingress)
{
  data.setTag(make_shared<lp::IncomingFaceIdTag>(ingress.face.getId()));
  ++m_counters.nInData;
  NFD_LOG_DEBUG("onIncomingData in=" << ingress << " data=" << data.getName());

  // /localhost scope control
  bool isViolatingLocalhost = ingress.face.getScope() == ndn::nfd::FACE_SCOPE_NON_LOCAL &&
                              scope_prefix::LOCALHOST.isPrefixOf(data.getName());
  if (isViolatingLocalhost) {
    NFD_LOG_DEBUG("onIncomingData in=" << ingress << " data=" << data.getName() << " violates /localhost");
    // drop
    return;
  }

  

  // PIT match
  pit::DataMatchResult pitMatches;

  // //reflexive data
  // auto data2 = data;
  // auto res = m_pit_assist.parse_application_parameter(readString(data.getContent()));
  // if(res.second)
  // {
  //   NFD_LOG_DEBUG("Reflexive data found");
  //   if(!res.second)
  //   {
  //     NFD_LOG_DEBUG("onIncomingReflexiveData error pit-token parsing");
  //     this->onDataUnsolicited(data, ingress);
  //     return ;
  //   }
  //   std::string pitToken = res.first;
  //   //  if(!m_pit_assist.exist_token(pitToken))
  //   // {
  //   //   NFD_LOG_DEBUG("on reflexive data from producer error pit-token matching"<< pitToken);
  //   //   return ;
  //   // }
  //   if(m_pit_assist.exist_token(pitToken))
  //   {
  //     Name originalname = m_pit_assist.token_to_name(pitToken);
  //     data2.setName(originalname);
  //     pitMatches = m_pit.findAllDataMatches(data2);
  //     if(m_pit_assist.name_to_prev_token(originalname) != "")
  //     {
  //       data2.setContent(std::string_view("pit-token="+m_pit_assist.name_to_prev_token(originalname)));
  //     }
  //   }
  //   else if(m_pit_assist.exist_prev_token(pitToken))
  //   {
  //     Name nextname = m_pit_assist.prev_token_to_name(pitToken);
  //     // data2.setName(nextname);
  //     pitMatches = m_pit.findAllDataMatches(data2);
  //     if(m_pit_assist.name_to_token(nextname) != "")
  //     {
  //       data2.setContent(std::string_view("pit-token="+m_pit_assist.name_to_token(nextname)));
  //     }
  //   }
  //   else
  //   {
  //     NFD_LOG_DEBUG("on reflexive data from producer error pit-token matching"<< pitToken);
  //     return ;
  //   }
   
  //   // data = data2; //?
  // }
  // else 
  
  pitMatches = m_pit.findAllDataMatches(data);
  if (pitMatches.size() == 0) {
    // goto Data unsolicited pipeline
    this->onDataUnsolicited(data, ingress);
    return;
  }

  // if(res.second)
  // {
  //   if (pitMatches.size() == 1) {
  //     auto& pitEntry = pitMatches.front();

  //     NFD_LOG_DEBUG("onIncomingData reflexive matching=" << pitEntry->getName());

  //     // set PIT expiry timer to now
  //     this->setExpiryTimer(pitEntry, 0_ms);

  //     // trigger strategy: after receive Data
  //     m_strategyChoice.findEffectiveStrategy(*pitEntry).afterReceiveData(data2, ingress, pitEntry);

  //     // mark PIT satisfied
  //     pitEntry->isSatisfied = true;
  //     pitEntry->dataFreshnessPeriod = data2.getFreshnessPeriod();

  //     // Dead Nonce List insert if necessary (for out-record of ingress face)
  //     this->insertDeadNonceList(*pitEntry, &ingress.face);

  //     // delete PIT entry's out-record
  //     pitEntry->deleteOutRecord(ingress.face);
  //   }
  //   else
  //   {
  //     NFD_LOG_DEBUG("there supposes to be only 1 pit entry for reflexive data"); //Is this true?
  //     return ;
  //   }
  // }

  // CS insert
  m_cs.insert(data);

  // when only one PIT entry is matched, trigger strategy: after receive Data
  if (pitMatches.size() == 1) {
    auto& pitEntry = pitMatches.front();

    NFD_LOG_DEBUG("onIncomingData matching=" << pitEntry->getName());

    // set PIT expiry timer to now
    this->setExpiryTimer(pitEntry, 0_ms);

    // trigger strategy: after receive Data
    m_strategyChoice.findEffectiveStrategy(*pitEntry).afterReceiveData(data, ingress, pitEntry);

    // mark PIT satisfied
    pitEntry->isSatisfied = true;
    pitEntry->dataFreshnessPeriod = data.getFreshnessPeriod();

    // Dead Nonce List insert if necessary (for out-record of ingress face)
    this->insertDeadNonceList(*pitEntry, &ingress.face);

    // delete PIT entry's out-record
    pitEntry->deleteOutRecord(ingress.face);
  }
  // when more than one PIT entry is matched, trigger strategy: before satisfy Interest,
  // and send Data to all matched out faces
  else {
    std::set<Face*> pendingDownstreams;
    auto now = time::steady_clock::now();

    for (const auto& pitEntry : pitMatches) {
      NFD_LOG_DEBUG("onIncomingData matching=" << pitEntry->getName());

      // remember pending downstreams
      for (const pit::InRecord& inRecord : pitEntry->getInRecords()) {
        if (inRecord.getExpiry() > now) {
          pendingDownstreams.insert(&inRecord.getFace());
        }
      }

      // set PIT expiry timer to now
      this->setExpiryTimer(pitEntry, 0_ms);

      // invoke PIT satisfy callback
      m_strategyChoice.findEffectiveStrategy(*pitEntry).beforeSatisfyInterest(data, ingress, pitEntry);

      // mark PIT satisfied
      pitEntry->isSatisfied = true;
      pitEntry->dataFreshnessPeriod = data.getFreshnessPeriod();

      // Dead Nonce List insert if necessary (for out-record of ingress face)
      this->insertDeadNonceList(*pitEntry, &ingress.face);

      // clear PIT entry's in and out records
      pitEntry->clearInRecords();
      pitEntry->deleteOutRecord(ingress.face);
    }

    // foreach pending downstream
    for (const auto& pendingDownstream : pendingDownstreams) {
      if (pendingDownstream->getId() == ingress.face.getId() &&
          pendingDownstream->getLinkType() != ndn::nfd::LINK_TYPE_AD_HOC) {
        continue;
      }
      // goto outgoing Data pipeline
      this->onOutgoingData(data, *pendingDownstream);
    }
  }
}

void
Forwarder::onDataUnsolicited(const Data& data, const FaceEndpoint& ingress)
{
  ++m_counters.nUnsolicitedData;

  // accept to cache?
  auto decision = m_unsolicitedDataPolicy->decide(ingress.face, data);
  NFD_LOG_DEBUG("onDataUnsolicited in=" << ingress << " data=" << data.getName()
                << " decision=" << decision);
  if (decision == fw::UnsolicitedDataDecision::CACHE) {
    // CS insert
    m_cs.insert(data, true);
  }
}

bool
Forwarder::onOutgoingData(const Data& data, Face& egress)
{
  if (egress.getId() == face::INVALID_FACEID) {
    NFD_LOG_WARN("onOutgoingData out=(invalid) data=" << data.getName());
    return false;
  }
  NFD_LOG_DEBUG("onOutgoingData out=" << egress.getId() << " data=" << data.getName());

  // /localhost scope control
  bool isViolatingLocalhost = egress.getScope() == ndn::nfd::FACE_SCOPE_NON_LOCAL &&
                              scope_prefix::LOCALHOST.isPrefixOf(data.getName());
  if (isViolatingLocalhost) {
    NFD_LOG_DEBUG("onOutgoingData out=" << egress.getId() << " data=" << data.getName()
                  << " violates /localhost");
    // drop
    return false;
  }

  // TODO traffic manager

  // send Data
  egress.sendData(data);
  ++m_counters.nOutData;

  return true;
}

void
Forwarder::onIncomingNack(const lp::Nack& nack, const FaceEndpoint& ingress)
{
  nack.setTag(make_shared<lp::IncomingFaceIdTag>(ingress.face.getId()));
  ++m_counters.nInNacks;

  // if multi-access or ad hoc face, drop
  if (ingress.face.getLinkType() != ndn::nfd::LINK_TYPE_POINT_TO_POINT) {
    NFD_LOG_DEBUG("onIncomingNack in=" << ingress << " nack=" << nack.getInterest().getName()
                  << "~" << nack.getReason() << " link-type=" << ingress.face.getLinkType());
    return;
  }

  // PIT match
  shared_ptr<pit::Entry> pitEntry = m_pit.find(nack.getInterest());
  // if no PIT entry found, drop
  if (pitEntry == nullptr) {
    NFD_LOG_DEBUG("onIncomingNack in=" << ingress << " nack=" << nack.getInterest().getName()
                  << "~" << nack.getReason() << " no-pit-entry");
    return;
  }

  // has out-record?
  auto outRecord = pitEntry->getOutRecord(ingress.face);
  // if no out-record found, drop
  if (outRecord == pitEntry->out_end()) {
    NFD_LOG_DEBUG("onIncomingNack in=" << ingress << " nack=" << nack.getInterest().getName()
                  << "~" << nack.getReason() << " no-out-record");
    return;
  }

  // if out-record has different Nonce, drop
  if (nack.getInterest().getNonce() != outRecord->getLastNonce()) {
    NFD_LOG_DEBUG("onIncomingNack in=" << ingress << " nack=" << nack.getInterest().getName()
                  << "~" << nack.getReason() << " nonce-mismatch " << nack.getInterest().getNonce()
                  << "!=" << outRecord->getLastNonce());
    return;
  }

  NFD_LOG_DEBUG("onIncomingNack in=" << ingress << " nack=" << nack.getInterest().getName()
                << "~" << nack.getReason());

  // record Nack on out-record
  outRecord->setIncomingNack(nack);

  // set PIT expiry timer to now when all out-record receive Nack
  if (!fw::hasPendingOutRecords(*pitEntry)) {
    this->setExpiryTimer(pitEntry, 0_ms);
  }

  // trigger strategy: after receive NACK
  m_strategyChoice.findEffectiveStrategy(*pitEntry).afterReceiveNack(nack, ingress, pitEntry);
}

bool
Forwarder::onOutgoingNack(const lp::NackHeader& nack, Face& egress,
                          const shared_ptr<pit::Entry>& pitEntry)
{
  if (egress.getId() == face::INVALID_FACEID) {
    NFD_LOG_WARN("onOutgoingNack out=(invalid)"
                 << " nack=" << pitEntry->getInterest().getName() << "~" << nack.getReason());
    return false;
  }

  // has in-record?
  auto inRecord = pitEntry->getInRecord(egress);

  // if no in-record found, drop
  if (inRecord == pitEntry->in_end()) {
    NFD_LOG_DEBUG("onOutgoingNack out=" << egress.getId()
                  << " nack=" << pitEntry->getInterest().getName()
                  << "~" << nack.getReason() << " no-in-record");
    return false;
  }

  // if multi-access or ad hoc face, drop
  if (egress.getLinkType() != ndn::nfd::LINK_TYPE_POINT_TO_POINT) {
    NFD_LOG_DEBUG("onOutgoingNack out=" << egress.getId()
                  << " nack=" << pitEntry->getInterest().getName() << "~" << nack.getReason()
                  << " link-type=" << egress.getLinkType());
    return false;
  }

  NFD_LOG_DEBUG("onOutgoingNack out=" << egress.getId()
                << " nack=" << pitEntry->getInterest().getName()
                << "~" << nack.getReason() << " OK");

  // create Nack packet with the Interest from in-record
  lp::Nack nackPkt(inRecord->getInterest());
  nackPkt.setHeader(nack);

  // erase in-record
  pitEntry->deleteInRecord(egress);

  // send Nack on face
  egress.sendNack(nackPkt);
  ++m_counters.nOutNacks;

  return true;
}

void
Forwarder::onDroppedInterest(const Interest& interest, Face& egress)
{
  m_strategyChoice.findEffectiveStrategy(interest.getName()).onDroppedInterest(interest, egress);
}

void
Forwarder::onNewNextHop(const Name& prefix, const fib::NextHop& nextHop)
{
  const auto affectedEntries = this->getNameTree().partialEnumerate(prefix,
    [&] (const name_tree::Entry& nte) -> std::pair<bool, bool> {
      // we ignore an NTE and skip visiting its descendants if that NTE has an
      // associated FIB entry (1st condition), since in that case the new nexthop
      // won't affect any PIT entries anywhere in that subtree, *unless* this is
      // the initial NTE from which the enumeration started (2nd condition), which
      // must always be considered
      if (nte.getFibEntry() != nullptr && nte.getName().size() > prefix.size()) {
        return {false, false};
      }
      return {nte.hasPitEntries(), true};
    });

  for (const auto& nte : affectedEntries) {
    for (const auto& pitEntry : nte.getPitEntries()) {
      m_strategyChoice.findEffectiveStrategy(*pitEntry).afterNewNextHop(nextHop, pitEntry);
    }
  }
}

void
Forwarder::setExpiryTimer(const shared_ptr<pit::Entry>& pitEntry, time::milliseconds duration)
{
  BOOST_ASSERT(pitEntry);
  duration = std::max(duration, 0_ms);

  pitEntry->expiryTimer.cancel();
  pitEntry->expiryTimer = getScheduler().schedule(duration, [=] { onInterestFinalize(pitEntry); });
}

void
Forwarder::insertDeadNonceList(pit::Entry& pitEntry, const Face* upstream)
{
  // need Dead Nonce List insert?
  bool needDnl = true;
  if (pitEntry.isSatisfied) {
    BOOST_ASSERT(pitEntry.dataFreshnessPeriod >= 0_ms);
    needDnl = pitEntry.getInterest().getMustBeFresh() &&
              pitEntry.dataFreshnessPeriod < m_deadNonceList.getLifetime();
  }

  if (!needDnl) {
    return;
  }

  // Dead Nonce List insert
  if (upstream == nullptr) {
    // insert all outgoing Nonces
    const auto& outRecords = pitEntry.getOutRecords();
    std::for_each(outRecords.begin(), outRecords.end(), [&] (const auto& outRecord) {
      m_deadNonceList.add(pitEntry.getName(), outRecord.getLastNonce());
    });
  }
  else {
    // insert outgoing Nonce of a specific face
    auto outRecord = pitEntry.getOutRecord(*upstream);
    if (outRecord != pitEntry.getOutRecords().end()) {
      m_deadNonceList.add(pitEntry.getName(), outRecord->getLastNonce());
    }
  }
}

void
Forwarder::setConfigFile(ConfigFile& configFile)
{
  configFile.addSectionHandler(CFG_FORWARDER, [this] (auto&&... args) {
    processConfig(std::forward<decltype(args)>(args)...);
  });
}

void
Forwarder::processConfig(const ConfigSection& configSection, bool isDryRun, const std::string&)
{
  Config config;

  for (const auto& pair : configSection) {
    const std::string& key = pair.first;
    if (key == "default_hop_limit") {
      config.defaultHopLimit = ConfigFile::parseNumber<uint8_t>(pair, CFG_FORWARDER);
    }
    else {
      NDN_THROW(ConfigFile::Error("Unrecognized option " + CFG_FORWARDER + "." + key));
    }
  }

  if (!isDryRun) {
    m_config = config;
  }
}

} // namespace nfd
