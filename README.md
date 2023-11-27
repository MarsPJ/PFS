# OS课程设计——基于FUSE的Inode文件系统设计与实现

- 首先准备项目结构如下:

    ```
    .
    ├── bin
    ├── fuse
    ├── README.md
    ├── res
    ├── run.sh
    └── src
        ├── init.c
        ├── PFS.c
        └── util.h
    ```

- 然后修改 `util.h` 文件中的第 46 行 `#define DISK_PATH "/root/data/PFS/res/diskimg"`，将`/root/data/PFS`改为项目路径，磁盘文件名不需要改，使用 `diskimg` 作为磁盘文件名。

- 给脚本 `run.sh` 增加执行权限，运行命令: `chmod +x run.sh`

- 运行脚本: `sh run.sh`

- 运行脚本后的项目目录树:

    ```
    .
    ├── bin
    │   ├── init
    │   └── PFS
    ├── fuse
    ├── README.md
    ├── res
    │   └── diskimg
    ├── run.sh
    └── src
        ├── init.c
        ├── PFS.c
        └── util.h
    ```

- 在当前项目目录（如`PFS`）下打开另一个终端，输入`cd fuse`进入到挂载目录`fuse`，然后输入操作命令即可，如：`mkdir dir1`
