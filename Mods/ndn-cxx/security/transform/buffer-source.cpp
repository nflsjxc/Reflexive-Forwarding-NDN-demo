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

#include "ndn-cxx/security/transform/buffer-source.hpp"

namespace ndn::security::transform {

BufferSource::BufferSource(span<const uint8_t> buffer)
  : m_bufs({buffer})
{
}

BufferSource::BufferSource(std::string_view string)
  : m_bufs({{reinterpret_cast<const uint8_t*>(string.data()), string.size()}})
{
}

BufferSource::BufferSource(InputBuffers buffers)
  : m_bufs(std::move(buffers))
{
}

void
BufferSource::doPump()
{
  BOOST_ASSERT(m_next != nullptr);

  for (auto buffer : m_bufs) {
    while (!buffer.empty()) {
      size_t nBytesWritten = m_next->write(buffer);
      buffer = buffer.subspan(nBytesWritten);
    }
  }

  m_next->end();
}

} // namespace ndn::security::transform
