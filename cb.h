#ifndef CB_H_
#define CB_H_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

// PUBLIC
typedef struct command_t command_t;
typedef struct argument_t argument_t;
typedef struct option_t option_t;

struct command_t {
    char *arg;
    command_t *next;
};

static inline command_t * command_init(const char *arg);
static inline void command_append(command_t *cmd, const char *arg);
static inline void command_append_n(command_t *cmd, const char *arg0, ...);
static inline void command_execute(command_t *cmd);

struct option_t {
    char * flag;
    char ** values;
    int option_count;
    option_t * next;
};

struct argument_t {
    char ** positional_arguments; // == argv
    int positional_arguments_count;

    option_t * options;
};

static argument_t _arguments;

static inline void parse_arguments(int argc, char ** argv);
static inline char * get_argument_at_index(int i);
static inline char ** get_arguments_from_flag(char * flag);
static inline char * get_argument_from_flag(char * flag);
static inline int has_flag(char * flag);
static inline int has_argument_at_intex(char * arg, int i);

#define CB_IMPLEMENTATION
#ifdef CB_IMPLEMENTATION

static inline void _panic(const char * fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    exit(EXIT_FAILURE);
}

static inline void * _safe_alloc(size_t size, const char * error_msg) {
    void * a = malloc(size);
    if(!a) {
        _panic(error_msg);
    }
    return a;
}

static inline option_t * _get_option(char * flag) {
    if(!_arguments.options) {
        _arguments.options = (option_t *)_safe_alloc(sizeof(option_t), "_get_option: failed to allocate initial option");
        option_t * o = _arguments.options;
        o->flag = flag;
        o->values = 0;
        o->option_count = 0;
        o->next = 0;
        return o;
    }

    option_t * o = _arguments.options;
    while(strcmp(o->flag, flag) != 0) {
        if(o->next == 0) {
            o->next = (option_t *)_safe_alloc(sizeof(option_t), "_get_option: failed to allocate option");
            o->next->flag = flag;
            o->next->next = 0;
            o->next->option_count = 0;
            o->next->values = 0;
            o = o->next;
            break;
        }
        o = o->next;
    }

    return o;
}

static inline void _add_value(option_t * o, char * value) {
    char ** t = o->values;
    o->values = (char **)_safe_alloc((o->option_count + 1) * sizeof(char *), "_add_value: failed to allocate");
    for(int i = 0; i < o->option_count; i++) {
        o->values[i] = t[i];
    }
    o->values[o->option_count] = value;
    o->option_count++;
}

static inline void parse_arguments(int argc, char ** argv) {
    _arguments.positional_arguments = argv;
    _arguments.positional_arguments_count = argc;
    for(int i = 0; i < argc; i++) {
        if(argv[i][0] == '-') {
            option_t * o = _get_option(argv[i]);
            if(i + 1 == argc) return;
            if(argv[i+1][0] == '-') continue; // Solves problem of flags without value: -g -o a, this makes -g not contain -o as its value
            _add_value(o, argv[i+1]);
        }
    }
}

static inline char ** get_arguments_from_flag(char * flag) {
    char ** arguments = NULL;
    option_t * o = _arguments.options;
    while(o != NULL) {
        if(strcmp(o->flag, flag) == 0) {
            arguments = o->values;
            break;
        }

        o = o->next;
    }

    return arguments;
}

static inline char * get_argument_from_flag(char * flag) {
    char ** arguments = get_arguments_from_flag(flag);
    if(arguments == NULL) return NULL;
    return arguments[0];
}

static inline int has_flag(char * flag) {
    option_t * o = _arguments.options;
    while(o != NULL) {
        if(strcmp(o->flag, flag) == 0) return 1;
        o = o->next;
    }
    return 0;
}

static inline int has_argument_at_intex(char * arg, int i) {
    if(i >= _arguments.positional_arguments_count) return 0;
    return strcmp(_arguments.positional_arguments[i], arg) == 0;
}

static inline command_t * command_init(const char * arg) {
    command_t * cmd = (command_t *)malloc(sizeof(command_t));

    {
        if(!cmd) {
            _panic("command_init: malloc failed");
        }
    }

    cmd->arg = strdup(arg);

    {
        if(!cmd->arg) {
            _panic("command_init: strdup failed");
        }
    }

    cmd->next = 0;
    return cmd;
}

static inline void command_append(command_t *cmd, const char * arg) {
    while(cmd->next) {
        cmd = cmd->next;
    }

    cmd->next = command_init(arg);
}

static inline void command_append_n(command_t *cmd, const char * arg0, ...) {
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

static inline char ** _command_assemble(const command_t *cmd) {
    int count = 0;
    command_t * t = (command_t *)cmd;

    while(t) {
        count += 1;
        t = t->next;
    }

    count += 1; // Add the null sentinal

    char ** argv = (char **)malloc(count * sizeof(char *));

    {
        if(!argv) {
            _panic("_command_assemble: malloc failed");
        }
    }

    argv[count - 1] = NULL;

    t = (command_t *)cmd;
    char ** argv_t = argv;
    while(t) {
        *argv_t = strdup(t->arg);

        {
            if(!*argv_t) {
                _panic("_command_assemble: strdup failed");
            }
        }

        t = t->next;
        argv_t += 1;
    }

    return argv;
}



// ---------- LINUX SPECIFIC ----------
#ifdef __linux__

#include <unistd.h>
#include <sys/wait.h>

static inline void command_execute(command_t *cmd) {
    char ** assembled = _command_assemble(cmd);

    pid_t p = fork();

    // Child
    if(p == 0) {
        int result = execve(assembled[0], assembled, (char **)NULL);
        {
            if(result == -1) { _panic("command_execute: execve failed"); }
        }
    } else {
        waitpid(p, NULL, 0);
    }

}

#endif
// ---------- END OF LINUX SPECIFIC ----------

#endif

#endif
