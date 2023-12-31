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

#ifndef NDN_CXX_SECURITY_TRANSFORM_BUFFER_SOURCE_HPP
#define NDN_CXX_SECURITY_TRANSFORM_BUFFER_SOURCE_HPP

#include "ndn-cxx/security/transform/transform-base.hpp"
#include "ndn-cxx/security/security-common.hpp"

namespace ndn::security::transform {

/**
 * @brief A source taking one or more memory buffers as input
 */
class BufferSource : public Source
{
public:
  /**
   * @brief Take @p buffer as input.
   *
   * Caller must not destroy the buffer before the transformation is completed.
   */
  explicit
  BufferSource(span<const uint8_t> buffer);

  /**
   * @brief Take @p string as input.
   *
   * Caller must not destroy the string before the transformation is completed.
   */
  explicit
  BufferSource(std::string_view string);

  /**
   * @brief Take @p buffers as input.
   *
   * Caller must not destroy any of the input buffers before the transformation is completed.
   */
  explicit
  BufferSource(InputBuffers buffers);

private:
  /**
   * @brief Write the whole buffer into the next module.
   */
  void
  doPump() final;

private:
  InputBuffers m_bufs;
};

using bufferSource = BufferSource;

} // namespace ndn::security::transform

#endif // NDN_CXX_SECURITY_TRANSFORM_BUFFER_SOURCE_HPP
