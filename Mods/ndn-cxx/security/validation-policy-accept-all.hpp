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

#ifndef NDN_CXX_SECURITY_VALIDATION_POLICY_ACCEPT_ALL_HPP
#define NDN_CXX_SECURITY_VALIDATION_POLICY_ACCEPT_ALL_HPP

#include "ndn-cxx/security/validation-policy.hpp"

namespace ndn::security {

/**
 * @brief A validator policy that accepts any signature of data and interest packets
 */
class ValidationPolicyAcceptAll : public ValidationPolicy
{
public:
  void
  checkPolicy(const Data& data, const shared_ptr<ValidationState>& state,
              const ValidationContinuation& continueValidation) final
  {
    continueValidation(nullptr, state);
  }

  void
  checkPolicy(const Interest& interest, const shared_ptr<ValidationState>& state,
              const ValidationContinuation& continueValidation) final
  {
    continueValidation(nullptr, state);
  }
};

} // namespace ndn::security

#endif // NDN_CXX_SECURITY_VALIDATION_POLICY_ACCEPT_ALL_HPP
