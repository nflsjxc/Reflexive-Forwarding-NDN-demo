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
 *
 * @author Jeff Thompson <jefft0@remap.ucla.edu>
 * @author Alexander Afanasyev <http://lasr.cs.ucla.edu/afanasyev/index.html>
 * @author Zhenkai Zhu <http://irl.cs.ucla.edu/~zhenkai/>
 */

#include "ndn-cxx/name.hpp"
#include "ndn-cxx/encoding/block.hpp"
#include "ndn-cxx/encoding/encoding-buffer.hpp"
#include "ndn-cxx/util/time.hpp"

#include <sstream>
#include <boost/functional/hash.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include <iostream>

namespace ndn {

// ---- constructors, encoding, decoding ----

Name::Name() = default;

Name::Name(const Block& wire)
  : m_wire(wire)
{
  m_wire.parse();
}

Name::Name(std::string_view uri, bool isRN)
{
 if (isRN)
 {
  if (uri.empty())
    return;

  if (size_t iColon = uri.find(':'); iColon != std::string_view::npos) {
    // Make sure the colon came before a '/', if any.
    size_t iFirstSlash = uri.find('/');
    if (iFirstSlash == std::string_view::npos || iColon < iFirstSlash) {
      // Strip the leading protocol such as "ndn:".
      uri.remove_prefix(iColon + 1);
    }
  }

  // Trim the leading slash and possibly the authority.
  if (uri.size() >= 1 && uri[0] == '/') {
    if (uri.size() >= 2 && uri[1] == '/') {
      // Strip the authority following "//".
      size_t iAfterAuthority = uri.find('/', 2);
      if (iAfterAuthority == std::string_view::npos) {
        // Unusual case: there was only an authority.
        return;
      }
      else {
        uri.remove_prefix(iAfterAuthority + 1);
      }
    }
    else {
      uri.remove_prefix(1);
    }
  }

  // Unescape the components.
  while (!uri.empty()) {
    auto component = uri.substr(0, uri.find('/'));
    if (component.size() + 1 >= uri.size()) {
      // We reached the end of the string.
      append(Component::RNfromEscapedString(component));
      // std::cerr<<"name mwire: "<<m_wire<<"\n";
      return;
    }
    else
    {
      append(Component::fromEscapedString(component));
    }
    uri.remove_prefix(component.size() + 1);
  }
 }
 else
 {
  *this = Name(uri);
 }
}

Name::Name(std::string_view uri)
{
  if (uri.empty())
    return;

  if (size_t iColon = uri.find(':'); iColon != std::string_view::npos) {
    // Make sure the colon came before a '/', if any.
    size_t iFirstSlash = uri.find('/');
    if (iFirstSlash == std::string_view::npos || iColon < iFirstSlash) {
      // Strip the leading protocol such as "ndn:".
      uri.remove_prefix(iColon + 1);
    }
  }

  // Trim the leading slash and possibly the authority.
  if (uri.size() >= 1 && uri[0] == '/') {
    if (uri.size() >= 2 && uri[1] == '/') {
      // Strip the authority following "//".
      size_t iAfterAuthority = uri.find('/', 2);
      if (iAfterAuthority == std::string_view::npos) {
        // Unusual case: there was only an authority.
        return;
      }
      else {
        uri.remove_prefix(iAfterAuthority + 1);
      }
    }
    else {
      uri.remove_prefix(1);
    }
  }

  // Unescape the components.
  while (!uri.empty()) {
    auto component = uri.substr(0, uri.find('/'));
    append(Component::fromEscapedString(component));
    if (component.size() + 1 >= uri.size()) {
      // We reached the end of the string.
      // std::cerr<<"Mwire: "<< m_wire<<"\n";  
      return;
    }
    uri.remove_prefix(component.size() + 1);
  }
}

template<encoding::Tag TAG>
size_t
Name::wireEncode(EncodingImpl<TAG>& encoder) const
{
  size_t totalLength = 0;
  for (const Component& comp : *this | boost::adaptors::reversed) {
    totalLength += comp.wireEncode(encoder);
  }

  totalLength += encoder.prependVarNumber(totalLength);
  totalLength += encoder.prependVarNumber(tlv::Name);
  return totalLength;
}

NDN_CXX_DEFINE_WIRE_ENCODE_INSTANTIATIONS(Name);

const Block&
Name::wireEncode() const
{
  if (m_wire.hasWire())
    return m_wire;

  EncodingEstimator estimator;
  size_t estimatedSize = wireEncode(estimator);

  EncodingBuffer buffer(estimatedSize, 0);
  wireEncode(buffer);

  m_wire = buffer.block();
  m_wire.parse();

  return m_wire;
}

void
Name::wireDecode(const Block& wire)
{
  if (wire.type() != tlv::Name)
    NDN_THROW(tlv::Error("Name", wire.type()));

  m_wire = wire;
  m_wire.parse();
}

Name
Name::deepCopy() const
{
  Name copiedName(*this);
  copiedName.m_wire.resetWire();
  copiedName.wireEncode(); // "compress" the underlying buffer
  return copiedName;
}

// ---- accessors ----

const name::Component&
Name::at(ssize_t i) const
{
  auto ssize = static_cast<ssize_t>(size());
  if (i < -ssize || i >= ssize) {
    NDN_THROW(Error("Component at offset " + to_string(i) + " does not exist (out of bounds)"));
  }

  if (i < 0) {
    i += ssize;
  }
  return static_cast<const Component&>(m_wire.elements()[static_cast<size_t>(i)]);
}

PartialName
Name::getSubName(ssize_t iStartComponent, size_t nComponents) const
{
  PartialName result;

  if (iStartComponent < 0)
    iStartComponent += static_cast<ssize_t>(size());
  size_t iStart = iStartComponent < 0 ? 0 : static_cast<size_t>(iStartComponent);

  size_t iEnd = size();
  if (nComponents != npos)
    iEnd = std::min(size(), iStart + nComponents);

  for (size_t i = iStart; i < iEnd; ++i)
    result.append(at(i));

  return result;
}

// ---- modifiers ----

Name&
Name::set(ssize_t i, const Component& component)
{
  if (i < 0) {
    i += static_cast<ssize_t>(size());
  }

  const_cast<Block::element_container&>(m_wire.elements())[i] = component;
  m_wire.resetWire();
  return *this;
}

Name&
Name::set(ssize_t i, Component&& component)
{
  if (i < 0) {
    i += static_cast<ssize_t>(size());
  }

  const_cast<Block::element_container&>(m_wire.elements())[i] = std::move(component);
  m_wire.resetWire();
  return *this;
}

Name&
Name::appendVersion(const std::optional<uint64_t>& version)
{
  return append(Component::fromVersion(version.value_or(time::toUnixTimestamp(time::system_clock::now()).count())));
}

Name&
Name::appendTimestamp(const std::optional<time::system_clock::time_point>& timestamp)
{
  return append(Component::fromTimestamp(timestamp.value_or(time::system_clock::now())));
}

Name&
Name::append(const PartialName& name)
{
  if (&name == this) {
    // Copying from this name, so need to make a copy first.
    return append(PartialName(name));
  }

  for (const auto& c : name) {
    append(c);
  }
  return *this;
}

static constexpr uint8_t SHA256_OF_EMPTY_STRING[] = {
  0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14,
  0x9a, 0xfb, 0xf4, 0xc8, 0x99, 0x6f, 0xb9, 0x24,
  0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c,
  0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55,
};

Name&
Name::appendParametersSha256DigestPlaceholder()
{
  static const Component placeholder(tlv::ParametersSha256DigestComponent, SHA256_OF_EMPTY_STRING);
  return append(placeholder);
}

void
Name::erase(ssize_t i)
{
  if (i >= 0) {
    m_wire.erase(std::next(m_wire.elements_begin(), i));
  }
  else {
    m_wire.erase(std::prev(m_wire.elements_end(), -i));
  }
}

void
Name::clear()
{
  m_wire = Block(tlv::Name);
}

// ---- algorithms ----

Name
Name::getSuccessor() const
{
  if (empty()) {
    static const Name n("/sha256digest=0000000000000000000000000000000000000000000000000000000000000000");
    return n;
  }

  return getPrefix(-1).append(get(-1).getSuccessor());
}

bool
Name::isPrefixOf(const Name& other) const noexcept
{
  // This name is longer than the name we are checking against.
  if (size() > other.size())
    return false;

  // Check if at least one of given components doesn't match.
  for (size_t i = 0; i < size(); ++i) {
    if (get(i) != other.get(i))
      return false;
  }

  return true;
}

bool
Name::equals(const Name& other) const noexcept
{
  if (size() != other.size())
    return false;

  for (size_t i = 0; i < size(); ++i) {
    if (get(i) != other.get(i))
      return false;
  }

  return true;
}

int
Name::compare(size_t pos1, size_t count1, const Name& other, size_t pos2, size_t count2) const
{
  count1 = std::min(count1, this->size() - pos1);
  count2 = std::min(count2, other.size() - pos2);
  size_t count = std::min(count1, count2);

  for (size_t i = 0; i < count; ++i) {
    int comp = get(pos1 + i).compare(other.get(pos2 + i));
    if (comp != 0) { // i-th component differs
      return comp;
    }
  }
  // [pos1, pos1+count) of this Name equals [pos2, pos2+count) of other Name
  return count1 - count2;
}

// ---- URI representation ----

void
Name::toUri(std::ostream& os, name::UriFormat format) const
{
  if (empty()) {
    os << "/";
    return;
  }

  for (const auto& component : *this) {
    os << "/";
    component.toUri(os, format);
  }
}

std::string
Name::toUri(name::UriFormat format) const
{
  std::ostringstream os;
  toUri(os, format);
  return os.str();
}

std::istream&
operator>>(std::istream& is, Name& name)
{
  std::string inputString;
  is >> inputString;
  name = Name(inputString);

  return is;
}

} // namespace ndn

namespace std {

size_t
hash<ndn::Name>::operator()(const ndn::Name& name) const
{
  return boost::hash_range(name.wireEncode().begin(), name.wireEncode().end());
}

} // namespace std
