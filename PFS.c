#include"util.h"

static int pzj_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi)
{
    PRINTF_FLUSH("pzj_getattr begin\n");
    PRINTF_FLUSH("getattr	 path : %s \n", path);
    struct inode* targe_inode = NULL;
    struct paths m_paths;
    if (strcmp("/", path) == 0) {
        targe_inode = &root_inode;
        stbuf->st_mode = S_IFDIR;
    }
    else 
    {
        targe_inode = (struct inode*)malloc(sizeof(struct inode));
        char* p = (char*)&m_paths;
        memset(p, '\0', sizeof(struct paths));
        int ret = split_path(path, &m_paths);
        printf("ret: %d\n", ret);
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
            stbuf->st_mode = S_IFDIR;
            int ret = return_inode_check(m_paths.new_dir_file_fullname, &root_inode, targe_inode);
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
            int ret = return_inode_2path_check(m_paths.parent_dir, m_paths.new_dir_file_fullname, &root_inode, targe_inode);
            if (ret != 0)
            {
                free(targe_inode);
                targe_inode = NULL;
                return ret;
            }
        }
    }
    
    stbuf->st_ino = targe_inode->st_ino;
    PRINTF_FLUSH("新的inode号为: %lu\n", stbuf->st_ino);
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
 * 检查路径有效性：首先，检查传入的路径是否表示一个目录，因为只有目录才需要列出内容。如果路径无效或者不是目录，你可以返回一个错误码（例如 ENOTDIR）来指示目录无效。
 * 使用 filler 函数：使用 filler 参数中提供的函数，填充目录项（dirent）。这是列出目录内容的关键步骤。
 * 列出目录项：遍历目录下的文件和子目录，为每个目录项调用 filler 函数，以填充目录项的名称和类型。通常，你需要添加当前目录 ("."，表示该目录自身) 和父目录 (".."，表示上级目录) 到目录项中。
 * 返回成功或错误：如果一切正常，返回0以指示成功。如果发生错误，可以返回适当的错误码（如 ENOENT 表示未找到目录，或其他错误码）来指示问题。
*/

// 先通过目录名（绝对路径）找出对应的inode号，再通过inode中的addr，访问这个目录的数据区，读取目录项

int pzj_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags)
{
    PRINTF_FLUSH( "pzj_readdir	 path : %s \n", path);
    // 所有目录都有的内容
    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);
    // 这是根目录
    if(strcmp(path, "/") == 0)
    {   
        return return_full_name_check(&root_inode, &filler, buf);
    }
    else
    {
        struct paths m_paths;
        char* p = (char*)&m_paths;
        memset(p, '\0', sizeof(struct paths));
        int ret = path_is_legal(path, &m_paths, 1);
        PRINTF_FLUSH("路径解析结果：%d\n", ret);
        if (0 != ret)
        {
            return ret;
        }
        struct inode* target_inode = (struct inode*)malloc(sizeof(struct inode));
        ret = return_inode_check(m_paths.new_dir_file_fullname, &root_inode, target_inode);
        PRINTF_FLUSH("父目录查找结果：%d\n", ret);
        if (ret != 0)
        {
            free(target_inode);
            target_inode = NULL;
            return ret;
        }
        ret = return_full_name_check(target_inode, &filler, buf);
        PRINTF_FLUSH("当前目录装填结果：%d\n", ret);
        free(target_inode);
        target_inode = NULL;
        return ret;
    }
}

/**
 * 创建目录： 首要任务是在底层文件系统中创建一个新的目录。这通常涉及到在文件系统中分配一个新的目录条目，设置正确的权限、所有者等信息。
 * 路径解析： 确保创建目录的路径是有效的，并且父目录存在。你需要解析路径以确定父目录的位置。
 * 权限检查： 检查用户是否有权限在指定的目录中创建子目录。这通常涉及到检查访问控制列表（ACL）、文件权限位（例如，读、写、执行权限）等。
 * 更新文件系统状态： 更新文件系统的元数据，包括目录的大小、时间戳、所有者等信息。
 * 回馈结果： 如果创建目录成功，通常需要向用户返回成功的响应。如果创建失败，需要返回错误码，如权限不足、磁盘空间不足等。
*/
// TODO:检查权限



static int pzj_mkdir (const char *path, mode_t mode)
{
    return create_dir_or_file(path, mode, 1);
}
static int pzj_mknod (const char *path, mode_t mode, dev_t dev)
{
    dev=0;
    return create_dir_or_file(path, mode, 2);
}
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
        printf("ret: %d\n", ret);
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
            int ret = return_inode_check(m_paths.new_dir_file_fullname, &root_inode, targe_inode);
            if (ret != 0)
            {
                free(targe_inode);
                targe_inode = NULL;
                return ret;
            }
        }
        else
        {
            int ret = return_inode_2path_check(m_paths.parent_dir, m_paths.new_dir_file_fullname, &root_inode, targe_inode);
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


static int pzj_rmdir (const char *path)
{
    return remove_dir_or_file(path, 1);
}
static int pzj_unlink (const char *path)
{
    return remove_dir_or_file(path, 2);
}
// size是用户要读取的数据大小，offset是文件偏移指针
static int pzj_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    PRINTF_FLUSH("pzj_read! size: %lu, offset: %ld\n", size, offset);
    struct paths m_paths;
    char* p = (char*)&m_paths;
    memset(p, '\0', sizeof(struct paths));
    int ret = path_is_legal(path, &m_paths, 2);
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
    ret = return_inode_2path_check(m_paths.parent_dir, m_paths.new_dir_file_fullname, &root_inode, target_inode);
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

static int pzj_write (const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    PRINTF_FLUSH("pzj_write %s\n", path);
    PRINTF_FLUSH("data: %s\n", buf);
    struct paths m_paths;
    char* p = (char*)&m_paths;
    memset(p, '\0', sizeof(struct paths));
    int ret = path_is_legal(path, &m_paths, 2);
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
    ret = return_inode_2path_check(m_paths.parent_dir, m_paths.new_dir_file_fullname, &root_inode, target_inode);
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
static int pzj_open(const char *path, struct fuse_file_info *fi)
{
    return 0;
}
//进入目录
static int pzj_access(const char *path, int flag)
{
	return 0;
}
void * pzj_init(struct fuse_conn_info * conn_info, struct fuse_config * config) 
{
    get_sb_info();
    get_root_inode(&root_inode);
	printf("pzj_init：函数结束返回\n\n");
}
static struct fuse_operations pzj_oper = {
    .init = pzj_init,
    .getattr = pzj_getattr,
    .readdir = pzj_readdir,
    .mkdir = pzj_mkdir,
    .mknod = pzj_mknod,
    .utimens = pzj_utimens,
    .rmdir = pzj_rmdir,
    .unlink = pzj_unlink,
    .read = pzj_read,
    .write = pzj_write,
    .open = pzj_open,
    .access = pzj_access};

int main(int argc, char *argv[]) 
{
	return fuse_main(argc, argv, &pzj_oper, NULL);
}

