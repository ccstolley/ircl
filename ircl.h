#include <ctype.h>
#include <errno.h>
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
#include <netdb.h>
#include <netinet/in.h>
#define MAX_NICKS 1024
#define ACTIVE_NICKS_QUEUE_SIZE 10
#define COLOR_RESET "\033[00m"
#define COLOR_OUTGOING "\033[01;33m"
#define COLOR_INCOMING "\033[01;32m"
#define COLOR_CHANNEL "\033[01;34m"
#define COLOR_PROMPT "\033[00;33m"


int insert_nick(const char *nick);
int remove_nick(const char *nick);
void init_nick(const char *nick);
char *stripwhite (char *string);
void initialize_readline();
char *username_generator PARAMS((const char *, int));
char *nick_generator PARAMS((const char *, int));
char **ircl_completion PARAMS((const char *, int, int));
void readline_nonblocking_cb(char* line);
int handle_return_cb();
static void update_active_nicks(const char*);
static int nick_is_active(const char *);

const char* usernames[] = { "replace", "with", "config", "file", "test", NULL};
