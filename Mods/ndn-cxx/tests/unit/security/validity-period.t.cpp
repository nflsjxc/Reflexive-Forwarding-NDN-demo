/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2013-2023 Regents of the University of California.
 *
 * This file is part of ndn-cxx library (NDN C++ library with eXperimental eXtensions).
 *
 * ndn-cxx library is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later version.
 *
 * ndn-cxx library is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.
 *
 * You should have received copies of the GNU General Public License and GNU Lesser
 * General Public License along with ndn-cxx, e.g., in COPYING.md file.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * See AUTHORS.md for complete list of ndn-cxx authors and contributors.
 */

#include "ndn-cxx/security/validity-period.hpp"
#include "ndn-cxx/util/concepts.hpp"

#include "tests/boost-test.hpp"
#include "tests/unit/clock-fixture.hpp"

#include <boost/lexical_cast.hpp>

namespace ndn::tests {

using ndn::security::ValidityPeriod;

BOOST_CONCEPT_ASSERT((boost::EqualityComparable<ValidityPeriod>));
BOOST_CONCEPT_ASSERT((WireEncodable<ValidityPeriod>));
BOOST_CONCEPT_ASSERT((WireEncodableWithEncodingBuffer<ValidityPeriod>));
BOOST_CONCEPT_ASSERT((WireDecodable<ValidityPeriod>));
static_assert(std::is_convertible_v<ValidityPeriod::Error*, tlv::Error*>,
              "ValidityPeriod::Error must inherit from tlv::Error");

BOOST_AUTO_TEST_SUITE(Security)
BOOST_AUTO_TEST_SUITE(TestValidityPeriod)

BOOST_AUTO_TEST_SUITE(MakeRelative)

BOOST_AUTO_TEST_CASE(FromNow)
{
  auto vp = ValidityPeriod::makeRelative(-1_s, 365_days, time::fromIsoString("20091117T203458,651387237"));
  auto period = vp.getPeriod();
  BOOST_CHECK_EQUAL(period.first, time::fromIsoString("20091117T203458"));
  BOOST_CHECK_EQUAL(period.second, time::fromIsoString("20101117T203458"));
}

BOOST_AUTO_TEST_CASE(Positive)
{
  auto vp = ValidityPeriod::makeRelative(10_s, 1_days, time::fromIsoString("20091117T203458,651387237"));
  auto period = vp.getPeriod();
  BOOST_CHECK_EQUAL(period.first, time::fromIsoString("20091117T203509"));
  BOOST_CHECK_EQUAL(period.second, time::fromIsoString("20091118T203458"));
}

BOOST_AUTO_TEST_CASE(Negative)
{
  auto vp = ValidityPeriod::makeRelative(-1_days, -10_s, time::fromIsoString("20091117T203458,651387237"));
  auto period = vp.getPeriod();
  BOOST_CHECK_EQUAL(period.first, time::fromIsoString("20091116T203459"));
  BOOST_CHECK_EQUAL(period.second, time::fromIsoString("20091117T203448"));
}

BOOST_AUTO_TEST_SUITE_END() // MakeRelative

BOOST_FIXTURE_TEST_CASE(ConstructorSetter, ClockFixture)
{
  auto now = m_systemClock->getNow();
  auto notBefore = now - 1_day;
  auto notAfter = notBefore + 2_days;
  ValidityPeriod validity1(notBefore, notAfter);

  auto period = validity1.getPeriod();
  BOOST_CHECK_GE(period.first, notBefore); // fractional seconds will be removed
  BOOST_CHECK_LT(period.first, notBefore + 1_s);

  BOOST_CHECK_LE(period.second, notAfter); // fractional seconds will be removed
  BOOST_CHECK_GT(period.second, notAfter - 1_s);
  BOOST_CHECK_EQUAL(validity1.isValid(), true);

  BOOST_CHECK_EQUAL(ValidityPeriod(now - 2_days, now - 1_day).isValid(), false);

  BOOST_CHECK_NO_THROW((ValidityPeriod()));
  ValidityPeriod validity2;
  BOOST_CHECK_EQUAL(validity2.isValid(), false);

  validity2.setPeriod(notBefore, notAfter);
  BOOST_CHECK(validity2.getPeriod() != std::pair(time::getUnixEpoch(), time::getUnixEpoch()));
  BOOST_CHECK_EQUAL(validity2, validity1);

  validity1.setPeriod(time::getUnixEpoch(), time::getUnixEpoch() + 10 * 365_days);
  BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(validity1),
                    "(19700101T000000, 19791230T000000)");

