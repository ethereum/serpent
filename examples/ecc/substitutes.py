import bitcoin as b
import math
import sys


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
    return signed(list(output[0]) + list(output[1]))


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
    Qr = b.jordan_add(Gz, XY)
    Q = b.jordan_multiply(Qr, pow(r, N - 2, N))
    Q = b.from_jordan(Q)
    return signed(Q)
