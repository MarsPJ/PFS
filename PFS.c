#define FUSE_USE_VERSION 31
#include<sys/types.h>
#include<fuse3/fuse.h>
#include"util.h"
#include <sys/stat.h>
#include<string.h>
#include<errno.h>
#define PRINTF_FLUSH(fmt, ...) do { \
    printf(fmt, ##__VA_ARGS__); \
    fflush(stdout); \
} while (0)
// 全局变量
struct super_block m_sb;
const char* disk_path = "/root/data/PFS/diskimg";

// 记录当前已经被使用的最大的inode的id
short int cur_inode_id = 1;
// 记录当前已经被使用的最大的数据块号的id(根据数据块进行编号)
short int cur_data_blk_id = -1;

struct inode_table_entry inode_table[INODE_AREA_BLOCK_NUM];


// 每个索引块指向的内容都属于同一个文件
// 同一个文件分配的数据块可以不连续，只需要把块地址都放到同一个索引块中即可（按顺序）
// 需要增加分配空闲块和回收数据块的方法，更好地管理数据块
// 需要增加分配inode号的方法，更好地管理inode

// 不考虑超出最大文件数的情况
// mode=1表示返回空闲数据块
// mode=2表示返回空闲inode号
void get_free_data_blk(short int* ids, int num, int mode)
{
    FILE* reader = NULL;
    reader = fopen(disk_path, "r+");
    if (mode == 1)
    {
        PRINTF_FLUSH("申请空闲数据块\n");
        fseek(reader, m_sb.first_blk_of_databitmap * BLOCK_SIZE, SEEK_SET);
    }
    else
    {
        PRINTF_FLUSH("申请空闲inode号\n");
        fseek(reader, m_sb.fisrt_blk_of_inodebitmap * BLOCK_SIZE, SEEK_SET);
    }
    
    long max_byte_num = 0;
    if (mode == 1)
    {
        max_byte_num = m_sb.databitmap_size * BLOCK_SIZE;
    }
    else
    {
        max_byte_num = m_sb.inodebitmap_size * BLOCK_SIZE;
    }

    unsigned char cur_byte;
    // 已经获得多少个空闲块号
    int count_ids = 0;
    // 已经读取了多少个字节
    int byte_count = 0;
    // 当读取字节数大于或等于BLOCK_SIZE时或者已经获得足够空闲块时，停止循环
    while (byte_count < max_byte_num && count_ids < num)
    {
        fread(&cur_byte, sizeof(unsigned char), 1, reader);
        for (int i = 0; i < 8; i++)
        {
            // 检查当前字节的每一位
            if ((cur_byte & (1 << i)) == 0) // 如果当前位是0
            {
                PRINTF_FLUSH("当前字节数：%d, 当前bit：%d\n", byte_count, i);
                // 置为1
                cur_byte |= (1 << i);
                // 获得空闲块inode号
                if (mode == 2)
                {
                    ids[count_ids++] = byte_count * 8 + i + 1;
                }
                // 获得空闲数据块号
                else
                {
                    ids[count_ids++] = m_sb.first_blk + byte_count * 8 + i;
                }
                
                if (count_ids >= num)
                {
                    break;
                }
            }
        }
        byte_count++;
    }
    fclose(reader);
    if (count_ids == num)
    {
        PRINTF_FLUSH("成功申请得到和预期一致的%d个空闲块或inode号\n", count_ids);
    }
    else if (count_ids > num)
    {
        PRINTF_FLUSH("申请得到多于预期的%d个空闲块或inode号\n", count_ids);
    }
    else
    {
        PRINTF_FLUSH("申请得到少于预期的%d个空闲块或inode号\n", count_ids);
    }
}


// 先读取超级块的内容
void get_sb_info() {
    PRINTF_FLUSH("get_sb_info begin\n");
    FILE* reader = fopen(disk_path, "rb");
    fread(&m_sb, sizeof(struct super_block), 1, reader);
    PRINTF_FLUSH("fs_size: %ld\nfirst_blk: %ld\ndatasize: %ld\nfirst_node: %ld\n", m_sb.fs_size, m_sb.first_blk, m_sb.datasize, m_sb.first_inode);
    fclose(reader);
    PRINTF_FLUSH("get_sb_info end\n");
}

