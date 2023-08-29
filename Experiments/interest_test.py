from time import sleep
import os
from mininet.log import setLogLevel, info
from minindn.minindn import Minindn
from minindn.util import MiniNDNCLI
from minindn.apps.app_manager import AppManager
from minindn.apps.nfd import Nfd
from minindn.apps.nlsr import Nlsr

import mininet

def set_topo():
    topo = mininet.topo.Topo()
    a = topo.addHost('a')
    # b = topo.addSwitch('s1')
    b = topo.addHost('b')
    c = topo.addHost('c')
    d = topo.addHost('d')
    topo.addLink(a, b, delay='10ms')
    topo.addLink(b, c, delay='10ms')
    topo.addLink(c, a, delay='10ms')
    topo.addLink(c, d, delay='10ms')
    return topo

if __name__ == '__main__':
    setLogLevel('info')

    Minindn.cleanUp()
    Minindn.verifyDependencies()

    mytopo=set_topo()
    
    ndn = Minindn(topo=mytopo)
    ndn.start()

    info('Starting nfd and nlsr on nodes \n')
    nfds_host = AppManager(ndn, ndn.net.hosts, Nfd, logLevel='DEBUG')
    nlsrs_host = AppManager(ndn, ndn.net.hosts, Nlsr, logLevel='DEBUG')
    sleep(5)

    # Default topology is used in this experiment "/topologies/default-topology.conf"
    # lets make node "a" as a producer node, and node "c" as a consumer node
    # consumer = ndn.net['a']
    # producer = ndn.net['c']

    # start producer
    # producerPrefix = "/example"
    # producer.cmd('nlsrc advertise {}'.format(producerPrefix))
    sleep(15) # sleep for routing convergence

    # Make sure that basic consumer/producer example are compiled and installed in the system
    info('Starting consumer and producer application \n')
    # print("producer: {}".format(producer.cmd("./producer")))
    # print("consumer: {}".format(producer.cmd("./consumer")))

    MiniNDNCLI(ndn.net)
    ndn.stop()