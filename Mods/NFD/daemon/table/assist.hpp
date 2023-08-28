#ifndef ASSIST_HPP
#define ASSIST_HPP

#include<map>
#include<set>
#include<string>
#include<ndn-cxx/util/random.hpp>
#include<ndn-cxx/name.hpp>
#include<ndn-cxx/encoding/buffer.hpp>
#include<ndn-cxx/lp/pit-token.hpp>
#include<ndn-cxx/interest.hpp>

ndn::lp::PitToken setPitToken(uint32_t tokenvalue);
uint32_t readPitToken(ndn::lp::PitToken pitToken);

uint32_t readInterestPitToken(const ndn::Interest& interest);

namespace nfd
{
namespace pit
{
class pit_assist
{
public:
    pit_assist()
    {
        m_reserved_value.insert(0); //illegal tokens
    }

    //Insert Name in assist, and create new pitToken for nexthop
    void createName(ndn::Name name, uint32_t prevToken)
    {
        uint32_t token = generateToken();
        m_Name_to_Token[name] = token;
        m_Token_to_Name[token] = name;
        m_Name_to_prevToken[name] = prevToken;
        m_prevToken_to_Name[prevToken] = name;
    }

    bool existName(ndn::Name name)
    {
        return (m_Name_to_Token.find(name) != m_Name_to_Token.end());
    }

    uint32_t generateToken()
    {
        for(;;)
        {
            uint32_t newtoken = generateNonce();
            if(m_reserved_value.find(newtoken) != m_reserved_value.end())
            {
                continue;
            }
            if(m_Token_to_Name.find(newtoken) == m_Token_to_Name.end())
            {
                return newtoken;
            }
        }
    }

    std::pair<ndn::Name, bool> prevTokenToName(uint32_t prevtoken)
    {
        if(m_prevToken_to_Name.find(prevtoken) != m_prevToken_to_Name.end())
        {
            return std::make_pair(m_prevToken_to_Name[prevtoken], true);
        }
        else
        {
            return std::make_pair(ndn::Name(), false);
        }
    }

    std::pair<uint32_t, bool> nameToPrevToken(ndn::Name name)
    {
        if(m_Name_to_prevToken.find(name) != m_Name_to_prevToken.end())
        {
            return std::make_pair(m_Name_to_prevToken[name], true);
        }
        else
        {
            return std::make_pair(0, false);
        }
    }

    std::pair<ndn::Name, bool> tokenToName(uint32_t token)
    {
        if(m_Token_to_Name.find(token) != m_Token_to_Name.end())
        {
            return std::make_pair(m_Token_to_Name[token], true);
        }
        else
        {
            return std::make_pair(ndn::Name(), false);
        }
    }

    std::pair<uint32_t, bool> nameToToken(ndn::Name name)
    {
        if(m_Name_to_Token.find(name) != m_Name_to_Token.end())
        {
            return std::make_pair(m_Name_to_Token[name], true);
        }
        else
        {
            return std::make_pair(0, false);
        }
    }


private:
    //Name is the Key
    std::set<uint32_t> m_reserved_value;
    std::map<ndn::Name, uint32_t> m_Name_to_Token, m_Name_to_prevToken;
    std::map<uint32_t, ndn::Name> m_Token_to_Name, m_prevToken_to_Name;


    uint32_t
    generateNonce()
    {
        uint32_t r = ndn::random::generateWord32();
        return r;
    }
};

}
}

#endif