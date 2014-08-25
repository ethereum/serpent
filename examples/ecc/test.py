import bitcoin as b
import random
from pyethereum import tester as t
s = t.state()
t.gas_limit = 10000000
h = b.encode(random.randrange(2**256), 256, 32)
k = random.randrange(2**256)
print h
print k
V, R, S = b.ecdsa_raw_sign(h, k)
print V, R, S
print 'actual pubkey', b.fast_multiply(b.G, k)
print 'pybitcointools ecrecovered pubkey', b.ecdsa_raw_recover(h, (V, R, S))
print '################'
c = s.contract('ecrecover.se')
# print t.serpent.deserialize(s.block.get_code(c))
# t.enable_logging()
print 'serpent ecrecovered pubkey', s.send(t.k0, c, 0, [h, V, R, S])
