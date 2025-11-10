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
    int status;
    int is_executed;
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

// Checks if the command has exited with the desired exit code, otherwise panic
static inline void command_has_exited_with_code(command_t *cmd, int exit_code);

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

// Checks if the command has exited with exit code 0
static inline void command_has_exited_normally(command_t *cmd);

// Returns the exit code of the executed command
static inline int command_get_exit_code(command_t * cmd);

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

#define CB_IMPLEMENTATION
#ifdef CB_IMPLEMENTATION

static inline void _panic(const char * fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[PANIC] ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    exit(EXIT_FAILURE);
}

static inline void * _safe_alloc(size_t size, const char * error_msg) {
    debug_print("starting _safe_alloc\n");
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

#include <unistd.h>
#include <limits.h>

char * _assemble_full_path(char * arg) {
    // No matter what, the first argument is allocated on the heap, this makes deinit cleaner
    size_t arg_len = strlen(arg);

    char * result = 0;

    // If argument is absolute path, just copy it over
    if(arg[0] == '/') {
        debug_print("absolute path\n");
        size_t len = strlen(arg);
        debug_print("strlen(arg)=%d\n", len);
        result = (char *)_safe_alloc((len + 1) * sizeof(char), "_assemble_full_path: failed to allocate full arg");
        result[len] = 0;
        strcpy(result, arg);
    }

    // Argument is not an absolute path
    // Get the current working directory
    // If argument starts with a ., prepend the current working directory to it
    else if(arg[0] == '.') {
        debug_print("relative path\n");
        debug_print("arg=%s\n", arg);
        char cwd[PATH_MAX] = { 0 };
        if(!getcwd(cwd, sizeof(cwd))) _panic("_assemble_full_path: failed to call getcwd\n");
        debug_print("cwd=%s\n", cwd);
        size_t cwd_len = strlen(cwd);
        size_t total_len = cwd_len + 1 + arg_len + 1;
        debug_print("total_len=%d\n", total_len);
        result = (char *)_safe_alloc(total_len * sizeof(char), "_assemble_full_path: failed to allocate total_len bytes");
        debug_print("past _safe_alloc\n");
        strcpy(result, cwd);
        result[cwd_len] = '/';
        strcpy(result + cwd_len + 1, arg);
        result[total_len - 1] = 0;
    }

    // Argument is a path variable
    //else {
    //    _panic("path variables are not supported yet!");
    //}

    return result;
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
    cmd->status = 0;
    cmd->is_executed = 0;
    // Assemble full path
    char * full_path = _assemble_full_path(arg);
    debug_print("full_path=%s\n", full_path);
    debug_print("address(full_path)=%p, address(arg)=%p\n", full_path, arg);

    command_append(cmd, arg);
    return cmd;
}

static inline void command_deinit(command_t * cmd) {
    debug_print("starting deinit\n");

    if(cmd->args) {
        // FIX: this gives error "free(): invalid pointer", but args[0] is allocated using malloc
        //free(cmd->args[0]);
        //debug_print("free(cmd->args[0]) OK\n");
        free(cmd->args);
        debug_print("free(cmd->args) OK\n");
    }
    free(cmd);
    debug_print("free(cmd) OK\n");
    debug_print("done deinit\n");
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
    char ** assembled = (char **)_safe_alloc(sizeof(char *) * (cmd->size + 1), "_command_assemble: failed to allocate assembled"); // Here was a bug as well, I forgot the ( ... )
    memcpy(assembled, cmd->args, sizeof(char *) * cmd->size);
    assembled[cmd->size] = NULL; // This line had an out of bounds write to cmd->size + 1 :skull:
    return assembled;
}



// ---------- LINUX SPECIFIC ----------
#ifdef __linux__

#define CC "/usr/sbin/cc"

#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <limits.h>
#include <errno.h>

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
            if(result == -1) { _panic("command_execute: execve failed with error '%s'", strerror(errno)); }
        }
    } else {
        debug_print("parent\n");
        waitpid(p, &cmd->status, 0);
        debug_print("before free\n");
        free(assembled);
        debug_print("after free\n");
        cmd->is_executed = 1;
    }
    debug_print("finished\n");
}

static inline long _last_modified(char *file) {
    struct stat sb;
    stat(file, &sb);
    return sb.st_mtim.tv_sec;
}

#endif
// ---------- END OF LINUX SPECIFIC ----------

#include <assert.h>

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
        debug_print("built new version\n");

        assert(exec_mtime < _last_modified(*argv) && "File is not changed!");
        // New version is built

        /*
        // Assemble the full path
        debug_print("running new instance\n");
        char cwd[PATH_MAX] = { 0 };
        if(!getcwd(cwd, sizeof(cwd))) _panic("cb_rebuild_on_change: failed to call getcwd\n");
        debug_print("cwd=%s\n", cwd);
        int cwd_len = strlen(cwd);

        // If program starts with . do not include the .
        int total_len = 0;
        int has_dot = 0;
        if(**argv == '.') { total_len = cwd_len + strlen(*argv); has_dot = 1; }
        else total_len = cwd_len + strlen(*argv) + 1;

        debug_print("total_len=%d\n", total_len);

        //char * abs_path = (char *)_safe_alloc(total_len, "cb_rebuild_on_change: failed to allocate abs_path");
        //strcpy(abs_path, cwd);

        if(has_dot) strcpy(cwd + cwd_len, (*argv) + 1);
        else strcpy(cwd + cwd_len, *argv);

        cwd[total_len - 1] = 0;
        debug_print("abs_path=%s\n", cwd);
        */
        //execve(cwd, argv, environ);
        char * full_path = _assemble_full_path(*argv);
        debug_print("full_path=%s\n", full_path);
        execve(full_path, argv, environ);
    }
}

static inline void command_has_exited_with_code(command_t *cmd, int exit_code) {
    if(!cmd->is_executed) _panic("Command has not been executed yet!\n");
    int exit_status = WEXITSTATUS(cmd->status);
    if(exit_code != exit_status) {
        _panic("Child exited abnormally with exit code %d!\n", exit_status);
    }
}

static inline void command_has_exited_normally(command_t *cmd) {
    command_has_exited_with_code(cmd, 0);
}

static inline int command_get_exit_code(command_t *cmd) {
    if(!cmd->is_executed) {
        return -1;
    }

    return WEXITSTATUS(cmd->status);
}

#endif

#endif
