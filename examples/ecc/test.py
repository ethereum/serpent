import bitcoin as b
import random
import sys
import math
from ethereum import tester as t
from ethereum import utils
import substitutes
import time

vals = [random.randrange(2**256) for i in range(12)]

test_points = [b.jacobian_multiply((b.Gx, b.Gy, 1), r) for r in vals]

G = [b.Gx, b.Gy, 1]
Z = [0, 0, 1]


def neg_point(p):
    return [p[0], b.P - p[1], p[2]]

s = t.state()
s.block.gas_limit = 100000000
t.gas_limit = 3000000

tests = sys.argv[1:]

if '--log' in tests:
    t.set_logging_level(int((tests+[1])[tests.index('--log') + 1]))

if '--modexp' in tests or not len(tests):
    c = s.abi_contract('jacobian_arith.se')
    print "Starting modexp tests"

    for i in range(0, len(vals) - 2, 3):
        o1 = substitutes.modexp_substitute(vals[i], vals[i+1], vals[i+2])
        o2 = c.exp(vals[i], vals[i+1], vals[i+2], profiling=1)
        print "gas", o2["gas"], "time", o2["time"]
        assert o1 == o2["output"], (o1, o2)

if '--double' in tests or not len(tests):
    print "Starting doubling tests"
    c = s.abi_contract('jacobian_arith.se')
    for i in range(5):
        print 'trying doubling test', vals[i]
        P = b.to_jacobian(b.privtopub(vals[i]))
        o1 = substitutes.jacobian_double_substitute(*list(P))
        o2 = c.double(*(list(P)), profiling=2)
        print "gas", o2["gas"], "time", o2["time"], "ops", o2["ops"]
        assert o1 == o2["output"], (o1, o2)

if '--add' in tests or not len(tests):
    print "Starting addition tests"
    c = s.abi_contract('jacobian_arith.se')
    for i in range(5):
        print 'trying addition test', vals[i * 2], vals[i * 2 + 1]
        P = b.to_jacobian(b.privtopub(vals[i * 2]))
        Q = b.to_jacobian(b.privtopub(vals[i * 2 + 1]))
        o1 = substitutes.jacobian_add_substitute(*(list(P) + list(Q)))
        o2 = c.add(*(list(P) + list(Q)), profiling=2)
        print "gas", o2["gas"], "time", o2["time"], "ops", o2["ops"]
        assert o1 == o2["output"], (o1, o2)

if '--mul' in tests or not len(tests):
    print "Starting multiplication tests"
    c = s.abi_contract('jacobian_arith.se')
    for i in range(5):
        print 'trying multiplication test', vals[i * 2], vals[i * 2 + 1]
        P = b.to_jacobian(b.privtopub(vals[i * 2]))
        q = vals[i * 2 + 1]
        o1 = substitutes.jacobian_mul_substitute(*(list(P) + [q]))
        a = time.time()
        print list(P) + [q]
        o2 = c.mul(*(list(P) + [q]), profiling=1)
        print "gas", o2["gas"], "time", o2["time"]
        assert o1 == o2["output"], (o1, o2)
        

if '--ecrecover' in tests or not len(tests):
    c = s.abi_contract('ecrecover.se')
    print "Starting ecrecover tests"

    for i in range(5):
        print 'trying ecrecover_test', vals[i*2], vals[i*2+1]
        k = vals[i*2]
        h = vals[i*2+1]
        V, R, S = b.ecdsa_raw_sign(b.encode(h, 256, 32), k)
        aa = time.time()
        o1 = substitutes.ecrecover_substitute(h, V, R, S)
        print 'Native execution time:', time.time() - aa
        a = time.time()
        o2 = c.ecrecover(h, V, R, S)
        print 'time', time.time() - a, 'gas', s.block.get_receipts()[-1].gas_used - s.block.get_receipts()[-2].gas_used
        assert o1 == o2, (o1, o2)

    # Explicit tests

    data = [[
        0xf007a9c78a4b2213220adaaf50c89a49d533fbefe09d52bbf9b0da55b0b90b60,
        0x1b,
        0x5228fc9e2fabfe470c32f459f4dc17ef6a0a81026e57e4d61abc3bc268fc92b5,
        0x697d4221cd7bc5943b482173de95d3114b9f54c5f37cc7f02c6910c6dd8bd107
    ]]

    for datum in data:
        o1 = substitutes.ecrecover_substitute(*datum)
        o2 = c.ecrecover(*datum)
        assert o1 == o2, (o1, o2)

if '--ringsig' in tests or not len(tests):
    print "Starting ringsig tests"
    c = s.abi_contract('ringsig.se')
    for L in range(2, 6):
        privs = vals[:L]
        my_priv = vals[1]
        pubs = map(b.privtopub, privs)
        for pub in pubs:
            assert map(substitutes.signed, list(substitutes.hash_to_pubkey(list(pub)))) == \
                c.hash_pubkey_to_pubkey(list(pub))
        pub_xs = [x[0] for x in pubs]
        pub_ys = []
        for i, p in enumerate(pubs):
            if (i % 8) == 0:
                pub_ys.append(0)
            pub_ys[-1] += (p[1] % 2) * 2**(i % 8)
        pub_ys = ''.join(map(chr, pub_ys))
        msghash = utils.sha3('cow')
        x0, s_vals, Ix, Iy = substitutes.ringsig_sign_substitute(msghash, my_priv, pub_xs, pub_ys)
        t1 = time.time()
        assert substitutes.ringsig_verify_substitute(msghash, x0, s_vals, Ix, Iy, pub_xs, pub_ys)
        print 'Native execution time: ', time.time() - t1
        ogl = t.gas_limit
        t.gas_limit = 10000000
        o = c.verify(msghash, x0, s_vals, Ix, Iy, pub_xs, pub_ys, profiling=1)
        assert o["output"]
        print 'number of pubkeys', L, 'gas', o["gas"], 'time', o["time"], \
            'totalgas', s.block.get_receipts()[-1].gas_used - s.block.get_receipts()[-2].gas_used
        t.gas_limit = ogl
