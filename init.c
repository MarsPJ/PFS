#include"util.h"
/**
 * 实现这个文件系统时，首先需要完成一个格式化程序。这个格式化完成的工作：
 * 1）生成一个8M大小的文件作为文件系统的载体；
 * 2）将文件系统的相关信息写入超级块；
 * 3）根目录作为文件系统中的第一个文件，需要做的事情：
    * a）inode位图的第一个字节的第一位置为1，表示第一个inode已分配；
    * b）将根目录的相关信息填写到inode区的第一个inode。
*/
int main() {
    // PRINTF_FLUSH("size_t: %ld\n", sizeof(size_t));
    // PRINTF_FLUSH("DIR_ENTRY_MAXNUM_PER_BLOCK: %d\n", DIR_ENTRY_MAXNUM_PER_BLOCK);
    // PRINTF_FLUSH("BLOCK_MAX_DATA_SIZE: %ld\n", BLOCK_MAX_DATA_SIZE);
    // PRINTF_FLUSH("DIR_ENTRY_SIZE: %ld\n", DIR_ENTRY_SIZE);
    // PRINTF_FLUSH("BLOCK_MAX_DATA_SIZE / DIR_ENTRY_SIZE: %ld\n", BLOCK_MAX_DATA_SIZE / DIR_ENTRY_SIZE);
    
    // PRINTF_FLUSH("MAX_ADDR_NUM_PER_INDEX_BLOCK: %ld\n", MAX_ADDR_NUM_PER_INDEX_BLOCK);
    
    struct dir_entry t;
    PRINTF_FLUSH("dir_entry: %ld\n", sizeof(t));
    PRINTF_FLUSH("dir_entry: %ld\n", sizeof(t.file_name));
    PRINTF_FLUSH("dir_entry: %ld\n", sizeof(t.extension));
    PRINTF_FLUSH("dir_entry: %ld\n", sizeof(t.inode_id));
    PRINTF_FLUSH("dir_entry: %ld\n", sizeof(t.type));
    struct super_block m_sb;
    m_sb.fs_size = FILE_SYSTEM_SIZE / BLOCK_SIZE;
    m_sb.inodebitmap_size = INODE_BITMAP_BLOCK_NUM;
    m_sb.databitmap_size = DATA_BITMAP_BLOCK_NUM;
    m_sb.inode_area_size = INODE_AREA_BLOCK_NUM;
    m_sb.datasize = m_sb.fs_size - INODE_BITMAP_BLOCK_NUM - DATA_BITMAP_BLOCK_NUM - INODE_AREA_BLOCK_NUM;

    m_sb.first_blk_of_inodebitmap = 1;
    m_sb.first_blk_of_databitmap = 2;
    m_sb.first_inode = 6;
    m_sb.first_blk = 518;
    // 声明一个文件指针
    FILE* file_des = get_file_singleton();
    if (file_des == NULL) {
        perror("无法打开文件'dikimg'");
        return 1;
    }
    // short int tmp = 0;
    // fread(&tmp, sizeof(short int), 1, disk_file);
    // printf("tmp: %hd\n", tmp);
    // 将超级块的信息写入磁盘文件
    if (1 != fwrite(&m_sb, sizeof(struct super_block), 1, file_des)) {
        perror("超级块写入diskimg失败!\n");
        return 1;
    }
    printf("超级块初始化成功！\n");
    // 将文件指针移动到inode位图第一个字节
    long off = m_sb.first_blk_of_inodebitmap * BLOCK_SIZE;
    fseek(file_des, off, SEEK_SET);
    printf("文件指针目前的位置：%ld\n", ftell(file_des));
    unsigned char inode_first_byte;
    // 读取第一个字节数据(文件指针会跟着偏移)
    // fread参数：要读取数据的目标内存地址、要读取的每个元素的大小（以字节为单位）、要读取的元素的数量、要从中读取数据的文件指针
    fread(&inode_first_byte, 1, 1, file_des);
    printf("inode_first_byte的值：%d\n", inode_first_byte);
    // 将该字节最低位置为1
    inode_first_byte |= 0x01;
    printf("更新后的inode_first_byte的值：%d\n", inode_first_byte);
    // 将文件指针复位
    fseek(file_des, off, SEEK_SET);
    printf("文件指针目前的位置：%ld\n", ftell(file_des));
    // 重新写回文件
    if (1 != fwrite(&inode_first_byte, 1, 1, file_des)) {
        perror("inode位图区更新失败\n");
        return 1;
    }
    printf("写入后文件指针目前的位置：%ld\n", ftell(file_des));
    printf("inode位图区初始化成功！\n");

    struct inode root_dir;
    root_dir.st_mode = 0666;
    // 在Unix和Linux文件系统中，i-node号1通常保留用于根目录。
    // 根据Unix文件系统的约定，inode号0通常被保留用于表示特殊情况，如未分配的inode或损坏的inode。
    root_dir.st_ino = 1;

    /**
     * 对于目录：目录的链接数通常初始化为2，因为它至少有两个硬链接：一个指向自身（.），一个指向其父目录（..）。随着在目录中创建子目录或文件，其链接数会逐渐增加。
     * 
     * 根目录是文件系统中的一个特殊情况，它具有以下两个链接：
     *      指向自身的链接：根目录的第一个链接是指向自身的硬链接。这个硬链接保证根目录在文件系统层次结构中始终存在，并且可以通过目录路径 / 访问它。这个硬链接确保根目录不会被删除或丢失。
            指向父目录的链接：根目录的第二个链接是指向父目录的链接。虽然根目录没有显式的父目录，但这个链接是文件系统中所有目录的通用属性。通常，父目录是包含当前目录的目录，但在根目录的情况下，通常将其链接到自身，表示它没有显式的父目录。
    */
    // 字符 '1' 的ASCII码值是49，但如果你将整数1分配给char类型变量，它将被视为整数1而不是字符'1'的ASCII码。
    // 在内部，C编译器将1存储为二进制值00000001，这是整数1的二进制表示，而不是字符'1'的ASCII码。
    root_dir.st_nlink = 1;
    // 假设创建者和组的ID都是0，表示root
    root_dir.st_uid = 0;
    root_dir.st_gid = 0;
    // 根目录的 st_size 字段通常设置为0，因为根目录本身不包含文件数据，只包含对其他目录和文件的引用（即子目录和文件的名称和i-node号）。
    root_dir.st_size = sizeof(struct dir_entry);  // 初始化时根目录下还没有目录项
    struct timespec access_time;
    clock_gettime(CLOCK_REALTIME, &access_time);
    root_dir.st_atim = access_time;
    // TODO:addr不知应该如何赋值，可以赋值为null吗？会不会导致访问到了超级块
    for (int i = 0; i < 7; ++i) {
        root_dir.addr[i] = -1;
    }

    // root_dir.addr[0] = m_sb.first_blk;  // 这样应该是正确的初始化

    fseek(file_des, BLOCK_SIZE * m_sb.first_inode, SEEK_SET);
    fwrite(&root_dir, sizeof(struct inode), 1, file_des);
    fseek(file_des, BLOCK_SIZE * m_sb.first_inode, SEEK_SET);
    printf("根目录初始化成功！\n");
    
    // 显示当前时间
    struct tm time_info;
    char time_str[80];
    // 将时间戳转换为本地时间
    localtime_r(&access_time.tv_sec, &time_info);
    // 格式化时间为字符串
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &time_info);
    printf("当前时间是：%s\n", time_str);

    // 查看文件信息
    const char *filename = "diskimg"; // 替换为你要查看的文件的路径
    struct stat file_stat;
    if (stat(filename, &file_stat) == 0) {
        printf("文件权限信息：%08x\n", file_stat.st_mode & 0777);
        printf("文件所有者的用户ID：%d\n", file_stat.st_uid);
        printf("文件所有者的组ID：%d\n", file_stat.st_gid);
        // 还可以查看其他文件属性，如大小、修改时间等
    } else {
        perror("无法获取文件信息");
        return 1;
    }
    printf("start\n");
    if (file_des == NULL) {
        printf("diskimg打开失败");
    }
    else {
        fseek(file_des, m_sb.first_inode * BLOCK_SIZE, SEEK_SET);
        struct inode tmp_node;
        fread(&tmp_node, sizeof(struct inode), 1, file_des);
        printf("%hd, %hd\n", tmp_node.st_ino, tmp_node.addr[0]);
    }
    close_file_singleton();
}

