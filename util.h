#include<unistd.h>
#include<sys/types.h>
#include<stdio.h>
#include<stdlib.h>
#include<sys/stat.h>
#include<time.h>
// 文件系统总大小
#define FILE_SYSTEM_SIZE 8 * 1024 * 1024
// 每个块总大小
#define BLOCK_SIZE 512
// 超级块数目
#define SUPER_BLOCK_NUM 1
// inode bitmap 区域的块数目
#define INODE_BITMAP_BLOCK_NUM 1
// data bitmap 区域的块数目
#define DATA_BITMAP_BLOCK_NUM 4
// INODE区的块数目
#define INODE_AREA_BLOCK_NUM 512
// 目录项大小
#define DIR_ENTRY_SIZE 16
// 每个数据块能存放的最大目录项数目
#define DIR_ENTRY_MAXNUM_PER_BLOCK BLOCK_MAX_DATA_SIZE / DIR_ENTRY_SIZE
// 最大的文件名或目录名长度
#define MAX_DIR_FILE_NAME_LEN 8
// 文件名全名（文件名+扩展名）最大长度
#define MAX_FILE_FULLNAME_LENGTH 8 + 3
// 每个索引块能存放的最多的直接地址数
#define MAX_ADDR_NUM_PER_INDEX_BLOCK BLOCK_SIZE / sizeof(short int) 
// 总共的数据块个数（包括索引块）
#define BLOCK_TOTAL_NUM FILE_SYSTEM_SIZE / BLOCK_SIZE
// 每个数据块真正能被使用的有效空间
#define BLOCK_MAX_DATA_SIZE BLOCK_SIZE - sizeof(size_t)

// 每个块512字节
// 超级块(1)，inode位图区(1)，数据块位图（4），inode区（512）
// 超级块占用1块，用于描述整个文件系统
// Inode 位图占用1块，共计1*512*8=4k位，即该文件系统最多能够有4k个文件。
// 数据块位图占用4块，4*512*8*512=8M。
// iNode区占用512块，每个inode占用64字节，共有512*512/64=512*8，4k个文件。

struct super_block {
    long fs_size;  //文件系统的大小，以块为单位
    long first_blk;  //数据区的第一块块号，根目录也放在此
    long datasize;  //数据区大小，以块为单位 
    long first_inode;    //inode区起始块号
    long inode_area_size;   //inode区大小，以块为单位
    long fisrt_blk_of_inodebitmap;   //inode位图区起始块号
    long inodebitmap_size;  // inode位图区大小，以块为单位
    long first_blk_of_databitmap;   //数据块位图起始块号
    long databitmap_size;      //数据块位图大小，以块为单位
};

// 存储权限信息以及内容的存储地址
struct inode { 
    short int st_mode; /* 权限，2字节 */ 
    short int st_ino; /* i-node号，2字节 */ 
    char st_nlink; /* 链接数，1字节 */ 
    uid_t st_uid; /* 拥有者的用户 ID ，4字节 */ 
    gid_t st_gid; /* 拥有者的组 ID，4字节  */ 
    off_t st_size; /*文件大小，4字节 */ 
    struct timespec st_atim;/* 16个字节time of last access */ 
    short int addr[7];    /* 磁盘地址，14字节 */
};

struct inode_table_entry {
    char* file_name;
    short int inode_id;
};

// 只有超级块(1)，inode位图区(1)，数据块位图（4），inode区（512）之后的块才会用到（即数据块和索引块）
struct data_block
{
    size_t used_size;
    char data[BLOCK_MAX_DATA_SIZE];
};

// 存储文件名或者目录名，以及inode号
struct dir_entry
{
    char file_name[MAX_DIR_FILE_NAME_LEN + 1];
    char extension[3 + 1];
    short int inode_id;
    char reserved[1];
};

