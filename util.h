
#pragma pack(1)
// 强制编译器以紧凑的方式排列结构体的成员，不考虑默认的对齐规则，使得struct dir_entry为16B而不进行填充
#define FUSE_USE_VERSION 31
#include<unistd.h>
#include<sys/types.h>
#include<stdio.h>
#include<stdlib.h>
#include<sys/stat.h>
#include<errno.h>
#include<time.h>
#include<sys/types.h>
#include<fuse3/fuse.h>
#include<string.h>
#include<math.h>

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
// 每个数据块真正能被使用的有效空间
#define BLOCK_MAX_DATA_SIZE BLOCK_SIZE - sizeof(size_t)
// 每个数据块能存放的最大目录项数目
#define DIR_ENTRY_MAXNUM_PER_BLOCK 31
// 最大的文件名或目录名长度
#define MAX_DIR_FILE_NAME_LEN 8
#define MAX_EXTENSION_LEN 3
// 文件名全名（文件名+扩展名）最大长度,不包括.
// MAX_FILE_FULLNAME_LENGTH + 2，指的是多预留了.和结尾\0符号
#define MAX_FILE_FULLNAME_LENGTH 8 + 3
// 每个索引块能存放的最多的直接地址数
#define MAX_ADDR_NUM_PER_INDEX_BLOCK BLOCK_SIZE / sizeof(short) 
// 总共的数据块个数（包括索引块）
#define BLOCK_TOTAL_NUM FILE_SYSTEM_SIZE / BLOCK_SIZE
// 磁盘文件路径
#define DISK_PATH  "/root/data/PFS/diskimg"

/**
 * 每个块512字节
 * 超级块(1)，inode位图区(1)，数据块位图（4），inode区（512）
 * 超级块占用1块，用于描述整个文件系统
 * Inode 位图占用1块，共计1*512*8=4k位，即该文件系统最多能够有4k个文件。
 * 数据块位图占用4块，4*512*8*512=8M。
 * iNode区占用512块，每个inode占用64字节，共有512*512/64=512*8，4k个文件。
 */
struct super_block
{
    long fs_size;                  // 文件系统的大小，以块为单位
    long first_blk;                // 数据区的第一块块号，根目录也放在此
    long datasize;                 // 数据区大小，以块为单位
    long first_inode;              // inode区起始块号
    long inode_area_size;          // inode区大小，以块为单位
    long first_blk_of_inodebitmap; // inode位图区起始块号
    long inodebitmap_size;         // inode位图区大小，以块为单位
    long first_blk_of_databitmap;  // 数据块位图起始块号
    long databitmap_size;          // 数据块位图大小，以块为单位
};

/**
 * st_size表示文件的字节大小，即文件中数据的实际大小。
 * 对于普通文件而言，这个字段会反映文件中的实际数据大小。
 * 对于目录文件，st_size通常是目录项所占用的字节数。
 * 对于符号链接，st_size是链接目标的路径长度。
*/

struct inode
{
    short st_mode;           // 权限，2字节
    short st_ino;            // i-node号，2字节
    char st_nlink;           // 链接数，1字节
    uid_t st_uid;            // 拥有者的用户 ID ，4字节
    gid_t st_gid;            // 拥有者的组 ID，4字节
    off_t st_size;           // 文件大小，4字节
    struct timespec st_atim; // time of last access，16字节
    short addr[7];           // 磁盘地址，14字节，其中addr[0]-addr[3]是直接地址，
                             // addr[4]是一次间接，addr[5]是二次间接，addr[6]是三次间接
};

// 只有超级块(1)，inode位图区(1)，数据块位图（4），inode区（512）之后的块才会用到（即数据块）,但是存储直接地址时不会用到
struct data_block
{
    size_t used_size;
    char data[BLOCK_MAX_DATA_SIZE];
};

// 存储文件名或者目录名，以及inode号
struct dir_entry
{
    char file_name[MAX_DIR_FILE_NAME_LEN + 1]; // 文件名（不包括扩展名）或者目录名
    char extension[MAX_EXTENSION_LEN + 1];     // 扩展名
    short inode_id;                            // inode号
    char type[1];                              // 文件还是目录标志
};

// 调用split_path时用到的结构体
struct paths
{
    char parent_dir[MAX_FILE_FULLNAME_LENGTH + 2];
    char new_dir_file_fullname[MAX_FILE_FULLNAME_LENGTH + 2];
};


/**
 * 全局变量
 * */ 
// 超级块
struct super_block m_sb;
// 根目录inode
struct inode root_inode;

/**
 * @brief 获取文件单例模式的文件指针
 *
 * 单例模式，确保只有一个文件指针实例，用于操作文件。
 *
 * @return FILE* 文件指针
 */
FILE *get_file_singleton() {
    // 静态局部变量，用于保存文件指针
    static FILE *file_singleton = NULL;

    // 如果文件指针为NULL，表示还没有打开文件
    if (file_singleton == NULL) {
        // 打开文件，这里假设文件名为 "diskimg"
        file_singleton = fopen(DISK_PATH, "r+");
        
        // 如果文件打开失败，输出错误信息并退出程序
        if (file_singleton == NULL) {
            perror("Error opening file");
            exit(EXIT_FAILURE);
        }

    }
    fflush(file_singleton);
    // 返回文件指针
    return file_singleton;
}

/**
 * @brief 关闭文件单例模式的文件指针
 *
 * 关闭之前获取的文件指针，确保文件操作完成后及时释放资源。
 */
void close_file_singleton() {
    // 获取文件指针
    FILE *file_singleton = get_file_singleton();

    // 关闭文件
    if (file_singleton != NULL) {
        fclose(file_singleton);
        file_singleton = NULL;
    }
}



/**
 * @brief 打印消息并刷新标准输出缓冲区
 *
 * 在打印消息后立即刷新标准输出缓冲区，确保可以立即得到输出信息。
 *
 * @param fmt 格式化字符串
 * @param ... 可变参数列表
 */
#define PRINTF_FLUSH(fmt, ...) do { \
    printf(fmt, ##__VA_ARGS__); \
    fflush(stdout); \
} while (0)

/**
 * @brief 分割文件名和扩展名
 *
 * 给定一个文件名，将文件名和扩展名分开。
 *
 * @param filename 待分割的文件名
 * @param name 存储文件名的字符数组
 * @param extension 存储扩展名的字符数组
 * @return int 分割成功返回0，扩展名过长返回-2
 */
int SplitFileNameAndExtension(const char* filename, char* name, char* extension) {
    const char* dot = strrchr(filename, '.');
    
    if (dot && dot < filename + strlen(filename) - 1) {
        // 找到点并且点不在字符串的最后一个位置
        size_t dotIndex = dot - filename;
        
        // 复制文件名部分
        strncpy(name, filename, dotIndex);
        name[dotIndex] = '\0'; // 确保文件名部分以null结尾

        // 复制后缀名部分
        if (strlen(dot + 1) > 3)
        {
            return -2;
        }
        strcpy(extension, dot + 1);
    } else {
        // 没有找到点或者点在字符串的最后一个位置
        strcpy(name, filename);
        extension[0] = '\0'; // 后缀名为空字符串
    }
    PRINTF_FLUSH("路径解析成功返回！\n");
    return 0;
}

