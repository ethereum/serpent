#include <Python.h>
#include "structmember.h"
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include "funcs.h"

#if PY_MAJOR_VERSION >= 3
#define PY_STRING_FORMAT "y#"
#else
#define PY_STRING_FORMAT "s#"
#endif

#define PYMETHOD(name, FROM, method, TO) \
    static PyObject * name(PyObject *, PyObject *args) { \
        try { \
        FROM(med) \
        return TO(method(med)); \
        } \
        catch (std::string e) { \
           PyErr_SetString(PyExc_Exception, e.c_str()); \
           return NULL; \
        } \
    }

#define PYMETHOD2(name, FROM, method, TO) \
    static PyObject * name(PyObject *, PyObject *args) { \
        try { \
        FROM(med) \
        return TO(method(a1med, a2med)); \
        } \
        catch (std::string e) { \
           PyErr_SetString(PyExc_Exception, e.c_str()); \
           return NULL; \
        } \
    }

#define FROMSTR(v) \
    const char *command; \
    int len; \
    if (!PyArg_ParseTuple(args, PY_STRING_FORMAT, &command, &len)) \
        return NULL; \
    std::string v = std::string(command, len); \

#define FROMSTRSTR(v) \
    const char *command1; \
    int len1; \
    const char *command2; \
    int len2; \
    if (!PyArg_ParseTuple(args, "s#s#", &command1, &len1, &command2, &len2)) \
        return NULL; \
    std::string a1##v = std::string(command1, len1); \
    std::string a2##v = std::string(command2, len2); \

#define FROMNODE(v) \
    PyObject *node; \
    if (!PyArg_ParseTuple(args, "O", &node)) \
        return NULL; \
    Node v = cppifyNode(node);

#define FROMLIST(v) \
    PyObject *node; \
    if (!PyArg_ParseTuple(args, "O", &node)) \
        return NULL; \
    std::vector<Node> v = cppifyNodeList(node);

// Convert metadata into python wrapper form [file, ln, ch]
PyObject* pyifyMetadata(Metadata m) {
    PyObject* a = PyList_New(0);
    PyList_Append(a, Py_BuildValue(PY_STRING_FORMAT, m.file.c_str(), m.file.length()));
    PyList_Append(a, Py_BuildValue("i", m.ln));
    PyList_Append(a, Py_BuildValue("i", m.ch));
    return a;
}

// Convert node into python wrapper form 
// [token=0/astnode=1, val, metadata, args]
PyObject* pyifyNode(Node n) {
    PyObject* a = PyList_New(0);
    PyList_Append(a, Py_BuildValue("i", n.type == ASTNODE));
    PyList_Append(a, Py_BuildValue(PY_STRING_FORMAT, n.val.c_str(), n.val.length()));
    PyList_Append(a, pyifyMetadata(n.metadata));
    for (unsigned i = 0; i < n.args.size(); i++)
        PyList_Append(a, pyifyNode(n.args[i]));
    return a;
}

// Convert string into python wrapper form
PyObject* pyifyString(std::string s) {
    return Py_BuildValue(PY_STRING_FORMAT, s.c_str(), s.length());
}

// Convert integer into python wrapper form
PyObject* pyifyInteger(unsigned int i) {
    return Py_BuildValue("i", i);
}

// Convert list of nodes into python wrapper form
PyObject* pyifyNodeList(std::vector<Node> n) {
    PyObject* a = PyList_New(0);
    for (unsigned i = 0; i < n.size(); i++)
        PyList_Append(a, pyifyNode(n[i]));
    return a;
}

// Convert pyobject int into normal form
int cppifyInt(PyObject* o) {
    int out;
    if (!PyArg_Parse(o, "i", &out))
        err("Argument should be integer", Metadata());
    return out;
}

// Convert pyobject string into normal form
std::string cppifyString(PyObject* o) {
    const char *command;
#if PY_MAJOR_VERSION >= 3
    if (!PyArg_Parse(o, "y", &command))
#else
    if (!PyArg_Parse(o, "s", &command))
#endif
        err("Argument should be string", Metadata());
    return std::string(command);
}

// Convert metadata from python wrapper form
Metadata cppifyMetadata(PyObject* o) {
    std::string file = cppifyString(PyList_GetItem(o, 0));
    int ln = cppifyInt(PyList_GetItem(o, 1));
    int ch = cppifyInt(PyList_GetItem(o, 2));
    return Metadata(file, ln, ch);
}

