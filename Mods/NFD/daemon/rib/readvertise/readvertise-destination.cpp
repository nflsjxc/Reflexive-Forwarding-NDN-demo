/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014-2022,  Regents of the University of California,
 *                           Arizona Board of Regents,
 *                           Colorado State University,
 *                           University Pierre & Marie Curie, Sorbonne University,
 *                           Washington University in St. Louis,
 *                           Beijing Institute of Technology,
 *                           The University of Memphis.
 *
 * This file is part of NFD (Named Data Networking Forwarding Daemon).
 * See AUTHORS.md for complete list of NFD authors and contributors.
 *
 * NFD is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * NFD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * NFD, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "readvertise-destination.hpp"
#include "common/logger.hpp"

namespace nfd::rib {

NFD_LOG_INIT(ReadvertiseDestination);

void
ReadvertiseDestination::setAvailability(bool isAvailable)
{
  if (m_isAvailable != isAvailable) {
    if (isAvailable) {
      NFD_LOG_DEBUG("Destination has become available.");
    }
    else {
      NFD_LOG_DEBUG("Destinatino has become unavailable.");
    }
    m_isAvailable = isAvailable;
    afterAvailabilityChange(isAvailable);
  }
}

} // namespace nfd::rib
