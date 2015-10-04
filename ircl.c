#include "ircl.h"

static char *host = "localhost";
static char *port = "6667";
static char *password = NULL;
static char bufout[4096];
static char default_channel[256];
static char default_nick[32];
static FILE *srv = NULL;
static char *all_nicks[MAX_NICKS];
static int nick_count = 0;
static int is_away = 0;
static const char *active_nicks[ACTIVE_NICKS_QUEUE_SIZE];
static const char *log_file_path = NULL;
static bool use_ssl = false;
static SSL *ssl = NULL;

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

static void
eprint_reconnect(const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(bufout, sizeof bufout, fmt, ap);
    va_end(ap);
    fprintf(stderr, "%s", bufout);
    if(fmt[0] && fmt[strlen(fmt) - 1] == ':')
        fprintf(stderr, " %s\n", strerror(errno));
    sleep(1);
    pout("ircl", "Reconnecting to %s:%s", host, port);
    remove_all_nicks();
    login();
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

static void
ssl_connect(const int sock) {
    SSL_CTX *ctx;
    const SSL_METHOD *method;
    int result = 0;

    SSL_library_init();
    SSL_load_error_strings(); 
    method = TLSv1_2_client_method();
    ctx = SSL_CTX_new(method); 
    if (ctx  == NULL)
        eprint("Unable to initialize SSL context");
    SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2);
    ssl = SSL_new(ctx);
    if (ssl  == NULL)
        eprint("Unable to initialize SSL struct");
    SSL_set_fd(ssl, sock);

    if (1 != SSL_connect(ssl)) {
        eprint("Unable to connect over SSL (err=%d)", SSL_get_error(ssl, result));
    }
//    SSL_free(ssl);
    SSL_CTX_free(ctx);
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
initialize_logging(const char *log_file) {
    const char *default_name = ".ircllog";
    char *base_path;
    int log_file_path_len = 0;

    SIMPLEQ_INIT(&hist_head); /* initialize history queue */

    if (!log_file) {
        base_path = getenv("HOME");
        if (!base_path)
            base_path = "/var/tmp";
        log_file_path_len = strlen(base_path) + strlen(default_name) +
                            strlen(default_nick) + 4;
        log_file_path = calloc(log_file_path_len, sizeof(char));
        snprintf((char*)log_file_path, log_file_path_len, "%s/%s-%s", base_path,
                 default_name, default_nick);
    } else {
        if (log_file[0] != '/') {
            base_path = getcwd(NULL, 0);
            log_file_path_len = strlen(base_path) + strlen(log_file) + 4;
            log_file_path = calloc(log_file_path_len, sizeof(char));
            snprintf((char*)log_file_path, log_file_path_len, "%s/%s", base_path,
                 log_file);
            free(base_path);
        } else {
            log_file_path = strdup(log_file);
        }
    
    }
    printf("Logging to %s\n", log_file_path);
    if (0 != access(log_file_path, R_OK|W_OK)) {
        if (errno == ENOENT) {
            char *lfp_dup = strdup(log_file_path);
            const char *dir_path = dirname(lfp_dup);
            if (dir_path && (0 == access(dir_path, R_OK|W_OK))) {
                free(lfp_dup);
                return;
            }
        }
        eprint("Unable to write to log file %s: %s\n", log_file_path,
                strerror(errno));
    }
}

static void
logmsg(const char *msg, const int len) {
    FILE *logf;
    logf = fopen(log_file_path, "a");
    fwrite(msg, len, 1, logf);
    fclose(logf);
}

static char*
highlight_user(const char *buf) {
    int nick_buf_len = 0, buf_len = 0;
    char *tmp_buf = NULL;
    char *nick_buf = NULL;

    nick_buf_len = strlen(default_nick) + 3;
    nick_buf = calloc(nick_buf_len, sizeof(char));
    snprintf(nick_buf, nick_buf_len, "%s: ", default_nick);

    if (!strncmp(buf, nick_buf, nick_buf_len - 1)) {
        buf_len = strlen(buf) + strlen(COLOR_PM_INCOMING COLOR_RESET) + 1;
        tmp_buf = calloc(buf_len, sizeof(char));
        snprintf(tmp_buf, buf_len, "%s%s%s: %s", COLOR_PM_INCOMING,
                 default_nick, COLOR_RESET, buf + nick_buf_len - 1);
    }
    free(nick_buf);
    return tmp_buf;
}

static void
pout(const char *channel, char *fmt, ...) {
    static char timestr[32];
    static char logbuf[4096];
    time_t t;
    va_list ap;
    int saved_point, save_prompt, len;
    char *saved_line;

    save_prompt = !RL_ISSTATE(RL_STATE_DONE);
    if (save_prompt) {
        saved_point = rl_point;
        saved_line = rl_copy_text(0, rl_end);
        rl_save_prompt();
        rl_replace_line("", 1);
        rl_clear_message();
        rl_redisplay();
    }

    va_start(ap, fmt);
    vsnprintf(bufout, sizeof bufout, fmt, ap);
    va_end(ap);
    t = time(NULL);

    strftime(timestr, sizeof timestr, "%R", localtime(&t));
    fprintf(rl_outstream, "%s : %s%s" COLOR_RESET " %s\n",
            timestr, channel_color(channel), channel, bufout);

    strftime(timestr, sizeof timestr, "%D %T", localtime(&t));
    len = snprintf(logbuf, sizeof(logbuf), "%s : %s %s\n", timestr, channel,
                   bufout);
    logmsg(logbuf, len);
    if (strcmp(channel, default_nick) == 0) {
        char *recip = parse_recipient(logbuf);
        if (recip) {
            add_msg_history(recip, logbuf);
            free(recip);
        }
    } else {
        add_msg_history(channel, logbuf);
    }

    if (save_prompt) {
        rl_restore_prompt();
        rl_replace_line(saved_line, 0);
        rl_point = saved_point;
        rl_redisplay();
        rl_resize_terminal();
        free(saved_line);
    }
}

static void
add_channel(const char *channel) {
    short i=0;

    for (i=0; i<MAX_CHANNELS; i++) {
        if (active_channels[i].name == NULL) {
            active_channels[i].name = strdup(channel);
            return;
        }
    }
    pout("ircl", "Error: Maximum (%d) channels reached; can't join.", MAX_CHANNELS);
}

static void
remove_channel(const char *channel) {
    short i=0;
    const char *name;

    for (i=0; i<MAX_CHANNELS; i++) {
        name = active_channels[i].name;
        if (name && !strcmp(name, channel)) {
            free((char*)name);
            active_channels[i].name = NULL;
            return;
        }
    }
    fprintf(stderr, "ERROR: Unable to remove channel %s\n", channel);
}

static const char*
channel_color(const char *channel) {
    short i=0;

    if (!strcmp(channel, default_nick)) {
        return COLOR_PM_INCOMING;
    }
    for (i=0; i<MAX_CHANNELS; i++) {
        if (active_channels[i].name && !strcmp(active_channels[i].name, channel)) {
            return active_channels[i].color;
        }
    }
    return active_channels[MAX_CHANNELS - 1].color; /* default */
}

static void
set_default_channel() {
    strlcpy(default_channel, IRCL_CHANNEL_NAME, sizeof default_channel);
    update_prompt(default_channel);
}

static int 
in_ircl_channel() { 
    return (*default_channel && (strcmp(IRCL_CHANNEL_NAME, default_channel) == 0));
}

static void
update_prompt(const char *channel) {
    char sep = '>';
    char prompt[128];
    if (is_away) {
        sep = '*';
    }
    snprintf(prompt, sizeof(prompt), "%s" "%c ", channel, sep);
    rl_set_prompt(prompt);
    rl_on_new_line_with_prompt();
    rl_redisplay();
}

static void
sout(char *fmt, ...) {
    va_list ap;
    int len = 0;

    va_start(ap, fmt);
    len = vsnprintf(bufout, sizeof(bufout), fmt, ap);
    va_end(ap);
/*    fprintf(stdout, "\nSRV: '%s'<END>\n", bufout); */
    if (use_ssl) {
        char *full_bufout = malloc(len + 3);
        len = snprintf(full_bufout, len + 3, "%s\r\n", bufout);
        if (SSL_write(ssl, full_bufout, len) <= 0 ) {
            eprint("Unable to write over SSL");
        }
        free(full_bufout);
    } else {
        fprintf(srv, "%s\r\n", bufout);
    }
}

static void
privmsg(char *channel, char *msg) {
    if(channel[0] == '\0') {
        pout("ircl", "No channel to send to");
        return;
    }
    insert_nick(channel);
    pout(channel, "<" COLOR_OUTGOING "%s" COLOR_RESET"> %s", default_nick, msg);
    sout("PRIVMSG %s :%s", channel, msg);
}

static int
is_colon(const int c) {
    return (c == ':');
}

static int
starts_with_symbol(const char *str) {
    return (str && (str[0] == '#' || str[0] == '&' || str[0] == '@'));
}

static int
match_command(const char *cmd_name, const char *str) {
    char * cmd = NULL;
    int cmd_len = 0;
    int result = 0;

    cmd_len = strlen(cmd_name) + 3; /* '/' + space + nul */
    cmd = malloc(cmd_len);


    /* check if it's the cmd + space */
    snprintf(cmd, cmd_len, "/%s ", cmd_name);
    result = !strncmp(cmd, str, cmd_len - 1);

    /* check if it's just the cmd, no args */
    cmd[cmd_len - 2] = '\0';
    result |= !strcmp(cmd, str);

    free(cmd);
    return result;
}


static void
handle_help() {
    pout("ircl", "Commands:\n"
        "\tg away   - AWAY <msg>\n"
        "\th help   - display this message\n"
        "\tj join   - JOIN <channel>\n"
        "\tp part   - PART [<channel>]\n"
        "\tl last   - replay last messages from <channel>\n"
        "\tm msg    - PRIVMSG <channel or nick> <msg>\n"
        "\ta me     - ACTION <msg>\n"
        "\ts switch - change channel to <channel> or list channels and return to default\n"
        "\tw who    - WHO [<channel>]\n"
        "\tW whoa   - WHO *\n"
        "\tQ quit   - quit\n"
    );
}

static void
handle_who_all() { sout("WHO *"); }

static void
handle_who_channel(const char* args) {
    if (args && *args) {
        sout("WHO %s", args);
    } else if (!in_ircl_channel()) {
        sout("WHO %s", default_channel);
    } else {
        pout("ircl", "Specify a channel.");
    }
}

static void 
handle_away(const char* args) {
    if (args && *args) {
        sout("AWAY %s", args);
        is_away = 1;
    } else {
        sout("AWAY");
        is_away = 0;
    }
    update_prompt(default_channel);
}


static void 
handle_join(const char* args) {
    if (args && *args) {
        sout("JOIN %s", args);
    } else {
        pout("ircl", "Must specify a channel to join.");
    }
}

static void 
handle_part(const char* args) {
    if (args && *args) {
        sout("PART %s", args);
    } else {
        if (!in_ircl_channel()) {
            sout("PART %s Peace.", default_channel);
        } else {
            pout("ircl", "Specify a channel.");
        }
    }
}

static void
handle_msg(const char* args) {
    char *channel, *msg;
    
    if (!*args) {
        pout("ircl", "Must specify a nick and a message");
        return;
    }

    channel = strdup(args);
    msg = eat(channel, isspace, 0);
    if(*msg)
        *msg++ = '\0';
    update_active_nicks(channel);
    privmsg(channel, msg);
    free(channel);
}

static void
handle_me(const char* args) {
    if (!*args) {
        pout("ircl", "Must specify a message");
        return;
    }

    if (!in_ircl_channel()) {
        sout("PRIVMSG %s \1ACTION %s", default_channel, args);
        pout(default_channel, "* " COLOR_OUTGOING "%s" COLOR_RESET " %s", default_nick, args);
    } else {
        pout("ircl", "No channel to send to");
    }
}

static void
handle_switch(const char* args) {
    if (!*args) {
        int i;
        const char *name, *color;
        pout("ircl", "Active Channels:");
        for (i=0; i<MAX_CHANNELS; i++) {
            name = active_channels[i].name;
            color = active_channels[i].color;
            if (name) {
                pout("ircl", "    %s%s%s", color, name, COLOR_RESET);
            }
        }
        strlcpy(default_channel, IRCL_CHANNEL_NAME, sizeof default_channel);
        update_prompt(default_channel);
    } else {
        char *channel;
        char *msg;

        channel = strdup(args);
        msg = eat(channel, isspace, 0);
        if(*msg) {
            *msg++ = '\0';
            privmsg(channel, msg);
        }
        strlcpy(default_channel, channel, sizeof default_channel);
        free(channel);
        update_prompt(default_channel);
    }
}

static void
handle_quit() {
    sout("QUIT Peace.");
    exit(0);
}

static void
parsein(char *s) {
    int i = 0;
    char* args = NULL;

    if(s[0] == '\0')
        return;
    skip(s, '\n');
    if ((s[0] != '/') || (s[0] == '/' && s[1] == '/' && s++)) {

        if (!in_ircl_channel()) {
            privmsg(default_channel, s);
        } else if (strchr(s, ':')) {
            char *channel = strdup(s);
            char *msg = eat(channel, is_colon, 0);
            *msg++ = '\0';
            while (*msg && isspace(*msg)) {
                *msg++ = '\0';
            }
            if(*msg) {
                privmsg(channel, msg);
            } else {
                pout("ircl", "Specify a message");
            }
            free(channel);
            channel = NULL;
        } else {
            pout("ircl", "Specify a channel");
        }
        return;
    }

    for (i=0; command_map[i].short_cmd != NULL; i++) {
        if (match_command(command_map[i].short_cmd, s) ||
            match_command(command_map[i].long_cmd, s)) {
            args = skip(s, ' ');
            command_map[i].func_ptr(args);
            return;
        }
    }
    /* pass commands though if they don't match one of ours */
    s++;
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
    if(!strcmp("PRIVMSG", cmd)) {
        update_active_nicks(usr);
        if (strncmp(txt, "\1ACTION ", 8) == 0) {
            /* action */
            txt += 8;
            pout(par, "* " COLOR_INCOMING "%s" COLOR_RESET " %s", usr, txt);
        } else {
            char *highlighted_txt = highlight_user(txt);
            if (highlighted_txt) {
                pout(par, "<" COLOR_INCOMING "%s" COLOR_RESET "> %s", usr,
                     highlighted_txt);
                free(highlighted_txt);
                highlighted_txt = NULL;
            } else {
                pout(par, "<" COLOR_INCOMING "%s" COLOR_RESET "> %s", usr, txt);
            }
        }
        insert_nick(usr);
    } else if(!strcmp("PING", cmd)) {
        sout("PONG %s", txt);
    } else {
        if (strcmp(cmd, "JOIN") == 0) {
            char * channel = (*txt) ? txt : par;
            if (!strcmp(usr, default_nick)) {
                add_channel(channel);
                pout(usr, "> joined %s%s%s", channel_color(channel), channel, COLOR_RESET);
                /* if we joined a room, add it to tab-complete */
                insert_nick(channel);
            }
            if (nick_is_active(usr)) {
                pout(usr, "> joined %s%s%s", channel_color(channel), channel, COLOR_RESET);
            }
            insert_nick(usr);
        } else if ((strcmp(cmd, "QUIT") == 0) || (strcmp(cmd, "PART") == 0)) {
            char * channel = par;
            if (!strcmp(usr, default_nick)) {
                remove_channel(channel);
                pout(usr, "> left %s %s", channel, txt);
                if (!strcmp(channel, default_channel)) {
                    strlcpy(default_channel, IRCL_CHANNEL_NAME, sizeof default_channel);
                    update_prompt(default_channel);
                }
            } else if (nick_is_active(usr)) {
                pout(usr, "> left %s %s", channel, txt);
            }
            remove_nick(usr);
        } else if (strcmp(cmd, "NICK") == 0) {
            pout(usr, "> is now known as " COLOR_CHANNEL "%s" COLOR_RESET, txt);
            remove_nick(usr);
            insert_nick(txt);
            if (strcmp(usr, default_nick) == 0) {
                strlcpy(default_nick, txt, sizeof default_nick);
            }
        } else if (strcmp(cmd, "NOTICE") == 0) {
            insert_nick(usr);
            pout(usr, "NOTICE > %s", txt);
        } else if (strcmp(cmd, "MODE") == 0) {
            /* eat it */
        } else if (strcmp(cmd, "001") == 0) {
            /* welcome message, make sure correct nick is stored. */
            skip(par, ' ');  /* strip anything beyond a space */
            strlcpy(default_nick, par, sizeof default_nick);
            pout(usr, "> is now known as " COLOR_CHANNEL "%s" COLOR_RESET, par);
            insert_nick(par);
        } else if (strcmp(cmd, "366") == 0) {
            /* end of names list, do nothing */
        } else if (strcmp(cmd, "332") == 0) {
            /* channel topic */
            pout(usr, "%s TOPIC: %s", par, txt);
        } else if (strcmp(cmd, "315") == 0) {
            /* end of who list, do nothing */
        } else if (strcmp(cmd, "352") == 0) {
            /* who response */
            char *name  = strtok(par, " ");
            char *state;
            int i = 0;
            for (i=0; i< 5; i++) {
                name = strtok(NULL, " ");
            }
            state = strtok(NULL, " ");
            pout(usr, "%s %s", name, state);
        } else if (strcmp(cmd, "353") == 0) {
            /* names response */
            char *client = strtok(txt, " ");
            while (client) {
                if (client[0] == '@') {
                    /* remove @ from operators */
                    client++;
                }
                insert_nick(client);
                client = strtok(NULL, " ");
            }
        } else {
            pout(usr, ">< %s (%s): %s", cmd, par, txt);
        }
    }
}

static void
update_active_nicks(const char *nick) {
    /* Maintain a queue of the 10 most recent nicks to say something.
     * Then mute everyone else's join/part activity. */
    int i;
    static int next_available = -1;

    if (next_available < 0) {
        /* initialize first */
        for (i=0; i<ACTIVE_NICKS_QUEUE_SIZE; i++) {
            active_nicks[i] = NULL;
        }
        next_available = 0;
    }

    for (i=0; i<ACTIVE_NICKS_QUEUE_SIZE; i++) {
        if (active_nicks[i] && (strcasecmp(active_nicks[i], nick) == 0)) {
            /* already in active queue, so skip */
            return;
        }   
    }
    if (active_nicks[next_available]) {
        free((char*)active_nicks[next_available]);
        active_nicks[next_available] = NULL;
    }
    active_nicks[next_available] = strdup(nick);
    next_available = (next_available + 1) % ACTIVE_NICKS_QUEUE_SIZE;
}

static int
nick_is_active(const char *nick) {
    int i;

    for (i=0; i<ACTIVE_NICKS_QUEUE_SIZE; i++) {
        if (active_nicks[i] && (strcasecmp(active_nicks[i], nick) == 0)) {
            return 1;
        }   
    }
    return 0;
}

void
login() {
    int i;

    if (srv != NULL) {
        fclose(srv); // check return value/errno?
        srv = NULL;
    }
    if (ssl != NULL) {
        SSL_free(ssl);
        ssl = NULL;
    }
    i = dial(host, port);
    if (use_ssl) {
        ssl_connect(i);
    }
    srv = fdopen(i, "r+");
    /* login */
    if(password)
        sout("PASS %s", password);
    sout("NICK %s", default_nick);
    sout("USER %s localhost %s :%s", default_nick, host, default_nick);
    if (!use_ssl) {
        fflush(srv);
        setbuf(srv, NULL);
    }
    setbuf(stdout, NULL);
    set_default_channel();
}

int
main(int argc, char *argv[]) {
    int i, c;
    time_t trespond = 0;
    struct timeval tv = {120, 0};
    const char *user = getenv("USER");
    char bufin[4096];
    fd_set rd;

    strlcpy(default_nick, user ? user : "unknown", sizeof default_nick);
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
            if(++i < argc) strlcpy(default_nick, argv[i], sizeof default_nick);
            break;
        case 'k':
            if(++i < argc) password = argv[i];
            break;
        case 'v':
            eprint("ircl-"VERSION"\n");
        case 's':
            use_ssl = true;
        case 'l':
            if(++i < argc) initialize_logging(argv[i]);
            break;
        default:
            eprint("usage: ircl [-h host] [-p port] [-s] [-l log file] [-n nick] [-k password] [-v]\n");
        }
    }
    if (!log_file_path) {
        initialize_logging(NULL);
    }

    initialize_readline();
    /* init */
    login();

    for(;;) { /* main loop */
        FD_ZERO(&rd);
        FD_SET(0, &rd);
        FD_SET(fileno(srv), &rd);
        i = select(fileno(srv) + 1, &rd, 0, 0, &tv);
        tv.tv_sec = 120; /* fuckin linux resets this */
        tv.tv_usec = 0; /* fuckin linux resets this */
        if(i < 0) {
            if (!(errno == EINTR || errno == EAGAIN)) {
                eprint_reconnect("ircl: error on select():");
            }
            continue;
        }
        else if(i == 0) {
            if(time(NULL) - trespond >= 300)
                eprint_reconnect("ircl shutting down: parse timeout\n");
            sout("PING %s", host);
            continue;
        }
        if(FD_ISSET(fileno(srv), &rd)) {
            if (use_ssl) {
                char *cmd, *ptr, *end;
                i = SSL_read(ssl, bufin, sizeof(bufin) - 1);
                if (i <= 0 ) {
                    eprint_reconnect("Unable to read over SSL (err=%d)\n", SSL_get_error(ssl, i));
                    continue;
                }
                ptr = cmd = bufin;
                end = bufin + i;
                *end = '\0';
                while(ptr < end) {
                    if (*ptr == '\r') {
                        *ptr = '\0';
                        if (*(ptr + 1) != '\n') { // accept only CR as EOM
                            parsesrv(cmd);
                            cmd = ptr + 1;
                        }
                    }
                    else if (*ptr == '\n') {
                        *ptr = '\0';
                        parsesrv(cmd);
                        cmd = ptr + 1;
                    } 
                    ptr++;
                }
                if (cmd != ptr) {
                    fprintf(stderr, "INCOMPLETE CMD: '%s'\n", cmd);
                }
            } else {
                if(fgets(bufin, sizeof bufin, srv) == NULL) {
                    eprint_reconnect("ircl: remote host closed connection\n");
                    continue;
                }
                parsesrv(bufin);
            }
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
        if (all_nicks[i] == NULL) {
            next_available = i;
            break;
        } else if (strcasecmp(all_nicks[i], nick) == 0) {
            /* duplicate found, so skip */
            return 0;
        }   
    }
    if (next_available != -1) {
        all_nicks[next_available] = strdup(nick);
        nick_count++;
        return 1;
    } else {
        fprintf(stderr, "ERROR: Out of space in activenicks (%d)!\n",
            nick_count);
        return -1;
    }   
}