// Convert node from python wrapper form
Node cppifyNode(PyObject* o) {
    Node n;
    int isAstNode = cppifyInt(PyList_GetItem(o, 0));
    n.type = isAstNode ? ASTNODE : TOKEN;
    n.val = cppifyString(PyList_GetItem(o, 1));
    n.metadata = cppifyMetadata(PyList_GetItem(o, 2));
    std::vector<Node> args;
    for (int i = 3; i < PyList_Size(o); i++) {
        args.push_back(cppifyNode(PyList_GetItem(o, i)));
    }
    n.args = args;
    return n;
}

//Convert list of nodes into normal form
std::vector<Node> cppifyNodeList(PyObject* o) {
    std::vector<Node> out;
    for (int i = 0; i < PyList_Size(o); i++) {
        out.push_back(cppifyNode(PyList_GetItem(o,i)));
    }
    return out;
}

PYMETHOD(ps_compile, FROMSTR, compile, pyifyString)
PYMETHOD(ps_compile_to_lll, FROMSTR, compileToLLL, pyifyNode)
PYMETHOD(ps_compile_lll, FROMNODE, compileLLL, pyifyString)
PYMETHOD(ps_parse, FROMSTR, parseSerpent, pyifyNode)
PYMETHOD(ps_rewrite, FROMNODE, rewrite, pyifyNode)
PYMETHOD(ps_pretty_compile, FROMSTR, prettyCompile, pyifyNodeList)
PYMETHOD(ps_pretty_compile_lll, FROMNODE, prettyCompileLLL, pyifyNodeList)
PYMETHOD(ps_serialize, FROMLIST, serialize, pyifyString)
PYMETHOD(ps_deserialize, FROMSTR, deserialize, pyifyNodeList)
PYMETHOD(ps_parse_lll, FROMSTR, parseLLL, pyifyNode)
PYMETHOD(ps_mk_signature, FROMSTR, mkSignature, pyifyString)
PYMETHOD(ps_mk_full_signature, FROMSTR, mkFullSignature, pyifyString)
PYMETHOD(ps_mk_contract_info_decl, FROMSTR, mkContractInfoDecl, pyifyString)
PYMETHOD(ps_get_prefix, FROMSTR, getPrefix, pyifyInteger)


static PyMethodDef PyextMethods[] = {
    {"compile",  ps_compile, METH_VARARGS,
        "Compile code."},
    {"compile_to_lll",  ps_compile_to_lll, METH_VARARGS,
        "Compile code to LLL."},
    {"compile_lll",  ps_compile_lll, METH_VARARGS,
        "Compile LLL to EVM."},
    {"parse",  ps_parse, METH_VARARGS,
        "Parse serpent"},
    {"rewrite",  ps_rewrite, METH_VARARGS,
        "Rewrite parsed serpent to LLL"},
    {"pretty_compile",  ps_pretty_compile, METH_VARARGS,
        "Compile to EVM opcodes"},
    {"pretty_compile_lll",  ps_pretty_compile_lll, METH_VARARGS,
        "Compile LLL to EVM opcodes"},
    {"serialize",  ps_serialize, METH_VARARGS,
        "Convert EVM opcodes to bin"},
    {"deserialize",  ps_deserialize, METH_VARARGS,
        "Convert EVM bin to opcodes"},
    {"parse_lll",  ps_parse_lll, METH_VARARGS,
        "Parse LLL"},
    {"mk_signature",  ps_mk_signature, METH_VARARGS,
        "Make an extern signature for a file"},
    {"mk_full_signature",  ps_mk_full_signature, METH_VARARGS,
        "Make an extern signature for ABI use"},
    {"mk_contract_info_decl",  ps_mk_contract_info_decl, METH_VARARGS,
        "Make an extern contract info declaration"},
    {"get_prefix",  ps_get_prefix, METH_VARARGS,
        "Get the prefix for a signature declaration (eg. \"foo:[int256]:int256\")"},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

#if PY_MAJOR_VERSION >= 3
static struct PyModuleDef SerpentModule = {
    PyModuleDef_HEAD_INIT,
    "serpent_pyext",
    "...",
    -1,
    PyextMethods
};

PyMODINIT_FUNC PyInit_serpent_pyext(void) {
    return PyModule_Create(&SerpentModule);
}

#else
PyMODINIT_FUNC initserpent_pyext(void)
{
     Py_InitModule( "serpent_pyext", PyextMethods );
}
#endif
