import serpent_pyext as pyext
import sys
import re
import binascii
import json
import os

VERSION = '2.0.2'

if sys.version_info.major == 2:
    str_types = (bytes, str, unicode)
else:
    str_types = (bytes, str)


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
        subs = list(map(repr, self.args))
        k = 0
        out = " "
        while k < len(subs) and o != "(seq":
            if '\n' in subs[k] or len(out + subs[k]) >= 80:
                break
            out += bytestostr(subs[k]) + " "
            k += 1
        if k < len(subs):
            o += out + "\n  "
            o += '\n  '.join('\n'.join(map(bytestostr, subs[k:])).split('\n'))
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
    return pyext.parse_lll(x) if is_string(x) else x.out()


def takelist(x):
    return map(take, parse(x).args if is_string(x) else x)


def pre_transform(code, params):
    code2 = ''
    for k, v in params.items():
        if isinstance(v, str_types):
            v = '"' + str(v) + '"'
        code2 += 'macro $%s:\n    %s\n' % (k, v)
    if os.path.exists(code):
        return code2 + "inset('" + code + "')"
    return code2 + code

compile = lambda code, **kwargs: pyext.compile(strtobytes(pre_transform(code, kwargs)))
compile_chunk = lambda code, **kwargs: pyext.compile_chunk(strtobytes(pre_transform(code, kwargs)))
compile_to_lll = lambda code, **kwargs: node(pyext.compile_to_lll(strtobytes(pre_transform(code, kwargs))))
compile_chunk_to_lll = lambda code, **kwargs: node(pyext.compile_chunk_to_lll(strtobytes(pre_transform(code, kwargs))))
compile_lll = lambda x: pyext.compile_lll(take(strtobytes(x)))
parse = lambda code, **kwargs: node(pyext.parse(strtobytes(pre_transform(code, kwargs))))
rewrite = lambda x: node(pyext.rewrite(take(strtobytes(x))))
rewrite_chunk = lambda x: node(pyext.rewrite_chunk(take(strtobytes(x))))
pretty_compile = lambda code, **kwargs: map(node, pyext.pretty_compile(strtobytes(pre_transform(code, kwargs))))
pretty_compile_chunk = lambda code, **kwargs: map(node, pyext.pretty_compile_chunk(strtobytes(pre_transform(code, kwargs))))
pretty_compile_lll = lambda code, **kwargs: map(node, pyext.pretty_compile_lll(take(strtobytes(pre_transform(code, kwargs)))))
serialize = lambda x: pyext.serialize(takelist(strtobytes(x)))
deserialize = lambda x: map(node, pyext.deserialize(x))
mk_signature = lambda code, **kwargs: pyext.mk_signature(strtobytes(pre_transform(code, kwargs)))
mk_full_signature = lambda code, **kwargs: json.loads(bytestostr(pyext.mk_full_signature(strtobytes(pre_transform(code, kwargs)))))
mk_contract_info_decl = lambda code, **kwargs: json.loads(bytestostr(pyext.mk_contract_info_decl(strtobytes(pre_transform(code, kwargs)))))
get_prefix = lambda x: pyext.get_prefix(strtobytes(x)) % 2**32

if sys.version_info.major == 2:
    is_string = lambda x: isinstance(x, (str, unicode, bytes))
    is_numeric = lambda x: isinstance(x, (int, long))
else:
    is_string = lambda x: isinstance(x, (str, bytes))
    is_numeric = lambda x: isinstance(x, int)

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
            argz = [sys.stdin.read()] + sys.argv[3:]
        elif sys.argv[1] == '-v':
            print(VERSION)
            sys.exit()
        else:
            cmd = sys.argv[1]
            argz = sys.argv[2:]
        args, kwargs = [], {}
        i = 0
        while i < len(argz):
            if argz[i][:2] == '--':
                kwargs[argz[i][2:]] = argz[i+1]
                i += 2
            else:
                args.append(argz[i])
                i += 1
        if cmd in ['deserialize', 'decode_datalist', 'decode_abi']:
            args[0] = args[0].strip().decode('hex')
        if cmd in ['encode_abi']:
            kwargs['source'] = 'cmdline'
        o = globals()[cmd](*args, **kwargs)
        if cmd in ['mk_full_signature', 'mk_contract_info_decl']:
            print(json.dumps(o))
        elif isinstance(o, (Token, Astnode, dict, list)):
            print(repr(o))
        elif cmd in ['mk_full_signature', 'get_prefix']:
            print(json.dumps(json.loads(o)))
        elif cmd in ['mk_signature', 'get_prefix']:
            print(o)
        else:
            print(binascii.b2a_hex(o).decode('ascii'))