/**
 * @brief 路径分割(以/开头)，将路径分割成父目录、文件名（不包括扩展名）、扩展名
 *
 * @param origin_path 待分割的路径
 * @param m_paths 存储分割结果的结构体
 * @return int 分割成功返回0，文件全名或目录名过长返回-1，扩展名过长返回-2
 */
int split_path(const char* origin_path, struct paths* m_paths)
{
    PRINTF_FLUSH("开始解析路径...\n");

    // 路径解析
    const char* path_cp = origin_path;
    // 跳过根目录/
    path_cp++;
    char* second_path = strchr(path_cp, '/');
    // second_path++;
    // 有二级目录，形如：/fada/....
    if (NULL != second_path)
    {
        // 跳过斜杠
        second_path++;
        if (strlen(second_path) >= MAX_FILE_FULLNAME_LENGTH)
        {
            PRINTF_FLUSH("%s\n", second_path);
            PRINTF_FLUSH("目录或文件全名过长！\n");
            return -1;
        }
        // 得到目录或文件全名
        strcpy(m_paths->new_dir_file_fullname, second_path);
        // printf("有二级目录，目录或文件全名: %s\n", m_paths->new_dir_file_fullname);
        // 计算长度
        size_t parent_dir_len = strlen(path_cp) - strlen(m_paths->new_dir_file_fullname) - 1;
        // 得到父目录名
        strncpy(m_paths->parent_dir, path_cp, parent_dir_len);
        // PRINTF_FLUSH("父目录：%s\n", m_paths->parent_dir);
        // 得到文件名和扩展名
        return 0;
        
    }
    // 没有二级目录
    else
    {
        if (strlen(path_cp) >= MAX_FILE_FULLNAME_LENGTH)
        {
            PRINTF_FLUSH("目录或文件全名过长！\n");
            return -1;
        }
        // PRINTF_FLUSH("1\n");
        // 得到文件或目录全名
        strcpy(m_paths->new_dir_file_fullname, path_cp);
        // printf("没有二级目录，目录或文件全名: %s\n", m_paths->new_dir_file_fullname);
        // PRINTF_FLUSH("1\n");
        // 得到文件名和扩展名
        return 0;
    }
}

/**
 * @brief 根据split_path的结果返回对应的error值
 *
 * 根据路径分割的结果和指定的操作类型，返回对应的错误值。
 *
 * @param origin_path 待检查的路径
 * @param m_paths 存储分割结果的结构体
 * @param type 操作类型，1表示创建目录，其他表示创建文件
 * @return int 操作成功返回0，文件或目录长度过长返回-ENAMETOOLONG，文件扩展名长度过长返回-ENAMETOOLONG，不能在根目录创建返回-EPERM
 */
int split_check_path_error(const char* origin_path, struct paths* m_paths, int type)
{
    int ret = split_path(origin_path, m_paths);
    printf("ret: %d\n", ret);
    // 检查长度问题
    if (-1 == ret)
    {
        PRINTF_FLUSH("文件或目录长度过长！\n");
        return -ENAMETOOLONG;
    }
    else if (-2 == ret)
    {
        PRINTF_FLUSH("文件扩展名长度过长！\n");
        return -ENAMETOOLONG;
    }
    // 创建目录
    if (1 == type)
    {
        printf("%s\n", m_paths->parent_dir);
        // 检查是否在根目录
        // 有父目录
        if (0 != strcmp(m_paths->parent_dir, "\0"))
        {
            return -EPERM;
        }

    }
    // 创建文件
    else
    {
        if (0 == strcmp(m_paths->parent_dir, "\0"))
        {
            PRINTF_FLUSH("不能创建在根目录创建文件！\n");
            return -EPERM;
        }
    }
    return 0;
}

/**
 * @brief 根据inode号或者块号(addr)重置inode位图区或者数据块位图区
 *
 * 根据指定的inode号或者块号(addr)以及模式，重置对应的inode位图区或者数据块位图区。
 *
 * @param id inode号或者块号(addr)
 * @param mode 模式，1表示重置数据块位图区，2表示重置inode位图区
 */
void recall_inode_or_datablk_id(const short id, int mode)
{
    if (mode == 1)
    {
        PRINTF_FLUSH("重置数据块位图区！\n");
    }
    else
    {
        PRINTF_FLUSH("重置inode位图区！\n");
    }
    
    FILE* file_des = get_file_singleton();
    short over_bytes;
    short over_bits;
    long off;
    // 数据块
    if (mode == 1)
    {
        short block_bit_map_id = id - m_sb.first_blk;
        // 跨越的字节数
        over_bytes = block_bit_map_id / 8;
        // 要更新的bit位
        over_bits = block_bit_map_id % 8;
        off = m_sb.first_blk_of_databitmap * BLOCK_SIZE + over_bytes;
    }
    // inode
    else
    {
        // 跨越的字节数
        over_bytes = (id - 1) / 8;
         // 要更新的bit位
        over_bits = (id - 1) % 8;
        off = m_sb.first_blk_of_inodebitmap * BLOCK_SIZE + over_bytes;
    }
    // 移动文件指针
    // PRINTF_FLUSH("文件指针目前的位置：%ld\n", ftell(file_des));
    fseek(file_des, off, SEEK_SET);
    // PRINTF_FLUSH("移动后文件指针目前的位置：%ld\n", ftell(file_des));


    unsigned char cur_byte;
    // 将指定位置置为0
    fread(&cur_byte, sizeof(char), 1, file_des);
    // PRINTF_FLUSH("原来cur_byte的值：%d\n", cur_byte);
    cur_byte &= ~(1 << over_bits);
    // PRINTF_FLUSH("更新后cur_byte的值：%d\n", cur_byte);
    // 文件指针复位
    fseek(file_des, off, SEEK_SET);
    // PRINTF_FLUSH("复位后文件指针目前的位置：%ld\n", ftell(file_des));
    // 将数据重新写入
    fwrite(&cur_byte, sizeof(char), 1, file_des);
    // PRINTF_FLUSH("写入后文件指针目前的位置：%ld\n", ftell(file_des));
    PRINTF_FLUSH("重置成功！\n");

}


/**
 * @brief 根据inode回收数据块
 *
 * 根据给定的inode，回收占用的数据块。
 *
 * @param cur_inode 待回收数据块的inode
 */
void recall_data_block(const struct inode cur_inode)
{
    PRINTF_FLUSH("开始回收数据块!\n");
    int addr_num = sizeof(cur_inode.addr) / sizeof(short);
    // PRINTF_FLUSH("addr_num: %d\n", addr_num);
    // 后面只需提供数据块的地址即可回收
    for (int i = 0; i < addr_num; ++i)
    {
        if (-1 == cur_inode.addr[i])
        {
            break;
        }
        // 直接地址
        if (i <= 3)
        {
            recall_inode_or_datablk_id(cur_inode.addr[i], 1);
            PRINTF_FLUSH("成功回收数据块!\n");
        }
    }
    PRINTF_FLUSH("没有占用数据块,不需要回收!\n");
   
}