struct inode root_inode;
// 根据addr（块号）读取块的数据，成功读取返回0，否则返回-1
int read_data_block(short int addr,struct data_block* data_blk)
{
    FILE* reader;
    reader = fopen(disk_path, "rb");
    if (NULL == reader)
    {
        PRINTF_FLUSH("read_data_block中diskimg打开失败！\n");
        perror("Error");
        return -1;
    }
    fseek(reader, addr * BLOCK_SIZE, SEEK_SET);
    fread(data_blk, sizeof(struct data_block), 1, reader);
    fclose(reader);
    return 0;
}
int get_root_inode(struct inode* tmp_node)
{
    PRINTF_FLUSH("get_root_inode begin\n");
    FILE* reader = fopen(disk_path, "rb");
    if (reader == NULL) {
        PRINTF_FLUSH("get_root_inode中diskimg打开失败\n");
        PRINTF_FLUSH("get_root_inode end\n");
        return -1;
    }
    else
    {
        // fseek(reader, m_sb.first_inode * BLOCK_SIZE + (root_inode.inode_id- 1) * sizeof(struct inode), SEEK_SET);
        // struct inode tmp_node;
        // fread(&tmp_node, sizeof(struct inode), 1, reader);
        // PRINTF_FLUSH("%hd, %hd\n", tmp_node.st_ino, tmp_node.addr[0]);

        PRINTF_FLUSH("inodetable:\ninode_table[0].inode_id:, %hd, inode_table[0].file_name: %s\n", inode_table[0].inode_id, inode_table[0].file_name);
        fseek(reader, m_sb.first_inode * BLOCK_SIZE, SEEK_SET);
        fread(tmp_node, sizeof(struct inode), 1, reader);
        PRINTF_FLUSH("get_root_inode:\n tmp_node->st_ino: %hd,  tmp_node->addr: %hd\n", tmp_node->st_ino, tmp_node->addr[0]);
        fclose(reader);
        PRINTF_FLUSH("get_root_inode end\n");
        return 0;
    }
    
}
int getFileFullNameByDataBlock(char* file_fullnames, struct data_block* data_blk)
{
    FILE* reader = NULL;
    reader = fopen(disk_path, "rb");
    int count = 0;
    PRINTF_FLUSH("getFileFullNameByDataBlock: %d\n", count);
    if (NULL == reader)
    {
        PRINTF_FLUSH("READER:NULL\n");
        return -1;
    }
    int pos = 0;
    struct dir_entry* tmp_dir_entry = (struct dir_entry*) data_blk->data;
    int i = 0;
    while (pos < data_blk->used_size)
    {
        char full_name[MAX_FILE_FULLNAME_LENGTH + 2];
        strcpy(full_name, tmp_dir_entry->file_name);
        if (0 != strlen(tmp_dir_entry->extension))
        {
            strcat(full_name, ".");
            strcat(full_name, tmp_dir_entry->extension);    
        }
        strcpy(file_fullnames, full_name);
        PRINTF_FLUSH("数据块中第%d个目录名：%s\n", i + 1, tmp_dir_entry->file_name);
        PRINTF_FLUSH("保存到file_fullnames中的第%d个目录名：%s\n", i + 1, file_fullnames);
        i++;
        file_fullnames += MAX_FILE_FULLNAME_LENGTH + 2;
        tmp_dir_entry++;
        pos += sizeof(struct dir_entry);
    }
    fclose(reader);
    return i;
}
static int pzj_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi)
{
    PRINTF_FLUSH("pzj_getattr begin\n");
    PRINTF_FLUSH("getattr	 path : %s \n", path);
    if (strcmp("/", path) == 0) {
        PRINTF_FLUSH("yes!!!\n");
        stbuf->st_ino = root_inode.st_ino;
        stbuf->st_mode = root_inode.st_mode | S_IFDIR;
        stbuf->st_nlink = root_inode.st_nlink;
        stbuf->st_uid = root_inode.st_uid;
        stbuf->st_gid = root_inode.st_gid;
        stbuf->st_size = root_inode.st_size;
        stbuf->st_atime = root_inode.st_atim.tv_sec;
        stbuf->st_blocks = m_sb.first_inode + root_inode.st_ino - 1;
        return 0;

    }
    else {
        const char* path_cp = path;
        // 跳过根目录/
        path_cp++;
        char* second_path = strchr(path_cp, '/');
        // 有二级目录，形如：/fada/....
        if (NULL != second_path)
        {
            PRINTF_FLUSH("不允许二级路径！\n");
            return -1;
        }
        const char* dir_file_name = path_cp;
        PRINTF_FLUSH("请求的文件或目录名：%s\n", dir_file_name);
        int addr_num = sizeof(root_inode.addr) / sizeof(root_inode.addr[0]);
        PRINTF_FLUSH("addr_num: %d\n", addr_num);
        int tmp_addr;
        for (int i = 0; i < addr_num; ++i)
        {
            tmp_addr = root_inode.addr[i];
            PRINTF_FLUSH("tmp_addr: %hd\n", tmp_addr);

            // 结束，没找到对应的目录
            if (-1 == tmp_addr)
            {
                
                return -ENOENT;
            }
            else
            {
                // 直接地址，直接读取数据块中存储的目录项（包括文件）
                if (i <= 3)
                {
                    // 停止条件
                    // 1.到达块末尾
                    // 2.读到数据为空

                    // TODO：-----------------以下内容可以复用
                    PRINTF_FLUSH("读取直接地址的数据\n");
                    // 申请数据块内存
                    struct data_block* data_blk = malloc(sizeof(struct data_block));
                    // 读取数据块数据
                    int ret = read_data_block(tmp_addr, data_blk);
                    if (ret < 0)
                    {
                        return -1;
                    }
                    PRINTF_FLUSH("成功读取数据块内容\n");
                    // 得到数据块中存储的dir_entry的个数
                    size_t dir_num = data_blk->used_size / sizeof(struct dir_entry);
                    PRINTF_FLUSH("used_size：%zu\n", data_blk->used_size);
                    PRINTF_FLUSH("数据块存储的目录项个数为%zu\n", dir_num);
                    int count = 0;
                    int pos = 0;
                    struct dir_entry* tmp_dir_entry = (struct dir_entry*) data_blk->data;
                    while (pos < data_blk->used_size)
                    {
                        char full_name[MAX_FILE_FULLNAME_LENGTH + 2];
                        strcpy(full_name, tmp_dir_entry->file_name);
                        if (0 != strlen(tmp_dir_entry->extension))
                        {
                            strcat(full_name, ".");
                            strcat(full_name, tmp_dir_entry->extension);    
                        }
                        if (0 == strcmp(dir_file_name, full_name))
                        {
                            short int target_inode_id = tmp_dir_entry->inode_id;
                            FILE* reader = NULL;
                            reader = fopen(disk_path, "rb");
                            long off = m_sb.first_inode * BLOCK_SIZE + (target_inode_id - 1) * sizeof(struct inode);
                            fseek(reader, off, SEEK_SET);
                            struct inode tmp_inode;
                            fread(&tmp_inode, sizeof(struct inode), 1, reader);
                            stbuf->st_ino = tmp_inode.st_ino;
                            stbuf->st_mode = tmp_inode.st_mode | S_IFDIR;
                            stbuf->st_nlink = tmp_inode.st_nlink;
                            stbuf->st_uid = tmp_inode.st_uid;
                            stbuf->st_gid = tmp_inode.st_gid;
                            stbuf->st_size = tmp_inode.st_size;
                            stbuf->st_atime = tmp_inode.st_atim.tv_sec;
                            stbuf->st_blocks = m_sb.first_inode + tmp_inode.st_ino - 1;
                            PRINTF_FLUSH("成功找到%s\n", dir_file_name);
                            fclose(reader);
                            free(data_blk);
                            return 0;

                        }
                        tmp_dir_entry++;
                        pos += sizeof(struct dir_entry);
                    }
                }
                // 一次间接寻址，先找到所有存储到数据（目录项）的数据块，再读取这些数据块中的目录项
                else if (i == 4)
                {
                    // 索引块地址
                    short int index_blk_addr = tmp_addr;
                    // 读出索引块的信息
                    // 申请索引块内存
                    struct data_block* data_blk = malloc(sizeof(struct data_block));
                    // 读取索引块数据
                    int ret = read_data_block(tmp_addr, data_blk);
                    if (ret < 0)
                    {
                        free(data_blk);
                        return -1;
                    }
                    PRINTF_FLUSH("成功读取一级索引块内容\n");
                    // 计算存储的数据块的地址个数
                    int addr_num = data_blk->used_size / sizeof(short int);
                    int pos = 0;
                    short int* data_addr = (short int*) data_blk->data;
                    while (pos < data_blk->used_size)
                    {
                        short int tmp_data_addr = *data_addr;
                        PRINTF_FLUSH("读取直接地址的数据\n");
                        // 申请数据块内存
                        struct data_block* tmp_data_blk = malloc(sizeof(struct data_block));
                        // 读取数据块数据
                        int ret = read_data_block(tmp_data_addr, tmp_data_blk);
                        if (ret < 0)
                        {
                            free(data_blk);
                            free(tmp_data_blk);
                            return -1;
                        }
                        PRINTF_FLUSH("成功读取数据块内容\n");
                        // 得到数据块中存储的dir_entry的个数
                        int dir_num = tmp_data_blk->used_size / sizeof(struct dir_entry);
                        PRINTF_FLUSH("数据块存储的目录项个数为%d\n", dir_num);
                        // 申请文件全名内存
                        char* file_fullnames = malloc(sizeof(char) * dir_num * (MAX_FILE_FULLNAME_LENGTH + 2));
                        // 得到当前数据块所有文件全名
                        int file_fullname_num = getFileFullNameByDataBlock(file_fullnames, tmp_data_blk);
                        if (file_fullname_num < 0)
                        {
                            free(data_blk);
                            free(tmp_data_blk);
                            free(file_fullnames);
                            return -1;
                        }
                        PRINTF_FLUSH("数据块存储的文件名个数为%d\n", file_fullname_num);
                        for (int i = 0; i < file_fullname_num; ++i)
                        {
                            if (file_fullnames != NULL)
                            {
                                if ( 0 == strcmp(file_fullnames, dir_file_name))
                                {
                                    PRINTF_FLUSH("目录已存在，无法创建重名的目录！\n");
                                    free(data_blk);
                                    free(tmp_data_blk);
                                    free(file_fullnames);
                                    return -EEXIST;
                                }
                            }
                            else
                            {
                                // 处理无效的文件全名
                                PRINTF_FLUSH("%d 无效\n", i);
                            }
                            file_fullnames += MAX_FILE_FULLNAME_LENGTH + 2;
                        }
                        data_addr++;
                        pos += sizeof(short int);
                        free(tmp_data_blk);
                        free(file_fullnames);
                    }
                    free(data_blk);
                    
                }
                
            }
            
        }



        
    }
}
int isEmptyDirEntry(struct dir_entry* tmp_dir_entry)
{
    PRINTF_FLUSH("%s, %d, %s, %s\n", tmp_dir_entry->file_name, tmp_dir_entry->inode_id, tmp_dir_entry->extension, tmp_dir_entry->reserved);
    if (strcmp(tmp_dir_entry->file_name, "") == 0 && 
        tmp_dir_entry->inode_id == 0 &&
        strcmp(tmp_dir_entry->extension, "") == 0 &&
        strcmp(tmp_dir_entry->reserved, "") == 0)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}
