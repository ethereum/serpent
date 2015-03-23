import serpent_pyext as pyext
import sys, re
import binascii

VERSION = '1.8.1'

def strtobytes(x):
    return x.encode('ascii') if isinstance(x, str) else x

def bytestostr(x):
    return x.decode('ascii') if isinstance(x, bytes) else x

class Metadata(object):
    def __init__(self, li):
        self.file = li[0]
        self.ln = li[1]
        self.ch = li[2]

    def out(self):
        return [self.file, self.ln, self.ch]


class Token(object):
    def __init__(self, val, metadata):
        self.val = val
        self.metadata = Metadata(metadata)

    def out(self):
        return [0, self.val, self.metadata.out()]

    def __repr__(self):
        return str(bytestostr(self.val))


class Astnode(object):
    def __init__(self, val, args, metadata):
        self.val = val
        self.args = map(node, args)
        self.metadata = Metadata(metadata)

    def out(self):
        o = [1, self.val, self.metadata.out()]+[x.out() for x in self.args]
        return o

    def __repr__(self):
        o = '(' + bytestostr(self.val)
        subs = list(map(bytestostr, map(repr, self.args)))
        k = 0
        out = " "
        while k < len(subs) and o != "(seq":
            if '\n' in subs[k] or len(out + subs[k]) >= 80:
                break
            out += subs[k] + " "
            k += 1
        if k < len(subs):
            o += out + "\n  "
            o += '\n  '.join('\n'.join(subs[k:]).split('\n'))
            o += '\n)'
        else:
            o += out[:-1] + ')'
        return o


def node(li):
    if li[0]:
        return Astnode(li[1], li[3:], li[2])
    else:
        return Token(li[1], li[2])


def take(x):
    if sys.version_info[0] < 3:
        return pyext.parse_lll(x) if isinstance(x, (str, unicode)) else x.out()
    else:
        return pyext.parse_lll(x) if isinstance(x, (str, bytes)) else x.out()

def takelist(x):
    if sys.version_info[0] < 3:
        return map(take, parse(x).args if isinstance(x, (str, unicode)) else x)
    else:
        return map(take, parse(x).args if isinstance(x, (str, bytes)) else x)

compile = lambda x: pyext.compile(strtobytes(x))
compile_to_lll = lambda x: node(pyext.compile_to_lll(strtobytes(x)))
compile_lll = lambda x: pyext.compile_lll(take(strtobytes(x)))
parse = lambda x: node(pyext.parse(strtobytes(x)))
rewrite = lambda x: node(pyext.rewrite(take(strtobytes(x))))
pretty_compile = lambda x: map(node, pyext.pretty_compile(strtobytes(x)))
pretty_compile_lll = lambda x: map(node, pyext.pretty_compile_lll(take(strtobytes(x))))
serialize = lambda x: pyext.serialize(takelist(strtobytes(x)))
deserialize = lambda x: map(node, pyext.deserialize(strtobytes(x)))
mk_signature = lambda x: pyext.mk_signature(strtobytes(x))
mk_full_signature = lambda x: pyext.mk_full_signature(strtobytes(x))
get_prefix = lambda x, y: pyext.get_prefix(x, y) % 2**32

is_numeric = lambda x: isinstance(x, (int, long))
is_string = lambda x: isinstance(x, (str, unicode))
tobytearr = lambda n, L: [] if L == 0 else tobytearr(n / 256, L - 1)+[n % 256]


# A set of methods for detecting raw values (numbers and strings) and
# converting them to integers
def frombytes(b):
    return 0 if len(b) == 0 else ord(b[-1]) + 256 * frombytes(b[:-1])


def fromhex(b):
    hexord = lambda x: '0123456789abcdef'.find(x)
    return 0 if len(b) == 0 else hexord(b[-1]) + 16 * fromhex(b[:-1])


def enc(n):
    if is_numeric(n) and n < 2**256 and n > -2**255:
        return ''.join(map(chr, tobytearr(n % 2**256, 32)))
    elif is_numeric(n):
        raise Exception("Number out of range: %r" % n)
    elif is_string(n) and len(n) == 40:
        return '\x00' * 12 + n.decode('hex')
    elif is_string(n) and len(n) <= 32:
        return '\x00' * (32 - len(n)) + n
    elif is_string(n) and len(n) > 32:
        raise Exception("String too long: %r" % n)
    elif n is True:
        return 1
    elif n is False or n is None:
        return 0
    else:
        raise Exception("Cannot encode integer: %r" % n)


def cmdline_enc(n):
    if n[:2] == '0x':
        o = int(n[2:], 16)
    elif re.match('^[0-9]*$', n):
        o = int(n)
    elif len(n) == 40:
        o = n
    else:
        raise Exception("Cannot encode integer: %r" % n)
    return enc(o)


def list_dec(l):
    assert l[0] == '[' and l[-1] == ']'
    return [cmdline_enc(x.strip()) for x in l[1:-1].split(',')]


def encode_datalist(*args):
    raise Exception("Encode datalist deprecated")


def decode_datalist(arr):
    if isinstance(arr, list):
        arr = ''.join(map(chr, arr))
    o = []
    for i in range(0, len(arr), 32):
        o.append(frombytes(arr[i:i + 32]))
    return o


def encode_abi(function_name, sig, *args, **kwargs):
    raise Exception("encode_abi deprecated, please use "
                    "the methods in pyethereum.abi")


def decode_abi(arr, *lens):
    o = []
    pos = 1
    i = 0
    if len(lens) == 1 and isinstance(lens[0], list):
        lens = lens[0]
    while pos < len(arr):
        bytez = int(lens[i]) if i < len(lens) else 32
        o.append(frombytes(arr[pos: pos + bytez]))
        i, pos = i + 1, pos + bytez
    return o


def main():
    if len(sys.argv) == 1:
        print("serpent <command> <arg1> <arg2> ...")
    else:
        cmd = sys.argv[2] if sys.argv[1] == '-s' else sys.argv[1]
        if sys.argv[1] == '-s':
            args = [sys.stdin.read()] + sys.argv[3:]
        elif sys.argv[1] == '-v':
            print(VERSION)
            sys.exit()
        else:
            cmd = sys.argv[1]
            args = sys.argv[2:]
        kwargs = {}
        if cmd in ['deserialize', 'decode_datalist', 'decode_abi']:
            args[0] = args[0].strip().decode('hex')
        if cmd in ['encode_abi']:
            kwargs['source'] = 'cmdline'
        o = globals()[cmd](*args, **kwargs)
        if isinstance(o, (Token, Astnode, list)):
            print(repr(o))
        elif cmd in ['mk_signature', 'mk_full_signature', 'get_prefix']:
            print(o)
        else:
            print(binascii.b2a_hex(o).decode('ascii'))
