import bitcoin as b
import math
import sys
from ethereum import utils


def signed(o):
    if not isinstance(o, (list, tuple)):
        return o - 2**256 if o >= 2**255 else o
    return map(lambda x: x - 2**256 if x >= 2**255 else x, o)


def hamming_weight(n):
    return len([x for x in b.encode(n, 2) if x == '1'])


def binary_length(n):
    return len(b.encode(n, 2))


def jacobian_mul_substitute(A, B, C, N):
    output = b.jacobian_multiply((A, B, C), N)
    return signed(output)


def jacobian_add_substitute(A, B, C, D, E, F):
    output = b.jacobian_add((A, B, C), (D, E, F))
    return signed(output)


def jacobian_double_substitute(A, B, C):
    output = b.jacobian_double((A, B, C))
    return signed(output)


def modexp_substitute(base, exp, mod):
    return signed(pow(base, exp, mod) if mod > 0 else 0)


def ecrecover_substitute(z, v, r, s):
    P, A, B, N, Gx, Gy = b.P, b.A, b.B, b.N, b.Gx, b.Gy
    x = r
    beta = pow(x*x*x+A*x+B, (P + 1) / 4, P)
    y = beta if v % 2 ^ beta % 2 else (P - beta)
    Gz = b.jacobian_multiply((Gx, Gy, 1), (N - z) % N)
    XY = b.jacobian_multiply((x, y, 1), s)
    Qr = b.jacobian_add(Gz, XY)
    Q = b.jacobian_multiply(Qr, pow(r, N - 2, N))
    Q = b.from_jacobian(Q)
    return signed(Q)

def recover_y(x, y_bit):
    N = 2**256-432420386565659656852420866394968145599
    P = 2**256-4294968273
    xcubed = x**3 % P
    beta = pow((x**3 + 7) % P, (P + 1) / 4, P)
    y_is_positive = y_bit ^ (beta % 2) ^ 1
    return(beta * y_is_positive + (P - beta) * (1 - y_is_positive))

def hash_to_pubkey(x):
    from bitcoin import A, B, P, encode_pubkey
    x = hash_array(x) if isinstance(x, list) else hash_value(x)
    while 1:
        xcubedaxb = (x*x*x+A*x+B) % P
        beta = pow(xcubedaxb, (P+1)//4, P)
        y = beta if beta % 2 else (P - beta)
        # Return if the result is not a quadratic residue
        if (xcubedaxb - y*y) % P == 0:
            return (x, y)
        x = (x + 1) % P

def decompose(Q):
    P = 2**256-4294968273
    ox = Q[0] * self.exp(Q[2], P - 3, P) % P
    oy = Q[1] * self.exp(Q[2], P - 4, P) % P
    return([ox, oy])

def hash_array(arr):
    o = ''
    for x in arr:
        if isinstance(x, (int, long)):
            x = utils.zpad(utils.encode_int(x), 32)
        o += x
    return utils.big_endian_to_int(utils.sha3(o))

def hash_value(x):
    if isinstance(x, (int, long)):
        x = utils.zpad(utils.encode_int(x), 32)
    return utils.big_endian_to_int(utils.sha3(x))

def ringsig_sign_substitute(msghash, priv, pub_xs, pub_ys):
    # Number of pubkeys
    n = len(pub_xs)
    # Create list of pubkeys as (x, y) points
    pubs = [(pub_xs[i], recover_y(pub_xs[i], bit(pub_ys, i))) for i in range(n)]
    # My pubkey
    my_pub = b.decode_pubkey(b.privtopub(priv))
    # Compute my index in the pubkey list
    my_index = 0
    while my_index < n:
        if pubs[my_index] == my_pub:
            break
        my_index += 1
    assert my_index < n
    # Compute the signer's I value
    I = b.multiply(hash_to_pubkey(list(my_pub)), priv)
    # Select a random ephemeral key
    k = b.hash_to_int(b.random_key())
    # Store the list of intermediate values in the "ring"
    e = [None] * n
    # Compute the entry in the ring corresponding to the signer's index
    kpub = b.privtopub(k)
    kmulpub = b.multiply(hash_to_pubkey(list(my_pub)), k)
    orig_left = hash_array([msghash, kpub[0], kpub[1], kmulpub[0], kmulpub[1]])
    orig_right = hash_value(orig_left)
    e[my_index] = {"left": orig_left, "right": orig_right}
    # Map of intermediate s values (part of the signature)
    s = [None] * n
    for i in list(range(my_index + 1, n)) + list(range(my_index + 1)):
        prev_i = (i - 1) % n
        # In your position in the ring, set the s value based on your private
        # knowledge of k; this lets you "invert" the hash function in order to
        # ensure a consistent ring. At all other positions, select a random s
        if i == my_index:
            s[prev_i] = b.add_privkeys(k, b.mul_privkeys(e[prev_i]["right"], priv))
        else:
            s[prev_i] = b.hash_to_int(b.random_key())
        # Create the next values in the ring based on the chosen s value
        pub1 = b.subtract_pubkeys(b.privtopub(s[prev_i]),
                                  b.multiply(pubs[i], e[prev_i]["right"]))
        pub2 = b.subtract_pubkeys(b.multiply(hash_to_pubkey(list(pubs[i])), s[prev_i]),
                                  b.multiply(I, e[prev_i]["right"]))
        left = hash_array([msghash, pub1[0], pub1[1], pub2[0], pub2[1]])
        right = hash_value(left)
        e[i] = {"left": left, "right": right}
    # Check that the ring is consistent
    assert (left, right) == (orig_left, orig_right)
    # Return the first value in the ring, the s values, and the signer's
    # I value in compressed form
    return (e[0]["left"], s, I[0], I[1] % 2)

def bit(bytez, i):
    return (ord(bytez[i // 8]) / 2**(i % 8)) % 2

def ringsig_verify_substitute(msghash, x0, s, Ix, Iy, pub_xs, pub_ys):
    # Number of pubkeys
    n = len(pub_xs)
    # Create list of pubkeys as (x, y) points
    pubs = [(pub_xs[i], recover_y(pub_xs[i], bit(pub_ys, i))) for i in range(n)]
    # Decompress the provided I value
    I = Ix, recover_y(Ix, Iy)
    # Store the list of intermediate values in the "ring"
    e = [None] * (n + 1)
    # Set the first value in the ring to that provided in the signature
    e[0] = [x0, hash_value(x0)]
    i = 1
    while i < n + 1:
        prev_i = (i - 1) % n
        # Create the next values in the ring based on the provided s value
        pub1 = b.subtract_pubkeys(b.privtopub(s[prev_i]),
                                  b.multiply(pubs[i % n], e[prev_i][1]))
        pub2 = b.subtract_pubkeys(b.multiply(hash_to_pubkey(list(pubs[i % n])), s[prev_i]),
                                  b.multiply(I, e[prev_i][1]))
        left = hash_array([msghash, pub1[0], pub1[1], pub2[0], pub2[1]])
        right = hash_value(left)
        # FOR DEBUGGING
        # if i >= 1:
        #     print 'pre', pubs[i % n]
        #     print 'pub1', pub1
        #     print 'pub2', pub2
        #     print 'left', left
        #     print 'right', right
        e[i] = [left, right]
        i += 1
    # Check that the ring is consistent
    return(e[n][0] == e[0][0] and e[n][1] == e[0][1])
