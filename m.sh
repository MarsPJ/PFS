sudo umount -l /root/data/PFS/fuse
dd if=/dev/zero of=diskimg bs=512 count=16384
gcc init.c -o init
./init
gcc PFS.c -o PFS -lfuse3
./PFS -d fuse