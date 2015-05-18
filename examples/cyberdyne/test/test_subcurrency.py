from ethereum import tester, utils
import os

class TestSubcurrencyContract(object):

    CONTRACT = os.path.join(os.path.dirname(__file__), '..', 'subcurrency.se')
    CONTRACT_GAS = 161361

    def setup_class(cls):
        cls.s = tester.state()
        cls.c = cls.s.abi_contract(cls.CONTRACT)
        cls.snapshot = cls.s.snapshot()

    def setup_method(self, method):
        self.s.revert(self.snapshot)

    def test_create_gas_used(self):
        print("create gas used:", self.s.block.gas_used)
        assert self.s.block.gas_used <= self.CONTRACT_GAS

    def test_init(self):
        assert self.s.block.get_code(self.c.address) != ''
        assert utils.int_to_addr(self.s.block.get_storage_data(self.c.address, 0)) == tester.a0

    def test_simple_send(self):
        self.c.issue(tester.a0, 100000000000)
        assert self.c.balance(tester.a0) == 100000000000

        assert self.c.send(tester.a1, 1000) == 1
        assert self.c.balance(tester.a0) == 100000000000 - 1000
        assert self.c.balance(tester.a1) == 1000
        last_tx = self.c.last_tx()
        assert utils.int_to_addr(last_tx[0]) == tester.a0
        assert utils.int_to_addr(last_tx[1]) == tester.a1
        assert last_tx[2:] == [1000, 1]

    def test_send_should_have_sufficient_balance(self):
        assert self.c.send(tester.a2, 1000, sender=tester.k1) == 0

    def test_send_with_negative_amount_should_fail(self):
        self.c.issue(tester.a0, 100000000000)
        assert self.c.send(tester.a0, -1000, sender=tester.k1) == 0
        assert self.c.balance(tester.a0) == 100000000000
        assert self.c.balance(tester.a1) == 0
