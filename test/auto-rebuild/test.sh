#!/bin/sh
cat << EOF > cb.c
#define CB_IMPLEMENTATION
#include "../../cb.h"
int main(int argc, char ** argv) {
    cb_rebuild_on_change(__FILE__, argv);
    return 0;
}
EOF

echo "Compiling"
gcc -o cb cb.c -DDEBUG
echo "Running first time"
./cb
sleep 1
echo "Changing cb.c"
touch cb.c
echo "Running second time"
./cb
