# NOR-based logic gates simulator

When NAND chips are too mainstream

## Compile

    $ make

## Run

To run the simulation you need a chip configuration file. A few are presented with the work:

  * DIP8-4packnot.txt - simple example, reverses inputs, works as XX7404 chip, but with 4 NOTs
  * DIP8-triplexor.txt - medium difficulty example, there are 4 inputs and 3 XOR'ed outputs A^B, B^C & C^D
  * DIP8-fulladder.txt - most complex example - 2 bit full adder, with A0 A1 and B0 B1 inputs and S0 and S1 as outputs (like 10 + 11 = 01)

To execute a simulation just give the simulation configuration file name to the prog:

    $ ./prog DIP8-triplexor.txt

If no file is presented the empty scheme is shown.

The program requires X-server to be started, tested on Ubuntu 18.04 x86_64 & Ubuntu 19.10 x86.
If the program still doesn't work apply instructions in App. I and App. II.

## Configuration files

Basically the file represents 8-legged chips. The following characters convention is applied:

  * ` ` - _space_ - no element
  * `.` - _dot_ - wire
  * `>` - _more sign_ - NOR-gate. Note, that it has inputs at top and bottom and provides signal to the right
  * `O` - _capital O_ - inputs and output of the chip. Cannot be moved.

The width and the height of the chip must be 19 characters, unused cells must
be filled with <space>, EOL doesn't matter, but must be one character wide.

Left _capital Oes_ will be used as inputs, right as outputs.

It is recommended to scribe some information after the schematics, but it's not obligatory.

## The source

### Global notes

The most challenging part of the work was to zip it down into 2053 tokens, most of the defines are used only to reduce the tokens number.

The program consists of three parts:

1. load chip configuration file
2. prepare X window and images
3. loop for different inputs and show the picture

#### Load

As there are no `include` directives we can go back into stone age and use syscalls directly. The `Makefile` checks if the arch is x86 or x86_64 and gives us the syscall numbers which are different on these arches.

So basically the load part is open/read/close sequence with the reading of (19+1)x19 = 380 bytes.
The file is being read every iteration to save some variables for holding it.

### X server stuff

As we don't have `include` directives AND the binary is NOT linked with X11 we
only can use dlopen/dlsym for X-related functions (I wish dlopen\sym were syscalls, everything in the world should be a syscall).
To hide the functions names simple xor is used. The only trick is to shift \0 into space to be able to get rid of unprintable characters in the string.

```
char _n[] = "libX11.so\0XOpenDisplay\0XCreateSimpleWindow\0XCreateImage\0XPutImage\0XMapWindow\0XFlush\0";
->
char _n[] = 
    "libX11.so           "
    "XOpenDisplay        "
    "XCreateSimpleWindow "
    "XCreateImage        "
    "XPutImage           "
    "XMapWindow          "
    "XFlush ";

#define D(t,s) ((t(*)())dlsym(T,_n+(s*20)))
```
where `t` - is the function return type and `s` is the function index calculated as the index of the 20-bytes block in the encrypted string.

Okay, with `dlopen("libX11.so")` and `dlsym("XDisplay")` we are able to call the functions, but some of them require complex structs to work with, like `Display` or `Screen`. The `Makefile` helps us again. I calculated the shifts for the structs for x86 & x86_64 machines and put them into `Makefile`, so basically, when we need, for example, `DefaultGC` property we interpret `Display` variable as `char *` step forward N bytes, then dereference that pointer (now we're _in_ `Screen` struct, and as it's an array and we need the very first element of it just skip the shift) then we interpret it as `char *`, step M bytes forward and dereference that pointer, getting the default GC. Sounds easy:

```
(*(long *)((char *)(*(long *)((char *)d + dS)) + dG))
```
where `dS` is a `Display`->`Screen` shift, and `dG` is `Screen`->`GC` shift.

As `()` (in difference with `(void)`) function allows us to put any number of any arguments no need to convert it into something like `XDisplay`, `XGC` or `XImage *`.

#### Images

Additionally XImage is created. Nothing special, as we know the bit depth and screen size, the static array is used for the data holding.

There is no space for raster data, so the textures are encoded in vector form with a few inctructions:

  * The line from top-left to bottom-right (`0`)
  * The line from top-right to bottom-left (`1`)
  * The line from top to bottom (`2`)
  * The flood fill (`3`)
  * Draw another texture (`5`)
  * Change the colour (`6`)
  * End of data (`4`)

