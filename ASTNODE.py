#  Author:  Deanna M. Wilborne
# College:  Berea College
#  Course:  CSC386 Fall 2021
# Purpose:  AST Node Class
# History:
#           2023-04-04, DMW, updated for 2023-Summer CSC420 Programming Languages
#           2021-10-28, DMW, corrected an issue with __init__ where valid values of 0 wouldn't be saved in the node
#           2021-10-21, DMW, created
#
#           Copyright (2023) Deanna M. Wilborne

from anytree import NodeMixin, RenderTree


# noinspection SpellCheckingInspection
class ASTNODE(NodeMixin):
    def __init__(self, name: str, value=None, parent=None, children=None) -> None:
        self.name = name
        self.parent = parent

        if value is not None:
            self.value = value

        if children is not None:
            self.children = children


# limited functional testing
if __name__ == "__main__":
    root = ASTNODE("root")
    child2 = ASTNODE("child2")
    child3 = ASTNODE("child3")
    child = ASTNODE("child", parent=root, children=[child2, child3])
    for pre, fill, node in RenderTree(root):
        print("%s%s" % (pre, node.name))