int
remove_nick(const char *nick) {
    int i;

    for (i=0; i<MAX_NICKS; i++) {
        if (all_nicks[i] && strcasecmp(all_nicks[i], nick) == 0) {
            free(all_nicks[i]);
            all_nicks[i] = NULL;
            nick_count--;
            assert(nick_count >= 0);
            return 1;
        }
    }
    return 0;
}

void
remove_all_nicks() {
    int i;
    for (i=0; i<MAX_NICKS; i++) {
        if (all_nicks[i]) {
            free(all_nicks[i]);
            all_nicks[i] = NULL;
            nick_count--;
            assert(nick_count >= 0);
        }
    }
}

void
init_nicks() {
    int i;

    for (i=0; i<MAX_NICKS; i++) {
        all_nicks[i] = NULL;
    }
}

void
initialize_readline () {
    char *complete = strdup("TAB:menu-complete");
    rl_readline_name = "ircl";
    rl_attempted_completion_function = ircl_completion;

    /* don't display matches, just complete them in place */
    rl_parse_and_bind(complete); /* can't supply a string literal here */
    free(complete);

    /* remove '@&' from defaults */
    rl_completer_word_break_characters = " \t\n\"\\'`$><=;|{(";
    rl_callback_handler_install("> ", (rl_vcpfunc_t*) &readline_nonblocking_cb);
    if (rl_bind_key(RETURN, handle_return_cb)) {
        eprint("failed to bind RETURN key");
    }
    init_nicks();
    load_usernames_file();
}