/**
 * @brief 写回更新的inode到文件系统
 *
 * 将更新后的inode写回文件系统，以保持文件系统的一致性。
 *
 * @param cur_inode 更新后的inode
 */
void write_inode(struct inode* cur_inode)
{
    FILE* file_des = get_file_singleton();
    long off = m_sb.first_inode * BLOCK_SIZE + (cur_inode->st_ino - 1) * sizeof(struct inode);
    fseek(file_des, off, SEEK_SET);
    fwrite(cur_inode, sizeof(struct inode), 1, file_des);
    PRINTF_FLUSH("inode号为%hd的目录(父目录)的inode成功更新到文件系统！\n", cur_inode->st_ino);
}

/**
 * @brief 删除目录时更新根目录信息
 *
 * 更新父目录(根目录)的信息，包括父目录的地址数组和独占的数据块的回收操作。
 *
 * @param parent_inode 父目录(根目录)的inode
 * @param data_blk 父目录(根目录)对应的数据块
 * @param tmp_addr_i 当前目录的地址在父目录的地址数组中的索引
 * @param rm_dir_entry 被删除的目录项信息
 */
void update_parent_info(struct inode* parent_inode, struct data_block* data_blk, const int tmp_addr_i, struct dir_entry* rm_dir_entry)
{
    PRINTF_FLUSH("开始更新删除目录的父目录(根目录)的信息...\n");
    
    FILE* file_des = get_file_singleton();    
    // 更新根目录数据块记录的目录项信息(只需要更新数据块已使用大小)
    // TODO:是否要物理清空对应位置数据
    short tmp_addr = parent_inode->addr[tmp_addr_i];
    // 紧凑数据块,前面空了就将数据块最后一个目录项拿到前面空位补
    int total_dir_num = data_blk->used_size / sizeof(struct dir_entry);
    // 指向数据块中最后一个数据项的有效地址
    struct dir_entry* p = (struct dir_entry*)data_blk->data;
    // 记录当前指向的目录项的idx
    int cur_p = 0;
    while (cur_p < (total_dir_num - 1))
    {
        p++;
        ++cur_p;
    }
    // 要删除的数据块的地址指向了最后一个数据项的有效地址
    *rm_dir_entry = *p;
    // 更新数据块已经使用的大小
    data_blk->used_size -= sizeof(struct dir_entry); 
    // 写回数据块内容
    // PRINTF_FLUSH("写回数据块的块地址为: %hd, 已使用大小为: %zu\n", tmp_addr, data_blk->used_size);
    long off = tmp_addr * BLOCK_SIZE;
    fseek(file_des, off, SEEK_SET);
    fwrite(data_blk, sizeof(struct data_block), 1, file_des);
    // 更新根目录大小
    // PRINTF_FLUSH("根目录原来的大小为：%ld\n", root_inode.st_size);
    // root_inode.st_size -= sizeof(struct dir_entry);
    // PRINTF_FLUSH("删除目录后根目录的大小为：%ld\n", root_inode.st_size);
    // 更新根目录数据区地址
    if (data_blk->used_size == 0)
    {
        // 回收该独占数据块的块号
        recall_inode_or_datablk_id(tmp_addr, 1);
        parent_inode->addr[tmp_addr_i] = -1;
    }
    // 写回根目录信息
    write_inode(parent_inode);
    PRINTF_FLUSH("成功更新根目录信息!\n");
}



/**
 * 每个索引块指向的内容都属于同一个文件
 * 同一个文件分配的数据块可以不连续，只需要把块地址都放到同一个索引块中即可（按顺序）
*/ 

/**
 * @brief 获取空闲数据块或inode号
 *
 * 根据给定的模式，获取空闲的数据块或inode号。
 *
 * @param ids 存储获取到的空闲块号或inode号的数组
 * @param num 需要获取的空闲块或inode号的数量
 * @param mode 模式，1表示获取空闲数据块，2表示获取空闲inode号
 */
