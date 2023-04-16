"""
 Author:
College: Berea College
 Course: CSC420 Programming Languages (Summer 2023)
Purpose: Abstract Syntax Tree Processor for mini.py

  Notes:

History:
            2023-04-05, DMW, created
"""

from ASTPROC import ASTPROC
from ASTNODE import ASTNODE
from Stack import Stack


class MiniAST(ASTPROC):

    def __init__(self, root_node: ASTNODE=None) -> None:
        super().__init__(root_node)
        self.stack = Stack()
        self.names = {}
        self.reserved_names = []

    def process_ast(self):

        # process_node() recursively descends the tree processing leaf nodes left to right
        # noinspection SpellCheckingInspection
        def process_node(node: ASTNODE) -> None:
            # print("NODE: {}".format(node.name))  # use for debugging AST nodes :-)
            if node.name == "program":
                process_node(node.children[0])
            elif node.name == "statement_list":
                for child in node.children:
                    process_node(child)
            elif node.name == "binop":
                process_node(node.children[0])
                process_node(node.children[1])
                self.stack.check_underflow(2)
                y = self.stack.pop()
                x = self.stack.pop()
                if node.value == "+":
                    self.stack.push(y + x)
                elif node.value == "*":
                    self.stack.push(y * x)
                else:
                    raise SyntaxError("Undefined binary operator {}".format(node.value))
                # TODO: implement additional binary operator
            elif node.name == "number":
                self.stack.push(node.value)
            elif node.name == "negate":
                process_node(node.children[0])
                self.stack.push(-self.stack.pop())
            elif node.name == "assign":
                process_node(node.children[0])
                self.names[node.value] = self.stack.pop()
            elif node.name == "name":
                try:
                    self.stack.push(self.names[node.value])
                except LookupError:
                    print("Undefined name '%s'" % node.value)
                    return
            elif node.name == "pass":
                pass
            else:
                error_message = "Undefined node name: {}".format(node.name)
                raise SyntaxError(error_message)

        try:
            process_node(self.root_node)
        except LookupError:
            pass
        except SyntaxError:
            pass
