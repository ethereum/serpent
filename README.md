# Introduction

Serpent is an assembly language that compiles to EVM code that is extended with various high-level features. It can be useful for writing code that requires low-level opcode manipulation as well as access to high-level primitives like the ABI.

**Being a low-level language, Serpent is NOT RECOMMENDED for building applications unless you really really know what you're doing. The creator recommends [Solidity](http://github.com/ethereum/solidity) as a default choice, LLL if you want close-to-the-metal optimizations, or [Vyper](http://github.com/ethereum/viper) if you like its features though it is still experimental.**

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