void get_free_data_blk(short* ids, int num, int mode)
{
    FILE* file_des = get_file_singleton();
    if (mode == 1)
    {
        PRINTF_FLUSH("申请空闲数据块\n");
        fseek(file_des, m_sb.first_blk_of_databitmap * BLOCK_SIZE, SEEK_SET);
    }
    else
    {
        PRINTF_FLUSH("申请空闲inode号\n");
        fseek(file_des, m_sb.first_blk_of_inodebitmap * BLOCK_SIZE, SEEK_SET);
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
    long off = 0;
    // 当读取字节数大于或等于BLOCK_SIZE时或者已经获得足够空闲块时，停止循环
    while (byte_count < max_byte_num && count_ids < num)
    {
        off = ftell(file_des);
        fread(&cur_byte, sizeof(unsigned char), 1, file_des);
        // 重置文件指针位置，方便后面写入
        fseek(file_des, off, SEEK_SET);
        for (int i = 0; i < 8; i++)
        {
            // 检查当前字节的每一位
            if ((cur_byte & (1 << i)) == 0) // 如果当前位是0
            {
                PRINTF_FLUSH("当前字节数：%d, 当前bit：%d\n", byte_count, i);
                // 置为1
                PRINTF_FLUSH("更新前的cur_byte的值为：%d\n", cur_byte);
                cur_byte |= (1 << i);
                PRINTF_FLUSH("更新后的cur_byte的值为：%d\n", cur_byte);
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
        // 写回文件，文件指针自动指向下一字节
        fwrite(&cur_byte, sizeof(unsigned char), 1, file_des);
        byte_count++;
    }
    if (count_ids == num)
    {
        PRINTF_FLUSH("成功申请得到和预期一致的%d个空闲块或inode号%hd\n", count_ids, *ids);
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


/**
 * @brief 根据块号读取块的数据
 *
 * 根据给定的块号，从磁盘文件中读取对应块的数据。
 *
 * @param addr 块号
 * @param data_blk 存储读取到的块数据的结构体指针
 * @return 成功读取返回0，否则返回-1
 */
int read_data_block(short addr,struct data_block* data_blk)
{
    FILE* file_des = get_file_singleton();
    if (NULL == file_des)
    {
        PRINTF_FLUSH("read_data_block中diskimg打开失败！\n");
        perror("Error");
        return -1;
    }
    PRINTF_FLUSH("正在读取数据块...\n");
    fseek(file_des, addr * BLOCK_SIZE, SEEK_SET);
    fread(data_blk, sizeof(struct data_block), 1, file_des);
    PRINTF_FLUSH("读取数据块的块地址为: %hd, 已使用大小为: %zu\n", addr, data_blk->used_size);
    return 0;
}

/**
 * @brief 根据当前地址在addr[7]中的索引和地址，得到下一个地址以及地址的索引
 *
 * 如果没有下一个有效地址，则next_addr和next_addr_idx都会被置为-1。
 *
 * @param addr 存储地址的数组
 * @param next_addr 用于存储下一个地址的指针
 * @param next_addr_idx 用于存储下一个地址的索引的指针
 * @param parent_data_blk 父级数据块
 * @param grandfather_data_blk 祖父级数据块
 */
void get_valid_addr(short addr[7], short* next_addr, short* next_addr_idx, struct data_block* parent_data_blk, struct data_block* grandfather_data_blk)
{
    short cur_addr_idx = *next_addr_idx;
    short cur_addr = *next_addr;
    for (int i = cur_addr_idx; i < 7; ++i)
    {
        // 下一个是直接地址
        if (i <= 2)
        {
            *next_addr =  addr[cur_addr_idx + 1];
            *next_addr_idx = cur_addr_idx + 1;
            return;
        }
        // 直接地址用完或读完了，下一个是一级间接地址
        else if (cur_addr_idx == 3)
        {
            // 下一个一级间接地址还没开辟
            if (-1 == addr[cur_addr_idx + 1])
            {
                *next_addr = -1;
                *next_addr_idx = cur_addr_idx + 1;
                
                return;
            }
            else
            {
                struct data_block* data_blk = malloc(sizeof(struct data_block));
                read_data_block(addr[cur_addr_idx + 1], data_blk);
                short* new_addr = (short*)data_blk->data;
                *next_addr = *new_addr;
                *next_addr_idx = cur_addr_idx + 1;
                parent_data_blk = data_blk;
                return;
            }
        }
        // 下一个是二级间接地址
        else if (cur_addr_idx == 4)
        {
            // 先在本一级地址块找现在的地址，并看看一级地址读完或者用完没有
            struct data_block* data_blk = malloc(sizeof(struct data_block)); 
            read_data_block(addr[cur_addr_idx], data_blk);
            parent_data_blk = data_blk;
            short* p = (short*)data_blk->data;
            int size = 0;
            while(size < data_blk->used_size)
            {
                if (cur_addr == *p)
                {
                    // 一级地址读完
                    if (size + sizeof(short) > data_blk->used_size)
                    {
                        // 一级地址用完了
                        if (data_blk->used_size >= BLOCK_MAX_DATA_SIZE)
                        {
                            break;
                        }
                        // 一级地址还没用完
                        else
                        {
                            *next_addr = -1;
                            *next_addr_idx = cur_addr_idx;
                        }
                    }
                    // 一级地址没读完
                    p++;
                    *next_addr = *p;
                    *next_addr_idx = cur_addr_idx;
                    return;
                }
                p++;
                size += sizeof(short);
            }
            
            // 一级间接地址没有剩余的了，找二级间接地址
            if (-1 == addr[cur_addr_idx + 1])
            {

                *next_addr = -1;
                *next_addr_idx = cur_addr_idx + 1;
                return;
            }
            else
            {
                struct data_block* data_blk = malloc(sizeof(struct data_block));
                // 拿到二级间接地址块
                read_data_block(addr[cur_addr_idx + 1], data_blk);
                grandfather_data_blk = data_blk;
                // 先拿到一个一级地址块地址
                short* new_addr = (short*)data_blk->data;

                *next_addr = *new_addr;
                *next_addr_idx = cur_addr_idx;
                return;
            }
        }

    }
}


/**
 * @brief 根据下一个地址和地址的索引更新地址数组信息
 *
 * @param addr 存储地址的数组
 * @param next_addr 用于存储下一个地址的指针
 * @param next_addr_idx 用于存储下一个地址的索引的指针
 * @param flag 操作标志，1表示只读取数据块，2表示用完数据块要重新申请数据块并更新addr数组信息
 */
void update_addr(short addr[7], short* next_addr, short* next_addr_idx, int flag)
{
    struct data_block* direct_data_blk = NULL;
    struct data_block* parent_data_blk = NULL;
    struct data_block* grandfather_data_blk = NULL;
    short cur_addr = *next_addr;
    get_valid_addr(addr, next_addr, next_addr_idx, parent_data_blk, grandfather_data_blk);
    if (flag == 1)
    {
        if (direct_data_blk != NULL)
        {
            free(direct_data_blk);
            direct_data_blk = NULL;
        }
        if (parent_data_blk != NULL)
        {
            free(parent_data_blk);
            parent_data_blk = NULL;
        }
        if (grandfather_data_blk != NULL)
        {
            free(grandfather_data_blk);
            grandfather_data_blk = NULL;
        }
        return;
    }
    // 如果parent_data_blk和grandfather_data_blk都为NULL，表示没有索引块
    if (parent_data_blk == NULL && grandfather_data_blk == NULL)
    {
        // 下一个是直接地址
        if (*next_addr_idx <= 3)
        {
            short blk_id;
            // 申请空闲数据块
            get_free_data_blk(&blk_id, 1, 1);
            // 更新addr数组
            *next_addr = blk_id;
            addr[*next_addr_idx] = blk_id;
        }
        // 下一个是一级索引
        else
        {
            short* blk_id = malloc(sizeof(short) * 2);
            // 申请空闲数据块作为数据块和索引块
            get_free_data_blk(blk_id, 2, 1);
            struct data_block* data_blk = malloc(sizeof(struct data_block));

            // 先读出索引块内容
            read_data_block(blk_id[0], data_blk);
            // 初始化
            data_blk->used_size = sizeof(short);
            // 将数据块地址写入索引块地址
            short* direct_addr = (short*)data_blk->data;
            *direct_addr = blk_id[1];
            *next_addr = *direct_addr;
            FILE* file_des = get_file_singleton();
            fseek(file_des, blk_id[0] * BLOCK_SIZE, SEEK_SET);
            fwrite(data_blk, sizeof(struct data_block), 1, file_des);
            // 更新addr数组
            addr[*next_addr_idx] = blk_id[0];
            free(blk_id);
            blk_id = NULL;
        }

    }
    // 还有一级索引
    else if (parent_data_blk != NULL && grandfather_data_blk == NULL)
    {
        int size = 0;
        short* p = (short*)parent_data_blk->data;
        while(size < parent_data_blk->used_size)
        {
            p++;
            size += sizeof(short);
        }
        if (size < BLOCK_MAX_DATA_SIZE)
        {
            get_free_data_blk(next_addr, 1, 1);
            *p = *next_addr;
            FILE* file_des = get_file_singleton();
            fwrite(parent_data_blk, sizeof(struct data_block), 1, file_des);
        }
        else
        {
            // 
        }

    }
    if (direct_data_blk != NULL)
    {
        free(direct_data_blk);
        direct_data_blk = NULL;
    }
    if (parent_data_blk != NULL)
    {
        free(parent_data_blk);
        parent_data_blk = NULL;
    }
    if (grandfather_data_blk != NULL)
    {
        free(grandfather_data_blk);
        grandfather_data_blk = NULL;
    }
}

/**
 * @brief 创建目录或文件，并更新文件系统中的相关信息。
 * 1.创建inode
 * 2.分配inode中的addr数组
 * 3.将inode放入inode数据区
 * 4.更新inode位图区
 * 5.创建dir_entry对象
 * 6.将dir_entry放入数据区，更新根目录的大小以及addr地址并更新数据位图区以及将根目录信息重新写入文件系统
 * 7.更新数据位图区
 * @param parent_inode 父目录的inode结构指针。
 * @param mode 文件模式。
 * @param type 创建的类型，1表示创建目录，2表示创建文件。
 * @param new_name 新目录或文件的名称。
 * @return 成功返回0，失败返回-1。
 */
static int create_dir_or_file_really(struct inode* parent_inode, mode_t mode, int type,const char* new_name)
{
    // 创建inode
    struct inode new_inode;
    new_inode.st_mode = 0755;
    short inode_ids;
    get_free_data_blk(&inode_ids, 1, 2);
    new_inode.st_ino = inode_ids;
    PRINTF_FLUSH("新建目录的inode值为：%hd\n", new_inode.st_ino);
    new_inode.st_nlink = 1;
    new_inode.st_uid = 0;
    new_inode.st_gid = 0;
    if (1 == type)
    {
        new_inode.st_size = sizeof(struct dir_entry);
    }
    else
    {
        new_inode.st_size = 0;
    }
    struct timespec access_time;
    clock_gettime(CLOCK_REALTIME, &access_time);
    new_inode.st_atim = access_time;
    // 分配inode中的addr数组
    for (int i = 0; i < 7; ++i) {
        new_inode.addr[i] = -1;
    }
    // 将inode放入inode数据区
    FILE* file_des = get_file_singleton();
    long off = m_sb.first_inode * BLOCK_SIZE + (new_inode.st_ino - 1) * sizeof(struct inode);
    fseek(file_des, off, SEEK_SET);
    fwrite(&new_inode, sizeof(struct inode), 1, file_des);
    // 将dir_entry放入数据区，更新根目录的大小以及addr地址并将根目录信息重新写入文件系统

    /**
     * 一个数据块只能供一个目录或文件使用
     * 但是由于只有根目录下能创建目录或文件，因此还是需要判断当前数据块是否满了
     * */ 

    // 记录地址不为-1的最大位置
    short count = -1;
    for (int i = 0; i< 7; ++i)
    {
        // PRINTF_FLUSH("i: %d\n", i);
        if (parent_inode->addr[i] == -1)
        {
            break;
        }
        else
        {
            ++count;
        }
    }
    short tmp_addr = -1;
    struct data_block* tmp_data_blk = malloc(sizeof(struct data_block));
    // 一定要初始化data部分
    // PRINTF_FLUSH("count为：%d\n", count);
    memset(tmp_data_blk->data, '\0', BLOCK_MAX_DATA_SIZE);
    tmp_data_blk->used_size = 0;
    struct dir_entry* new_dir_entry = NULL;
    
    // linux中的inode，对于目录的大小是指dir_entry的大小，不会改变
    // 更新父目录大小信息
    // parent_inode->st_size += sizeof(struct dir_entry);
    // 从来没有数据，只需新建数据块即可
    if (count == -1)
    {
        PRINTF_FLUSH("父目录从来没有数据，需新建数据块\n");
        // 为父目录申请数据块地址
        short data_blk_ids;
        get_free_data_blk(&data_blk_ids, 1, 1);
        parent_inode->addr[0] = data_blk_ids;
        off = data_blk_ids * BLOCK_SIZE;
        // 更新当前数据块已使用大小和数据信息
        new_dir_entry = (struct dir_entry*) tmp_data_blk->data;
    }
    // 原来的数据块有数据，要追加数据
    // 先读取原来的数据
    else
    {
        FILE* file_des = get_file_singleton();
        tmp_addr = parent_inode->addr[count];
        off = tmp_addr * BLOCK_SIZE;
        fseek(file_des, off, SEEK_SET);
        fread(tmp_data_blk, sizeof(struct data_block), 1, file_des);
        // 检查原来的数据块是否已经满了
        // 当前数据块已存的目录数
        int cur_dir_num = tmp_data_blk->used_size / sizeof(struct dir_entry);
        // 原来的数据块已经满了,需要新建块存放新的目录项
        if (cur_dir_num >= DIR_ENTRY_MAXNUM_PER_BLOCK)
        {
            PRINTF_FLUSH("当前有效块已满，需要新建块\n");
            // 申请可用地址addr以及更新addr数组
            // PRINTF_FLUSH("原来tmp_addr为：%hd\n", tmp_addr);
            // PRINTF_FLUSH("原来count为：%hd\n", count);
            update_addr(parent_inode->addr, &tmp_addr, &count, 2);
            // PRINTF_FLUSH("update_addr返回的地址为：%hd\n", tmp_addr);
            //  PRINTF_FLUSH("update_addr返回的count为：%hd\n", count);
            read_data_block(tmp_addr, tmp_data_blk);
            // 指向等下要把数据重新写回数据块的位置
            off = tmp_addr * BLOCK_SIZE;
            // 指向要写入目录项的位置
            new_dir_entry = (struct dir_entry*) tmp_data_blk->data;
        }
        // 原来的数据块还没满
        else
        {
            PRINTF_FLUSH("原来的数据块还没满\n");
            // 更新当前数据块已使用大小和数据信息
            new_dir_entry = (struct dir_entry*) tmp_data_blk->data;
            // 移到要追加的位置

            while (cur_dir_num > 0)
            {
                new_dir_entry++;
                --cur_dir_num;
            }
        }
    
    }
    // PRINTF_FLUSH("跳出来了\n");
    // 目录
    if (type == 1)
    {
        strcpy(new_dir_entry->file_name, new_name);
        PRINTF_FLUSH("新目录名为：%s\n", new_name);
        PRINTF_FLUSH("写入的新目录名为：%s\n", new_dir_entry->file_name);
    }
    // 文件
    else
    {
        SplitFileNameAndExtension(new_name, new_dir_entry->file_name, new_dir_entry->extension);
        PRINTF_FLUSH("新文件全名为：%s\n", new_name);
        PRINTF_FLUSH("写入的新文件名为：%s\n", new_dir_entry->file_name);
        PRINTF_FLUSH("extension内容以及长度：%s, %zu\n", new_dir_entry->extension, strlen(new_dir_entry->extension));
    }
    new_dir_entry->inode_id = new_inode.st_ino;
    tmp_data_blk->used_size += sizeof(struct dir_entry);
    // 将数据块信息写入文件
    file_des = get_file_singleton();
    fseek(file_des, off, SEEK_SET);
    fwrite(tmp_data_blk, sizeof(struct data_block), 1, file_des);
    PRINTF_FLUSH("目录项写入数据区成功！\n");
    // 将父目录信息重新写入文件系统
    write_inode(parent_inode);
    free(tmp_data_blk);
    return 0;
}


/**
 * @brief 读取超级块的内容。
 */
void get_sb_info() 
{
    // PRINTF_FLUSH("get_sb_info begin\n");
    FILE* file_des = get_file_singleton();
    fread(&m_sb, sizeof(struct super_block), 1, file_des);
    // PRINTF_FLUSH("fs_size: %ld\nfirst_blk: %ld\ndatasize: %ld\nfirst_node: %ld\n", m_sb.fs_size, m_sb.first_blk, m_sb.datasize, m_sb.first_inode);
    // PRINTF_FLUSH("get_sb_info end\n");
}


/**
 * @brief 将根目录inode加载到内存。
 * 
 * @param tmp_node 用于存储根目录inode的结构指针。
 * @return 成功返回0，失败返回-1。
 */
int get_root_inode(struct inode* tmp_node)
{
    // PRINTF_FLUSH("get_root_inode begin\n");
    FILE* file_des = get_file_singleton();
    if (file_des == NULL) {
        PRINTF_FLUSH("get_root_inode中diskimg打开失败\n");
        PRINTF_FLUSH("get_root_inode end\n");
        return -1;
    }
    else
    {
        // fseek(file_des, m_sb.first_inode * BLOCK_SIZE + (root_inode.inode_id- 1) * sizeof(struct inode), SEEK_SET);
        // struct inode tmp_node;
        // fread(&tmp_node, sizeof(struct inode), 1, file_des);
        // PRINTF_FLUSH("%hd, %hd\n", tmp_node.st_ino, tmp_node.addr[0]);
        fseek(file_des, m_sb.first_inode * BLOCK_SIZE, SEEK_SET);
        fread(tmp_node, sizeof(struct inode), 1, file_des);
        // PRINTF_FLUSH("get_root_inode:\n tmp_node->st_ino: %hd,  tmp_node->addr: %hd\n", tmp_node->st_ino, tmp_node->addr[0]);
        // PRINTF_FLUSH("get_root_inode end\n");
        PRINTF_FLUSH("成功加载根目录indoe!\n");
        return 0;
    }
    PRINTF_FLUSH("成功加载根目录indoe!\n");
    
}

/**
 * @brief 根据存放了目录项的数据块提取出所有文件名或者目录名，返回总的文件名或目录名个数。
 * 
 * @param file_fullnames 用于存储文件名或目录名的字符串数组。
 * @param data_blk 包含目录项的数据块。
 * @return 成功返回总的文件名或目录名个数，失败返回-1。
 */
int getFileFullNameByDataBlock(char* file_fullnames, struct data_block* data_blk)
{
    FILE* file_des = get_file_singleton();
    int count = 0;
    // PRINTF_FLUSH("getFileFullNameByDataBlock: %d\n", count);
    if (NULL == file_des)
    {
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
        // PRINTF_FLUSH("数据块中第%d个目录名：%s\n", i + 1, tmp_dir_entry->file_name);
        // PRINTF_FLUSH("保存到file_fullnames中的第%d个目录名：%s\n", i + 1, file_fullnames);
        i++;
        file_fullnames += MAX_FILE_FULLNAME_LENGTH + 2;
        tmp_dir_entry++;
        pos += sizeof(struct dir_entry);
    }
    return i;
}

/**
 * @brief 根据父目录的inode得到目标文件或目录的inode。
 * 
 * @param dir_file_name 目标文件或目录的名称。
 * @param parent_inode 父目录的inode结构指针。
 * @param target_inode 用于存储目标文件或目录的inode结构指针。
 * @return 成功返回0，目标不存在返回-ENOENT，读取数据块失败返回-1。
 */
int get_target_by_parent_inode(const char* dir_file_name, struct inode* parent_inode, struct inode* target_inode)
{
    int addr_num = sizeof(parent_inode->addr) / sizeof(parent_inode->addr[0]);
    // PRINTF_FLUSH("addr_num: %d\n", addr_num);
    short cur_addr_idx = -1;
    short cur_addr = -1;
    // 获取父目录inodeaddr数组的第一个有效直接地址
    update_addr(parent_inode->addr, &cur_addr, &cur_addr_idx, 1);
    int count = 0;
    struct data_block* data_blk = malloc(sizeof(struct data_block));
    // 循环获取有效直接地址
    while (-1 != cur_addr)
    {
        // 读取数据块数据
        int ret = read_data_block(cur_addr, data_blk);
        if (ret < 0)
        {
            free(data_blk);
            data_blk = NULL;
            return -1;
        }
        // PRINTF_FLUSH("成功读取数据块内容\n");
        // 得到数据块中存储的dir_entry的个数
        size_t dir_num = data_blk->used_size / sizeof(struct dir_entry);
        // PRINTF_FLUSH("used_size：%zu\n", data_blk->used_size);
        // PRINTF_FLUSH("数据块存储的目录项个数为%zu\n", dir_num);
        int count = 0;
        int pos = 0;
        struct dir_entry* tmp_dir_entry = (struct dir_entry*) data_blk->data;
        // 读取数据块的目录项内容
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
                short target_inode_id = tmp_dir_entry->inode_id;
                FILE* file_des = get_file_singleton();
                long off = m_sb.first_inode * BLOCK_SIZE + (target_inode_id - 1) * sizeof(struct inode);
                fseek(file_des, off, SEEK_SET);
                fread(target_inode, sizeof(struct inode), 1, file_des);
                free(data_blk);
                data_blk = NULL;
                return 0;

            }
            tmp_dir_entry++;
            pos += sizeof(struct dir_entry);
        }
        // 获取下一个直接地址
        update_addr(parent_inode->addr, &cur_addr, &cur_addr_idx, 1);
    }
    free(data_blk);
    data_blk = NULL;
    return -ENOENT;
}


/**
 * @brief 在父目录下创建文件。
 * 
 * @param parent_dir 父目录的名称。
 * @param new_filename 新文件的名称。
 * @param data_blk 包含目录项的数据块。
 * @param type 创建的类型，1表示创建目录，2表示创建文件。
 * @param mode 文件模式。
 * @return 成功返回0，失败返回-1。
 */
int create_file_under_pardir(const char* parent_dir,const char* new_filename, struct data_block* data_blk, int type, mode_t mode)
{
    // PRINTF_FLUSH("在父目录下创建文件......\n");
    // 得到数据块中存储的dir_entry的个数
    int dir_num = data_blk->used_size / sizeof(struct dir_entry);
    // PRINTF_FLUSH("数据块存储的目录项个数为%d\n", dir_num);
    struct dir_entry* parent_dir_entry = (struct dir_entry*)data_blk->data;
    while(0 != strcmp(parent_dir_entry->file_name, parent_dir))
    {
        parent_dir_entry++;
    }
    short parent_inode_id = parent_dir_entry->inode_id;
    // 在父目录下创建文件
    // 先得到父目录的inode
    FILE* file_des = get_file_singleton();
    long off = m_sb.first_inode * BLOCK_SIZE + (parent_inode_id - 1) * sizeof(struct inode);
    fseek(file_des, off, SEEK_SET);
    struct inode parent_inode;
    fread(&parent_inode, sizeof(struct inode), 1, file_des);
    // 检查是否有重名的文件
    return create_dir_or_file_really(&parent_inode, mode, type, new_filename);
}

/**
 * @brief 根据祖父inode得到目标inode。
 * 
 * @param parent_dir 父目录的名称。
 * @param dir_file_name 目标文件或目录的名称。
 * @param par_parent_inode 祖父目录的inode结构指针。
 * @param target_inode 用于存储目标文件或目录的inode结构指针。
 * @return 成功返回0，目标不存在返回-ENOENT，读取数据块失败返回-1。
 */
int get_target_by_granpa_inode(const char* parent_dir, const char* dir_file_name, struct inode* par_parent_inode, struct inode* target_inode)
{
    PRINTF_FLUSH("第一次查找父目录：%s\n", parent_dir);
    struct inode* parent_inode = (struct inode*)malloc(sizeof(struct inode));
    int ret = get_target_by_parent_inode(parent_dir, par_parent_inode, parent_inode);
    if (ret != 0)
    {
        free(parent_inode);
        parent_inode = NULL;
        return ret;
    }
    PRINTF_FLUSH("第二次查找文件：%s\n", dir_file_name);
    ret = get_target_by_parent_inode(dir_file_name, parent_inode, target_inode);

    if (ret != 0)
    {
        free(parent_inode);
        parent_inode = NULL;
        return ret;
    }
    free(parent_inode);
    parent_inode = NULL;
    return 0;
}

/**
 * @brief 创建文件或目录。
 * 
 * @param path 要创建的文件或目录的路径。
 * @param mode 文件或目录的权限模式。
 * @param type 创建的类型，1表示创建目录，2表示创建文件。
 * @return 成功返回0，目标已存在返回-EEXIST，其他错误返回相应错误码。
 */
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
    
    struct paths m_paths;
    char* p = (char*)&m_paths;
    memset(p, '\0', sizeof(struct paths));
    int ret = split_check_path_error(path, &m_paths, type);
    // PRINTF_FLUSH("路径解析结果：%d\n", ret);
    if (0 != ret)
    {
        return ret;
    }
    
    struct inode* target_inode = (struct inode*)malloc(sizeof(struct inode));
    // 如果是创建目录，检查根目录下是否有重名目录
    if (type == 1)
    {

        ret = get_target_by_parent_inode(m_paths.new_dir_file_fullname, &root_inode, target_inode);
        // PRINTF_FLUSH("查找目录结果：%d\n", ret);
        // 目录已存在
        if (0 == ret)
        {
            free(target_inode);
            target_inode = NULL;
            return -EEXIST;
        }
        // 其他错误
        else if (ret != -ENOENT)
        {
            // PRINTF_FLUSH("其他错误！%d!=%d\n", ret, -EPERM);
            free(target_inode);
            target_inode = NULL;
            return ret;
        }
        // PRINTF_FLUSH("准备创建目录\n");
        // 如果是创建目录，说明可以创建，在根目录下创建
        free(target_inode);
        target_inode = NULL;
        return create_dir_or_file_really(&root_inode, mode, 1, m_paths.new_dir_file_fullname);
    }
    // 如果是创建文件，检查根目录下是否有父目录
    else
    {
        struct inode* parent_inode = (struct inode*)malloc(sizeof(struct inode));
        ret = get_target_by_parent_inode(m_paths.parent_dir, &root_inode, parent_inode);
        // 父目录不存在或有其他错误
        if (0 != ret)
        {
            free(target_inode);
            target_inode = NULL;
            free(parent_inode);
            parent_inode = NULL;
            return ret;
        }
        // 父目录存在，检查文件是否存在
        ret = get_target_by_parent_inode(m_paths.new_dir_file_fullname, parent_inode, target_inode);
        if (ret !=0 && ret != -ENOENT)
        {
            free(target_inode);
            target_inode = NULL;
            free(parent_inode);
            parent_inode = NULL;
            return ret;
        }
        if (ret == 0)
        {
            free(target_inode);
            target_inode = NULL;
            free(parent_inode);
            parent_inode = NULL;
            return -EEXIST;
        }
        // PRINTF_FLUSH("准备创建文件\n");
        // 如果是创建文件，说明可以创建，在父目录下创建
        // PRINTF_FLUSH("父目录inode为：%hd\n", parent_inode->st_ino);
        free(target_inode);
        target_inode = NULL;
        int ret = create_dir_or_file_really(parent_inode, mode, 2, m_paths.new_dir_file_fullname);
        free(parent_inode);
        parent_inode = NULL;
        return ret;
    }

}

/**
 * @brief 删除文件或目录的辅助函数。
 * 
 * @param dir_file_name 要删除的文件或目录的名称。
 * @param parent_inode 父目录的inode结构指针。
 * @param type 删除的类型，1表示删除目录，2表示删除文件。
 * @return 成功返回0，目标不存在返回-ENOENT，目录非空返回-ENOTEMPTY，不是目录返回-ENOTDIR，其他错误返回相应错误码。
 */
int remove_help(const char *dir_file_name, struct inode* parent_inode, int type)
{
    int addr_num = sizeof(parent_inode->addr) / sizeof(parent_inode->addr[0]);
    // PRINTF_FLUSH("addr_num: %d\n", addr_num);
    short cur_addr_idx = -1;
    short cur_addr = -1;
    update_addr(parent_inode->addr, &cur_addr, &cur_addr_idx, 1);
    int count = 0;
    struct data_block* data_blk = malloc(sizeof(struct data_block));
    while (-1 != cur_addr)
    {
        // 申请数据块内存

        // 读取数据块数据
        int ret = read_data_block(cur_addr, data_blk);
        if (ret < 0)
        {
            free(data_blk);
            data_blk = NULL;
            return -1;
        }
        // PRINTF_FLUSH("成功读取数据块内容\n");
        // 得到数据块中存储的dir_entry的个数
        size_t dir_num = data_blk->used_size / sizeof(struct dir_entry);
        // PRINTF_FLUSH("used_size：%zu\n", data_blk->used_size);
        // PRINTF_FLUSH("数据块存储的目录项个数为%zu\n", dir_num);
        int count = 0;
        int pos = 0;
        struct dir_entry* tmp_dir_entry = (struct dir_entry*) data_blk->data;
        // 记录要删除的目录是该数据块的目录项index
        while (pos < data_blk->used_size)
        {
            char full_name[MAX_FILE_FULLNAME_LENGTH + 2];
            strcpy(full_name, tmp_dir_entry->file_name);
            // TODO:先按照linux默认使用touch创建文件时，目录名和文件全名不可以相同的设定进行处理
            // 如果先创建目录，再使用touch创建重名文件名，则创建文件失败，只会更新mkdir的访问时间
            // 而先使用touch创建文件名，再创建重名目录，会显示文件已存在

            // 由于不会存在两个重名的目录和文件
            // 因此，只需要在删除目录时检查file_name是否一致
            // 在删除文件时检查file_name+extension是否一致
            // 即可

            // 删除的时候不抹除inode区和数据区的内容，只是简单的将inode和数据块的位图置为0
            // 删除目录时,检查要删除的是不是目录(根据是否有后缀名判断实际类型)
            if (0 != strlen(tmp_dir_entry->extension))
            {
                if (1 == type)
                {
                    free(data_blk);
                    data_blk = NULL;
                    return -ENOTDIR;
                }
                strcat(full_name, ".");
                strcat(full_name, tmp_dir_entry->extension);    
            }
            if (0 == strcmp(dir_file_name, full_name))
            {
                // PRINTF_FLUSH("查找到相同的目录或文件名！\n");
                short target_inode_id = tmp_dir_entry->inode_id;
                FILE* file_des = get_file_singleton();
                long off = m_sb.first_inode * BLOCK_SIZE + (target_inode_id - 1) * sizeof(struct inode);
                fseek(file_des, off, SEEK_SET);
                struct inode tmp_inode;
                fread(&tmp_inode, sizeof(struct inode), 1, file_des);
                // 判断是否是空目录
                // PRINTF_FLUSH("tmp_inode: %hd\ntmp_inode.addr[0]: %hd\n", tmp_inode.st_ino, tmp_inode.addr[0]);
                if (type == 1 && -1 != tmp_inode.addr[0])
                {
                    free(data_blk);
                    data_blk = NULL;
                    return -ENOTEMPTY;
                }
                // 更新父目录(根目录)信息
                // 1.父目录大小
                // 2.父目录addr地址数组
                // 3.如果当前目录独占一个父目录的数据块,则需要对这个数据块进行回收操作
                update_parent_info(parent_inode, data_blk, cur_addr_idx, tmp_dir_entry);
                // 回收inode
                recall_inode_or_datablk_id(tmp_inode.st_ino, 2);
                // 回收要删除的目录自己的数据块
                recall_data_block(tmp_inode);
                free(data_blk);
                data_blk = NULL;
                PRINTF_FLUSH("成功删除目录或文件!\n");
                return 0;
            }
            tmp_dir_entry++;
            pos += sizeof(struct dir_entry);
        }
        update_addr(parent_inode->addr, &cur_addr, &cur_addr_idx, 1);
    }
    free(data_blk);
    data_blk = NULL;
    return -ENOENT;
}

/**
 * @brief 删除文件或目录的接口。
 * 
 * @param path 要删除的文件或目录的路径。
 * @param type 删除的类型，1表示删除目录，2表示删除文件。
 * @return 成功返回0，目标不存在返回-ENOENT，目录非空返回-ENOTEMPTY，不是目录返回-ENOTDIR，其他错误返回相应错误码。
 */
static int remove_dir_or_file (const char *path, int type)
{
    if (type == 1)
    {
        PRINTF_FLUSH("删除目录, path:%s\n", path);
    }
    else
    {
        PRINTF_FLUSH("删除文件, path:%s\n", path);
    }
    if (strcmp("/", path) == 0) 
    {
        printf("无法删除根目录！\n");
        return -1;
    }
    else {
        struct paths m_paths;
        char* p = (char*)&m_paths;
        memset(p, '\0', sizeof(struct paths));
        int ret = split_check_path_error(path, &m_paths, type);
        // PRINTF_FLUSH("路径解析结果：%d\n", ret);
        if (0 != ret)
        {
            return ret;
        }
        struct inode* parent_inode = (struct inode*)malloc(sizeof(struct inode));
        if (1 == type)
        {
            parent_inode = &root_inode;
        }
        else
        { 
            ret = get_target_by_parent_inode(m_paths.parent_dir, &root_inode, parent_inode);
            if (ret != 0)
            {
                free(parent_inode);
                parent_inode = NULL;
                return ret;
            }
        }
        // PRINTF_FLUSH("通过父目录检查\n");
        // 父目录有效，开始将目标从父目录删除
        ret = remove_help(m_paths.new_dir_file_fullname, parent_inode, type);
        if (&root_inode != parent_inode)
        {
            free(parent_inode);
            parent_inode = NULL;
        }

        return ret;
   
    }

}

/**
 * @brief 根据父目录inode，将目录的所有目录项名装填进buf。
 * 
 * @param par_parent_inode 父目录的inode。
 * @param filler FUSE提供的回调函数，用于将文件或目录名填充到文件系统。
 * @param buf 用于填充文件或目录名的缓冲区。
 * @return 成功返回0，失败返回相应错误码。
 */
int fill_fullname_by_parent_inode(struct inode* par_parent_inode, fuse_fill_dir_t* filler, void *buf)
{
    int addr_num = sizeof(par_parent_inode->addr) / sizeof(par_parent_inode->addr[0]);
    // PRINTF_FLUSH("addr_num: %d\n", addr_num);
    short cur_addr_idx = -1;
    short cur_addr = -1;
    update_addr(par_parent_inode->addr, &cur_addr, &cur_addr_idx, 1);
    struct data_block* data_blk = malloc(sizeof(struct data_block));

    int count = 0;
    while (-1 != cur_addr)
    {
        // 申请数据块内存

        // 读取数据块数据
        int ret = read_data_block(cur_addr, data_blk);
        if (ret < 0)
        {
            free(data_blk);
            data_blk = NULL;
            return -1;
        }
        // PRINTF_FLUSH("成功读取数据块内容\n");
        // 得到数据块中存储的dir_entry的个数
        int dir_num = data_blk->used_size / sizeof(struct dir_entry);
        // PRINTF_FLUSH("数据块存储的目录项个数为%d\n", dir_num);
        // 申请文件全名内存
        char* file_fullnames = malloc(sizeof(char) * dir_num * (MAX_FILE_FULLNAME_LENGTH + 2));
        // 得到当前数据块所有文件全名
        int file_fullname_num = getFileFullNameByDataBlock(file_fullnames, data_blk);
        if (file_fullname_num < 0)
        {
            free(data_blk);
            data_blk = NULL;
            free(file_fullnames);
            file_fullnames = NULL;
            return -1;
        }
        // PRINTF_FLUSH("数据块存储的文件名个数为%d\n", dir_num);
        // PRINTF_FLUSH("以下是数据块中的文件名：\n");
        char* p = file_fullnames;
        for (int i = 0; i < file_fullname_num; ++i)
        {
            char fullname[MAX_FILE_FULLNAME_LENGTH + 2];
            if (file_fullnames != NULL)
            {
                strcpy(fullname, file_fullnames);
                PRINTF_FLUSH("%s\n", fullname);
                (*filler)(buf, fullname, NULL, 0, 0);
            }
            else
            {
                // 处理无效的文件全名
                PRINTF_FLUSH("%d 无效\n", i);
            }
            file_fullnames += MAX_FILE_FULLNAME_LENGTH + 2;
        }
        update_addr(par_parent_inode->addr, &cur_addr, &cur_addr_idx, 1);
        free(p);
        p = NULL;
    }

    PRINTF_FLUSH("装填filler完毕\n");
    free(data_blk);
    data_blk = NULL;

}
