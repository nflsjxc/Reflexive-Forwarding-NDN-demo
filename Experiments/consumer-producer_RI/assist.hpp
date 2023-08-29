#include<ndn-cxx/util/random.hpp>
#include<ndn-cxx/name.hpp>
#include<ndn-cxx/encoding/buffer.hpp>
#include<ndn-cxx/lp/pit-token.hpp>
#include<ndn-cxx/interest.hpp>

ndn::lp::PitToken setPitToken(uint32_t tokenvalue)
{
    std::vector<uint8_t> byteArray;
    for (size_t i = 0; i < sizeof(uint32_t); ++i) {
        uint8_t byte = (tokenvalue >> (8 * i)) & 0xFF;
        byteArray.push_back(byte);
    }
    // std::reverse(byteArray.begin(), byteArray.end());
    ndn::Buffer buf = ndn::Buffer(byteArray.data(), 4);
    ndn::lp::PitToken pittoken = ndn::lp::PitToken(std::make_pair(buf.begin(),buf.end()));
    return pittoken;
}

uint32_t readPitToken(ndn::lp::PitToken pitToken)
{
    uint32_t tokenvalue = 0;
    for (size_t i = 0; i < sizeof(uint32_t); ++i) {
        tokenvalue |= static_cast<uint32_t>(pitToken[i]) << (8 * i);
    }
    return tokenvalue;
}

uint32_t readInterestPitToken(const ndn::Interest& interest)
{
    auto lpPitToken = interest.getTag<ndn::lp::PitToken>();
    if(lpPitToken == nullptr) return 0;
    uint32_t tokenvalue = readPitToken(*lpPitToken);
    return tokenvalue;
}