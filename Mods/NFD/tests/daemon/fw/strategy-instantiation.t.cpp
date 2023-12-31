/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014-2022,  Regents of the University of California,
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

/** \file
 *  This test suite tests instantiation logic in strategies.
 */

// All strategies, sorted alphabetically.
#include "fw/access-strategy.hpp"
#include "fw/asf-strategy.hpp"
#include "fw/best-route-strategy.hpp"
#include "fw/multicast-strategy.hpp"
#include "fw/self-learning-strategy.hpp"
#include "fw/random-strategy.hpp"

#include "tests/test-common.hpp"

#include <boost/mpl/vector.hpp>

namespace nfd::tests {

using namespace nfd::fw;

BOOST_AUTO_TEST_SUITE(Fw)
BOOST_AUTO_TEST_SUITE(TestStrategyInstantiation)

template<typename S, bool CanAcceptParameters, uint64_t MinVersion>
class Test
{
public:
  using Strategy = S;

  static bool
  canAcceptParameters()
  {
    return CanAcceptParameters;
  }

  static uint64_t
  getMinVersion()
  {
    return MinVersion;
  }

  static Name
  getVersionedStrategyName(uint64_t version)
  {
    return S::getStrategyName().getPrefix(-1).appendVersion(version);
  }
};

using Tests = boost::mpl::vector<
  Test<AccessStrategy, false, 1>,
  Test<AsfStrategy, true, 4>,
  Test<BestRouteStrategy, true, 5>,
  Test<MulticastStrategy, true, 4>,
  Test<SelfLearningStrategy, false, 1>,
  Test<RandomStrategy, false, 1>
>;

BOOST_AUTO_TEST_CASE_TEMPLATE(Registration, T, Tests)
{
  BOOST_CHECK_EQUAL(Strategy::listRegistered().count(T::Strategy::getStrategyName()), 1);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(InstanceName, T, Tests)
{
  BOOST_REQUIRE(T::Strategy::getStrategyName().at(-1).isVersion());
  uint64_t maxVersion = T::Strategy::getStrategyName().at(-1).toVersion();
  BOOST_REQUIRE_LE(T::getMinVersion(), maxVersion);

  FaceTable faceTable;
  Forwarder forwarder(faceTable);
  for (uint64_t version = T::getMinVersion(); version <= maxVersion; ++version) {
    Name versionedName = T::getVersionedStrategyName(version);
    unique_ptr<typename T::Strategy> instance;
    BOOST_CHECK_NO_THROW(instance = make_unique<typename T::Strategy>(forwarder, versionedName));
    BOOST_CHECK_EQUAL(instance->getInstanceName(), versionedName);

    if (!T::canAcceptParameters()) {
      Name nameWithParameters = Name(versionedName).append("param");
      BOOST_CHECK_THROW(typename T::Strategy(forwarder, nameWithParameters), std::invalid_argument);
    }
  }

  if (T::getMinVersion() > 0) {
    Name version0Name = T::getVersionedStrategyName(0);
    BOOST_CHECK_THROW(typename T::Strategy(forwarder, version0Name), std::invalid_argument);
    Name earlyVersionName = T::getVersionedStrategyName(T::getMinVersion() - 1);
    BOOST_CHECK_THROW(typename T::Strategy(forwarder, earlyVersionName), std::invalid_argument);
  }

  if (maxVersion < std::numeric_limits<uint64_t>::max()) {
    Name versionMaxName = T::getVersionedStrategyName(std::numeric_limits<uint64_t>::max());
    BOOST_CHECK_THROW(typename T::Strategy(forwarder, versionMaxName), std::invalid_argument);
    Name lateVersionName = T::getVersionedStrategyName(maxVersion + 1);
    BOOST_CHECK_THROW(typename T::Strategy(forwarder, lateVersionName), std::invalid_argument);
  }
}

template<typename S>
class SuppressionParametersFixture
{
public:
  std::unique_ptr<S>
  checkValidity(const std::string& parameters, bool isCorrect)
  {
    Name strategyName(Name(S::getStrategyName()).append(parameters));
    std::unique_ptr<S> strategy;
    BOOST_TEST_CONTEXT(parameters) {
      if (isCorrect) {
        strategy = make_unique<S>(m_forwarder, strategyName);
        BOOST_CHECK(strategy->m_retxSuppression != nullptr);
      }
      else {
        BOOST_CHECK_THROW(make_unique<S>(m_forwarder, strategyName), std::invalid_argument);
      }
    }
    return strategy;
  }

private:
  FaceTable m_faceTable;
  Forwarder m_forwarder{m_faceTable};
};

using StrategiesWithRetxSuppressionExponential = boost::mpl::vector<
  AsfStrategy,
  BestRouteStrategy,
  MulticastStrategy
>;

BOOST_FIXTURE_TEST_CASE_TEMPLATE(SuppressionParameters, S, StrategiesWithRetxSuppressionExponential,
                                 SuppressionParametersFixture<S>)
{
  auto strategy = this->checkValidity("", true);
  BOOST_TEST(strategy->m_retxSuppression->m_initialInterval == RetxSuppressionExponential::DEFAULT_INITIAL_INTERVAL);
  BOOST_TEST(strategy->m_retxSuppression->m_maxInterval == RetxSuppressionExponential::DEFAULT_MAX_INTERVAL);
  BOOST_TEST(strategy->m_retxSuppression->m_multiplier == RetxSuppressionExponential::DEFAULT_MULTIPLIER);

  strategy = this->checkValidity("/retx-suppression-initial~20", true);
  BOOST_TEST(strategy->m_retxSuppression->m_initialInterval == 20_ms);
  BOOST_TEST(strategy->m_retxSuppression->m_maxInterval == RetxSuppressionExponential::DEFAULT_MAX_INTERVAL);
  BOOST_TEST(strategy->m_retxSuppression->m_multiplier == RetxSuppressionExponential::DEFAULT_MULTIPLIER);
  this->checkValidity("/retx-suppression-initial~0", false);
  this->checkValidity("/retx-suppression-initial~20.5", false);
  this->checkValidity("/retx-suppression-initial~-10", false);
  this->checkValidity("/retx-suppression-initial~ -5", false);
  this->checkValidity("/retx-suppression-initial~NaN", false);

  strategy = this->checkValidity("/retx-suppression-max~1000", true);
  BOOST_TEST(strategy->m_retxSuppression->m_initialInterval == RetxSuppressionExponential::DEFAULT_INITIAL_INTERVAL);
  BOOST_TEST(strategy->m_retxSuppression->m_maxInterval == 1_s);
  BOOST_TEST(strategy->m_retxSuppression->m_multiplier == RetxSuppressionExponential::DEFAULT_MULTIPLIER);
  strategy = this->checkValidity("/retx-suppression-initial~40/retx-suppression-max~500", true);
  BOOST_TEST(strategy->m_retxSuppression->m_initialInterval == 40_ms);
  BOOST_TEST(strategy->m_retxSuppression->m_maxInterval == 500_ms);
  this->checkValidity("/retx-suppression-initial~20/retx-suppression-max~10", false);
  this->checkValidity("/retx-suppression-max~ 500", false);
  this->checkValidity("/retx-suppression-max~521.5", false);

  strategy = this->checkValidity("/retx-suppression-multiplier~2.25", true);
  BOOST_TEST(strategy->m_retxSuppression->m_initialInterval == RetxSuppressionExponential::DEFAULT_INITIAL_INTERVAL);
  BOOST_TEST(strategy->m_retxSuppression->m_maxInterval == RetxSuppressionExponential::DEFAULT_MAX_INTERVAL);
  BOOST_TEST(strategy->m_retxSuppression->m_multiplier == 2.25);
  this->checkValidity("/retx-suppression-multiplier~0", false);
  this->checkValidity("/retx-suppression-multiplier~0.9", false);
  this->checkValidity("/retx-suppression-multiplier~-2.1", false);
  this->checkValidity("/retx-suppression-multiplier~foo", false);

  strategy = this->checkValidity("/retx-suppression-initial~20/retx-suppression-max~500/retx-suppression-multiplier~3",
                                 true);
  BOOST_TEST(strategy->m_retxSuppression->m_initialInterval == 20_ms);
  BOOST_TEST(strategy->m_retxSuppression->m_maxInterval == 500_ms);
  BOOST_TEST(strategy->m_retxSuppression->m_multiplier == 3);

  this->checkValidity("/foo~42", true); // unknown parameters are ignored
}

BOOST_AUTO_TEST_SUITE_END() // TestStrategyInstantiation
BOOST_AUTO_TEST_SUITE_END() // Fw

} // namespace nfd::tests