/**
 * 检查路径有效性：首先，检查传入的路径是否表示一个目录，因为只有目录才需要列出内容。如果路径无效或者不是目录，你可以返回一个错误码（例如 ENOTDIR）来指示目录无效。
 * 使用 filler 函数：使用 filler 参数中提供的函数，填充目录项（dirent）。这是列出目录内容的关键步骤。
 * 列出目录项：遍历目录下的文件和子目录，为每个目录项调用 filler 函数，以填充目录项的名称和类型。通常，你需要添加当前目录 ("."，表示该目录自身) 和父目录 (".."，表示上级目录) 到目录项中。
 * 返回成功或错误：如果一切正常，返回0以指示成功。如果发生错误，可以返回适当的错误码（如 ENOENT 表示未找到目录，或其他错误码）来指示问题。
*/

// 先通过目录名（绝对路径）找出对应的inode号，再通过inode中的addr，访问这个目录的数据区，读取目录项



static int pzj_readdir(const char *path, void *buf, fuse_fill_dir_t filler,off_t offset, struct fuse_file_info *fi)
{
    PRINTF_FLUSH("pzj_readdir begin\n");
    PRINTF_FLUSH( "tfs_readdir	 path : %s \n", path);
    // 所有目录都有的内容
    filler(buf, ".", NULL, 0, 0);
    int first_ret = filler(buf, "..", NULL, 0, 0);
    PRINTF_FLUSH("hello!\n");
    // 这是根目录
    if(strcmp(path, "/") == 0)
    {   
        PRINTF_FLUSH("这是根目录\n");
        // 由于是根目录，因此可以直接得到inode
        int addr_num = sizeof(root_inode.addr) / sizeof(root_inode.addr[0]);
        PRINTF_FLUSH("addr_num: %d\n", addr_num);
        int tmp_addr;
        for (int i = 0; i < addr_num; ++i)
        {
            tmp_addr = root_inode.addr[i];
            PRINTF_FLUSH("tmp_addr: %hd\n", tmp_addr);

            // 结束，没找到对应的目录
            if (-1 == tmp_addr)
            {
                if (i == 0)
                {
                    return first_ret;
                }
                return -ENOENT;
            }
            else
            {
                // 直接地址，直接读取数据块中存储的目录项（包括文件）
                if (i <= 3)
                {
                    // 停止条件
                    // 1.到达块末尾
                    // 2.读到数据为空

                    // TODO：-----------------以下内容可以复用
                    PRINTF_FLUSH("读取直接地址的数据\n");
                    // 申请数据块内存
                    struct data_block* data_blk = malloc(sizeof(struct data_block));
                    // 读取数据块数据
                    int ret = read_data_block(tmp_addr, data_blk);
                    if (ret < 0)
                    {
                        return -1;
                    }
                    PRINTF_FLUSH("成功读取数据块内容\n");
                    // 得到数据块中存储的dir_entry的个数
                    int dir_num = data_blk->used_size / sizeof(struct dir_entry);
                    PRINTF_FLUSH("数据块存储的目录项个数为%d\n", dir_num);
                    // 申请文件全名内存
                    char* file_fullnames = malloc(sizeof(char) * dir_num * (MAX_FILE_FULLNAME_LENGTH + 2));
                    // 得到当前数据块所有文件全名
                    int file_fullname_num = getFileFullNameByDataBlock(file_fullnames, data_blk);
                    if (file_fullname_num < 0)
                    {
                        return -1;
                    }
                    PRINTF_FLUSH("数据块存储的文件名个数为%d\n", dir_num);
                    PRINTF_FLUSH("以下是数据块中的文件名：\n");
                    for (int i = 0; i < file_fullname_num; ++i)
                    {
                        char fullname[MAX_FILE_FULLNAME_LENGTH + 2];
                        if (file_fullnames != NULL)
                        {
                            strcpy(fullname, file_fullnames);
                            PRINTF_FLUSH("%s\n", fullname);
                            filler(buf, fullname, NULL, 0, 0);
                        }
                        else
                        {
                            // 处理无效的文件全名
                            PRINTF_FLUSH("%d 无效\n", i);
                        }
                        file_fullnames += MAX_FILE_FULLNAME_LENGTH + 2;
                    }
                    // -----------------以上内容可以复用
                    PRINTF_FLUSH("装填filler完毕\n");
                    
                }
                // 一次间接寻址，先找到所有存储到数据（目录项）的数据块，再读取这些数据块中的目录项
                else if (i == 4)
                {
                    // 索引块地址
                    short int index_blk_addr = tmp_addr;
                    // 读出索引块的信息
                    // 申请索引块内存
                    struct data_block* data_blk = malloc(sizeof(struct data_block));
                    // 读取索引块数据
                    int ret = read_data_block(tmp_addr, data_blk);
                    if (ret < 0)
                    {
                        free(data_blk);
                        return -1;
                    }
                    PRINTF_FLUSH("成功读取一级索引块内容\n");
                    // 计算存储的数据块的地址个数
                    int addr_num = data_blk->used_size / sizeof(short int);
                    int pos = 0;
                    short int* data_addr = (short int*) data_blk->data;
                    while (pos < data_blk->used_size)
                    {
                        short int tmp_data_addr = *data_addr;
                        PRINTF_FLUSH("读取直接地址的数据\n");
                        // 申请数据块内存
                        struct data_block* tmp_data_blk = malloc(sizeof(struct data_block));
                        // 读取数据块数据
                        int ret = read_data_block(tmp_data_addr, tmp_data_blk);
                        if (ret < 0)
                        {
                            
                            free(tmp_data_blk);
                            free(data_blk);
                            return -1;
                        }
                        PRINTF_FLUSH("成功读取数据块内容\n");
                        // 得到数据块中存储的dir_entry的个数
                        int dir_num = tmp_data_blk->used_size / sizeof(struct dir_entry);
                        PRINTF_FLUSH("数据块存储的目录项个数为%d\n", dir_num);
                        // 申请文件全名内存
                        char* file_fullnames = malloc(sizeof(char) * dir_num * (MAX_FILE_FULLNAME_LENGTH + 2));
                        // 得到当前数据块所有文件全名
                        int file_fullname_num = getFileFullNameByDataBlock(file_fullnames, tmp_data_blk);
                        if (file_fullname_num < 0)
                        {
                            free(file_fullnames);
                            free(tmp_data_blk);
                            free(data_blk);
                            return -1;
                        }
                        PRINTF_FLUSH("数据块存储的文件名个数为%d\n", dir_num);
                        for (int i = 0; i < file_fullname_num; ++i)
                        {
                            char fullname[MAX_FILE_FULLNAME_LENGTH + 2];
                            if (file_fullnames != NULL)
                            {
                                strcpy(fullname, file_fullnames);
                                PRINTF_FLUSH("%s\n", fullname);
                                filler(buf, fullname, NULL, 0, 0);
                            }
                            else
                            {
                                // 处理无效的文件全名
                                PRINTF_FLUSH("%d 无效\n", i);
                            }
                            file_fullnames += MAX_FILE_FULLNAME_LENGTH + 2;
                        }
                        data_addr++;
                        pos += sizeof(short int);
                        free(file_fullnames);
                        free(tmp_data_blk);
                        free(data_blk);
                    }
                    
                }
                return 0;
            }
            
        }

    }
    return filler(buf, "Hello-world", NULL, 0, 0);
}

