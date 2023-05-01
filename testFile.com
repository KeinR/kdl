

(!{push fib} ? fib 0 1 0 21)
({push fib} , {push fib _2} < 20 ? fib {push fib _1} ({push fib _0} + {push fib _1}) ({push fib _2} + 1))
({push fib} , {push fib _2} = 20 ? result {push fib _1})



