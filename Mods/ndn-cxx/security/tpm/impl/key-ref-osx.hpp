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

#ifndef NDN_CXX_SECURITY_TPM_IMPL_KEY_REF_OSX_HPP
#define NDN_CXX_SECURITY_TPM_IMPL_KEY_REF_OSX_HPP

#include "ndn-cxx/detail/common.hpp"

#ifndef NDN_CXX_HAVE_OSX_FRAMEWORKS
#error "This file should not be compiled ..."
#endif

#include "ndn-cxx/detail/cf-releaser-osx.hpp"

#include <Security/Security.h>

namespace ndn::security::tpm {

using KeyRefOsx = detail::CFReleaser<SecKeyRef>;

} // namespace ndn::security::tpm

#endif // NDN_CXX_SECURITY_TPM_IMPL_KEY_REF_OSX_HPP
