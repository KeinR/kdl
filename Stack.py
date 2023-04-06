#  Author:  Deanna M. Wilborne
# College:  Berea College
#  Course:  CSC386 Fall 2021
# Purpose:  Create a class for stacks
#
#           Copyright (2021) Deanna M. Wilborne
#
# History:
#           2023-04-04, DMW, added check for underflow on the peek methods
#           2023-03-27, DMW, updated for CSC420 Programming Languages
#           2021-09-19, DMW, added many stack manipulating methods
#           2021-09-08, DMW, created

from Common import Common


class Stack:
    # construct a new stack
    def __init__(self):
        self.stack = []

    # check for stack underflow, raise an exception
    def check_underflow(self, items: int):
        if len(self.stack) < items:
            raise IndexError("Stack underflow.")

    def check_top_int(self):
        if not self.is_top_int():
            raise TypeError("Integer expected.")

    # duplicate the top of stack
    def dup(self):
        self.check_underflow(1)
        self.stack.append(self.stack[-1])

    # drop, without returning the value, the top of the stack
    def drop(self):
        self.pop()

    # drop everything on the stack
    def drop_all(self):
        self.stack = []

    # drop n items on the stack
    def drop_n(self):
        self.check_underflow(1)
        n = self.pop()
        self.check_underflow(n)
        for i in range(0, n):
            self.drop()

    # the tertiary operator
    def question(self):
        self.check_underflow(3)
        cond = self.pop()  # get the condition to be tested
        false = self.pop()  # get the result if false
        true = self.pop()  # get the result if true
        self.push(true if cond else false)

    def is_top_int(self):
        if self.top_type() == 'int':
            return True
        return False

    # make a list
    def make_list(self):
        self.check_top_int() # the top of the stack should be an integer, and is the size of the list
        n = self.pop()
        self.check_underflow(n) # make sure there are n items on the stack, or there is an underflow
        l = []
        for i in range(0, n):
            l.insert(0, self.pop())
        self.push(l)

    # generic peek stack function
    def peek(self, item=1):
        self.check_underflow(item)
        return self.stack[-item]

    # peek stack functions
    def peek_x(self):
        self.check_underflow(1)
        return self.peek()

    def peek_y(self):
        self.check_underflow(2)
        return self.peek(2)

    def peek_z(self):
        self.check_underflow(3)
        return self.peek(3)

    def peek_t(self):
        self.check_underflow(4)
        return self.peek(4)

    # pop the last item off the stack
    def pop(self):
        self.check_underflow(1)
        return self.stack.pop()

    # push an item onto the stack
    def push(self, item):
        self.stack.append(item)

    # rotate the third item to the top of the stack
    def rot(self):
        self.check_underflow(3)
        self.stack.append(self.stack.pop(-3))

    def true(self):
        self.push(True)

    def false(self):
        self.push(False)

    # return the number of items on the stack
    def size(self):
        return len(self.stack)

    # swap the top two stack items
    def swap(self):
        self.check_underflow(2)
        tmp_x = self.pop()
        tmp_y = self.pop()
        self.push(tmp_x)
        self.push(tmp_y)

    # for internal use by class Stack
    def top_type(self, depth = -1):
        return Common.object_type(self.stack[depth])

    # to be used by RPL programs
    def type(self):
        self.check_underflow(1)
        self.push(Common.object_type(self.stack.pop()))


if __name__ == "__main__":
    x = Stack()
    x.push(13)
    for i in range(0, 6):
        x.push(i)
    x.push(6)
    #print(x.stack)
    x.make_list()
    x.push(17)
    print(x.stack)
