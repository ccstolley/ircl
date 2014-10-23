#include "ircl.h"

static char *host = "localhost";
static char *port = "6667";
static char *password;
static char nick[32];
static char bufin[4096];
static char bufout[4096];
static char channel[256];
static time_t trespond;
static FILE *srv;
static char *activenicks[MAX_NICKS];
static int nactivenicks = 0;

static void
eprint(const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(bufout, sizeof bufout, fmt, ap);
    va_end(ap);
    fprintf(stderr, "%s", bufout);
    if(fmt[0] && fmt[strlen(fmt) - 1] == ':')
        fprintf(stderr, " %s\n", strerror(errno));
    exit(1);
}

static int
dial(char *host, char *port) {
	static struct addrinfo hints;
	int srv;
	struct addrinfo *res, *r;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	if(getaddrinfo(host, port, &hints, &res) != 0)
		eprint("error: cannot resolve hostname '%s':", host);
	for(r = res; r; r = r->ai_next) {
		if((srv = socket(r->ai_family, r->ai_socktype, r->ai_protocol)) == -1)
			continue;
		if(connect(srv, r->ai_addr, r->ai_addrlen) == 0)
			break;
		close(srv);
	}
	freeaddrinfo(res);
	if(!r)
		eprint("error: cannot connect to host '%s'\n", host);
	return srv;
}

#define strlcpy _strlcpy
static void
strlcpy(char *to, const char *from, int l) {
	memccpy(to, from, '\0', l);
	to[l-1] = '\0';
}

static char *
eat(char *s, int (*p)(int), int r) {
	while(*s != '\0' && p(*s) == r)
		s++;
	return s;
}

static char*
skip(char *s, char c) {
	while(*s != c && *s != '\0')
		s++;
	if(*s != '\0')
		*s++ = '\0';
	return s;
}

static void
trim(char *s) {
	char *e;

	e = s + strlen(s) - 1;
	while(isspace(*e) && e > s)
		e--;
	*(e + 1) = '\0';
}

static void
logmsg(const char *msg, const int len) {
    FILE *logf;
    char logfilepath[512];
    const char *homepath;
    homepath = getenv("HOME");
    if (!homepath)
        homepath = "/var/tmp";
    snprintf(logfilepath, sizeof(logfilepath), "%s/.ircllog", homepath);
    logf = fopen(logfilepath, "a");
    fwrite(msg, len, 1, logf);
    fclose(logf);
}

static void
pout(char *channel, char *fmt, ...) {
	static char timestr[18];
    static char logbuf[4096];
	time_t t;
	va_list ap;
    int saved_point, save_prompt, len;
    char* saved_line;

    save_prompt = !RL_ISSTATE(RL_STATE_DONE);

    if (save_prompt) {
        saved_point = rl_point;
        saved_line = rl_copy_text(0, rl_end);
        rl_save_prompt();
        rl_replace_line("", 0);
        rl_redisplay();
    }

	va_start(ap, fmt);
	vsnprintf(bufout, sizeof bufout, fmt, ap);
	va_end(ap);
	t = time(NULL);
	strftime(timestr, sizeof timestr, "%D %R", localtime(&t));

    fprintf(stdout, "%s : %s %s\n", timestr, channel, bufout);
	len = snprintf(logbuf, sizeof(logbuf), "%s : %s %s\n", timestr, channel, bufout);
    logmsg(logbuf, len);

    if (save_prompt) {
        rl_restore_prompt();
        rl_replace_line(saved_line, 0);
        rl_point = saved_point;
        rl_redisplay();
        free(saved_line);
    }
}

static void
sout(char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(bufout, sizeof bufout, fmt, ap);
	va_end(ap);
//	fprintf(stdout, "\nSRV: '%s'<END>\n", bufout);
	fprintf(srv, "%s\r\n", bufout);
}

static void
privmsg(char *channel, char *msg) {
	if(channel[0] == '\0') {
		pout("", "No channel to send to");
		return;
	}
	pout(channel, "<%s> %s", nick, msg);
	sout("PRIVMSG %s :%s", channel, msg);
}

