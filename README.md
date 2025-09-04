# CBuild

CBuild, CB, is a build system for C written in C. It can also be used to execute arbitrary commands so it can build other languages. But that removes the point of making a build system for a language in that same language!

## Usage

1. Download the cb.h header file
2. Make a build file (for example called "cb.c")
3. Define the "CB_IMPLEMENTATION" macro
4. Include "cb.h"
5. Make a main function and use the interface to construct a command to build the project
6. Compile the build system file using your compiler of choise

Example:

cb.c
```C
#define CB_IMPLEMENTATION
#include "cb.h"

int main() {
    command_t *cmd = command_init("gcc");
    command_append_n(cmd, "hello.c", "-o hello", NULL);
    command_execute(cmd);
    return 0;
}
```

Shell
```Shell
gcc cb.c -o cb
./cb
```

## Disclaimer

!!THE API IS NOT STABLE YET!!

## Future plans

- Make it cross platform, allowing expandability for different operating systems
- Incremental builds
- Piping commands