  validity1.setPeriod(time::getUnixEpoch() + 1_ns,
                      time::getUnixEpoch() + 3650_days + 1_ns);
  BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(validity1),
                    "(19700101T000001, 19791230T000000)");

  validity1.setPeriod(time::getUnixEpoch() + 999999999_ns,
                      time::getUnixEpoch() + 3650_days + 999999999_ns);
  BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(validity1),
                    "(19700101T000001, 19791230T000000)");

  validity1.setPeriod(time::getUnixEpoch() - 2_days + 1_ns,
                      time::getUnixEpoch() - 1_day + 1_ns);
  BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(validity1),
                    "(19691230T000001, 19691231T000000)");

  validity1.setPeriod(time::getUnixEpoch() - 2_days + 999999999_ns,
                      time::getUnixEpoch() - 1_day + 999999999_ns);
  BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(validity1),
                    "(19691230T000001, 19691231T000000)");

  BOOST_CHECK_EQUAL(ValidityPeriod(now, now).isValid(), true);
  BOOST_CHECK_EQUAL(ValidityPeriod(now + 1_s, now).isValid(), false);
}

const uint8_t VP1[] = {
  0xfd, 0x00, 0xfd, 0x26, // ValidityPeriod
    0xfd, 0x00, 0xfe, 0x0f, // NotBefore
      0x31, 0x39, 0x37, 0x30, 0x30, 0x31, 0x30, 0x31, // 19700101T000000
      0x54, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
    0xfd, 0x00, 0xff, 0x0f, // NotAfter
      0x31, 0x39, 0x37, 0x30, 0x30, 0x31, 0x30, 0x32, // 19700102T000000
      0x54, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30
};

BOOST_AUTO_TEST_CASE(EncodingDecoding)
{
  time::system_clock::time_point notBefore = time::getUnixEpoch();
  time::system_clock::time_point notAfter = notBefore + 1_day;
  ValidityPeriod v1(notBefore, notAfter);
  BOOST_CHECK_EQUAL_COLLECTIONS(v1.wireEncode().begin(), v1.wireEncode().end(),
                                VP1, VP1 + sizeof(VP1));

  ValidityPeriod v2(Block{VP1});
  BOOST_CHECK(v1.getPeriod() == v2.getPeriod());
}

const uint8_t VP2[] = {
  0xfd, 0x00, 0xfd, 0x26, // ValidityPeriod
    0xfd, 0x00, 0xfe, 0x0f, // NotBefore
      0x30, 0x30, 0x30, 0x31, 0x30, 0x31, 0x30, 0x31, // 00010101T000000
      0x54, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
    0xfd, 0x00, 0xff, 0x0f, // NotAfter
      0x39, 0x39, 0x39, 0x39, 0x31, 0x32, 0x33, 0x31, // 99991231T235959
      0x54, 0x32, 0x33, 0x35, 0x39, 0x35, 0x39
};

BOOST_AUTO_TEST_CASE(DecodingLarge)
{
  ValidityPeriod v(Block{VP2});
  BOOST_CHECK(v.isValid(time::fromIsoString("16770921T001245")));
  BOOST_CHECK(v.isValid(time::fromIsoString("19010120T120000")));
  BOOST_CHECK(v.isValid(time::fromIsoString("20230725T120000")));
  BOOST_CHECK(v.isValid(time::fromIsoString("22001030T120000")));
  BOOST_CHECK(v.isValid(time::fromIsoString("22620411T234716")));
}

const uint8_t VP_E1[] = {
  0xfd, 0x00, 0xff, 0x26, // ValidityPeriod (wrong TLV-TYPE)
    0xfd, 0x00, 0xfe, 0x0f, // NotBefore
      0x31, 0x39, 0x37, 0x30, 0x30, 0x31, 0x30, 0x31, // 19700101T000000
      0x54, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
    0xfd, 0x00, 0xff, 0x0f, // NotAfter
      0x31, 0x39, 0x37, 0x30, 0x30, 0x31, 0x30, 0x32, // 19700102T000000
      0x54, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30
};

