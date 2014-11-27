# Using `ircl` with HipChat

`ircl` is especially well-suited for use with HipChat with the help
of the excellent gateway [bitlbee](http://www.bitlbee.org). Bitlbee
provides an irc-like interface to a number of chat protocols,
including XMPP, which HipChat uses. Google Chat uses XMPP as well.

#### Getting your hipchat XMPP settings

1. Log into hipchat.com normally.
2. Go to Account Settings, then [XMPP/Jabber Info](https://hipchat.com/account/xmpp).
3. Note your Jabber ID (typically `something@chat.hipchat.com`)
4. Note all channels you want to join (usually something like `11111_sales`)

#### Setting up Bitlbee

Once bitlbee is installed (`pkg_add bitlbee`, `yum install bitlbee`,
`apt-get bitlbee`, etc), start the daemon. I prefer that bitlbee
bind to localhost, run as a daemon and use config files in my home
directory. I start it like this:

```
/usr/local/sbin/bitlbee -D -i 127.0.0.1 -d ${HOME}/.bitlbee
```

With bitlbee running, you can connect to it with `ircl` like so:

```
ircl -h localhost -p 6667
```

#### Configuring Bitlbee to talk to the HipChat XMPP interface

1. Register your nick (your username by default) with a password:
```
register somepassword
```

2. Create the account in bitlbee, set the nick format:
```
account add jabber <hipchat jabber ID> <hipchat password>
account 0 set nick_format %full_name
```

3. Add channels:
```
chat add 0 <hipchat room name>@conf.hipchat.com #<channel name>
channel 1 set nick "Your Full Name"
channel 1 set auto_join true

chat add 0 <hipchat room name>@conf.hipchat.com #<channel name>
channel 2 set nick "Your Full Name"
channel 2 set auto_join true

...

```

4. Save your work and log in.
```
save
account 0 on
```

#### Example configuration transcript

```
11:04 : &bitlbee <stolley> register foo
11:04 : &bitlbee <root> Account successfully created
11:04 : &bitlbee <stolley> account add jabber 11111_2222222@chat.hipchat.com myhipchatpassword
11:04 : &bitlbee <root> Account successfully added with tag jabber
11:05 : &bitlbee <stolley> account 0 set nick_format %full_name
11:05 : &bitlbee <root> nick_format = `%full_name'
11:05 : &bitlbee <stolley> chat add 0 11111_developers@conf.hipchat.com #developers
11:05 : &bitlbee <root> Chatroom successfully added.
11:05 : &bitlbee <stolley> channel 1 set nick "Colin Stolley"
11:05 : &bitlbee <root> nick = `Colin Stolley'
11:05 : &bitlbee <stolley> channel 1 set auto_join true
11:05 : &bitlbee <root> auto_join = `true'
11:05 : &bitlbee <stolley> save
11:05 : &bitlbee <root> Configuration saved
11:05 : &bitlbee <stolley> account on
11:05 : &bitlbee <root> jabber - Logging in: Connecting
11:05 : &bitlbee <root> jabber - Logging in: Connected to server, logging in
11:05 : &bitlbee <root> jabber - Logging in: Converting stream to TLS
11:05 : &bitlbee <root> jabber - Logging in: Connected to server, logging in
...
```
