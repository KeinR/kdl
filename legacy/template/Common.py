#  Author:  Deanna M. Wilborne
# College:  Berea College
#  Course:  CSC386 Fall 2021
# Purpose:  A class for common methods used by other classes
#
#           Copyright (2021) Deanna M. Wilborne
#
# History:
#           2023-03-28, DMW, updated for 23SU-CSC420
#           2021-09-17, DMW, created

class Common:

    # provide a short string description of the object's type or class
    # for example, 'int', 'float', 'str', etc.
    # noinspection PyMethodParameters
    def object_type(obj: any) -> str:
        s = str(type(obj)).split("'")[1]
        if '.' in s:
            return s.split('.')[1]
        return s

    # noinspection PyMethodParameters
    def is_int(source: str="") -> bool:
        try:
            int_number = int(source)
        except ValueError:
            return False
        return True

    # noinspection PyMethodParameters
    def is_float(source: str="") -> bool:
        try:
            float_number = float(source)
        except ValueError:
            return False
        return True


# limited testing
if __name__ == '__main__':
    print(Common.object_type(5.1))
    # print(Common.objectType(Token()))
