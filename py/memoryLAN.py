# memoryLAN.py

# a "memory switch" implementation for UDP multicast
# (c) 2026-07-24 <christian.tschudin@unibas.ch> - MIT license

import asyncio
import collections
import hashlib            # or use murmur for efficiency
import os
import socket
import struct

MEMLAN_CACHE_LEN    = 5   # can go up to 20 for chosen Bloom filter below
MEMLAN_HISTORY_LEN  = 2 * MEMLAN_CACHE_LEN

BEACON_PERIOD = 0.5       # in sec, high value for demo purposes

BLOOM_N = 18*8 # 18 Bytes
BLOOM_K = 5    # optimal number of hash functions for 20 elements: 144/20*ln(2)

# ---------------------------------------------------------------------------

class Switch:

    def __init__(self):
        self.cache = collections.deque([], MEMLAN_CACHE_LEN)
        self.history = collections.deque([], MEMLAN_HISTORY_LEN)
        self.BFtag = hashlib.sha1(b"memoryLAN-20260701-BF").digest()

    async def begin(self, ticket, update_cb, error=0.0):
        protocol, self.mc_group, port = ticket.split(':')[:3]
        if protocol != 'udp':
            raise Exception("only udp multicast is support");
        self.mc_port = int(port)
        self.update = update_cb
        self.error = error

        class MC_endpoint(asyncio.DatagramProtocol):
            def datagram_received(self2, pdu, addr):
                if self.error > 0: # simulate pkt collision
                    e = self.error * 255
                    if e >= os.urandom(1)[0]:
                        return
                if pdu[:20] == self.BFtag:
                    self._incoming_BF(pdu[20:])
                else:
                    self._incoming_mpg(pdu)

        # socket for sending
        self.ssock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM,
                                   socket.IPPROTO_UDP)
        # socket for receiving, bound to the multicast group
        rsock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM,
                              socket.IPPROTO_UDP)
        rsock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        rsock.bind((self.mc_group, self.mc_port))
        mreq = struct.pack("4sl", socket.inet_aton(self.mc_group),
                           socket.INADDR_ANY)
        rsock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
        
        loop = asyncio.get_running_loop()
        _,endpt = await loop.create_datagram_endpoint(MC_endpoint, sock=rsock)

        asyncio.create_task(self._periodically())

    def add(self, mpg): # request to replicate a memory page, called by the app
        self._incoming_mpg(mpg)

    # internal methods -------------------------------------------------------

    def _broadcast(self, pkt):
        self.ssock.sendto(pkt, (self.mc_group, self.mc_port))

    def _incoming_mpg(self, mpg):
        if mpg in self.cache:
            self.cache.remove(mpg)
        self.cache.appendleft(mpg)
        h = hashlib.sha1(mpg).digest()
        if h in self.history: # this suppresses loops
            return
        self.history.appendleft(h)
        self._broadcast(mpg)
        if self.update:
            self.update(mpg)
        
    def _incoming_BF(self, bf):
        # only react for discrepancies in "region of interest" (top entries)
        R = list(self.cache)[:3*len(self.cache)//4] # 75% of cache
        M = [ m for m in R if not self._is_in_BF(m, bf) ]
        if len(M) == 0: return
        i = os.urandom(1)[0] % len(M) # random pick
        # FIXME: track number of neighbors and modulate broadcast rate
        self._broadcast(M[i])
        
    async def _periodically(self):
        while True:
            await asyncio.sleep(BEACON_PERIOD)
            beacon = self.BFtag + self._mk_BF()
            self._broadcast(beacon)
            if len(self.history) > 0: # autodrain history
                self.history.pop()
            if len(self.cache) > 0: # autodrain cache, only above 75% fill level?
                self.cache.pop()

    def _mk_BF(self): # populate the Bloom filter with cache content
        nonce = os.urandom(2)
        f = bytearray(BLOOM_N//8)
        for h in [hashlib.sha1(nonce + m).digest() for m in list(self.cache)]:
            # consume 2 bytes from hash for K distinct BF map functions
            for i in range(BLOOM_K):
                pos = int.from_bytes(h[0:2]) % BLOOM_N
                f[pos//8] |= 1 << (pos%8)
                h = h[2:] # next BF hash function
        return nonce + bytes(f)

    def _is_in_BF(self, m, bf): # "is m (potentially) in the given BF?"
        n = bf[:2]   # extract nonce
        bf = bf[2:]  # remainder is the bit vector
        h = hashlib.sha1(n+m).digest()
        for i in range(BLOOM_K):
            pos = int.from_bytes(h[0:2]) % BLOOM_N
            if bf[pos//8] & (1 << (pos%8)) == 0:
                return False
            h = h[2:]
        return True

    pass

# eof
