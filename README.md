# Project5:Defragmentation

## 文件整理系统



#### 目标：

将所给出文件的内部碎片合并到文件末尾，并重命名整理完成的文件，在源文件名后面加上“-defrag”

尽管看了上课的ppt，但是对于所说的文件系统的具体结构还不清晰，在参考了韦阳学长的github

https://github.com/godweiyang/ecnu-oslabs-project5-Defragmentation  才明白

所假设的文件系统如下：



| boot | super | inode0-4 | inode5-9 | inode10-14 | inode15-19 | data | ......   |
| ---- | ----- | -------- | -------- | ---------- | ---------- | ---- | -------- |
|      |       |          |          |            |            | 0    | 1,2,3... |

每个block是512字节；前面两个是bootblock和superblock，接着是4个block，共20个inode；后面是数据块，从0开始编号；数据结构如下：

```C
struct inode {
    int next_inode; /* list for free inodes */
    int protect; /* protection field */
    int nlink; /* number of links to this file */
    int size; /* numer of bytes in file */
    int uid; /* owner's user ID */
    int gid; /* owner's group ID */
    int ctime; /* time field */
    int mtime; /* time field */
    int atime; /* time field */
    int dblocks[N_DBLOCKS]; /* pointers to data blocks */
    int iblocks[N_IBLOCKS]; /* pointers to indirect blocks */
    int i2block; /* pointer to doubly indirect block */
    int i3block; /* pointer to triply indirect block */
};

struct superblock {
    int size; /* size of blocks in bytes */
    int inode_offset; /* offset of inode region in bytes blocks */
    int data_offset; /* data region offset in blocks */
    int swap_offset; /* swap region offset in blocks */
    int free_inode; /* head of free inode list */
    int free_iblock; /* head of free block list */
};
```

可以发现第6、9、14个inode是空的；N_DBLOCKS是10，每个dblock指向一个数据block，而N_IBLOCKS是4，每一个iblock指向一个指针block，该block中每4个字节指向一个数据block，即一共指向128个数据block；而一个i2block指向128个iblock，一个i3block指向128个i2block；

#### 算法思路：

1. 对于swap区域不是很了解，就先将原文件整体复制一遍；
2. 遍历20个inode，如果inode非空的话将其对应的block进行整合，去除中间的空闲；对于空闲的inode保持原位置，不要将其移动到后面；
3. 先将dblock指向的数据block复制到第一个空闲数据block，依次往后面排；修改新文件inode的dblock，使其指向整合后的数据block；如果
4. 对于iblock，新文件将第一个空闲block作为一级指针block，然后依次将原文件中iblock的指针block指向的数据block复制到新文件第一个空闲block，将block序号记录在指针block中；修改新文件inode的iblock，使其指向指针block；
5. 对于i2block将新文件第一个空闲block作为二级指针，指向128个一级指针block，然后按照第四步的方法继续处理一级指针，更新inode的i2block指针；
6. 对于i3block，同样将第一个空闲块作为三级指针指向128个二级指针，按照第五步处理二级指针，更新inode得i3block；
7. 将inode写回新文件，进行更新；

作者：刘金昊

https://github.com/secrul







