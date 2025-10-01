#ifndef CB_H_
#define CB_H_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>

#ifdef DEBUG
#define debug_print(fmt, ...) fprintf(stderr, "[DEBUG] %s: " fmt, __FUNCTION__, ##__VA_ARGS__) // ## needed to remove the , if no arguments are supplied
#else
#define debug_print(fmt, ...)
#endif

// PUBLIC
typedef struct command_t command_t;
typedef struct argument_t argument_t;
typedef struct option_t option_t;

static inline void cb_rebuild_on_change(char * source, char ** argv);

struct command_t {
    char ** args;
    uint64_t size;
    uint64_t capacity;
};

// Initialises a command on the heap
static inline command_t * command_init(char *arg);

// Deinitialises a command, frees the memory 
static inline void command_deinit(command_t * cmd);

// Adds 1 argument to the command
static inline void command_append(command_t *cmd, char *arg);

// Adds n arguments to the command, end the var arg with NULL
static inline void command_append_n(command_t *cmd, char *arg0, ...);

// Executes the command in a separate process
static inline void command_execute(command_t *cmd);

// Equivalent to command_append(cmd, path)
static inline void command_add_source_file(command_t * cmd, char * path);

// Equivalent to command_append_n(cmd, "-I", path, NULL)
static inline void command_add_include_dir(command_t * cmd, char * path);

// Equivalent to command_append_n(cmd, "-o", name, NULL)
static inline void command_set_output_file(command_t * cmd, char * name);

// Equivalent to command_append_n(cmd, "-Wall", "-Werror", NULL);
static inline void command_enable_all_errors(command_t * cmd);

// Equivalent to command_append_n(cmd, "-l", name, NULL);
static inline void command_add_dynamic_library(command_t * cmd, char * name);

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

static inline time_t _last_modified(char * file);

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

static inline command_t * command_init(char * arg) {
    debug_print("command_init: arg = %s\n", arg);
    command_t * cmd = (command_t *)_safe_alloc(sizeof(command_t), "command_init: failed to allocate");
    cmd->args = NULL;
    cmd->size = 0;
    cmd->capacity = 0;
    command_append(cmd, arg);
    return cmd;
}

static inline void command_deinit(command_t * cmd) {
    if(cmd->args) free(cmd->args);
    free(cmd);
}

static inline void command_append(command_t *cmd, char * arg) {
    if(cmd->capacity == 0) {
        cmd->capacity = 1;
        cmd->args = (char **)_safe_alloc(sizeof(char *) * cmd->capacity, "command_append: failed to initially allocate");
    } else if(cmd->size == cmd->capacity) {
        cmd->capacity *= 2;
        char ** t = (char **)_safe_alloc(sizeof(char *) * cmd->capacity, "command_append: failed to double array size");
        memcpy(t, cmd->args, cmd->size * sizeof(char *));
        free(cmd->args);
        cmd->args = t;
    }
    cmd->args[cmd->size++] = arg;
}

static inline void command_append_n(command_t *cmd, char * arg0, ...) {
    va_list ap;
    char * arg;

    va_start(ap, arg0);
    arg = arg0;

    while(arg) {
        debug_print("command_append_n: arg=%s, strlen(arg)=%d, *arg=%d\n", arg, strlen(arg), *arg);
        command_append(cmd, arg);
        arg = va_arg(ap, char *);
    }

    va_end(ap);
}

static inline void command_add_source_file(command_t * cmd, char * path) {
    command_append(cmd, path);
}

static inline void command_add_include_dir(command_t * cmd, char * path) {
    command_append_n(cmd, "-I", path, NULL);
}

static inline void command_set_output_file(command_t * cmd, char * name) {
    command_append_n(cmd, "-o", name, NULL);
}

static inline void command_enable_all_errors(command_t * cmd) {
    command_append_n(cmd, "-Wall", "-Werror", NULL);
}

static inline void command_add_dynamic_library(command_t * cmd, char * name) {
    command_append_n(cmd, "-l", name, NULL);
}

static inline char ** _command_assemble(const command_t *cmd) {
    char ** assembled = (char **)_safe_alloc(sizeof(char *) * cmd->size + 1, "_command_assemble: failed to allocate assembled");
    memcpy(assembled, cmd->args, sizeof(char *) * cmd->size);
    assembled[cmd->size + 1] = NULL;
    return assembled;
}



// ---------- LINUX SPECIFIC ----------
#ifdef __linux__

#define CC "/usr/sbin/cc"

#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>

extern char ** environ;

static inline void command_execute(command_t *cmd) {
    debug_print("starting executing\n");
    char ** assembled = _command_assemble(cmd);
    char * a = *assembled;

    pid_t p = fork();

    // Child
    if(p == 0) {
        debug_print("child\n");
        int result = execve(assembled[0], assembled, environ);
        {
            if(result == -1) { _panic("command_execute: execve failed"); }
        }
    } else {
        debug_print("parent\n");
        waitpid(p, NULL, 0);
        debug_print("before free\n");
        free(assembled);
        debug_print("after free\n");
    }
    debug_print("done\n");

}

static inline long _last_modified(char *file) {
    struct stat sb;
    stat(file, &sb);
    return sb.st_mtim.tv_sec;
}

#endif
// ---------- END OF LINUX SPECIFIC ----------

void cb_rebuild_on_change(char * source, char ** argv) {
    long source_mtime = _last_modified(source);
    long exec_mtime = _last_modified(*argv);

    debug_print("source_mtime=%ld, exec_mtime=%ld\n", source_mtime, exec_mtime);

    if(source_mtime > exec_mtime) {
        debug_print("building new version\n");
        // Build new version
        command_t * cmd = command_init((char *)CC);
        command_add_source_file(cmd, source);
        command_set_output_file(cmd, *argv);
        command_execute(cmd);
        command_deinit(cmd);

        // Assemble the full path
        debug_print("running new instance\n");
        char * cwd = getcwd(NULL, 0);
        int cwd_len = strlen(cwd);

        // If program starts with . do not include the .
        int total_len = 0;
        int has_dot = 0;
        if(**argv == '.') { total_len = cwd_len + strlen(*argv); has_dot = 1; }
        else total_len = cwd_len + strlen(*argv) + 1;

        char * abs_path = (char *)_safe_alloc(total_len, "cb_rebuild_on_change: failed to allocate abs_path");
        strcpy(abs_path, cwd);
        free(cwd);

        if(has_dot) strcpy(abs_path + cwd_len, (*argv) + 1);
        else strcpy(abs_path + cwd_len, *argv);

        abs_path[total_len - 1] = 0;
        debug_print("abs_path=%s\n", abs_path);
        execve(abs_path, argv, environ);
    }
}

#endif

#endif
