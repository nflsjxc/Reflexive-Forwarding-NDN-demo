/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2013-2022 Regents of the University of California.
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

#ifndef NDN_CXX_TRANSPORT_UNIX_TRANSPORT_HPP
#define NDN_CXX_TRANSPORT_UNIX_TRANSPORT_HPP

#include "ndn-cxx/transport/transport.hpp"

#include <boost/asio/local/stream_protocol.hpp>

namespace ndn {

namespace detail {

template<typename BaseTransport, typename Protocol>
class StreamTransportImpl;

} // namespace detail

/** \brief A transport using Unix stream socket.
 */
class UnixTransport : public Transport
{
public:
  explicit
  UnixTransport(const std::string& unixSocket);

  ~UnixTransport() override;

  void
  connect(boost::asio::io_service& ioService, ReceiveCallback receiveCallback) override;

  void
  close() override;

  void
  pause() override;

  void
  resume() override;

  void
  send(const Block& wire) override;

  /** \brief Create transport with parameters defined in URI.
   *  \throw Transport::Error incorrect URI or unsupported protocol is specified
   */
  static shared_ptr<UnixTransport>
  create(const std::string& uri);

NDN_CXX_PUBLIC_WITH_TESTS_ELSE_PRIVATE:
  static std::string
  getSocketNameFromUri(const std::string& uri);

private:
  std::string m_unixSocket;

  using Impl = detail::StreamTransportImpl<UnixTransport, boost::asio::local::stream_protocol>;
  friend Impl;
  shared_ptr<Impl> m_impl;
};

} // namespace ndn

#endif // NDN_CXX_TRANSPORT_UNIX_TRANSPORT_HPP
