"""
 Author: Deanna M. Wilborne
College: Berea College
 Course: CSC420 Programming Languages (Summer 2023)
Purpose: Abstract Syntax Tree Processor

  Notes:
            Copyright (2023) Deanna M. Wilborne

History:
            2023-04-05, DMW, created
"""

from ASTNODE import ASTNODE
from anytree import RenderTree


# noinspection SpellCheckingInspection
class ASTPROC:

    def __init__(self, root_node: ASTNODE=None) -> None:
        self.root_node = root_node

    def __str__(self):
        output = ""
        for pre, fill, node in RenderTree(self.root_node):
            # print("{}{}\n".format(pre, node.name), end='')
            output += "{}{}\n".format(pre, node.name)
        return output


# 2023-04-05, DMW, limited testing
if __name__ == "__main__":
    child1_node = ASTNODE("child1")
    child2_node = ASTNODE("child2")
    my_root_node = ASTNODE("root", children=[child1_node, child2_node])
    my_ast = ASTPROC(my_root_node)
    print(my_ast)
