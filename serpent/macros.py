
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

def _return(ast):
    if len(ast) == 2 and ast[1][0] == 'array_lit':
        return ['return', ast[1], str(len(ast[1][1:]))]

    
def _if(ast):
    return ['ifelse' if len(ast) == 4 else 'if'] + ast[1:]

def _set(ast):
    if ast[1][0] == 'access':
        if ast[1][1] == 'contract.storage':
            return ['sstore', ast[1][2], ast[2]]
        else:
            return ['arrset', ast[1][1], ast[1][2], ast[2]]

dont_recurse = ['access', 'if', '']
# Note: lambda not easy enough to use, apparently.
macros = {
    'access' : _access,
    'array_lit' : _array_lit,
    'import'   : _import,
    'inset'    : _inset,
    'return' : _return,
    'if'     : _if,
    'set'    : _set,
    'access' : _access
}

#def _code(ast): # Doesnt really do anything? Just check number of args instead?
#    return ['code', ast[1]]

def macroexpand_list(list):
    ret = []
    for el in list:
        ret.append(macroexpand(el))
    return ret

def macroexpand(ast):
    if type(ast) is list:
        if ast[0] in macros:
            ret = macros[ast[0]](ast)
            if ast[0] in dont_recurse:
                return ret
            elif ret is None:
                return macroexpand_list(ast)
            else:
                return macroexpand(ret)
        else:
            return macroexpand_list(ast)
    else:
        return ast
