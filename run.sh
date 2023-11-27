sudo umount -l ./fuse
dd if=/dev/zero of=./res/diskimg bs=512 count=16384
gcc ./src/init.c -o ./bin/init -lm
./bin/init
gcc ./src/PFS.c -o ./bin/PFS -lfuse3 -lm
./bin/PFS -d ./fuse