#include <ctype.h>
#include <errno.h>
#include <libgen.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <netdb.h>
#include <netinet/in.h>
#define UNUSED(x) (void)(x)
#define MAX_NICKS 1024
#define ACTIVE_NICKS_QUEUE_SIZE 10
#define COLOR_RESET "\033[00m"
#define COLOR_OUTGOING "\033[00;33m"
#define COLOR_INCOMING "\033[00;32m"
#define COLOR_PM_INCOMING "\033[01;31m"
#define COLOR_CHANNEL "\033[07;34m"
#define COLOR_PROMPT "\033[00;33m"
#ifndef PATH_MAX
  #define PATH_MAX 1024
#endif


int insert_nick(const char *nick);
int remove_nick(const char *nick);
void init_nick(const char *nick);
char *stripwhite (char *string);
void initialize_readline();
static char *username_generator PARAMS((const char *, int));
static char *nick_generator PARAMS((const char *, int));
static char *cmd_generator PARAMS((const char *, int));
char **ircl_completion PARAMS((const char *, int, int));
void readline_nonblocking_cb(char* line);
int handle_return_cb();
static void update_active_nicks(const char*);
static int nick_is_active(const char *);
static void load_usernames_file();
static void add_channel(const char *); 
static void remove_channel(const char *);
static const char* channel_color(const char *);

/* command handlers */
struct command_handler {
    const char *short_cmd;
    const char *long_cmd;
    void (*func_ptr)(const char*);
};
static void handle_help();
static void handle_who_channel(const char*);
static void handle_join(const char*);
static void handle_part(const char*);
static void handle_who_all();
static void handle_msg(const char*);
static void handle_me(const char*);
static void handle_switch(const char*);
static void handle_away(const char*);
static void handle_quit();

const struct command_handler command_map[] = {
    { "g", "away", handle_away},
    { "h", "help", handle_help},
    { "j", "join", handle_join},
    { "p", "part", handle_part},
    { "m", "msg", handle_msg},
    { "a", "me", handle_me},
    { "s", "switch", handle_switch},
    { "w", "who", handle_who_channel},
    { "W", "whoa", handle_who_all},
    { "Q", "quit", handle_quit},
    { NULL, NULL, 0 }  /* sentinel */
};
struct irc_channel {
    const char *name;
    const char const *color;
};
struct irc_channel active_channels[] = {
    {NULL, "\033[01;37m"}, /* white */
    {NULL, "\033[01;35m"}, /* magenta */
    {NULL, "\033[01;36m"}, /* cyan */
    {NULL, "\033[01;32m"}, /* green */
    {NULL, "\033[01;33m"}, /* yellow */
    {NULL, "\033[01;34m"}, /* blue */
};
const int MAX_CHANNELS = sizeof(active_channels)/sizeof(struct irc_channel);
const char** usernames;
