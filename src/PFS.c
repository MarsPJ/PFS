#include"util.h"


/**
 * @brief 获取文件或目录的基本属性
 * 基本思路参照提交的课程设计报告书
 *
 * @param path 文件或目录的路径
 * @param stbuf 存储文件或目录属性的结构体指针
 * @param fi 文件信息，包含文件打开的信息
 * 
 * @return 成功时返回0，错误时返回相应的负值（用于FUSE文件系统）
 */
static int pzj_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi)
{
    PRINTF_FLUSH("pzj_getattr begin\n");
    PRINTF_FLUSH("getattr	 path : %s \n", path);
    struct inode* targe_inode = NULL;
    struct paths m_paths;
    if (strcmp("/", path) == 0) 
    {
        targe_inode = &root_inode;
        stbuf->st_mode = S_IFDIR;
    }
    else 
    {
        targe_inode = (struct inode*)malloc(sizeof(struct inode));
        char* p = (char*)&m_paths;
        memset(p, '\0', sizeof(struct paths));
        int ret = split_path(path, &m_paths);
        // printf("ret: %d\n", ret);
        // 检查长度问题
        if (-1 == ret)
        {
            PRINTF_FLUSH("文件或目录长度过长！\n");
            free(targe_inode);
            targe_inode = NULL;
            return -ENAMETOOLONG;
        }
        else if (-2 == ret)
        {
            PRINTF_FLUSH("文件扩展名长度过长！\n");
            free(targe_inode);
            targe_inode = NULL;
            return -ENAMETOOLONG;
        }
        
        PRINTF_FLUSH("请求的文件或目录名：%s\n", m_paths.new_dir_file_fullname);
        
        if (0 == strcmp(m_paths.parent_dir, "\0"))
        {
            stbuf->st_mode = S_IFDIR;
            int ret = get_target_by_parent_inode(m_paths.new_dir_file_fullname, &root_inode, targe_inode);
            if (ret != 0)
            {
                free(targe_inode);
                targe_inode = NULL;
                return ret;
            }
        }
        else
        {
            stbuf->st_mode = S_IFREG;
            int ret = get_target_by_granpa_inode(m_paths.parent_dir, m_paths.new_dir_file_fullname, &root_inode, targe_inode);
            if (ret != 0)
            {
                free(targe_inode);
                targe_inode = NULL;
                return ret;
            }
        }
    }
    
    stbuf->st_ino = targe_inode->st_ino;
    stbuf->st_mode = targe_inode->st_mode | stbuf->st_mode;
    stbuf->st_nlink = targe_inode->st_nlink;
    stbuf->st_uid = targe_inode->st_uid;
    stbuf->st_gid = targe_inode->st_gid;
    stbuf->st_size = targe_inode->st_size;
    stbuf->st_atime = targe_inode->st_atim.tv_sec;
    stbuf->st_blocks = m_sb.first_inode + targe_inode->st_ino - 1;
    PRINTF_FLUSH("成功找到%s\n", m_paths.new_dir_file_fullname);

    // if (1) {
    //     // 提取文件类型
    //     if (S_ISREG(stbuf->st_mode)) {
    //         printf("Regular file\n");
    //     } else if (S_ISDIR(stbuf->st_mode)) {
    //         printf("Directory\n");
    //     } else if (S_ISLNK(stbuf->st_mode)) {
    //         printf("Symbolic link\n");
    //     } else {
    //         printf("Other type\n");
    //     }

    //     // 提取文件权限
    //     mode_t file_permissions = stbuf->st_mode & (S_IRWXU | S_IRWXG | S_IRWXO);
    //     printf("File permissions: %o\n", file_permissions);
    // } else {
    //     perror("stat");
    // }
    if (&root_inode != targe_inode)
    {
        free(targe_inode);
        targe_inode = NULL;
    }

    return 0;
}

/**
 * @brief 读取目录信息
 * 基本思路参照提交的课程设计报告书
 *
 * @param path 目录的路径
 * @param buf 用于填充目录信息的缓冲区
 * @param filler 用于向缓冲区填充目录项的回调函数
 * @param offset 在目录流中的偏移量
 * @param fi 文件信息，包含文件打开的信息
 * @param flags 读取目录的标志位
 * 
 * @return 成功时返回0，错误时返回相应的负值（用于FUSE文件系统）
 */
