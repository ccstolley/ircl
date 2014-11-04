ircl
====

![](https://github.com/ccstolley/misc/blob/master/img/ircl.png)

Simple IRC client with customized tab completion.


This is a work in progress. The goal is the simplest possible IRC
client with a small set of features that make using it fast and
easy.

Files of note:

`${HOME}/.ircllog` - transcript of all traffic in ircl

`${HOME}/.irclusers` - supplemental list of names for tab completion (one per line)

Dependencies
------------

1. A C compiler
2. The [GNU Readline](http://www.gnu.org/software/readline/) library
and header files. Most BSD and Linux distributions include this,
but for example Mac OSX does not.
