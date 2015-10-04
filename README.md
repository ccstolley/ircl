ircl
====

![](https://github.com/ccstolley/misc/blob/master/img/ircl.png)

Simple IRC client with customized tab completion.


This is a work in progress. The goal is the simplest possible IRC
client with a small set of features that make using it fast and
easy.

Stuck with HipChat? See [Using ircl with HipChat](using_with_hipchat.md).

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

Usage
-----
From the command line:
```
usage: ircl [-h host] [-p port] [-s] [-l log file] [-n nick] [-k password] [-v]

  -s Enable SSL
  -v Verbose
```

The following commands (each beginning with the usual "/") are supported:
```
        g away   - AWAY <msg>
        h help   - display this message
        j join   - JOIN <channel>
        p part   - PART [<channel>]
        l last   - replay last messages from <channel>
        m msg    - PRIVMSG <channel or nick> <msg>
        a me     - ACTION <msg>
        s switch - change channel to <channel> or list channels and return to default
        w who    - WHO [<channel>]
        W whoa   - WHO *
        Q quit   - quit
```

Upon successful login, you enter the `ircl% ` channel, which is a command-only channel.

- To join a room, use the `/j` command.
- To send a message to a user or channel, just start typing the name
  and hit TAB. Then enter the message.
- To direct all outgoing messages to a particular user or channel, use
  `/s` to switch the default channel.

Screen Snapshot
---------------
![](https://github.com/ccstolley/misc/blob/master/img/ircl_snap.png)

Getting Help
------------

Stop by #ircl on irc.foonetic.net and ask your question!

Future plans
------------

- replace libreadline with editline/libedit/linenoise
- improved internal management of nicks, tab-completion and message history
- command scripting
- who knows? Lodge an issue if you have ideas!
