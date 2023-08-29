#include <ndn-cxx/face.hpp>
#include <ndn-cxx/util/scheduler.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/security/signing-helpers.hpp>
#include <boost/asio/io_service.hpp>
#include <iostream>
#include <ctime>
#include <ndn-cxx/lp/pit-token.hpp>
#include "assist.hpp"

using namespace ndn;

class Producer
{
public:
  void
  run()
  {
    auto name=Name("/example/testApp");
    m_face.setInterestFilter(name,
                             std::bind(&Producer::onInterest, this, _2),
                             nullptr, // RegisterPrefixSuccessCallback is optional
                             std::bind(&Producer::onRegisterFailed, this, _1, _2));

    auto cert = m_keyChain.getPib().getDefaultIdentity().getDefaultKey().getDefaultCertificate();
    m_certServeHandle = m_face.setInterestFilter(security::extractIdentityFromCertName(cert.getName()),
                                                 [this, cert] (auto&&...) {
                                                   m_face.put(cert);
                                                 },
                                                 std::bind(&Producer::onRegisterFailed, this, _1, _2));
    m_ioService.run();
  }

private:
  // void reflect(std::string pitToken)
  // {
  //   std::cout<<"outcoming!";
  //   auto inter0=Interest(Name("/testApp/reflect/9999",true));
  //   inter0.setApplicationParameters(pitToken);
  //   m_face.expressInterest(
  //     inter0,
  //     std::bind(&Producer::datacallback, this, _1, _2),
  //               std::bind(&Producer::nackcallback, this, _1, _2),
  //               std::bind(&Producer::timeoutcallback, this, _1));
  // }

  void reflect(const Interest& interest)
  {
    std::cout << "At reflect: \n";
    auto lpPitToken = interest.getTag<lp::PitToken>();
    if(lpPitToken == nullptr)
    {
      std::cout<<"no pit token\n";
      return ;
    }
    uint32_t pitToken = readPitToken(*lpPitToken);


    auto reflectInterest = Interest(Name("/testApp/reflect/9999",true));
    auto lpPitToken2 = setPitToken(pitToken);
    reflectInterest.setTag(make_shared<lp::PitToken>(lpPitToken2));

    std::cout <<"reflect interest from producer: "<<reflectInterest << std::endl;
    m_face.expressInterest(
      reflectInterest,
      std::bind(&Producer::datacallback, this, _1, _2),
                std::bind(&Producer::nackcallback, this, _1, _2),
                std::bind(&Producer::timeoutcallback, this, _1));

    
  }

   void reflect2(const Interest& interest)
  {
    std::cout << "At reflect2: \n";
    auto lpPitToken = interest.getTag<lp::PitToken>();
    if(lpPitToken == nullptr)
    {
      std::cout<<"no pit token\n";
      return ;
    }
    uint32_t pitToken = readPitToken(*lpPitToken);


    auto reflectInterest = Interest(Name("/testApp/reflect2/9999",true));
    auto lpPitToken2 = setPitToken(pitToken);
    reflectInterest.setTag(make_shared<lp::PitToken>(lpPitToken2));

    std::cout <<"reflect interest from producer: "<<reflectInterest << std::endl;
    m_face.expressInterest(
      reflectInterest,
      std::bind(&Producer::datacallback, this, _1, _2),
                std::bind(&Producer::nackcallback, this, _1, _2),
                std::bind(&Producer::timeoutcallback, this, _1));

    
  }

  void onInterest(const Interest& interest)
  {
    if(!flag)
    {
      std::cout << ">> Incoming interest: " << interest << std::endl;
      std::cout << interest<<'\n';
      std::cout<<"interest pit-token: "<<readInterestPitToken(interest)<<'\n';
      std::cout << "<< Outcomming Reflexive Intereset\n";
      
      m_interest=interest;
      reflect(interest);
      flag=true;
    }
    else
    {
      std::cout << ">> Incoming interest: " << interest << std::endl;
      std::cout << interest.getName()<<'\n';
    }
  }

  //reply data for initial interest
  void replyData()
  {
    //Prepare final answer:
    auto data = std::make_shared<Data>();
    data->setName(m_interest.getName());
    data->setFreshnessPeriod(10_s);
    m_keyChain.sign(*data);
    std::cout << "<<Final Data(D1): " << *data << std::endl;
    m_face.put(*data);
  }

  void
  onRegisterFailed(const Name& prefix, const std::string& reason)
  {
    std::cerr << "ERROR: Failed to register prefix '" << prefix
              << "' with the local forwarder (" << reason << ")\n";
    m_face.shutdown();
  }

   void datacallback(const Interest& interest, const Data& data)
    {
        std::cout << "Received Data: \n" << data << std::endl;
        m_incoming_RD++;
        std::cout<<"IncomingRD =:"<<m_incoming_RD<<'\n';
        if(m_incoming_RD == 1)
          reflect2(m_interest);
        if(m_incoming_RD == 2)
          replyData();
    }
    void nackcallback(const Interest&, const lp::Nack& nack) const
    {
        std::cout << "Received Nack with reason " << nack.getReason() << std::endl;
    }
    void timeoutcallback(const Interest& interest) const
    {
        std::cout << "Timeout for " << interest << std::endl;
    }
    void RIcallback(const Interest& interest, const Interest& Rinterest)
    {
        std::cout << "Received Reflexive Interest " << Rinterest << std::endl;
    }

private:
  boost::asio::io_service m_ioService;
  Face m_face{m_ioService};
  KeyChain m_keyChain;
  ScopedRegisteredPrefixHandle m_certServeHandle;
  Scheduler m_scheduler{m_ioService};
  Interest m_interest; //initial interest
  int m_incoming_RD=0;
  bool flag =false;
};

int main()
{
  try {
    Producer producer;
    producer.run();
    return 0;
  }
  catch (const std::exception& e) {
    std::cerr << "ERROR: " << e.what() << std::endl;
    return 1;
  }
}