static void
parsein(char *s) {
	char c, *p;
    char prompt[128];

	if(s[0] == '\0')
		return;
	skip(s, '\n');
	if(s[0] != '/') {
		privmsg(channel, s);
		return;
	}
	c = *++s;
    if (c != '\0' && strlen(s) == 1) {
		switch(c) {
        case 'c':
            printf("Current channel: %s\n", channel);
            return;
		case 'h':
			printf(
                 "a - AWAY <msg>\n"
                 "c - show current channel\n"
                 "h - help\n"
                 "j - JOIN <channel>\n"
                 "l - PART <channel>\n"
                 "m - PRIVMSG <msg>\n"
                 "s - SWITCH <channel>\n"
                 "w - WHO <names>\n"
                 );
			return;
		case 'w':
			sout("WHO *");
			return;
        }

    } else if(c != '\0' && isspace(s[1])) {
		p = s + 2;
		switch(c) {
		case 'j':
			sout("JOIN %s", p);
		    strlcpy(channel, p, sizeof channel);
            snprintf(prompt, sizeof(prompt), "%s> ", channel);
            rl_set_prompt(prompt);
			return;
        case 'a':
			sout("AWAY %s", p);
            return;
		case 'w':
			sout("WHO %s", p);
			return;
		case 'l':
			s = eat(p, isspace, 1);
			p = eat(s, isspace, 0);
			if(!*s)
				s = channel;
			if(*p)
				*p++ = '\0';
			if(!*p)
				p = "Peace.";
			sout("PART %s :%s", s, p);
            snprintf(prompt, sizeof(prompt), "%s> ", channel);
            rl_set_prompt(prompt);
			return;
		case 'm':
			s = eat(p, isspace, 1);
			p = eat(s, isspace, 0);
			if(*p)
				*p++ = '\0';
			privmsg(s, p);
			return;
		case 's':
			strlcpy(channel, p, sizeof channel);
            snprintf(prompt, sizeof(prompt), "%s> ", channel);
            rl_set_prompt(prompt);
			return;
		}
	}
	sout("%s", s);
}

static void
parsesrv(char *cmd) {
	char *usr, *par, *txt;

	usr = host;
	if(!cmd || !*cmd)
		return;
	if(cmd[0] == ':') {
		usr = cmd + 1;
		cmd = skip(usr, ' ');
		if(cmd[0] == '\0')
			return;
		skip(usr, '!');
	}
	skip(cmd, '\r');
	par = skip(cmd, ' ');
	txt = skip(par, ':');
	trim(par);
	if(!strcmp("PONG", cmd))
		return;
	if(!strcmp("PRIVMSG", cmd))
		pout(par, "<%s> %s", usr, txt);
	else if(!strcmp("PING", cmd))
		sout("PONG %s", txt);
	else {
        if (strcmp(cmd, "JOIN") == 0) {
            if (channel[0] == '\0' && !strcmp(usr, nick)) {
                char prompt[128];
                strlcpy(channel, txt, sizeof channel);
                snprintf(prompt, sizeof(prompt), "%s> ", channel);
                rl_set_prompt(prompt);
            }
            insert_nick(usr);
        } else if (strcmp(cmd, "QUIT") == 0) {
            remove_nick(usr);
        } else if (strcmp(cmd, "MODE") == 0) {
            /* eat it */
        } else {
            pout(usr, ">< %s (%s): %s", cmd, par, txt);
    		if(!strcmp("NICK", cmd) && !strcmp(usr, nick))
    			strlcpy(nick, txt, sizeof nick);
        }
	}
}

int
main(int argc, char *argv[]) {
	int i, c;
	struct timeval tv;
	const char *user = getenv("USER");
	fd_set rd;

	strlcpy(nick, user ? user : "unknown", sizeof nick);
	for(i = 1; i < argc; i++) {
		c = argv[i][1];
		if(argv[i][0] != '-' || argv[i][2])
			c = -1;
		switch(c) {
		case 'h':
			if(++i < argc) host = argv[i];
			break;
		case 'p':
			if(++i < argc) port = argv[i];
			break;
		case 'n':
			if(++i < argc) strlcpy(nick, argv[i], sizeof nick);
			break;
		case 'k':
			if(++i < argc) password = argv[i];
			break;
		case 'v':
			eprint("ircl-"VERSION);
		default:
			eprint("usage: ircl [-h host] [-p port] [-n nick] [-k keyword] [-v]\n");
		}
	}
	/* init */
	i = dial(host, port);
	srv = fdopen(i, "r+");
	/* login */
	if(password)
		sout("PASS %s", password);
	sout("NICK %s", nick);
	sout("USER %s localhost %s :%s", nick, host, nick);
	fflush(srv);
	setbuf(stdout, NULL);
	setbuf(srv, NULL);
    initialize_readline();
	for(;;) { /* main loop */
		FD_ZERO(&rd);
		FD_SET(0, &rd);
		FD_SET(fileno(srv), &rd);
		tv.tv_sec = 120;
		tv.tv_usec = 0;
		i = select(fileno(srv) + 1, &rd, 0, 0, &tv);
		if(i < 0) {
			if(errno == EINTR)
				continue;
			eprint("ircl: error on select():");
		}
		else if(i == 0) {
			if(time(NULL) - trespond >= 300)
				eprint("ircl shutting down: parse timeout\n");
			sout("PING %s", host);
			continue;
		}
		if(FD_ISSET(fileno(srv), &rd)) {
			if(fgets(bufin, sizeof bufin, srv) == NULL)
				eprint("ircl: remote host closed connection\n");
			parsesrv(bufin);
			trespond = time(NULL);
		}
		if(FD_ISSET(0, &rd)) {
            rl_callback_read_char();
        }
    }
	return 0;
}

