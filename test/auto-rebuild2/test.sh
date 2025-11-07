#!/bin/sh

echo "Patching back to original"
patch --forward --batch --no-backup-if-mismatch -s cb.c < reset.patch > /dev/null 2>&1
gcc -o cb cb.c #-DDEBUG
./cb
echo Exit value: $?
sleep 1
echo "Patching to new version"
patch cb.c < v1_to_v2.patch
./cb
echo Exit value: $?
echo "Patching back to original"
patch cb.c < reset.patch
