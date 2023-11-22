#include"util.h"

static int pzj_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi)
{
    PRINTF_FLUSH("pzj_getattr begin\n");
    PRINTF_FLUSH("getattr	 path : %s \n", path);
    struct inode* targe_inode = (struct inode*)malloc(sizeof(struct inode));
    struct paths m_paths;
    if (strcmp("/", path) == 0) {
        targe_inode = &root_inode;
    }
    else 
    {
        char* p = (char*)&m_paths;
        memset(p, '\0', sizeof(struct paths));
        int ret = split_path(path, &m_paths);
        printf("ret: %d\n", ret);
        // 检查长度问题
        if (-1 == ret)
        {
            PRINTF_FLUSH("新建的文件或目录长度过长！\n");
            return -ENAMETOOLONG;
        }
        else if (-2 == ret)
        {
            PRINTF_FLUSH("新建的文件扩展名长度过长！\n");
            return -ENAMETOOLONG;
        }
        
        PRINTF_FLUSH("请求的文件或目录名：%s\n", m_paths.new_dir_file_fullname);
        
        if (0 == strcmp(m_paths.parent_dir, "\0"))
        {
            
            int ret = return_inode_check(m_paths.new_dir_file_fullname, &root_inode, targe_inode);
            if (ret != 0)
            {
                return ret;
            }
        }
        else
        {
            int ret = return_inode_2path_check(m_paths.parent_dir, m_paths.new_dir_file_fullname, &root_inode, targe_inode);
            if (ret != 0)
            {
                return ret;
            }
        }
    }
    stbuf->st_ino = targe_inode->st_ino;
    PRINTF_FLUSH("新的inode号为: %lu\n", stbuf->st_ino);
    stbuf->st_mode = targe_inode->st_mode | S_IFDIR;
    stbuf->st_nlink = targe_inode->st_nlink;
    stbuf->st_uid = targe_inode->st_uid;
    stbuf->st_gid = targe_inode->st_gid;
    stbuf->st_size = targe_inode->st_size;
    stbuf->st_atime = targe_inode->st_atim.tv_sec;
    stbuf->st_blocks = m_sb.first_inode + targe_inode->st_ino - 1;
    PRINTF_FLUSH("成功找到%s\n", m_paths.new_dir_file_fullname);
    return 0;
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


static int pzj_rmdir (const char *path)
{
    return remove_dir_or_file(path, 1);
}
static int pzj_unlink (const char *path)
{
    return remove_dir_or_file(path, 2);
}
static struct fuse_operations pzj_oper = {
    .getattr = pzj_getattr,
    .readdir = pzj_readdir,
    .mkdir = pzj_mkdir,
    .mknod = pzj_mknod,
    .utimens = pzj_utimens,
    .rmdir = pzj_rmdir,
    .unlink = pzj_unlink
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