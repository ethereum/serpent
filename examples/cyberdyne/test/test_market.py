from ethereum import abi, tester, utils
import os
from serpent import mk_full_signature

def decode_buy_order(order):
    buyprice = -(order // 2**208)
    buyfcvalue = (order // 2**160) % 2**48
    buyer = order % 2**160
    return (buyprice, buyfcvalue, buyer)

def decode_sell_order(order):
    sellprice = order // 2**208
    sellscvalue = (order // 2**160) % 2**48
    seller = order % 2**160
    return (sellprice, sellscvalue, seller)

class TestSimpleContract(object):

    MARKET_CONTRACT = os.path.join(os.path.dirname(__file__), '..', 'market.se')
    SUBCURRENCY_CONTRACT = os.path.join(os.path.dirname(__file__), '..', 'subcurrency.se')
    HEAP_CONTRACT = os.path.join(os.path.dirname(__file__), '..', 'heap.se')
    CONTRACT_GAS = 1356080

    def setup_class(cls):
        cls.s = tester.state()
        cls.m = cls.s.abi_contract(cls.MARKET_CONTRACT, gas=cls.CONTRACT_GAS)

        cls.heap_abi = abi.ContractTranslator(mk_full_signature(cls.HEAP_CONTRACT))

        cls.snapshot = cls.s.snapshot()

    def setup_method(self, method):
        self.s.revert(self.snapshot)

    def init_market(self):
        self.buy_heap = self.s.abi_contract(self.HEAP_CONTRACT)
        self.sell_heap = self.s.abi_contract(self.HEAP_CONTRACT)

        self.buy_heap.set_owner(self.m.address)
        self.sell_heap.set_owner(self.m.address)

        self.c1 = self.s.abi_contract(self.SUBCURRENCY_CONTRACT)
        self.c2 = self.s.abi_contract(self.SUBCURRENCY_CONTRACT)

        self.c1.issue(tester.a0, 1000)
        self.c1.issue(tester.a1, 1000)
        self.c2.issue(tester.a2, 1000000)
        self.c2.issue(tester.a3, 1000000)

        self.m.init_market(self.buy_heap.address, self.sell_heap.address, self.c1.address, self.c2.address)

    def test_create_gas_used(self):
        print("create gas used:", self.s.block.gas_used)
        assert self.s.block.gas_used <= self.CONTRACT_GAS

    def test_init(self):
        assert self.s.block.get_code(self.m.address) != ''

    def test_init_market(self):
        self.init_market()

        assert utils.int_to_addr(self.m.first_currency()) == self.c1.address
        assert utils.int_to_addr(self.m.second_currency()) == self.c2.address

    def test_buy_order(self):
        self.init_market()

        self.c1.send(self.m.address, 1000, sender=tester.k0)
        assert self.m.buy(1200, sender=tester.k0) == 1

        assert self.buy_heap.size() == 1
        order = self.buy_heap.top()
        assert decode_buy_order(order) == (1200, 1000, utils.decode_int256(tester.a0))

    def test_sell_order(self):
        self.init_market()

        self.c2.send(self.m.address, 1000000, sender=tester.k2)
        assert self.m.sell(800, sender=tester.k2) == 1

        assert self.sell_heap.size() == 1
        order = self.sell_heap.top()
        assert decode_sell_order(order) == (800, 1000000, utils.decode_int256(tester.a2))

    def test_tick_before_epoch100(self):
        self.init_market()

        assert self.m.tick() == 0

    def test_tick_partial_buy(self):
        self.init_market()

        self.c1.send(self.m.address, 1000, sender=tester.k0)
        assert self.m.buy(1200, sender=tester.k0) == 1
        self.c2.send(self.m.address, 100000, sender=tester.k2)
        assert self.m.sell(800, sender=tester.k2) == 1

        self.s.mine(100)
        assert self.m.tick() == 1

        assert self.m.volume() == 83
        assert self.m.price() == 1000

        assert self.c1.balance(tester.a0) == 0
        assert self.c2.balance(tester.a0) == 100000
        assert self.c1.balance(tester.a2) == 83
        assert self.c2.balance(tester.a2) == 1000000 - 100000

        assert self.c1.balance(self.m.address) == 917
        assert self.c2.balance(self.m.address) == 0

        assert self.buy_heap.size() == 1
        order = self.buy_heap.top()
        assert decode_buy_order(order) == (1200, 917, utils.decode_int256(tester.a0))

        assert self.sell_heap.size() == 0

        assert self.m.tick() == 0

    def test_tick_partial_sell(self):
        self.init_market()

        self.c1.send(self.m.address, 100, sender=tester.k0)
        assert self.m.buy(1200, sender=tester.k0) == 1
        self.c2.send(self.m.address, 1000000, sender=tester.k2)
        assert self.m.sell(800, sender=tester.k2) == 1

        self.s.mine(100)
        assert self.m.tick() == 1

        assert self.m.volume() == 100
        assert self.m.price() == 1000

        assert self.c1.balance(tester.a0) == 1000 - 100
        assert self.c2.balance(tester.a0) == 80000
        assert self.c1.balance(tester.a2) == 100
        assert self.c2.balance(tester.a2) == 0

        assert self.c1.balance(self.m.address) == 0
        assert self.c2.balance(self.m.address) == 920000

        assert self.buy_heap.size() == 0
        assert self.sell_heap.size() == 1
        order = self.sell_heap.top()
        assert decode_sell_order(order) == (800, 920000, utils.decode_int256(tester.a2))

        assert self.m.tick() == 0

    def test_full_scenario(self):
        self.init_market()

        # Place orders
        self.c1.send(self.m.address, 1000, sender=tester.k0)
        self.m.buy(1200, sender=tester.k0)

        self.c1.send(self.m.address, 1000, sender=tester.k1)
        self.m.buy(1400, sender=tester.k1)

        self.c2.send(self.m.address, 1000000, sender=tester.k2)
        self.m.sell(800, sender=tester.k2)

        self.c2.send(self.m.address, 1000000, sender=tester.k3)
        self.m.sell(600, sender=tester.k3)

        print("Orders placed")

        # Next epoch and ping
        self.s.mine(100)
        print("Mined 100")
        assert self.m.tick() == 1
        print("Updating")

        # Check
        assert self.c2.balance(tester.a0) == 800000
        assert self.c2.balance(tester.a1) == 600000
        assert self.c1.balance(tester.a2) == 833
        assert self.c1.balance(tester.a3) == 714
        print("Balance checks passed")

        assert self.m.volume() == 1547
        assert self.m.price() == 1000
        assert self.c1.balance(self.m.address) == 453
        assert self.c2.balance(self.m.address) == 600000
        assert self.buy_heap.size() == 0
        assert self.sell_heap.size() == 0
