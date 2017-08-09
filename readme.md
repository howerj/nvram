# readme.md

This is project presents a small tutorial for a technique to automatically
store and load designated variables with minimal hassle for the programmer. The
variables can be declared normally, without massive amounts of serialization
or deserialization code. 

The method is not without limitations or problems, but it is quick and simple
to implement.

The tutorial is written in [nvram.c][], this file should be viewed to see how
it works, **spoiler** - get your [linker][] to put your variables in a custom
section and write this to disk using [atexit][].

This code works with either [GCC][] or [Clang][].

## Prerequisites 

* Either [GCC][] or [Clang][] must be installed and on the PATH.
* As must [GNU Make][]

## Building

To build and run the program:

	make nvram
	make run

When run, the nvram program will attempt to load a file off disk called
"nvram.blk", if it cannot do that then the default values as specified in the C
source code are used.

The variables get written back to disk on program exit, the demonstration
program does nothing fancy, it is simply there to illustrate the technique.

This program has to be run multiple times to see any affect.

## Editing the data

A hacked together editor using [doxygen][] and [perl][] has been added, it is
another demonstration of a concept. The editor script, [editor.pl][], takes as
its input two files, an [XML][] file produced by [doxygen][] which contains a 
processed version of [nvram.c][]. The declarations are easily extractable from
this [XML][] file (the [perl][] module [TreePP][] does the parsing), and
variables with the correct declaration are searched for. This used to
automatically construct an editor for the "nvram.blk" file. 

The editor itself is very primitive.

[nvram.c]: nvram.c
[linker]: https://en.wikipedia.org/wiki/Linker_(computing)
[atexit]: http://man7.org/linux/man-pages/man3/atexit.3.html
[GCC]: https://gcc.gnu.org/
[Clang]: https://clang.llvm.org/
[GNU Make]: https://www.gnu.org/software/make/
[doxygen]: http://www.stack.nl/~dimitri/doxygen/
[perl]: https://www.perl.org/
[editor.pl]: editor.pl
[XML]: https://en.wikipedia.org/wiki/XML
[TreePP]: http://search.cpan.org/~kawasaki/XML-TreePP-0.43/lib/XML/TreePP.pm
