#ifndef UTILITY_HPP
#define UTILITY_HPP

#include <iostream>
#include <ndn-cxx/util/random.hpp>

using namespace ndn;

uint32_t
generateNonce()
{
  uint32_t r = random::generateWord32();
  return r;
}

#endif