int
handle_return_cb() {
    char* line = NULL;
    int i = 0, prompt_len = 0;

    rl_done = 1;

    line = rl_copy_text(0, rl_end);
    line = stripwhite(line);
    rl_replace_line("", 1);
    rl_redisplay();

    if (line && *line) {
        add_history(line);
    }

    parsein(line);
    free(line);

    /* erase prior prompt */
    prompt_len = strlen(rl_prompt) - 17; /* subtract color escape codes */
    if (prompt_len < 0) {
        /* unless there are no escape codes */
        prompt_len = strlen(rl_prompt);
    }
    for (i=0; i < prompt_len; i++) {
        fprintf(rl_outstream, "\b \b");
    }
    return 0;
}

void
readline_nonblocking_cb(char* line) {
    /* This is a false callback. The real action is in handle_return_cb(). */
    if (NULL==line) {
        eprint("ircl: broken pipe\n");
        return;
    }
    free(line);
}

char **
ircl_completion(const char *text, int start, int end) {
    char **matches = NULL;
    int len = strlen(text);
    UNUSED(start);
    UNUSED(end);

    /* Matching proceeds with the following rules:
       (1) If we're at the beginning of the line and text starts with
           a '/', match only on commands.
       (2) If text starts with an '@', match only on external usernames.
       (3) Else, match against nicks. If no nicks match, fall back to
           external usernames. */

    rl_attempted_completion_over = 1;  /* don't match on filenames, etc */
    if (text[0] == '/' && (rl_point == len)) {
        text++;
        matches = rl_completion_matches(text, cmd_generator);
    } else {
        if (text[0] == '@') {
            matches = rl_completion_matches(text, username_generator);
        } else {
            matches = rl_completion_matches(text, nick_generator);
            if (!matches && text[0] != '@') {
                matches = rl_completion_matches(text, username_generator);
            }
        }
    }
    return matches;
}