int
insert_nick(const char *nick) {
    int i;
    int next_available = -1;

    for (i=0; i<MAX_NICKS; i++) {
        if (activenicks[i] == NULL) {
            next_available = i;
        } else if (strcasecmp(activenicks[i], nick) == 0) {
            // duplicate found, so skip
            return 0;
        }   
    }
    if (next_available != -1) {
        activenicks[next_available] = strdup(nick);
        nactivenicks++;
        return 1;
    } else {
        fprintf(stderr, "ERROR: Out of space in activenicks (%d)!\n",
            nactivenicks);
        return -1;
    }   
}

int
remove_nick(const char *nick) {
    int i;

    for (i=0; i<MAX_NICKS; i++) {
        if (activenicks[i] && strcasecmp(activenicks[i], nick) == 0) {
            free(activenicks[i]);
            activenicks[i] = NULL;
            nactivenicks--;
            return 1;
        }
    }
    fprintf(stderr, "ERROR: Failed to remove '%s' from activenicks (%d)!\n",
            nick, nactivenicks);
    return 0;
}

void
init_nicks() {
    int i;

    for (i=0; i<MAX_NICKS; i++) {
        activenicks[i] = NULL;
    }
}

void
initialize_readline () {
  rl_readline_name = "ircl";
  rl_attempted_completion_function = ircl_completion;

  /* don't display matches, just complete them in place */
  rl_parse_and_bind(strdup("TAB:menu-complete"));

  /* remove '@&' from defaults */
  rl_completer_word_break_characters = " \t\n\"\\'`$><=;|{(";
  rl_callback_handler_install("> ", (rl_vcpfunc_t*) &readline_nonblocking_cb);
  init_nicks();
}

void
readline_nonblocking_cb(char* line) {
    if (NULL==line) {
	    eprint("ircl: broken pipe\n");
        return;
    }

    line = stripwhite(line);
    if(strlen(line) > 0 && *line) {
       add_history(line);
    }
    

    parsein(line);
    free(line);
}

char **
ircl_completion(const char *text, int start, int end) {
  char **matches = NULL;
  rl_attempted_completion_over = 1;  /* don't match on filenames, etc */
  matches = rl_completion_matches(text, nick_generator);
  if (!matches) {
    matches = rl_completion_matches(text, username_generator);
  }
  return matches;
}

char *
username_generator(const char *text, int state) {
  static int list_index, len;
  const char *name;

  if (!state) {
      list_index = 0;
      len = strlen (text);
  }

  while ((name = usernames[list_index])) {
      list_index++;

      if (strncasecmp (name, text, len) == 0)
        return strdup(name);
  }

  return ((char *)NULL);
}

char *
nick_generator(const char *text, int state) {
  static int list_index, len;
  const char *name;

  if (!state) {
      list_index = 0;
      len = strlen (text);
  }

  for (; list_index < MAX_NICKS;) {
      name = activenicks[list_index];
      list_index++;
      if (name && strncasecmp (name, text, len) == 0) {
        return strdup(name);
      }
  }
  return ((char *)NULL);
}

char*
stripwhite (char *string) {
  register char *s, *t;

  for (s = string; whitespace (*s); s++)
    ;
    
  if (*s == 0)
    return (s);

  t = s + strlen (s) - 1;
  while (t > s && whitespace (*t))
    t--;
  *++t = '\0';

  return s;
}
