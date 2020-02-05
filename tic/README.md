# GDB-controlled tic-tac-toe

## Usage

`make`

Note that as program uses several syscalls and the syscalls table differs for
x86 and x86_64 linux, so `Makefile` should detect it or you need to write the numbers
by yourself.

Run under gdb, set breakpoint to `move()` function and start the process:

```
(gdb) b move
(gdb) r
```

When asked for your move make it via `set()` function:

```
(gdb) p set(2,2)
(gdb) c
```

Allowed coordinates are 0,0 -> 2,2:

```
0,0 | 1,0 | 2,0
----+-----+----
0,1 | 1,1 | 2,1
----+-----+----
0,2 | 1,2 | 2,2 
```

Program contains checks for

    * If it's not run under gdb
    * If the player skips move
    * If the player makes move twice
    * If the player makes incorrect move

## Obfuscation techniques

    * One-letter variables, overuse of ternary operator, ai based on 9 'or's
    * Strings are stored as double with enough precision
    * Win/lose checks are made via hash calculation
    * No external includes or function calls, only plain syscalls

## Caveats

Through the program checks if the player makes his move twice, the checking
could be abused via resetting the `p` variable.