static char *
cmd_generator(const char *text, int state) {
    static int list_index, len;
    const char *name;

    if (!state) {
        list_index = 0;
        len = strlen (text);
    }

    while ((name = command_map[list_index].long_cmd)) {
        list_index++;
        if (strncasecmp (name, text, len) == 0) {
            char* cmd_name;
            int cmd_len = 0;
            cmd_len = strlen(name) + 2;
            cmd_name = calloc(cmd_len, sizeof(char));
            snprintf(cmd_name, cmd_len, "/%s", name);
            return cmd_name;
        }
    }
    return ((char *)NULL);
}

static char *
username_generator(const char *text, int state) {
    static int list_index, len;
    const char *name;
    int offset =  0;

    if (!usernames) {
        return ((char *)NULL);
    }

    if (!state) {
        list_index = 0;
        len = strlen (text);
    }

    while ((name = usernames[list_index])) {
        offset = 0;
        list_index++;
        if (!starts_with_symbol(text) && starts_with_symbol(name)) {
            offset = 1; /* skip @ in @name, etc. */
        }

        if (strncasecmp (name + offset, text, len) == 0) {
            return strdup(name);
        }
    }
    return ((char *)NULL);
}

static char *
nick_generator(const char *text, int state) {
  static int list_index = 0, len = 0;
  const char *fullnick;
  const char *name;

  if (!state) {
      list_index = 0;
      len = strlen (text);
  }

  for (; list_index < MAX_NICKS;) {
      name = all_nicks[list_index];
      fullnick = name;
      
      list_index++;
      if (name && !starts_with_symbol(text) && starts_with_symbol(name)) {
          /* skip prefixes like @person and #jerks */
          name++;
      }

      if (name && strncasecmp(name, text, len) == 0) {
          if (rl_point == len) {
              /* completing a nick at the beginning of a line, so
               * append a colon:*/

              char *nick_with_colon;
              int sz;

              sz = strlen(fullnick) + 2;
              nick_with_colon = calloc(sz, sizeof(char));
              snprintf(nick_with_colon, sz, "%s:", fullnick);
              return nick_with_colon;
          } else {
              return strdup(fullnick);
          }
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
    while (t > s && whitespace (*t)) {
        t--;
    }
    *++t = '\0';

    return s;
}

static char*
parse_recipient(const char * logbuf) {
    char *rcp = strstr(logbuf, "<" COLOR_INCOMING);
    char *ercp = strstr(logbuf, COLOR_RESET ">");
    char *recip = NULL;
    int len = 0;

    if (rcp && ercp) {
        rcp += strlen("<" COLOR_INCOMING);
        len = ercp - rcp + 1;

        recip = calloc(len + 1, sizeof(char));
        strlcpy(recip, rcp, len);
        return recip;
    } else {
        return NULL;
    }

}

static void
add_msg_history(const char *channel, const char *message) {
    hist_elem e;
    if (hist_size > MAX_HISTORY) {
        e = SIMPLEQ_FIRST(&hist_head);
        SIMPLEQ_REMOVE_HEAD(&hist_head, entries);
        free(e->channel);
        free(e->msg);
    } else {
        hist_size++;
    }
    e = malloc(sizeof(struct hist_elem));
    e->msg = strdup(message);
    e->channel = strdup(channel);
    SIMPLEQ_INSERT_TAIL(&hist_head, e, entries);
}

static void
handle_last(const char *channel) {
    hist_elem e;
    if (!channel || !*channel) {
        pout("ircl", "Must specify a channel to replay.");
        return;
    }
    fprintf(rl_outstream, "\n");
    SIMPLEQ_FOREACH(e, &hist_head, entries) {
        if (e != NULL && strncmp(channel, e->channel, strlen(channel)) == 0) {
            fprintf(rl_outstream, "> %s", e->msg);
        }
    }
}


static void
load_usernames_file() {
    FILE *user_file = NULL;
    char user_file_path[PATH_MAX];
    char *line = NULL;
    const char *home_path = NULL;
    ssize_t chars_read = 0;
    size_t len = 0;
    int count = 0, i = 0;
    SLIST_HEAD(listhead, entry) head = SLIST_HEAD_INITIALIZER(head);
    struct entry {
        SLIST_ENTRY(entry) entries;
        const char *name;
    } *entp;

    home_path = getenv("HOME");
    if (home_path == NULL) {
        home_path = "/tmp";
    }
    snprintf(user_file_path, sizeof user_file_path, "%s/.irclusers", home_path);
    user_file = fopen(user_file_path, "r");
    if (user_file == NULL) {
        usernames = NULL;
        return;
    }
    while ((chars_read = getline(&line, &len, user_file)) != -1) {
        if (chars_read > 2) {
            line[chars_read - 1] = '\0'; /* remove delimiter */
            entp = malloc(sizeof(struct entry));
            entp->name = strdup(line);
            SLIST_INSERT_HEAD(&head, entp, entries);
            count++;
        }
    }
    fclose(user_file);
    free(line);

    usernames = calloc(count+1, sizeof(char*));
    for (i=0; i<count; i++) {
        entp = SLIST_FIRST(&head);
        SLIST_REMOVE_HEAD(&head, entries);
        usernames[i] = entp->name;
        free(entp);
    }
    usernames[count] = NULL; /* sentinel */
}
