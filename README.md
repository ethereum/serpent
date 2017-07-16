# Introduction

Serpent is an assembly language that compiles to EVM code that is extended with various high-level features. It's particularly useful for writing code that requires low-level opcode manipulation, but at the same time has access to high-level primitives like in-storage data structures and functions, and seamlessly switches between low and high level formats.

**Being a low-level language, Serpent is not recommended for building applications unless you really really know what you're doing. The creator recommends [Solidity](http://github.com/ethereum/solidity) for most applications or [Viper](http://github.com/ethereum/viper) if you want something python-flavored though it is still experimental.**

# Installation:

`make && sudo make install`


# Testing

Testing is done using `pytest` and `tox`.

```bash
$ pip install tox -r requirements-dev.txt
```


To run the test suite in your current python version:

```bash
$ py.test
```


To run the full test suite across all supported python versions:

```bash
$ tox
```
