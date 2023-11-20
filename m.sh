sudo umount -l /home/mars/PFS/fuse
dd if=/dev/zero of=diskimg bs=512 count=16384
./init
gcc PFS.c -o PFS -lfuse3
./PFS -d fuse