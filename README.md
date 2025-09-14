# Emoticode
A brainfuck based Esolang.
https://esolangs.org/wiki/Emoticode

## Emoticode to Brainfuck mapping
```
:D = >
D: = <
:) = +
:( = -
:P = .
:() = ,
-_- = ]
O_O = [
```

## Enhanced Syntax Stuff

```:O_[name]``` is the opening tag for a macro. to close a macro you need a matching ```:|``` tag. Names must be in brackets.

```:>_[name]``` is how you call a defined macro. Names must be in brackets.

```:N_[syntax]``` Repeats the given syntax N times. Example: If ```N``` is 3, ```:N_:)``` expands to ```:) :) :)```. Syntax should not be in brackets.

```F_F``` is a command which forces the text buffer to flush, useful since the interpreter only flushes the frame buffer on a newline or at the end of the program for performance.

```:S_"string"``` allows you to store strings to the tape. For example if you are in cell 3 and do :S_"Hello, World!\n" you will still be in cell 3 but now cells 3-17 will be overwritten to contain the ascii codes for "Hello, World!" + the newline.

```:q``` exists solely for quines. This command writes the entire source code of the program to the tape from the current pointer.

```:C``` starts a comment. To close a comment you need a matching ```:l```. These are pretty simple I don't think I need a long explanation.

Macros are recursive, you can call a macro inside of another macro, **but self referencing macros are disallowed.**

### If you want to check out some Emoticode code, check the samples folder.


## Credits:

- [Simon Forsberg](https://github.com/Zomis) for the original fizzbuzz program which fizzbuzz.O_O is based off of
- Erik Bosman (Idk where to find him sorry) for the original Mandelbrot brainfuck program which mandelbrot.O_O is based off of.
