#define CB_IMPLEMENTATION
#include "../../cb.h"

int main(int argc, char ** argv) {
    command_t * cmd = command_init("/usr/sbin/cc");
    command_append_n(cmd, "01.c", "-o", "01", NULL);
    command_execute(cmd);
    command_deinit(cmd);
    return 0;
}
