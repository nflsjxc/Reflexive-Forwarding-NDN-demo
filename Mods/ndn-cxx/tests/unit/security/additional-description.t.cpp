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

#include "ndn-cxx/security/additional-description.hpp"
#include "ndn-cxx/util/concepts.hpp"

#include "tests/boost-test.hpp"

namespace ndn::tests {

using ndn::security::AdditionalDescription;

BOOST_CONCEPT_ASSERT((boost::EqualityComparable<AdditionalDescription>));
BOOST_CONCEPT_ASSERT((WireEncodable<AdditionalDescription>));
BOOST_CONCEPT_ASSERT((WireEncodableWithEncodingBuffer<AdditionalDescription>));
BOOST_CONCEPT_ASSERT((WireDecodable<AdditionalDescription>));
static_assert(std::is_convertible_v<AdditionalDescription::Error*, tlv::Error*>,
              "AdditionalDescription::Error must inherit from tlv::Error");

BOOST_AUTO_TEST_SUITE(Security)
BOOST_AUTO_TEST_SUITE(TestAdditionalDescription)

const uint8_t DESC[] = {
  0xfd, 0x01, 0x02, 0x28,
    0xfd, 0x02, 0x00, 0x10, // DescriptionEntry
      0xfd, 0x02, 0x01, 0x04, // DescriptionKey
        0x6b, 0x65, 0x79, 0x31, // "key1"
      0xfd, 0x02, 0x02, 0x04, // DescriptionValue
        0x76, 0x61, 0x6c, 0x31, // "val1"
    0xfd, 0x02, 0x00, 0x10, // DescriptionEntry
      0xfd, 0x02, 0x01, 0x04, // DescriptionKey
        0x6b, 0x65, 0x79, 0x32, // "key2"
      0xfd, 0x02, 0x02, 0x04, // DescriptionValue
        0x76, 0x61, 0x6c, 0x32, // "val2"
};

BOOST_AUTO_TEST_CASE(Basic)
{
  AdditionalDescription aDescription;

  aDescription.set("key2", "val2");
  aDescription.set("key1", "val1");

  BOOST_REQUIRE_NO_THROW(aDescription.get("key1"));
  BOOST_REQUIRE_NO_THROW(aDescription.get("key2"));
  BOOST_REQUIRE_THROW(aDescription.get("key3"), AdditionalDescription::Error);

  BOOST_CHECK_EQUAL(aDescription.has("key1"), true);
  BOOST_CHECK_EQUAL(aDescription.has("key2"), true);
  BOOST_CHECK_EQUAL(aDescription.has("key3"), false);

  auto val1 = aDescription.get("key1");
  auto val2 = aDescription.get("key2");

  BOOST_CHECK_EQUAL(val1, "val1");
  BOOST_CHECK_EQUAL(val2, "val2");

  auto it = aDescription.begin();
  BOOST_CHECK_EQUAL(it->second, "val1");
  it++;
  BOOST_CHECK_EQUAL(it->second, "val2");
  it++;
  BOOST_CHECK(it == aDescription.end());

  BOOST_TEST(aDescription.wireEncode() == DESC, boost::test_tools::per_element());

  AdditionalDescription aDescription2(Block{DESC});
  BOOST_CHECK_EQUAL(aDescription2, aDescription);

  AdditionalDescription aDescription3;
  aDescription3.set("key3", "val3");
  aDescription3.set("key2", "val2");

  BOOST_CHECK_NE(aDescription2, aDescription3);

  std::ostringstream os;
  os << aDescription;
  BOOST_CHECK_EQUAL(os.str(), "[(key1:val1), (key2:val2)]");
}

BOOST_AUTO_TEST_SUITE_END() // TestAdditionalDescription
BOOST_AUTO_TEST_SUITE_END() // Security

} // namespace ndn::tests
