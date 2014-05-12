import os.path
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), '..'))

from macros import macroexpand

def assert_eq(expand, should_be=None, say='macroexpand wrong'):
    if should_be is None:  # Latter not specified, then it sohuld not change.
        should_be = expand

    ret = macroexpand(expand)
    if ret != should_be:
        raise Exception(say, ret, 'vs\n', should_be)

assert_eq(['set', ['access', 'contract.storage', 'index'], 'value'],
          ['sstore', 'index', 'value'])
assert_eq(['set', 'x', ['+', '3', '5']])
assert_eq(['if', 1, 2, 3], ['ifelse', 1, 2 ,3])
assert_eq(['if', 1, 2],    ['if', 1, 2])
assert_eq(['access', 'msg.data', 'index'], ['calldataload', 'index'])
assert_eq(['access', 'contract.storage', 'index'], ['sload', 'index'])

assert_eq(['array_lit', 1, 2, 3],
          ['-', ['set_and_inc', 3, ['set_and_inc', 2,
                                    ['set_and_inc', 1, ['array', '3']]]], '96'])
