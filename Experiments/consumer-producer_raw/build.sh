g++ consumer.cpp -o consumer -std=c++17 -lndn-cxx -lpthread
g++ producer.cpp -o producer -std=c++17 -lndn-cxx -lpthread

# rm /tmp/minindn/a/consumer
# rm /tmp/minindn/b/producer
# cp ./consumer /tmp/minindn/a/consumer
# cp ./producer /tmp/minindn/b/producer

rm /tmp/minindn/a/consumer
rm /tmp/minindn/b/producer
cp ./consumer /tmp/minindn/a/consumer
cp ./producer /tmp/minindn/b/producer