const uint8_t VP_E2[] = {
  0xfd, 0x00, 0xfd, 0x26, // ValidityPeriod
    0xfd, 0x00, 0xff, 0x0f, // NotBefore (wrong TLV-TYPE)
      0x31, 0x39, 0x37, 0x30, 0x30, 0x31, 0x30, 0x31, // 19700101T000000
      0x54, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
    0xfd, 0x00, 0xff, 0x0f, // NotAfter
      0x31, 0x39, 0x37, 0x30, 0x30, 0x31, 0x30, 0x32, // 19700102T000000
      0x54, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30
};

const uint8_t VP_E3[] = {
  0xfd, 0x00, 0xfd, 0x26, // ValidityPeriod
    0xfd, 0x00, 0xfe, 0x0f, // NotBefore
      0x31, 0x39, 0x37, 0x30, 0x30, 0x31, 0x30, 0x31, // 19700101T000000
      0x54, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
    0xfd, 0x00, 0xfe, 0x0f, // NotAfter (wrong TLV-TYPE)
      0x31, 0x39, 0x37, 0x30, 0x30, 0x31, 0x30, 0x32, // 19700102T000000
      0x54, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30
};

const uint8_t VP_E4[] = {
  0xfd, 0x00, 0xfd, 0x39, // ValidityPeriod
    0xfd, 0x00, 0xfe, 0x0f, // NotBefore
      0x31, 0x39, 0x37, 0x30, 0x30, 0x31, 0x30, 0x31, // 19700101T000000
      0x54, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
    0xfd, 0x00, 0xff, 0x0f, // NotAfter
      0x31, 0x39, 0x37, 0x30, 0x30, 0x31, 0x30, 0x32, // 19700102T000000
      0x54, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
    0xfd, 0x00, 0xff, 0x0f, // NotAfter (duplicate)
      0x31, 0x39, 0x37, 0x30, 0x30, 0x31, 0x30, 0x32, // 19700102T000000
      0x54, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30
};

const uint8_t VP_E5[] = {
  0xfd, 0x00, 0xfd, 0x13, // ValidityPeriod
    0xfd, 0x00, 0xfe, 0x0f, // NotBefore
      0x31, 0x39, 0x37, 0x30, 0x30, 0x31, 0x30, 0x31, // 19700101T000000
      0x54, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30
      // missing NotAfter
};

const uint8_t VP_E6[] = {
  0xfd, 0x00, 0xfd, 0x26, // ValidityPeriod
    0xfd, 0x00, 0xfe, 0x0f, // NotBefore
      0x31, 0x39, 0x37, 0x30, 0x30, 0x31, 0x30, 0x31, // 19700101T00000\xFF
      0x54, 0x30, 0x30, 0x30, 0x30, 0x30, 0xFF,
    0xfd, 0x00, 0xff, 0x0f, // NotAfter
      0x31, 0x39, 0x37, 0x30, 0x30, 0x31, 0x30, 0x32, // 19700102T000000
      0x54, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30
};

BOOST_AUTO_TEST_CASE(DecodingError)
{
  BOOST_CHECK_THROW(ValidityPeriod(Block{VP_E1}), ValidityPeriod::Error);
  BOOST_CHECK_THROW(ValidityPeriod(Block{VP_E2}), ValidityPeriod::Error);
  BOOST_CHECK_THROW(ValidityPeriod(Block{VP_E3}), ValidityPeriod::Error);
  BOOST_CHECK_THROW(ValidityPeriod(Block{VP_E4}), ValidityPeriod::Error);
  BOOST_CHECK_THROW(ValidityPeriod(Block{VP_E5}), ValidityPeriod::Error);
  BOOST_CHECK_THROW(ValidityPeriod(Block{VP_E6}), ValidityPeriod::Error);

  Block emptyBlock;
  BOOST_CHECK_THROW(ValidityPeriod{emptyBlock}, ValidityPeriod::Error);
}

BOOST_AUTO_TEST_CASE(Comparison)
{
  auto notBefore = time::getUnixEpoch();
  auto notAfter = notBefore + 1_day;
  auto notAfter2 = notBefore + 2_days;

  ValidityPeriod validity1(notBefore, notAfter);
  ValidityPeriod validity2(notBefore, notAfter);
  BOOST_CHECK(validity1 == validity2);

  ValidityPeriod validity3(notBefore, notAfter2);
  BOOST_CHECK(validity1 != validity3);
}

BOOST_AUTO_TEST_SUITE_END() // TestValidityPeriod
BOOST_AUTO_TEST_SUITE_END() // Security

} // namespace ndn::tests
