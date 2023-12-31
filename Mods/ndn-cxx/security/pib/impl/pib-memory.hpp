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

#ifndef NDN_CXX_SECURITY_PIB_IMPL_PIB_MEMORY_HPP
#define NDN_CXX_SECURITY_PIB_IMPL_PIB_MEMORY_HPP

#include "ndn-cxx/security/pib/pib-impl.hpp"

namespace ndn::security::pib {

/**
 * @brief An in-memory PIB implementation.
 *
 * All the contents of an instance of this PIB are stored in memory only
 * and have the same lifetime as the class instance itself.
 */
class PibMemory : public PibImpl
{
public:
  /**
   * @brief Create memory-based PIB backend.
   * @param location Ignored (required by the PIB registration interface)
   */
  explicit
  PibMemory(const std::string& location = "");

  static const std::string&
  getScheme();

public: // TpmLocator management
  std::string
  getTpmLocator() const override
  {
    return m_tpmLocator;
  }

  void
  setTpmLocator(const std::string& tpmLocator) override;

public: // Identity management
  bool
  hasIdentity(const Name& identity) const override;

  void
  addIdentity(const Name& identity) override;

  void
  removeIdentity(const Name& identity) override;

  void
  clearIdentities() override;

  std::set<Name>
  getIdentities() const override;

  void
  setDefaultIdentity(const Name& identityName) override;

  Name
  getDefaultIdentity() const override;

public: // Key management
  bool
  hasKey(const Name& keyName) const override;

  void
  addKey(const Name& identity, const Name& keyName, span<const uint8_t> key) override;

  void
  removeKey(const Name& keyName) override;

  Buffer
  getKeyBits(const Name& keyName) const override;

  std::set<Name>
  getKeysOfIdentity(const Name& identity) const override;

  void
  setDefaultKeyOfIdentity(const Name& identity, const Name& keyName) override;

  Name
  getDefaultKeyOfIdentity(const Name& identity) const override;

public: // Certificate management
  bool
  hasCertificate(const Name& certName) const override;

  void
  addCertificate(const Certificate& certificate) override;

  void
  removeCertificate(const Name& certName) override;

  Certificate
  getCertificate(const Name& certName) const override;

  std::set<Name>
  getCertificatesOfKey(const Name& keyName) const override;

  void
  setDefaultCertificateOfKey(const Name& keyName, const Name& certName) override;

  Certificate
  getDefaultCertificateOfKey(const Name& keyName) const override;

private:
  std::string m_tpmLocator;

  std::set<Name> m_identities;
  std::optional<Name> m_defaultIdentity;

  /// identity name => default key name
  std::map<Name, Name> m_defaultKeys;

  /// key name => key bits
  std::map<Name, Buffer> m_keys;

  /// key name => default certificate name
  std::map<Name, Name> m_defaultCerts;

  /// certificate name => certificate object
  std::map<Name, Certificate> m_certs;
};

} // namespace ndn::security::pib

#endif // NDN_CXX_SECURITY_PIB_IMPL_PIB_MEMORY_HPP