int pzj_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags)
{
    PRINTF_FLUSH( "pzj_readdir	 path : %s \n", path);
    // 所有目录都有的内容
    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);
    // 这是根目录
    if(strcmp(path, "/") == 0)
    {   
        return fill_fullname_by_parent_inode(&root_inode, &filler, buf);
    }
    else
    {
        struct paths m_paths;
        char* p = (char*)&m_paths;
        memset(p, '\0', sizeof(struct paths));
        int ret = split_check_path_error(path, &m_paths, 1);
        // PRINTF_FLUSH("路径解析结果：%d\n", ret);
        if (0 != ret)
        {
            return ret;
        }
        struct inode* target_inode = (struct inode*)malloc(sizeof(struct inode));
        ret = get_target_by_parent_inode(m_paths.new_dir_file_fullname, &root_inode, target_inode);
        // PRINTF_FLUSH("父目录查找结果：%d\n", ret);
        if (ret != 0)
        {
            free(target_inode);
            target_inode = NULL;
            return ret;
        }
        ret = fill_fullname_by_parent_inode(target_inode, &filler, buf);
        // PRINTF_FLUSH("当前目录装填结果：%d\n", ret);
        free(target_inode);
        target_inode = NULL;
        return ret;
    }
}


/**
 * @brief 创建目录
 * 基本思路参照提交的课程设计报告书
 *
 * @param path 目录的路径
 * @param mode 创建目录时使用的权限模式
 * 
 * @return 成功时返回0，错误时返回相应的负值（用于FUSE文件系统）
 */
static int pzj_mkdir (const char *path, mode_t mode)
{
    return create_dir_or_file(path, mode, 1);
}

/**
 * @brief 创建文件
 * 基本思路参照提交的课程设计报告书
 *
 * @param path 文件的路径
 * @param mode 创建文件时使用的权限模式
 * @param dev 设备号
 * 
 * @return 成功时返回0，错误时返回相应的负值（用于FUSE文件系统）
 */
static int pzj_mknod (const char *path, mode_t mode, dev_t dev)
{
    dev=0;
    return create_dir_or_file(path, mode, 2);
}

/**
 * @brief 更新文件或目录的访问时间
 * 基本思路参照提交的课程设计报告书。
 *
 * @param path 文件或目录的路径
 * @param tv   包含要设置的访问时间和修改时间的 timespec 数组
 * @param fi   FUSE 文件信息结构指针，用于访问文件的有关信息
 *
 * @return 成功时返回0，错误时返回相应的负值（用于FUSE文件系统）
 */
static int pzj_utimens(const char *path, const struct timespec tv[2], struct fuse_file_info *fi) 
{   
    PRINTF_FLUSH("pzj_utimens begin\n");
    PRINTF_FLUSH("pzj_utimens	 path : %s \n", path);
    struct inode* targe_inode = NULL;
    struct paths m_paths;
    if (strcmp("/", path) == 0) {
        targe_inode = &root_inode;
    }
    else 
    {
        targe_inode = (struct inode*)malloc(sizeof(struct inode));
        char* p = (char*)&m_paths;
        memset(p, '\0', sizeof(struct paths));
        int ret = split_path(path, &m_paths);
        // printf("ret: %d\n", ret);
        // 检查长度问题
        if (-1 == ret)
        {
            PRINTF_FLUSH("新建的文件或目录长度过长！\n");
            free(targe_inode);
            targe_inode = NULL;
            return -ENAMETOOLONG;
        }
        else if (-2 == ret)
        {
            PRINTF_FLUSH("新建的文件扩展名长度过长！\n");
            free(targe_inode);
            targe_inode = NULL;
            return -ENAMETOOLONG;
        }
        
        PRINTF_FLUSH("请求的文件或目录名：%s\n", m_paths.new_dir_file_fullname);
        
        if (0 == strcmp(m_paths.parent_dir, "\0"))
        {
            int ret = get_target_by_parent_inode(m_paths.new_dir_file_fullname, &root_inode, targe_inode);
            if (ret != 0)
            {
                free(targe_inode);
                targe_inode = NULL;
                return ret;
            }
        }
        else
        {
            int ret = get_target_by_granpa_inode(m_paths.parent_dir, m_paths.new_dir_file_fullname, &root_inode, targe_inode);
            if (ret != 0)
            {
                free(targe_inode);
                targe_inode = NULL;
                return ret;
            }
        }
    }
    targe_inode->st_atim = tv[0];
    return 0; 
}

/**
 * @brief 删除目录
 * 基本思路参照提交的课程设计报告书。
 *
 * @param path 要删除的目录的路径
 *
 * @return 成功时返回0，错误时返回相应的负值（用于FUSE文件系统）
 */

static int pzj_rmdir (const char *path)
{
    return remove_dir_or_file(path, 1);
}

/**
 * @brief 删除文件
 * 基本思路参照提交的课程设计报告书。
 *
 * @param path 要删除的文件的路径
 *
 * @return 成功时返回0，错误时返回相应的负值（用于FUSE文件系统）
 */
static int pzj_unlink (const char *path)
{
    return remove_dir_or_file(path, 2);
}

