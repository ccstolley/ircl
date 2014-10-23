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
#define MAX_NICKS 256

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

const char* usernames[] = { "replace", "with", "config", "file", "test", NULL};
