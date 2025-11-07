#define CB_IMPLEMENTATION
#include "../../cb.h"

#include <stdio.h>

const char * msg = "version 1";
int main(int argc, char ** argv) {
    cb_rebuild_on_change(__FILE__, argv);
    printf("%s\n", msg);
    return 0;
}
