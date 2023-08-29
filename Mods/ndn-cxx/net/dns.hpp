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

#ifndef NDN_CXX_NET_DNS_HPP
#define NDN_CXX_NET_DNS_HPP

#include "ndn-cxx/detail/asio-fwd.hpp"
#include "ndn-cxx/util/time.hpp"

#include <boost/asio/ip/address.hpp>

namespace ndn::dns {

using IpAddress = boost::asio::ip::address;
using AddressSelector = std::function<bool(const IpAddress&)>;

struct AnyAddress
{
  bool
  operator()(const IpAddress& address) const
  {
    return true;
  }
};

struct Ipv4Only
{
  bool
  operator()(const IpAddress& address) const
  {
    return address.is_v4();
  }
};

struct Ipv6Only
{
  bool
  operator()(const IpAddress& address) const
  {
    return address.is_v6();
  }
};

class Error : public std::runtime_error
{
public:
  using std::runtime_error::runtime_error;
};

using SuccessCallback = std::function<void(const IpAddress& address)>;
using ErrorCallback = std::function<void(const std::string& reason)>;

/** \brief Asynchronously resolve host
 *
 * If an address selector predicate is specified, then each resolved IP address
 * is checked against the predicate.
 *
 * Available address selector predicates:
 *
 * - dns::AnyAddress()
 * - dns::Ipv4Address()
 * - dns::Ipv6Address()
 *
 * \warning Even after the DNS resolution has timed out, it's possible that
 *          \p ioService keeps running and \p onSuccess is invoked at a later time.
 *          This could cause segmentation fault if \p onSuccess is deallocated.
 *          To stop the io_service, explicitly invoke \p ioService.stop().
 */
void
asyncResolve(const std::string& host,
             const SuccessCallback& onSuccess,
             const ErrorCallback& onError,
             boost::asio::io_service& ioService,
             const AddressSelector& addressSelector = AnyAddress(),
             time::nanoseconds timeout = 4_s);

/** \brief Synchronously resolve host
 *
 * If an address selector predicate is specified, then each resolved IP address
 * is checked against the predicate.
 *
 * Available address selector predicates:
 *
 * - dns::AnyAddress()
 * - dns::Ipv4Address()
 * - dns::Ipv6Address()
 */
IpAddress
syncResolve(const std::string& host,
            boost::asio::io_service& ioService,
            const AddressSelector& addressSelector = AnyAddress());

} // namespace ndn::dns

#endif // NDN_CXX_NET_DNS_HPP
