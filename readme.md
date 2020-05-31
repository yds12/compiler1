# Compiler Project

This project is for learning purposes only.

We implement a compiler for a simple language. The compiler will be divided
into a *lexer* (process the input and generates a list of tokens), 
an [LR(1)](https://en.wikipedia.org/wiki/LR_parser)
*parser* (generates an 
[Abstract Syntax Tree](https://en.wikipedia.org/wiki/Abstract_syntax_tree)
-- or AST) and *code generator* (generates x64 assembly code targeted at
Linux). The resulting assembly will be processed by `nasm` into object 
code, and then linked with `ld`.

## The Language

An incomplete specification of the language is in the `docs`
directory. For the lexer we have [`docs/lexicon.txt`](docs/lexicon.txt), and
for the parser we have [`docs/grammar.txt`](docs/grammar.txt). For now,
the language will just be called **ulp**, for "uma linguagem de programação".

Many examples can be found at the [`test/cases`](test/cases) directory.

## Roadmap

Currently, the compiler is capable of parsing the program
[`docs/current.ulp`](docs/current.ulp). If you have `graphviz` installed in
your machine, you can check the parse trees by using the auxiliar script 
in `aux/view`:

    $ aux/view docs/current.ulp

This is an example of AST:

![AST](docs/ast.png)

Version **0.0.x**: each merge to master should increase `x`.

Version **0.1.0**: some executable being generated. Doesn't have to be the
full language, just a small subset of it (for example: just int arithmetic).