/**
 * 创建目录： 首要任务是在底层文件系统中创建一个新的目录。这通常涉及到在文件系统中分配一个新的目录条目，设置正确的权限、所有者等信息。
 * 路径解析： 确保创建目录的路径是有效的，并且父目录存在。你需要解析路径以确定父目录的位置。
 * 权限检查： 检查用户是否有权限在指定的目录中创建子目录。这通常涉及到检查访问控制列表（ACL）、文件权限位（例如，读、写、执行权限）等。
 * 更新文件系统状态： 更新文件系统的元数据，包括目录的大小、时间戳、所有者等信息。
 * 回馈结果： 如果创建目录成功，通常需要向用户返回成功的响应。如果创建失败，需要返回错误码，如权限不足、磁盘空间不足等。
*/
// TODO:检查权限

// 检查数据块是否已满，已满返回1，否则返回0，读取数据块发生错误返回-1
static int block_isfull(short int addr)
{
    struct data_block* data_blk = malloc(sizeof(struct data_block));
    int ret = read_data_block(addr, data_blk);
    if (ret != -1)
    {
        if (BLOCK_MAX_DATA_SIZE <= data_blk->used_size)
        {
            PRINTF_FLUSH("数据块%hd已满！\n", addr);
            free(data_blk);
            return 1;
        }
        else
        {
            free(data_blk);
            return 0;
        }
    }
    else{
        PRINTF_FLUSH("读取数据块出错！\n");
        free(data_blk);
        return -1;
    }
}
const char* GetFileExtension(const char* filename) {
    const char* dot = strrchr(filename, '.'); // 在字符串中查找最后一个点的位置
    if (dot && dot < filename + strlen(filename) - 1) {
        // 找到点并且点不在字符串的最后一个位置
        return dot + 1; // 返回点之后的部分作为后缀名
    } else {
        // 没有找到点或者点在字符串的最后一个位置
        return ""; // 或者可以返回一个默认的后缀名
    }
}
void SplitFileNameAndExtension(const char* filename, char* name, char* extension) {
    const char* dot = strrchr(filename, '.');
    
    if (dot && dot < filename + strlen(filename) - 1) {
        // 找到点并且点不在字符串的最后一个位置
        size_t dotIndex = dot - filename;
        
        // 复制文件名部分
        strncpy(name, filename, dotIndex);
        name[dotIndex] = '\0'; // 确保文件名部分以null结尾

        // 复制后缀名部分
        strcpy(extension, dot + 1);
    } else {
        // 没有找到点或者点在字符串的最后一个位置
        strcpy(name, filename);
        extension[0] = '\0'; // 后缀名为空字符串
    }
}
// type=1表示目录，type=2表示文件
static int create_dir_or_file (const char *path, mode_t mode, int type)
{
    if (type == 1)
    {
        PRINTF_FLUSH("创建目录, path:%s\n", path);
    }
    else
    {
        PRINTF_FLUSH("创建文件, path:%s\n", path);
    }
    // 路径解析
    const char* path_cp = path;
    // 跳过根目录/
    path_cp++;
    char* second_path = strchr(path_cp, '/');
    // 有二级目录，形如：/fada/....
    if (NULL != second_path)
    {
        PRINTF_FLUSH("目录或文件创建失败，不能创建非根目录下的目录或文件！\n");
        return -EPERM;
    }
    const char* new_dir_or_file_name = path_cp;
    if (type == 1)
    {
        PRINTF_FLUSH("新的目录名：%s\n", new_dir_or_file_name);
    }
    else
    {
        PRINTF_FLUSH("新的文件名：%s\n", new_dir_or_file_name);
    }
    
    // 一级目录，形如/....
    // 提取要创建的目录名
    int dir_or_file_name_len = strlen(new_dir_or_file_name);

  
    // 检查目录名长度
    if (dir_or_file_name_len > MAX_DIR_FILE_NAME_LEN)
    {
        PRINTF_FLUSH("目录或文件创建失败，目录或文件名超出规定的8个字符的长度！\n");
        return -ENAMETOOLONG;
    }
    // 检查后缀名长度
    const char* exten = GetFileExtension(new_dir_or_file_name);
    if (type == 2)
    {
        if (strlen(exten) > 3)
        {
            PRINTF_FLUSH("文件的后缀名大于3个字节，创建失败！\n");
            return -ENAMETOOLONG;
        }
    }

    // 检查是否有重名目录
    // 1.读取根目录下的所有目录名
    int addr_num = sizeof(root_inode.addr) / sizeof(root_inode.addr[0]);
    PRINTF_FLUSH("addr_num: %d\n", addr_num);
    int tmp_addr;
    for (int i = 0; i < addr_num; ++i)
    {
        tmp_addr = root_inode.addr[i];
        PRINTF_FLUSH("tmp_addr: %hd\n", tmp_addr);

        // 结束，没找到对应的目录
        if (-1 == tmp_addr)
        {
            break;
        }
        else
        {
            // 直接地址，直接读取数据块中存储的目录项（包括文件）
            if (i <= 3)
            {
                // 停止条件
                // 1.到达块末尾
                // 2.读到数据为空

                // TODO：-----------------以下内容可以复用
                PRINTF_FLUSH("读取直接地址的数据\n");
                // 申请数据块内存
                struct data_block* data_blk = malloc(sizeof(struct data_block));
                // 读取数据块数据
                int ret = read_data_block(tmp_addr, data_blk);
                if (ret < 0)
                {
                    free(data_blk);
                    return -1;
                }
                PRINTF_FLUSH("成功读取数据块内容\n");
                // 得到数据块中存储的dir_entry的个数
                int dir_num = data_blk->used_size / sizeof(struct dir_entry);
                PRINTF_FLUSH("数据块存储的目录项个数为%d\n", dir_num);
                // 申请文件全名内存
                char* file_fullnames = malloc(sizeof(char) * dir_num * (MAX_FILE_FULLNAME_LENGTH + 2));
                memset(file_fullnames, '\0', sizeof(char) * dir_num * (MAX_FILE_FULLNAME_LENGTH + 2));
                // 得到当前数据块所有文件全名
                int file_fullname_num = getFileFullNameByDataBlock(file_fullnames, data_blk);
                if (file_fullname_num < 0)
                {
                    // free(file_fullnames);
                    free(data_blk);
                    return -1;
                }
                PRINTF_FLUSH("%s\n", file_fullnames);
                PRINTF_FLUSH("数据块存储的文件名个数为%d\n", file_fullname_num);
                for (int i = 0; i < file_fullname_num; ++i)
                {
                    PRINTF_FLUSH("%s\n", file_fullnames);
                    if (file_fullnames != NULL)
                    {
                        PRINTF_FLUSH("%s", file_fullnames);
                        if ( 0 == strcmp(file_fullnames, new_dir_or_file_name))
                        {
                            PRINTF_FLUSH("目录已存在，无法创建重名的目录！\n");
                            // free(file_fullnames);
                            free(data_blk);
                            return -EEXIST;
                        }
                    }
                    else
                    {
                        // 处理无效的文件全名
                        PRINTF_FLUSH("%d 无效\n", i);
                    }
                    file_fullnames += MAX_FILE_FULLNAME_LENGTH + 2;
                }
                // free(file_fullnames);
                free(data_blk);
                // -----------------以上内容可以复用
                
            }
            // 一次间接寻址，先找到所有存储到数据（目录项）的数据块，再读取这些数据块中的目录项
            else if (i == 4)
            {
                // 索引块地址
                short int index_blk_addr = tmp_addr;
                // 读出索引块的信息
                // 申请索引块内存
                struct data_block* data_blk = malloc(sizeof(struct data_block));
                // 读取索引块数据
                int ret = read_data_block(tmp_addr, data_blk);
                if (ret < 0)
                {
                    return -1;
                }
                PRINTF_FLUSH("成功读取一级索引块内容\n");
                // 计算存储的数据块的地址个数
                int addr_num = data_blk->used_size / sizeof(short int);
                int pos = 0;
                short int* data_addr = (short int*) data_blk->data;
                while (pos < data_blk->used_size)
                {
                    short int tmp_data_addr = *data_addr;
                    PRINTF_FLUSH("读取直接地址的数据\n");
                    // 申请数据块内存
                    struct data_block* tmp_data_blk = malloc(sizeof(struct data_block));
                    // 读取数据块数据
                    int ret = read_data_block(tmp_data_addr, tmp_data_blk);
                    if (ret < 0)
                    {
                        return -1;
                    }
                    PRINTF_FLUSH("成功读取数据块内容\n");
                    // 得到数据块中存储的dir_entry的个数
                    int dir_num = tmp_data_blk->used_size / sizeof(struct dir_entry);
                    PRINTF_FLUSH("数据块存储的目录项个数为%d\n", dir_num);
                    // 申请文件全名内存
                    char* file_fullnames = malloc(sizeof(char) * dir_num * (MAX_FILE_FULLNAME_LENGTH + 2));
                    // 得到当前数据块所有文件全名
                    int file_fullname_num = getFileFullNameByDataBlock(file_fullnames, tmp_data_blk);
                    if (file_fullname_num < 0)
                    {
                        return -1;
                    }
                    PRINTF_FLUSH("数据块存储的文件名个数为%d\n", file_fullname_num);
                    for (int i = 0; i < file_fullname_num; ++i)
                    {
                        if (file_fullnames != NULL)
                        {
                            if ( 0 == strcmp(file_fullnames, new_dir_or_file_name))
                            {
                                PRINTF_FLUSH("目录已存在，无法创建重名的目录！\n");
                                return -EEXIST;
                            }
                        }
                        else
                        {
                            // 处理无效的文件全名
                            PRINTF_FLUSH("%d 无效\n", i);
                        }
                        file_fullnames += MAX_FILE_FULLNAME_LENGTH + 2;
                    }
                    data_addr++;
                    pos += sizeof(short int);
                }
                
            }
            
        }
        
    }

    // 不存在此目录名

    // 开始创建目录
    // 1.创建inode
    // 2.分配inode中的addr数组
    // 3.将inode放入inode数据区
    // 4.更新inode位图区
    // 5.创建dir_entry对象
    // 6.将dir_entry放入数据区，更新根目录的大小以及addr地址并更新数据位图区以及将根目录信息重新写入文件系统
    // 7.更新数据位图区

    // 创建inode
    struct inode new_inode;
    new_inode.st_mode = mode;
    short int* inode_ids = (short int*)malloc(sizeof(short int));
    get_free_data_blk(inode_ids, 1, 2);
    new_inode.st_ino = *inode_ids;
    // free(inode_ids);
    PRINTF_FLUSH("新建目录的inode值为：%hd\n", new_inode.st_ino);
    new_inode.st_nlink = 1;
    new_inode.st_uid = 0;
    new_inode.st_gid = 0;
    new_inode.st_size = 0;
    struct timespec access_time;
    clock_gettime(CLOCK_REALTIME, &access_time);
    new_inode.st_atim = access_time;
    // 分配inode中的addr数组
    for (int i = 0; i < 7; ++i) {
        new_inode.addr[i] = -1;
    }
    // 将inode放入inode数据区
    FILE* reader = NULL;
    reader = fopen(disk_path, "r+");
    long off = m_sb.first_inode * BLOCK_SIZE + (new_inode.st_ino - 1) * sizeof(struct inode);
    fseek(reader, off, SEEK_SET);
    fwrite(&new_inode, sizeof(struct inode), 1, reader);

    // 更新inode位图区
    int over_bytes_num = (new_inode.st_ino - 1) / 8;
    PRINTF_FLUSH("over_bytes_num：%d\n", over_bytes_num);
    int over_bit = (new_inode.st_ino - 1) % 8;
    PRINTF_FLUSH("over_bit：%d\n", over_bit);
    off = m_sb.fisrt_blk_of_inodebitmap * BLOCK_SIZE + over_bytes_num;
    PRINTF_FLUSH("off：%ld\n", off);

    // 将文件指针移动到inode位图第一个字节
    fseek(reader, off, SEEK_SET);
    PRINTF_FLUSH("文件指针目前的位置：%ld\n", ftell(reader));
    unsigned char tmp_byte;

    fread(&tmp_byte, 1, 1, reader);
    PRINTF_FLUSH("inode_first_byte的值：%d\n", tmp_byte);

    // tmp_byte += 0x01;
    tmp_byte |= (1 << over_bit);
    PRINTF_FLUSH("更新后的inode_first_byte的值：%d\n", tmp_byte);
    // 将文件指针复位
    fseek(reader, off, SEEK_SET);
    PRINTF_FLUSH("文件指针目前的位置：%ld\n", ftell(reader));
    // 重新写回文件
    if (1 != fwrite(&tmp_byte, 1, 1, reader)) {

        PRINTF_FLUSH("inode位图区更新失败 %d\n", errno);
        return -1;
    }
    PRINTF_FLUSH("写入后文件指针目前的位置：%ld\n", ftell(reader));
    PRINTF_FLUSH("inode位图区更新成功！\n");
    fclose(reader);
    // 将dir_entry放入数据区，更新根目录的大小以及addr地址并将根目录信息重新写入文件系统

    /**
     * 一个数据块只能供一个目录或文件使用
     * 但是由于只有根目录下能创建目录或文件，因此还是需要判断当前数据块是否满了
     * */ 

    // 记录地址不为-1的最大位置
    int count = -1;
    for (int i = 0; i< 7; ++i)
    {
        // PRINTF_FLUSH("i: %d\n", i);
        if (root_inode.addr[i] == -1)
        {
            break;
        }
        else
        {
            ++count;
        }
    }
    tmp_addr = -1;
    struct data_block* tmp_data_blk = malloc(sizeof(struct data_block));
    // 一定要初始化data部分
    PRINTF_FLUSH("1\n");
    PRINTF_FLUSH("count为：%d\n", count);
    memset(tmp_data_blk->data, '\0', BLOCK_MAX_DATA_SIZE);
    // 从来没有数据，只需新建数据块即可
    if (count == -1)
    {
         PRINTF_FLUSH("2\n");
        // 更新根目录大小信息
        root_inode.st_size += sizeof(struct dir_entry);
        root_inode.addr[0] = m_sb.first_blk;

        // 更新当前数据块已使用大小和数据信息
        struct dir_entry* new_dir_entry = (struct dir_entry*) tmp_data_blk->data;
        // 目录
        if (type == 1)
        {
            strcpy(new_dir_entry->file_name, new_dir_or_file_name);
            PRINTF_FLUSH("新目录名为：%s\n", new_dir_or_file_name);
            PRINTF_FLUSH("写入的新目录名为：%s\n", new_dir_entry->file_name);
        }
        // 文件
        else
        {
            SplitFileNameAndExtension(new_dir_or_file_name, new_dir_entry->file_name, new_dir_entry->extension);
            PRINTF_FLUSH("新文件全名为：%s\n", new_dir_or_file_name);
            PRINTF_FLUSH("写入的新文件名为：%s\n", new_dir_entry->file_name);
            PRINTF_FLUSH("extension内容以及长度：%s, %zu\n", new_dir_entry->extension, strlen(new_dir_entry->extension));
        }
        new_dir_entry->inode_id = new_inode.st_ino;
        tmp_data_blk->used_size = sizeof(struct dir_entry);

        // 将数据块信息写入文件
        FILE* reader = NULL;
        reader = fopen(disk_path, "r+");
       
        short int* data_blk_ids = (short int*)malloc(sizeof(short int));
        get_free_data_blk(data_blk_ids, 1, 1);
        long off = *data_blk_ids * BLOCK_SIZE;
        fseek(reader, off, SEEK_SET);
        fwrite(tmp_data_blk, sizeof(struct data_block), 1, reader);
        PRINTF_FLUSH("目录项写入数据区成功！\n");

        // 将根目录信息重新写入文件系统
        off = m_sb.first_inode * BLOCK_SIZE;
        fseek(reader, off, SEEK_SET);
        fwrite(&root_inode, sizeof(struct inode), 1, reader);
        PRINTF_FLUSH("根目录inode成功更新到文件系统！\n");
        fclose(reader);
        free(tmp_data_blk);
        return 0;
    }
    // 原来的数据块有数据，要追加数据
    // 先读取原来的数据
    else{
        FILE* reader = NULL;
        reader = fopen(disk_path, "r+");
        tmp_addr = root_inode.addr[count];
        long off = tmp_addr * BLOCK_SIZE;
        fseek(reader, off, SEEK_SET);
        fread(tmp_data_blk, sizeof(struct data_block), 1, reader);
        fclose(reader);
    }
    PRINTF_FLUSH("跳出来了\n");
    // 检查原来的数据块是否已经满了
    // 当前数据块已存的目录数
    int cur_dir_num = tmp_data_blk->used_size / sizeof(struct dir_entry);
    // 原来的数据块已经满了
    if (cur_dir_num >= DIR_ENTRY_MAXNUM_PER_BLOCK)
    {
        PRINTF_FLUSH("当前有效块已满，需要新建块\n");
        // 下一个是直接地址
        if (count <= 2)
        {
            // 更新根目录的addr数组信息和大小信息
            short int* data_blk_ids = (short int*)malloc(sizeof(short int));
            get_free_data_blk(data_blk_ids, 1, 1);
            root_inode.addr[count + 1] = *data_blk_ids;
            // free(data_blk_ids);
            root_inode.st_size += sizeof(struct dir_entry);
            
            
            // 新创建数据块
            struct data_block* new_data_blk = malloc(sizeof(struct data_block));
            new_data_blk->used_size = sizeof(struct dir_entry);
            // 创建dir_entry对象并写入数据块
            struct dir_entry* new_dir_entry = (struct dir_entry*) new_data_blk->data;
            strcpy(new_dir_entry->file_name, new_dir_or_file_name);
            new_dir_entry->inode_id = new_inode.st_ino;

            // 将数据块信息写入文件
            FILE* reader = NULL;
            reader = fopen(disk_path, "r+");
            long off = root_inode.addr[count + 1] * BLOCK_SIZE;
            fseek(reader, off, SEEK_SET);
            fwrite(new_data_blk, sizeof(struct data_block), 1, reader);
            PRINTF_FLUSH("目录项写入数据区成功！\n");

            // 将根目录信息重新写入文件系统
            off = m_sb.first_inode * BLOCK_SIZE;
            fseek(reader, off, SEEK_SET);
            fwrite(&root_inode, sizeof(struct inode), 1, reader);
            PRINTF_FLUSH("目录inode成功更新到文件系统！\n");
            fclose(reader);
            free(new_data_blk);
            return 0;
        }
        // 下一个是一级间接地址
        else if (count == 3)
        {

        }
        // 下一个是二级间接地址
        else if (count == 4)
        {

        }
        // 下一个是三级间接地址
        else if (count == 5)
        {

        }
        // 文件系统满了
        else
        {

        }
    }
    // 原来的数据块还没满
    else
    {
        // 更新根目录大小信息
        root_inode.st_size += sizeof(struct dir_entry);
        
        // 更新当前数据块已使用大小和数据信息
        struct dir_entry* new_dir_entry = (struct dir_entry*) tmp_data_blk->data;
        PRINTF_FLUSH("cur_dir_num: %d\n", cur_dir_num);
        PRINTF_FLUSH("sizeof(struct dir_entry): %zu\n", sizeof(struct dir_entry));
        PRINTF_FLUSH("new_dir_name: %s\n", new_dir_or_file_name);
        PRINTF_FLUSH("第1个目录名：%s\n", new_dir_entry->file_name);
        PRINTF_FLUSH("原来的new_dir_entry地址：%p\n", new_dir_entry);
        // 移到要追加的位置

        while (cur_dir_num > 0)
        {
            new_dir_entry++;
            --cur_dir_num;
        }
        PRINTF_FLUSH("偏移后的new_dir_entry地址：%p\n", new_dir_entry);
        // 追加新的目录内容

        if (type == 1)
        {
            strcpy(new_dir_entry->file_name, new_dir_or_file_name);
            PRINTF_FLUSH("新目录名为：%s\n", new_dir_or_file_name);
            PRINTF_FLUSH("写入的新目录名为：%s\n", new_dir_entry->file_name);
        }
        // 文件
        else
        {
            SplitFileNameAndExtension(new_dir_or_file_name, new_dir_entry->file_name, new_dir_entry->extension);
            PRINTF_FLUSH("新文件全名为：%s\n", new_dir_or_file_name);
            PRINTF_FLUSH("写入的新文件名为：%s\n", new_dir_entry->file_name);
            PRINTF_FLUSH("extension内容以及长度：%s, %zu\n", new_dir_entry->extension, strlen(new_dir_entry->extension));
        }
        new_dir_entry->inode_id = new_inode.st_ino;
        PRINTF_FLUSH("inode号：%hd\n", new_dir_entry->inode_id);
        // 更新数据块已使用大小
        tmp_data_blk->used_size += sizeof(struct dir_entry);

        // 将数据块信息写入文件
        FILE* reader = NULL;
        reader = fopen(disk_path, "r+");
        long off = root_inode.addr[count] * BLOCK_SIZE;
        fseek(reader, off, SEEK_SET);
        fwrite(tmp_data_blk, sizeof(struct data_block), 1, reader);
        PRINTF_FLUSH("目录项写入数据区成功！\n");

        // 将根目录信息重新写入文件系统
        off = m_sb.first_inode * BLOCK_SIZE;
        fseek(reader, off, SEEK_SET);
        fwrite(&root_inode, sizeof(struct inode), 1, reader);
        PRINTF_FLUSH("根目录inode成功更新到文件系统！\n");
        fclose(reader);
        return 0;
    }

}
static int pzj_mkdir (const char *path, mode_t mode)
{
    return create_dir_or_file(path, mode, 1);
}
static int pzj_mknod (const char *path, mode_t mode, dev_t dev)
{
    dev=0;
    return create_dir_or_file(path, mode, 2);
}
static int pzj_utimens(const char *path, const struct timespec tv[2], struct fuse_file_info *fi) {
    // 在这里实现处理 UTIMENS 操作的逻辑，设置文件的访问时间和修改时间
    // ...

    return 0; // 成功时返回0，失败时返回-errno
}
static struct fuse_operations pzj_oper = {
    .getattr = pzj_getattr,
    .readdir = pzj_readdir,
    .mkdir = pzj_mkdir,
    .mknod = pzj_mknod,
    .utimens = pzj_utimens
};

int main(int argc, char *argv[]) {

    FILE* reader = fopen(disk_path, "rb");
    if (reader == NULL) {
        PRINTF_FLUSH("main中diskimg打开失败\n");
        return -1;
    }
    PRINTF_FLUSH("成功打开\n");
    fclose(reader);
    // 读取超级块
    get_sb_info();
    // 初始化inode表第一个元素
    struct inode_table_entry root_inode_entry;
    root_inode_entry.file_name = "/";
    root_inode_entry.inode_id = 1;
    inode_table[0] = root_inode_entry;
    // 读取根节点
    int ret = get_root_inode(&root_inode);
    if (-1 == ret)
    {
        return -1;
    }

    // return NULL;
	// umask(0);
	return fuse_main(argc, argv, &pzj_oper, NULL);
}