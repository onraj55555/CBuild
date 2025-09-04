#ifndef CB_H_
#define CB_H_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>

typedef struct command_t command_t;

struct command_t {
    char *arg;
    command_t *next;
};

command_t * command_init(const char *arg);
void command_append(command_t *cmd, const char *arg);
void command_append_n(command_t *cmd, const char *arg0, ...);
void command_execute(command_t *cmd);

#ifdef CB_IMPLEMENTATION

// TODO: check allocation errors
command_t * command_init(const char * arg) {
    command_t * cmd = (command_t *)malloc(sizeof(command_t));
    cmd->arg = strdup(arg);
    cmd->next = 0;
    return cmd;
}

void command_append(command_t *cmd, const char * arg) {
    while(cmd->next) {
        cmd = cmd->next;
    }

    cmd->next = command_init(arg);
}

void command_append_n(command_t *cmd, const char * arg0, ...) {
    va_list ap;
    const char * arg;

    va_start(ap, arg0);
    arg = arg0;

    while(arg) {
        command_append(cmd, arg);
        arg = va_arg(ap, const char *);
    }

    va_end(ap);
}

char ** command_assemble(const command_t *cmd) {
    int count = 0;
    command_t * t = (command_t *)cmd;

    while(t) {
        count += 1;
        t = t->next;
    }

    count += 1; // Add the null sentinal

    char ** argv = (char **)malloc(count * sizeof(char *));
    argv[count - 1] = NULL;

    t = (command_t *)cmd;
    char ** argv_t = argv;
    while(t) {
        *argv_t = strdup(t->arg);
        t = t->next;
        argv_t += 1;
    }

    return argv;
}

void command_execute(command_t *cmd) {
    char ** assembled = command_assemble(cmd);
    execve(assembled[0], assembled, (char **)NULL);
}

#endif

#endif
