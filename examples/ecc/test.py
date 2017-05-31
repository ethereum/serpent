import bitcoin as b
import random
import sys
import math
from ethereum.tools import tester as t
from ethereum import utils
import substitutes
import time

vals = [random.randrange(2**256) for i in range(12)]

test_points = [b.jacobian_multiply((b.Gx, b.Gy, 1), r) for r in vals]

G = [b.Gx, b.Gy, 1]
Z = [0, 0, 1]


def neg_point(p):
    return [p[0], b.P - p[1], p[2]]

s = t.Chain()
s.head_state.gas_limit = 10**9

tests = sys.argv[1:]

if '--log' in tests:
    t.set_logging_level(int((tests+[1])[tests.index('--log') + 1]))

if '--modexp' in tests or not len(tests):
    c = s.contract('jacobian_arith.se', language='serpent')
    print("Starting modexp tests")

    for i in range(0, len(vals) - 2, 3):
        o1 = substitutes.modexp_substitute(vals[i], vals[i+1], vals[i+2])
        o2 = c.exp(vals[i], vals[i+1], vals[i+2])
        assert o1 == o2

if '--double' in tests or not len(tests):
    print("Starting doubling tests")
    c = s.contract('jacobian_arith.se', language='serpent')
    for i in range(5):
        print('trying doubling test', vals[i])
        P = b.to_jacobian(b.privtopub(vals[i]))
        o1 = substitutes.jacobian_double_substitute(*list(P))
        o2 = c.double(*(list(P)))
        assert o1 == o2, (o1, o2)

if '--add' in tests or not len(tests):
    print("Starting addition tests")
    c = s.contract('jacobian_arith.se', language='serpent')
    for i in range(5):
        print('trying addition test', vals[i * 2], vals[i * 2 + 1])
        P = b.to_jacobian(b.privtopub(vals[i * 2]))
        Q = b.to_jacobian(b.privtopub(vals[i * 2 + 1]))
        o1 = substitutes.jacobian_add_substitute(*(list(P) + list(Q)))
        o2 = c.add(*(list(P) + list(Q)))
        assert o1 == o2

if '--mul' in tests or not len(tests):
    print("Starting multiplication tests")
    c = s.contract('jacobian_arith.se', language='serpent')
    for i in range(5):
        print('trying multiplication test', vals[i * 2], vals[i * 2 + 1])
        P = b.to_jacobian(b.privtopub(vals[i * 2]))
        q = vals[i * 2 + 1]
        o1 = substitutes.jacobian_mul_substitute(*(list(P) + [q]))
        a = time.time()
        print(list(P) + [q])
        o2 = c.mul(*(list(P) + [q]))
        assert o1 == o2
        

if '--ecrecover' in tests or not len(tests):
    c = s.contract('ecrecover.se', language='serpent')
    print("Starting ecrecover tests")

    for i in range(5):
        print('trying ecrecover_test', vals[i*2], vals[i*2+1])
        k = vals[i*2]
        h = vals[i*2+1]
        V, R, S = b.ecdsa_raw_sign(b.encode(h, 256, 32), k)
        aa = time.time()
        o1 = substitutes.ecrecover_substitute(h, V, R, S)
        print('Native execution time:', time.time() - aa)
        a = time.time()
        o2 = c.ecrecover(h, V, R, S)
        print('time', time.time() - a, 'gas', s.head_state.receipts[-1].gas_used - s.head_state.receipts[-2].gas_used)
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
    print("Starting ringsig tests")
    c = s.contract('ringsig.se', language='serpent')
    for L in range(2, 6):
        privs = vals[:L]
        my_priv = vals[1]
        pubs = list(map(b.privtopub, privs))
        for pub in pubs:
            assert list(map(substitutes.signed, list(substitutes.hash_to_pubkey(list(pub))))) == \
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
        print('Native execution time: ', time.time() - t1)
        t2 = time.time()
        o = c.verify(msghash, x0, s_vals, Ix, Iy, pub_xs, pub_ys, startgas=10**7)
        assert o
        print('number of pubkeys', L, \
            'totalgas', s.head_state.receipts[-1].gas_used - s.head_state.receipts[-2].gas_used, 'EVM verification time:', time.time() - t2)