/**
 * @brief 读取文件内容
 * 基本思路参照提交的课程设计报告书。
 *
 * @param path 文件的路径
 * @param buf  用于存储读取内容的缓冲区
 * @param size 用户要读取的数据大小
 * @param offset 文件偏移指针，指示从文件的哪个位置开始读取
 * @param fi   FUSE 文件信息结构指针，用于访问文件的有关信息
 *
 * @return 成功返回读取的字节数，错误时返回相应的负值（用于FUSE文件系统）
 */
static int pzj_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    PRINTF_FLUSH("pzj_read! size: %lu, offset: %ld\n", size, offset);
    struct paths m_paths;
    char* p = (char*)&m_paths;
    memset(p, '\0', sizeof(struct paths));
    int ret = split_check_path_error(path, &m_paths, 2);
    PRINTF_FLUSH("路径解析结果：%d\n", ret);
    if (-ENAMETOOLONG == ret)
    {
        return ret;
    }
    else if (-EPERM == ret )
    {
        return -EISDIR;
    }


    struct inode* target_inode = (struct inode*)malloc(sizeof(struct inode));
    ret = get_target_by_granpa_inode(m_paths.parent_dir, m_paths.new_dir_file_fullname, &root_inode, target_inode);
    PRINTF_FLUSH("查找目录结果：%d\n", ret);
    if (ret !=0 )
    {
        free(target_inode);
        target_inode = NULL;
        return ret;
    }
    // TODO:判断是不是目录
    // 拿到文件inode
    // 检查文件指针是否超出文件大小范围
    off_t file_size = target_inode->st_size;
    if (file_size == 0)
    {
        free(target_inode);
        target_inode = NULL;
        return 0;
    }
    if (offset >= file_size)
    {
        PRINTF_FLUSH("读入位置或大小超出文件大小！\n");
        free(target_inode);
        target_inode = NULL;
        return -EFBIG;
    }
    else
    {
        if (offset + size > file_size) size = file_size - offset;
    }

    int addr_num = sizeof(target_inode->addr) / sizeof(target_inode->addr[0]);
    PRINTF_FLUSH("addr_num: %d\n", addr_num);
    short cur_addr_idx = -1;
    short cur_addr = -1;
    update_addr(target_inode->addr, &cur_addr, &cur_addr_idx, 1);
    size_t remain_size = size;
    while (-1 != cur_addr)
    {

        size_t read_size = BLOCK_SIZE < remain_size ? BLOCK_SIZE : remain_size;
        PRINTF_FLUSH("cur_addr: %hd, read_size: %ld\n", cur_addr, read_size);
        FILE* file_des = get_file_singleton();
        fseek(file_des, cur_addr * BLOCK_SIZE, SEEK_SET);
        fread(buf, 1, read_size, file_des);

        remain_size -= read_size;
        read_size = BLOCK_SIZE < remain_size ? BLOCK_SIZE : remain_size;

        update_addr(target_inode->addr, &cur_addr, &cur_addr_idx, 1);
    }
    PRINTF_FLUSH("%s\n", buf);
    PRINTF_FLUSH("成功读出了%ld个字节到文件%s中!\n", size, m_paths.new_dir_file_fullname);
    free(target_inode);
    target_inode = NULL;
    return size;
}

/**
 * @brief 写入文件内容
 * 基本思路参照提交的课程设计报告书。
 *
 * @param path    文件的路径
 * @param buf     包含要写入文件的数据的缓冲区
 * @param size    要写入的数据大小
 * @param offset  文件偏移指针，指示从文件的哪个位置开始写入
 * @param fi      FUSE 文件信息结构指针，用于访问文件的有关信息
 *
 * @return 成功返回写入的字节数，错误时返回相应的负值（用于FUSE文件系统）
 */
