Bitcoin SPV Relay
=================

This example has 2 main capabilities.

1.  Stores Bitcoin blockheaders, including verifying PoW and a chain is formed.

2.  Given a Bitcoin transaction (and its merkle proof), verify that the
the tx is confirmed on the Bitcoin blockchain, and then relay this tx to an
arbitrary Ethereum contract.

The relay allows any other Ethereum contract to take action based on a Bitcoin transaction, with the assurance that that Bitcoin transaction has been sufficiently confirmed and is on the Bitcoin blockchain. The relay is fully trustless and depend only on the security of Bitcoin and Ethereum. It is a building block that other systems and Dapps can leverage.

The documented code should be able to provide further clarification.

Merkle Proof
------------

The format required is a transaction hash, its index (within the block), and the sibling transaction hashes.  Sample libraries that can help generate these proofs:

* [bitcoin-proof](https://github.com/ethers/bitcoin-proof)
* [pybitcointools](https://github.com/vbuterin/pybitcointools/blob/f81a5fc186d16eee57f76e9519fe4c4123939701/bitcoin/blocks.py#L29)


Tests
-----

Tests are provided but they are not polished code (references to btcrelay.py need to be replaced with btcrelay.se). Tests require:

* [pyethereum](https://github.com/ethereum/pyethereum) 

#### Running tests

Exclude slow tests:
```
py.test test/ -s -m "not slow"
```

Run slow tests without veryslow tests
```
py.test test/ -s -m "slow and not veryslow"
```

All tests:
```
py.test test/ -s
```
