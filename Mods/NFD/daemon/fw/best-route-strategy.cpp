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

#include "best-route-strategy.hpp"
#include "algorithm.hpp"
#include "common/logger.hpp"

namespace nfd::fw {

NFD_LOG_INIT(BestRouteStrategy);
NFD_REGISTER_STRATEGY(BestRouteStrategy);

BestRouteStrategy::BestRouteStrategy(Forwarder& forwarder, const Name& name)
  : Strategy(forwarder)
  , ProcessNackTraits(this)
{
  ParsedInstanceName parsed = parseInstanceName(name);
  if (parsed.version && *parsed.version != getStrategyName()[-1].toVersion()) {
    NDN_THROW(std::invalid_argument(
      "BestRouteStrategy does not support version " + to_string(*parsed.version)));
  }

  StrategyParameters params = parseParameters(parsed.parameters);
  m_retxSuppression = RetxSuppressionExponential::construct(params);

  this->setInstanceName(makeInstanceName(name, getStrategyName()));

  NDN_LOG_DEBUG(*m_retxSuppression);
}

const Name&
BestRouteStrategy::getStrategyName()
{
  static const auto strategyName = Name("/localhost/nfd/strategy/best-route").appendVersion(5);
  return strategyName;
}

void
BestRouteStrategy::afterReceiveInterest(const Interest& interest, const FaceEndpoint& ingress,
                                        const shared_ptr<pit::Entry>& pitEntry)
{
  NFD_LOG_DEBUG("At afterReceiveInterest interst= "<<interest<<", pitentry name: "<<pitEntry->getName() << ", pit-token = "<<readInterestPitToken(interest));
  
  if (interest.isReflexiveInterestFromProducer()) //reflexive interest from producer?
  {
    NFD_LOG_DEBUG("Reflexvie interest from producer detected! "<<interest);
    const fib::Entry& fibEntry = this->lookupFib(*pitEntry);
    const fib::NextHopList& nexthops = fibEntry.getNextHops();
    NFD_LOG_DEBUG("For RF's original Interest Nexthops empty?"<<nexthops.empty());

    auto it = pitEntry->getInRecords().begin();
    if (it == pitEntry->getInRecords().end()) {
      NFD_LOG_INTEREST_FROM(interest, ingress, "new no-nexthop");
      lp::NackHeader nackHeader;
      nackHeader.setReason(lp::NackReason::NO_ROUTE);
      this->sendNack(nackHeader, ingress.face, pitEntry);
      this->rejectPendingInterest(pitEntry);
      return;
    }

    Face& outFace = it->getFace();
    NFD_LOG_DEBUG("Reflexive interest find original ingress, sending...!");
    NFD_LOG_INTEREST_FROM(interest, ingress, "new to=" << outFace.getId());
    this->sendInterest(interest, outFace, pitEntry); 
    // outFace.sendInterest(interest);
    return;
  }


  auto suppression = m_retxSuppression->decidePerPitEntry(*pitEntry);
  if (suppression == RetxSuppressionResult::SUPPRESS) {
    NFD_LOG_INTEREST_FROM(interest, ingress, "suppressed");
    return;
  }

  const fib::Entry& fibEntry = this->lookupFib(*pitEntry);
  const fib::NextHopList& nexthops = fibEntry.getNextHops();
  auto it = nexthops.end();

  if (suppression == RetxSuppressionResult::NEW) {
    // forward to nexthop with lowest cost except downstream
    it = std::find_if(nexthops.begin(), nexthops.end(), [&] (const auto& nexthop) {
      return isNextHopEligible(ingress.face, interest, nexthop, pitEntry);
    });

    if (it == nexthops.end()) {
      NFD_LOG_INTEREST_FROM(interest, ingress, "new no-nexthop");
      lp::NackHeader nackHeader;
      nackHeader.setReason(lp::NackReason::NO_ROUTE);
      this->sendNack(nackHeader, ingress.face, pitEntry);
      this->rejectPendingInterest(pitEntry);
      return;
    }

    Face& outFace = it->getFace();
    NFD_LOG_INTEREST_FROM(interest, ingress, "new to=" << outFace.getId());
    this->sendInterest(interest, outFace, pitEntry);
    return;
  }

  // find an unused upstream with lowest cost except downstream
  it = std::find_if(nexthops.begin(), nexthops.end(),
                    [&, now = time::steady_clock::now()] (const auto& nexthop) {
                      return isNextHopEligible(ingress.face, interest, nexthop, pitEntry, true, now);
                    });

  if (it != nexthops.end()) {
    Face& outFace = it->getFace();
    this->sendInterest(interest, outFace, pitEntry);
    NFD_LOG_INTEREST_FROM(interest, ingress, "retx unused-to=" << outFace.getId());
    return;
  }

  // find an eligible upstream that is used earliest
  it = findEligibleNextHopWithEarliestOutRecord(ingress.face, interest, nexthops, pitEntry);
  if (it == nexthops.end()) {
    NFD_LOG_INTEREST_FROM(interest, ingress, "retx no-nexthop");
  }
  else {
    Face& outFace = it->getFace();
    this->sendInterest(interest, outFace, pitEntry);
    NFD_LOG_INTEREST_FROM(interest, ingress, "retx retry-to=" << outFace.getId());
  }
}

void
BestRouteStrategy::afterReceiveNack(const lp::Nack& nack, const FaceEndpoint& ingress,
                                    const shared_ptr<pit::Entry>& pitEntry)
{
  this->processNack(nack, ingress.face, pitEntry);
}

} // namespace nfd::fw
