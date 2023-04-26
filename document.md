




```

assembly point: 41,543.
bad place: 50, 500.

akatsuki, taihou, hibiki, akagi:
do assemble at assembly point

akatsuki:
do overthrottle.
do passive radar.
do join patrol 6, link up if enemy spotted.
do assemble at HQ if location is bad place or 

akatsuki, taihou, hibiki:
do form fleet patrol 6.

patrol 6: engage on sight

patrol 6, akagi: practice radio silence

akagi:
Report via aircraft

akagi:
if enemy detected then retreat, 


packet akagi's instructions [
    > assembly point
    ! launch planes
    ! patrol with planes
    ! 
]

akatsuki:
>> assembly point
&>> do radio akagi 
|>>

enemy radio > decrypt * 

-

```

- Reporting (radio, aircraft reports)
- Scouting
- Fleet creation
- Movement
- Attacking
- Defending (ie, strategies) / retreating
- Conditionals/statuses
    - Health
        - of individual componenets
        - Fleet overall
    - Enemy fleet composition
    - Current number of planes in the air
        - Scouting
        - Attacking
        - Patroling
        - Screening
    - Current orders
        - Screen
        - Patrol
            - Radius
        - Sumbmerge (submarine)
        - Attack on sight (any number of enemies)
        - Attack enemy
        - Go to location
            - Merge with fleet
            - Dock in port
        - Enemy focus
        - Use/don't use main guns/torpedoes
        - Resupply (at ports only)
        - Bombarding area
            - target factories
            - target supply depots
            - target fortifications
    - Current action (not orders)
        - Defending
        - Taking on water
    - Materiel
        - Ammo
            - Shells
            - Torpedoes
        - Fuel
- Events
    - Enemy spotted
        - Submarine
        - Surface fleet
        - Count
    - Land spotted
        - Port spotted
    - Ally spotted
    - Low oxygen / oxygen at % (submarines)
    - Port report
        - Fortifications
        - Supplies
        - Controller


Ports:
- Can be controlled by fleets
- Combat favors destroyers if ordered to storm, battleships and carriers if
  ordered to bombard
- The more a port is bombarded, the more its limited supplies are depleated.
- Ports replenish some supplies every day, though their stockpile is what
  matters
- Islands can be bombarded, though you may not know where key resources are -
  this is where intelligence comes into play.




Three attributes:
- Thigns about the ship
- Things about what's around the ship
- What the ship is doing




```

<condition> ? <orders> .

referencing higher contexts:
^ health:
^^^ health:
Inside parenthesies:
(health: )
(^^health: )

To delcare variables:
on use: <name> ? write <value>.

To declare functions:
on call: <name> ? inject & <instructions>.

Value literal:
word
(word word word)
(x: 4, y: 9)

----

(push: order ? inject ::
    (? engage)
    (? go HQ)
    (? activate _0)
    (? activate _1))
(pull: testing char ? write [radar])

(akatsuki: (^armor: $health = 80%) ? forget ::
    (akagi:? go (root: $HQ) ::
        (? dock $here))
    (? wait 100 :: target 40 80))
(akatsuki armor: 80% health = maintinence needed = & ? ::
    (akagi:? go $HQ ::
        (? dock $here))
    (akatsuki:? wait 100 ::
        (? goto 40 80)))
(akatsuki: (^spotted enemies: ($count = 4, $distance - 3 > 40); $count = 5) ? order radar [scanlines])
(akatsuki moving speed = high ? ::
    (akatsuki:? radar activate [do passively])
    (akatsuki:? do {testing char})
    (? (akatsuki: order)))
(? akatsuki: radar passive)

# Fib?
(? fib 0 1 0)
(push: fib & (^fib: _2 < 100) ? inject ::
    (? fib _1 (_0 + _1) (_2 + 1)))
(push: fib & (^fib: _2 == 100) ? inject ::
    (? result _2))

>> persist the context as the default, across the scope defiend by the parentiesies

```

Special characters
- $, (, ), %, >, !, ?, :, ;, @, =, <, >, -, +, *, /, 

```
S = PROGRAM
PROGRAM = ST_LIST
ST_LIST = ST_LIST ST
ST = CTX_MK
ST = CMD_LIST
ST = COND_LIST
CTX_MK = CTRLC LABEL :
CTRLC = @
CTRLC = $
CTRLC = >
CTRLC = !
LABEL = ([a-zA-Z][a-zA-Z0-9_]+\n?)+
CMD_LIST = CMD_LIST CMD
CMD_LIST = CMD
CMD = COM .





-------


S* = PROGRAM
PROGRAM = STUFF
STUFF = THING STUFF
STUFF = THING
THING = FULL_CMD
THING = TOP_LABEL


FULL_CMD = COND_LIST ? CMD_LIST
FULL_CMD = CMD_LIST
COND_LIST = LABEL COND_LIST
// , = and, ; = or
COND_LIST = COND , COND_LIST
COND_LIST = COND ; COND_LIST
COND_LIST = COND

COND = ( COND )
COND = VALUE = VALUE
COND = VALUE != VALUE
COND = VALUE < VALUE
COND = VALUE > VALUE
COND = VALUE <= VALUE
COND = VALUE >= VALUE
VALUE = ( VALUE )
VALUE = VALUE + VALUE
VALUE = VALUE - VALUE
VALUE = VALUE / VALUE
VALUE = VALUE * VALUE
VALUE = LITERAL
VALUE = VARIABLE
// So that we can break out of the context temporarially
VARIABLE = LABEL NAME
VARIABLE = NAME
LITERAL = NUMBER
LITERAL = STRING
NUMBER = \d+(\.\d*)?
STRING = " WORDS "
STRING = RAW_WORDS
WORDS = [literally anything lol]
RAW_WORDS = RAW_WORD RAW_WORDS
RAW_WORD = [a-zA-Z_]+

LABEL = ID NAME :
TOP_LABEL = ID2 NAME >>
ID = #
ID = !
ID = >
ID = $
ID2 = @
ID2 = #
NAME = ([a-zA-Z][a-zA-Z0-9_]+[ ]?)+

CMD_LIST = TIMELINE . CMD_LIST
CMD_LIST = TIMELINE .

TIMELINE = ORDER_PRE & TIMELINE
TIMELINE = ORDER_PRE | TIMELINE
TIMELINE = ORDER_PRE

ORDER_PRE = LABEL ORDER
ORDER_PRE = ORDER

ORDER = VERB
ORDER = VERB NOUN
ORDER = VERB NOUN ADJECTIVE

VERB = RAW_WORD
NOUN = RAW_WORDS
ADJECTIVE = ~ RAW_WORD


```