static int pzj_write (const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    PRINTF_FLUSH("pzj_write %s\n", path);
    PRINTF_FLUSH("data: %s\n", buf);
    struct paths m_paths;
    char* p = (char*)&m_paths;
    memset(p, '\0', sizeof(struct paths));
    int ret = split_check_path_error(path, &m_paths, 2);
    PRINTF_FLUSH("路径解析结果：%d\n", ret);
    if (-ENAMETOOLONG == ret)
    {
        return ret;
    }
    else if (-EPERM == ret )
    {
        return -EISDIR;
    }

    struct inode* target_inode = (struct inode*)malloc(sizeof(struct inode));
    ret = get_target_by_granpa_inode(m_paths.parent_dir, m_paths.new_dir_file_fullname, &root_inode, target_inode);
    PRINTF_FLUSH("查找目录结果：%d\n", ret);
    if (ret !=0 )
    {
        free(target_inode);
        target_inode = NULL;
        return ret;
    }
    // 拿到文件inode
    // 检查文件指针是否超出文件大小范围
    off_t file_size = target_inode->st_size;
    if (offset > file_size)
    {
        PRINTF_FLUSH("写入位置超出文件大小！\n");
        free(target_inode);
        target_inode = NULL;
        return -EFBIG;
    }
    // 如果文件大小为0,只需要直接创建数据块即可(由于offset<=file_size,因此此时offset也为0)
    PRINTF_FLUSH("开始写入文件!\n");
    if (1)
    {
        short data_blk_num = ceil((double)size / (double)BLOCK_SIZE);
        short* data_blk_id = malloc(data_blk_num * sizeof(short));
        get_free_data_blk(data_blk_id, data_blk_num, 1);
        size_t remain_size = size;
        size_t write_size = BLOCK_SIZE < remain_size ? BLOCK_SIZE : remain_size;
        FILE* file_des = get_file_singleton();
        for (int i = 0; i < data_blk_num; ++i)
        {
            target_inode->addr[i] = data_blk_id[i];
            fseek(file_des, data_blk_id[i] * BLOCK_SIZE, SEEK_SET);
            fwrite(buf, 1, write_size, file_des);
            remain_size -= write_size;
            write_size = BLOCK_SIZE < remain_size ? BLOCK_SIZE : remain_size;

        }

        PRINTF_FLUSH("data: %s\n", buf);
        PRINTF_FLUSH("成功写入了%ld个字节到文件%s中!\n", size, m_paths.new_dir_file_fullname);
        // 更新inode
        target_inode->st_size = size;
        
        // 更新inode到文件系统
        write_inode(target_inode);
        free(target_inode);
        target_inode = NULL;
        return size;
    }


    // 检查写入数据大小是否大于当前数据块的当前文件指针位置后面的大小
    // 获得文件指针指向的块号
    // short blk_num_id = offset / BLOCK_MAX_DATA_SIZE;
    // short off_real_addr;
    // short off_real_addr_idx;
    // cal_curaddr_idx_curaddr(blk_num_id, &off_real_addr, &off_real_addr_idx);
    // // 根据真实块号得到数据块
    // struct data_block* data_blk = malloc(sizeof(struct data_block));
    // read_data_block(off_real_addr, data_blk);
    // // 检查写入数据大小是否大于当前数据块的当前文件指针位置及后面的大小
    // // 得到当前文件指针位置及后面的大小
    // short later_size = data_blk->used_size - offset;
    // // ——不是的话，直接覆盖即可
    // if (size < later_size)
    // {
    //     FILE* file_des = get_file_singleton();

    // }

    // ——是的话，检查后面是否还有数据块
    // —— ——是的话直接覆盖数据，剩下的数据块被系统回收
    // —— ——不是的话申请空闲数据块并写入
    // 从写入位置开始，覆盖之前的数据内容
}

/**
 * @brief 打开文件
 *
 * @param path 文件的路径
 * @param fi   FUSE 文件信息结构指针，用于访问文件的有关信息
 *
 * @return 成功返回 0，错误时返回相应的负值（用于FUSE文件系统）
 */
static int pzj_open(const char *path, struct fuse_file_info *fi)
{
    return 0;
}


/**
 * @brief 权限检查
 *
 * @param path 文件或目录的路径
 * @param flag 权限标志，用于指定检查的权限类型
 *
 * @return 默认返回 0
 */
static int pzj_access(const char *path, int flag)
{
	return 0;
}

/**
 * @brief 初始化文件系统
 *
 * @param conn_info FUSE 连接信息结构指针，用于配置文件系统的连接信息
 * @param config    FUSE 配置信息结构指针，用于配置文件系统的相关参数
 *
 * @return 无返回值
 */
void * pzj_init(struct fuse_conn_info * conn_info, struct fuse_config * config) 
{
    get_sb_info();
    get_root_inode(&root_inode);
}

/**
 * @brief 注册并实现的文件系统操作
 *
 */
static struct fuse_operations pzj_oper = {
    .init = pzj_init,       // 初始化函数
    .getattr = pzj_getattr, // 获取文件或目录属性函数
    .readdir = pzj_readdir, // 读取目录函数
    .mkdir = pzj_mkdir,     // 创建目录函数
    .mknod = pzj_mknod,     // 创建文件函数
    .utimens = pzj_utimens, // 更新访问时间函数
    .rmdir = pzj_rmdir,     // 删除目录函数
    .unlink = pzj_unlink,   // 删除文件函数
    .read = pzj_read,       // 读文件函数
    .write = pzj_write,     // 写文件函数
    .open = pzj_open,       // 打开文件函数
    .access = pzj_access    // 权限检查函数
};

int main(int argc, char *argv[]) 
{
	return fuse_main(argc, argv, &pzj_oper, NULL);
}

