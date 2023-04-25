```
All integral values are encoded in little endianness

k d l 0x00 - opening format sequnce version 0

0x1 - context begin
0x2 - context end

String header:
String is a series of words and numbers seperated by spaces
0x3 - begin header
XX XX - value length

Integer header:
Integers are encoded in directly
0x4 - begin header
XX XX XX XX - value

Compute:
Compute logic
Stack based, items are pushed onto stack, actions are
then performed on objects on the stack
0x5 - begin header
XX XX XX XX - length
The operators are the typical <>/-=+*

state:
set the current state for the next variable
0x6 - begin header
Then one of
    0x0 - label state
    0x1 - area state
    0x2 - characteristic state
STRING VALUE - the name of the state to set

persistant state:
set the current state for the entire context
0x7 - begin header
Then one of
    0x0 - label state
    0x1 - area state
    0x2 - characteristic state
STRING VALUE - the name of the state to set

variable:
0x8 - begin header
STRING VALUE - the variable name

variable declaration:
0x9 - begin header
STRING VALUE - name
STRING VALUE or NUMBER VALUE - the value

function declaration:
0x10 - begin header
STRING VALUE - name
INT VALUE - length (in bytes)

Number headers:
0x11 - begin header
XX XX XX XX XX XX XX XX - value

Order sequence:
0x12 - begin header
INT VALUE - length (in bytes)

Order timeline:
0x13 begin header
INT VALUE - length (in bytes)
INT VALUE - items (count)

Order

```


