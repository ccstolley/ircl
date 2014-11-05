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

Features
--------

- tab completion of commands, nicks, channels and custom names from .irclusers. 
- simple, colorized display
- maintains complete log of all activity
- quiet output: intelligently mutes join/part/mode messages unless the nick is recently active.
- simple, small codebase, small memory requirements

Future plans
------------

- add command scripting
- support a `/last` command
- improved internal management of nicks and tab-completion
- editline/libedit support
- who knows? Lodge an issue if you have ideas!
