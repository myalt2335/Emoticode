# Emoticode
A brainfuck based Esolang.

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

## Additional stuff

```:O_[name]``` is the opening tag for a macro. to close a macro you need a matching ```:|``` tag.

```:N_[syntax]``` Repeats the given syntax N times. Example: If ```N``` is 3, ```:N_:)``` expands to ```:) :) :)```.

```:F``` is a command which forces the text buffer to flush, useful since the interpreter only flushes the frame buffer on a newline or at the end of the program.

```:S_"string"``` allows you to store strings to the tape. For example if you are in cell 3 and do :S_"Hello, World!\n" you will still be in cell 3 but now cells 3-16 will be overwritten to contain the ascii codes for "Hello, World!" + the newline.

```:q``` exists solely for quines. This command writes the entire source code of the program to the tape from the current pointer.

Macros are recursive, you can call a macro inside of another macro, **but self referencing macros are disallowed.**

### If you want to check out some Emoticode code, check the samples folder.
