#define CB_IMPLEMENTATION
#include "../../cb.h"
int main(int argc, char ** argv) {
    cb_rebuild_on_change(__FILE__, argv);
    return 0;
}
