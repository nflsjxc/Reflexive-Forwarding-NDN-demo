from time import sleep

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
    b = topo.addHost('b')
    c = topo.addHost('c')
    d = topo.addHost('d')
    topo.addLink(a, b, delay='10ms')
    topo.addLink(a, c, delay='10ms')
    topo.addLink(b, d, delay='10ms')
    return topo

if __name__ == '__main__':
    setLogLevel('info')

    Minindn.cleanUp()
    Minindn.verifyDependencies()

    topo=set_topo()

    ndn = Minindn(topo=topo)
    ndn.start()

    ndn.net['a'].cmd('export NDN_LOG="ndn.*=FATAL"')

    info('Starting nfd and nlsr on nodes')
    nfds = AppManager(ndn, ndn.net.hosts, Nfd, logLevel='DEBUG')
    nlsrs = AppManager(ndn, ndn.net.hosts, Nlsr, logLevel='DEBUG')
    sleep(5)

    MiniNDNCLI(ndn.net)
    ndn.stop()