Palette table consist of 16+1 (pseudo black) colours and zipped into 4 colours. The derived colours are made by multiplying index by 1643277 and adding it to the base colour. "Pseudo black" is used to simplify `if (c == 0) do nothing` drawing algo.

A lot of tokens and characters were saved with special macro directives, packing the creation of all the textures into something around 100 bytes.

### Loop

Okay, now we can loop over every input, marking wires `-` and `+` from input points. Flood fill algo is used for that (with a help of macro - the same one from the textures). Then we iterate over the field 20 times, changing the output wires of the NOR gates and using the flood fill for that.

After the field is drawn in memory (technically we can show it in the terminal with `printf("%s", (char *)m);`, that's how I debugged it) we can draw it on the screen, and for that we need to map character code into texture's index. A bit of bit touching and voila:

```
((m[y][x] >> 4) & 1) << 2 | (m[y][x] & 3)
```
with this algo the input ` `, `-`, `+` and `.` characters are interpreted as 0..7 indexes (some are skipped) of the texture.

```
0 space     ' '  = 32 = 0010 0000 =  00
1 wire off  '-'  = 45 = 0010 1101 =  01 
2 wire off  '.'  = 46 = 0010 1110 =  10
3 wire on   '+'  = 43 = 0010 1011 =  11
4                     =             100 // empty, for simplifying the algo
5                     =             101 // empty
6 nor       '>'  = 62 = 0011 1110 = 110
7 hole
```

After the draw is done we flush X-server messages (to remove the fat event loop) and sleep for 536 870 912 nanoseconds (0.5 s) with nanosleep syscall.

#### Shaders

To achieve nice and cozy old LCD screen effect I do R-G-B power reducing and a bit of RGB noise. 

##### LCD

For the slim and fast LCD effect the following algo is used:

```
  0110 0101  1011 0100 1101 1111 - the original pixel colour
&
    1110 0000  1110 0000 1110 0000 - the reducing mask
  |
    0001 1111  0000 0000 0000 0000 - `floating` mask, made as (31 << (x % 3)) << 3.
```

##### Noise

As we don't have stdlib and the simulation of the rand() is too expensive I use FNV-hashing function with no input data every time I need some random:

```
int rn = 2166136261; // init

//                reducing u32 value by mask
xbf[y][x] += b{0000 1111  0000 1111  0000 1111} & (rn *= 16777619);
```
The result is... acceptable. Some visual reprises are noticeable, but don't burn the eyes.

## Troubleshooting

### Appendix I. Bad syscall numbers.

If your system has different syscall numbers than described in the `Makefile` the following macro definitions must be defined in `Makefile` differently:

```
# DS - syscall nanosleep
# DO - syscall open
# DR - syscall read
# DC - syscall close
```

### Appendix II. Bad struct shifts.

If by some reasons your implementation of Xlib differs with Ubuntu's one you can use the following program to rewrite `Makefile`'s macro differently:
```
#include <stdio.h>
#include <X11/Xlib.h>
#include <stddef.h>

#define MY_OFFSET(type, field) ((unsigned long ) &(((type)0)->field))

int main(void) {
    void * dpy = XOpenDisplay(NULL);
    
    _XPrivDisplay xd = dpy;
    printf("dpy = %p\n", dpy);
    
    printf("     src diff = %ld\n", MY_OFFSET(_XPrivDisplay, screens));
    
    printf("     root diff  = %ld\n", MY_OFFSET(Screen *, root));
    printf("     depth diff = %ld\n", MY_OFFSET(Screen *, root_depth));
    printf("     visual diff= %ld\n", MY_OFFSET(Screen *, root_visual));
    printf("     gc diff    = %ld\n", MY_OFFSET(Screen *, default_gc));
    
    printf("real:\n");
    printf("    src   = %p\n", ScreenOfDisplay(dpy, 0));
    printf("    root  = %ld\n", RootWindow(dpy, 0));
    printf("    depth = %d\n", DefaultDepth(dpy, 0));
    printf("    visual= %p\n", DefaultVisual(dpy, 0));
    printf("    gc    = %p\n", DefaultGC(dpy, 0));
    
    return 0;
}
```

```
# X11 structures offsets
# dS - offset of screens in Display
# dR - offset of root in Screen
# dD - offset of root_depth in Screen
# dV - offset of root_visual in Screen
# dG - offset of default_gc in Screen
```
