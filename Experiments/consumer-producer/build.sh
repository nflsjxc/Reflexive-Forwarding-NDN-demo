g++ assist.hpp consumer.cpp -o consumer -std=c++17 -lndn-cxx -lpthread
g++ assist.hpp producer.cpp -o producer -std=c++17 -lndn-cxx -lpthread

# rm /tmp/minindn/a/consumer
# rm /tmp/minindn/b/producer
# cp ./consumer /tmp/minindn/a/consumer
# cp ./producer /tmp/minindn/b/producer

rm /tmp/minindn/a/consumer
rm /tmp/minindn/d/producer
cp ./consumer /tmp/minindn/a/consumer
cp ./producer /tmp/minindn/d/producer