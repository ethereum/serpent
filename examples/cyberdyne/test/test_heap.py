from ethereum import tester, utils
import os

class TestHeapContract(object):

    CONTRACT = os.path.join(os.path.dirname(__file__), '..', 'heap.se')
    CONTRACT_GAS = 325169

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
        assert self.c.size() == 0
        assert self.c.top() == 0

    def test_pop_empty_heap(self):
        assert self.c.pop() == 0
        assert self.c.size() == 0
        assert self.c.top() == 0

        assert self.c.pop() == 0
        assert self.c.size() == 0
        assert self.c.top() == 0

    def test_only_owner_can_modify(self):
        self.c.push(1, sender=tester.k1)
        assert self.c.size() == 0
        assert self.c.top() == 0

        self.c.push(1)
        assert self.c.pop(sender=tester.k1) is None
        assert self.c.size() == 1
        assert self.c.top() == 1

    def test_push_1_pop_1(self):
        self.c.push(1)
        assert self.c.size() == 1
        assert self.c.top() == 1

        assert self.c.pop() == 1
        assert self.c.size() == 0
        assert self.c.top() == 0

    def test_push_1_2_pop_1_2(self):
        self.c.push(1)
        assert self.c.size() == 1
        assert self.c.top() == 1

        self.c.push(2)
        assert self.c.size() == 2
        assert self.c.top() == 1

        assert self.c.pop() == 1
        assert self.c.size() == 1
        assert self.c.top() == 2

        assert self.c.pop() == 2
        assert self.c.size() == 0
        assert self.c.top() == 0

    def test_push_3_1_2_pop_1_2_3(self):
        self.c.push(3)
        assert self.c.size() == 1
        assert self.c.top() == 3

        self.c.push(1)
        assert self.c.size() == 2
        assert self.c.top() == 1

        self.c.push(2)
        assert self.c.size() == 3
        assert self.c.top() == 1

        assert self.c.pop() == 1
        assert self.c.size() == 2
        assert self.c.top() == 2

        assert self.c.pop() == 2
        assert self.c.size() == 1
        assert self.c.top() == 3

        assert self.c.pop() == 3
        assert self.c.size() == 0
        assert self.c.top() == 0
