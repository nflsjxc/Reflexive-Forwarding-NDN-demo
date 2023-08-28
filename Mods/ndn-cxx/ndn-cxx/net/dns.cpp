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

#include "ndn-cxx/net/dns.hpp"
#include "ndn-cxx/util/scheduler.hpp"

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/udp.hpp>
#if BOOST_VERSION >= 106600
#include <boost/asio/post.hpp>
#endif

namespace ndn::dns {

class Resolver : noncopyable
{
public:
  using protocol = boost::asio::ip::udp;
  using iterator = protocol::resolver::iterator;
  using query = protocol::resolver::query;

public:
  Resolver(boost::asio::io_service& ioService,
           const AddressSelector& addressSelector)
    : m_resolver(ioService)
    , m_addressSelector(addressSelector)
    , m_scheduler(ioService)
  {
    BOOST_ASSERT(m_addressSelector != nullptr);
  }

  void
  asyncResolve(const query& q,
               const SuccessCallback& onSuccess,
               const ErrorCallback& onError,
               time::nanoseconds timeout,
               const shared_ptr<Resolver>& self)
  {
    m_onSuccess = onSuccess;
    m_onError = onError;

    m_resolver.async_resolve(q, [=] (auto&&... args) {
      onResolveResult(std::forward<decltype(args)>(args)..., self);
    });

    m_resolveTimeout = m_scheduler.schedule(timeout, [=] { onResolveTimeout(self); });
  }

  iterator
  syncResolve(const query& q)
  {
    return selectAddress(m_resolver.resolve(q));
  }

private:
  void
  onResolveResult(const boost::system::error_code& error,
                  iterator it, const shared_ptr<Resolver>& self)
  {
    m_resolveTimeout.cancel();

    // ensure the Resolver isn't destructed while callbacks are still pending, see #2653
#if BOOST_VERSION >= 106600
    boost::asio::post(m_resolver.get_executor(), [self] {});
#else
    m_resolver.get_io_service().post([self] {});
#endif

    if (error) {
      if (error == boost::asio::error::operation_aborted)
        return;

      if (m_onError)
        m_onError("Hostname cannot be resolved: " + error.message());

      return;
    }

    it = selectAddress(it);

    if (it != iterator() && m_onSuccess) {
      m_onSuccess(it->endpoint().address());
    }
    else if (m_onError) {
      m_onError("No endpoints match the specified address selector");
    }
  }

  void
  onResolveTimeout(const shared_ptr<Resolver>& self)
  {
    m_resolver.cancel();

    // ensure the Resolver isn't destructed while callbacks are still pending, see #2653
#if BOOST_VERSION >= 106600
    boost::asio::post(m_resolver.get_executor(), [self] {});
#else
    m_resolver.get_io_service().post([self] {});
#endif

    if (m_onError)
      m_onError("Hostname resolution timed out");
  }

  iterator
  selectAddress(iterator it) const
  {
    while (it != iterator() && !m_addressSelector(it->endpoint().address())) {
      ++it;
    }
    return it;
  }

private:
  protocol::resolver m_resolver;

  AddressSelector m_addressSelector;
  SuccessCallback m_onSuccess;
  ErrorCallback m_onError;

  Scheduler m_scheduler;
  scheduler::EventId m_resolveTimeout;
};

void
asyncResolve(const std::string& host,
             const SuccessCallback& onSuccess,
             const ErrorCallback& onError,
             boost::asio::io_service& ioService,
             const AddressSelector& addressSelector,
             time::nanoseconds timeout)
{
  auto resolver = make_shared<Resolver>(ioService, addressSelector);
  resolver->asyncResolve(Resolver::query(host, ""), onSuccess, onError, timeout, resolver);
  // resolver will be destroyed when async operation finishes or ioService stops
}

IpAddress
syncResolve(const std::string& host,
            boost::asio::io_service& ioService,
            const AddressSelector& addressSelector)
{
  Resolver resolver(ioService, addressSelector);
  auto it = resolver.syncResolve(Resolver::query(host, ""));

  if (it == Resolver::iterator()) {
    NDN_THROW(Error("No endpoints match the specified address selector"));
  }

  return it->endpoint().address();
}

} // namespace ndn::dns
