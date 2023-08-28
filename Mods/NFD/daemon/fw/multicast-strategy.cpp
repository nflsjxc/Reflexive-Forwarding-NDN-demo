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

#include "multicast-strategy.hpp"
#include "algorithm.hpp"
#include "common/logger.hpp"

namespace nfd::fw {

NFD_REGISTER_STRATEGY(MulticastStrategy);

NFD_LOG_INIT(MulticastStrategy);

MulticastStrategy::MulticastStrategy(Forwarder& forwarder, const Name& name)
  : Strategy(forwarder)
{
  ParsedInstanceName parsed = parseInstanceName(name);
  if (parsed.version && *parsed.version != getStrategyName()[-1].toVersion()) {
    NDN_THROW(std::invalid_argument(
      "MulticastStrategy does not support version " + to_string(*parsed.version)));
  }

  StrategyParameters params = parseParameters(parsed.parameters);
  m_retxSuppression = RetxSuppressionExponential::construct(params);

  this->setInstanceName(makeInstanceName(name, getStrategyName()));

  NDN_LOG_DEBUG(*m_retxSuppression);
}

const Name&
MulticastStrategy::getStrategyName()
{
  static const auto strategyName = Name("/localhost/nfd/strategy/multicast").appendVersion(4);
  return strategyName;
}

void
MulticastStrategy::afterReceiveInterest(const Interest& interest, const FaceEndpoint& ingress,
                                        const shared_ptr<pit::Entry>& pitEntry)
{
  const fib::Entry& fibEntry = this->lookupFib(*pitEntry);
  const fib::NextHopList& nexthops = fibEntry.getNextHops();

  for (const auto& nexthop : nexthops) {
    Face& outFace = nexthop.getFace();

    auto suppressResult = m_retxSuppression->decidePerUpstream(*pitEntry, outFace);
    if (suppressResult == RetxSuppressionResult::SUPPRESS) {
      NFD_LOG_INTEREST_FROM(interest, ingress, "to=" << outFace.getId() << " suppressed");
      continue;
    }

    if (!isNextHopEligible(ingress.face, interest, nexthop, pitEntry)) {
      continue;
    }

    NFD_LOG_INTEREST_FROM(interest, ingress, "to=" << outFace.getId());
    auto* sentOutRecord = this->sendInterest(interest, outFace, pitEntry);
    if (sentOutRecord && suppressResult == RetxSuppressionResult::FORWARD) {
      m_retxSuppression->incrementIntervalForOutRecord(*sentOutRecord);
    }
  }
}

void
MulticastStrategy::afterNewNextHop(const fib::NextHop& nextHop,
                                   const shared_ptr<pit::Entry>& pitEntry)
{
  // no need to check for suppression, as it is a new next hop

  auto nextHopFaceId = nextHop.getFace().getId();
  const auto& interest = pitEntry->getInterest();

  // try to find an incoming face record that doesn't violate scope restrictions
  for (const auto& r : pitEntry->getInRecords()) {
    auto& inFace = r.getFace();

    if (isNextHopEligible(inFace, interest, nextHop, pitEntry)) {
      NFD_LOG_INTEREST_FROM(interest, inFace.getId(), "new-nexthop to=" << nextHopFaceId);
      this->sendInterest(interest, nextHop.getFace(), pitEntry);
      break; // just one eligible incoming face record is enough
    }
  }

  // if nothing found, the interest will not be forwarded
}

} // namespace nfd::fw
