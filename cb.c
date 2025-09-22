#include <stdio.h>
#define CB_IMPLEMENTATION
#include "cb.h"

int main(int argc, char ** argv) {
    parse_arguments(argc, argv);
    char * argument = get_argument_from_flag("-o");
    printf("Arg: %s\n", argument);
    printf("Has flag -g: %d\n", has_flag("-g"));
    printf("Argument at index 1 is run: %d\n", has_argument_at_intex("run", 1));
    return 0;
}
