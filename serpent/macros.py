
def _set(ast):
    if ast[1][0] == 'access':
        if ast[1][1] == 'contract.storage':
            return ['sstore', ast[1][2], ast[2]]
        else:
            return ['arrset', ast[1][1], ast[1][2], ast[2]]


def _access(ast):
    if ast[1] == 'msg.data':
        return ['calldataload', ast[2]]
    elif ast[1] == 'contract.storage':
        return ['sload', ast[2]]


def _array_lit(ast):
    o = ['array', str(len(ast[1:]))]
    for a in ast[1:]:
        o = ['set_and_inc', a, o]
    return ['-', o, str(len(ast[1:])*32)]


def _import(ast): # Import is to be used specifically for creates
    return ['code', parse(open(ast[1]).read())]


def _inset(ast): # Inset is to be used like a macro in C++
    return parse(open(ast[1]).read())

# Note: lambda not easy enough to use, apparently.
macros ={ 'set' : _set,
    'access' : _access,
    'array_lit' : _array_lit,
    'import'   : _import,
    'inset'    : _inset
}

#def _code(ast): # Doesnt really do anything? Just check number of args instead?
#    return ['code', ast[1]]

def _return(ast):
    if len(ast) == 2 and ast[1][0] == 'array_lit':
        return ['return', ast[1], str(len(ast[1][1:]))]

    
def _if(ast):
    return ['ifelse' if len(ast) == 4 else 'if'] + ast[1:]


compiler_macros ={ 'return' : _return,
                   'if':_if
}
    
def macroexpand(ast):
    if type(ast) is list:
        if ast[0] in macros:
            return macroexpand(macros[ast[0]](ast))
        else:
            ret = []
            for el in ast:
                ret.append(macroexpand(el))

            if ret[0] in compiler_macros:  # These only optionally run.
                alt_ret = compiler_macros[ret[0]](ret)
                return ret if alt_ret is None else alt_ret
            else:
                return ret
    else:
        return ast
