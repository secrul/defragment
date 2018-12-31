#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "defrag.h"

struct superblock *superblk;

int block_size;
int block_id = 0;//数据block起始序号

int copy_dblock(FILE *fs,FILE *fd, struct inode *nodes, struct inode *noded){

    int j,base_id = block_id;//记录初始block序号，供修改fd的inode->dblock
    char buffer[515];
    for(j = 0; j < N_DBLOCKS; j ++){
        if(nodes->dblocks[j] == 0){
            noded->dblocks[j] = 0;
            continue;
        }
        fseek(fs,3072 + nodes->dblocks[j] *block_size,SEEK_SET);//偏移到该blocks
        fread(buffer,block_size,1,fs);
        noded->dblocks[j] = block_id;
        fseek(fd,3072 + block_id * block_size,SEEK_SET);//偏移fd到第一个空闲block
        block_id ++;
        fwrite(buffer,block_size,1,fd);
    }
    return (block_id - base_id >= N_DBLOCKS) ? 1 : 0;
}

int copy_iblock(FILE *fs,FILE *fd, struct inode *nodes, struct inode *noded){// 4 x 128 blocks

    int i,j,tmp,con = 0,base_id;//记录初始block序号，供修改fd的inode->dblock
    char buffers[515];
    int b_id = block_id;
    for(j = 0; j < N_IBLOCKS; j ++){
        if(nodes->iblocks[j] == 0){
          noded->iblocks[j] = 0;
          continue;
        }
        fseek(fs,3072 + nodes->iblocks[j] * block_size,SEEK_SET);//偏移到存储block指针得block
        noded->iblocks[j] = block_id;//将空闲块分配给iblocks[j]
        fseek(fd,3072 + block_id * block_size,SEEK_SET);//偏移到当前第一个空闲块
        block_id ++;
        base_id = block_id;
        for(i = 0; i < block_size / 4; i ++){//循环处理一个iblock
            fseek(fs,3072 + nodes->iblocks[j] * block_size + 4 * i,SEEK_SET);//读取iblock指向得block序号
            fread(&tmp,4,1,fs);
            if(tmp == 0){//文件已经结束
                break;
            }
            fseek(fs,3072 + tmp * block_size,SEEK_SET);//偏移fs到数据block
            fread(buffers,block_size,1,fs);
            fseek(fd,3072 + block_id * block_size,SEEK_SET);
            fwrite(buffers,block_size,1,fd);
            block_id ++;
        }

        //修改fd中inode的iblock列表值
        fseek(fd,3072 + (base_id - 1) * block_size,SEEK_SET);
        for(i = base_id; i < base_id + block_size / 4; i ++){
            if(i < block_id){
                fwrite(&i,4,1,fd);
            }
            if(i >= block_id){//未占用,补标记0
                fwrite(&con,4,1,fd);
            }
        }
    }
    return (block_id - b_id > (block_size / 4 + 1) * N_IBLOCKS) ? 1 : 0;//iblock 全部使用，接下来使用i2block，否则返回0结束
}

int copy_i2block(FILE *fs,FILE *fd,struct inode *nodes, struct inode *noded){ // 128 iblocks

    int i,j,tmp,con = 0;//记录初始block序号，供修改fd的inode->dblock
    char buffers[515];
    if(nodes->i2block== 0){//没用到i2block,直接返回
        return 0;
    }

    int b_id = block_id;//作为基址判断i2block是不是全部使用了
    fseek(fs,3072 + nodes->i2block,SEEK_SET);//偏移到存储128个iblock的block
    noded->i2block = block_id ++;

    for(i = 0; i < block_size / 4; i ++){
        fseek(fs, 3072 + nodes->i2block + i * 4,SEEK_SET);//偏移到每一个iblock序号
        fread(&tmp,4,1,fs);
        if(tmp == 0) {//在fd中后面补0
            fseek(fd,1024+noded->i2block + i * 4,SEEK_SET);
            fwrite(&con,4,1,fd);
            continue;
        }
        //开辟一个iblock,就是把当前block序号写入到i2block指向的block
        fseek(fd,3072+noded->i2block + i * 4,SEEK_SET);
        fwrite(&block_id,4,1,fd);//作为iblock
        int tmp_iblockid_d = block_id;//记录此时得iblock，方面后面写入指向得block的序号
        block_id ++ ;
        //接下来处理iblock,   包括fd和fs
        for(j = 0; j < block_size / 4; j ++){
            int tmp1;
            fseek(fs,3072 + tmp * block_size + 4 * j,SEEK_SET);//偏移到fs的每一个iblock
            fread(&tmp1,4,1,fs);//读取block的序号
            fseek(fs,3072 + tmp1 * block_size,SEEK_SET);// 偏移到block，读出复制
            fread(buffers,block_size,1,fs);
            fseek(fd,3072 + block_id * block_size, SEEK_SET);
            fwrite(buffers,block_size,1,fd);
            fseek(fd,3072 + tmp_iblockid_d * block_size + 4 * j,SEEK_SET);//偏移到iblock
            fwrite(&block_id,4,1,fd);//将block序号写到iblock
            block_id ++;
        }
    }
    return (block_id - b_id > ((block_size / 4) * (block_size / 4 + 1) +1)) ? 1 : 0;
}

int copy_i3block(FILE *fs,FILE *fd, struct inode *nodes, struct inode *noded){// 128 i2blocks

    int i,j,tmp,con = 0;//记录初始block序号，供修改fd的inode->dblock
    char buffers[515];
    if(nodes->i3block== 0){//使用没用到iblock,直接返回
        return 0;
    }
    fseek(fs,3072 + nodes->i3block,SEEK_SET);//偏移到存储128个i2block的block
    noded->i3block = block_id ++;//i3block指针
    //逐个处理128个i2block
    for(i = 0; i < block_size /4; i ++){
        fseek(fs,3072 + nodes->i3block + i * 4, SEEK_SET);//fs偏移到每个i2block序号前
        fread(&tmp,4,1,fs);//读取i2block序号
        if(tmp == 0) {//文件已经结束，填充0
            fseek(fd,3072 + noded->i3block + i * 4,SEEK_SET);
            fwrite(&con,4,1,fd);
            continue;
        }
        fseek(fd,3072 + noded->i3block + i * 4,SEEK_SET);//i3block指向block的位置，是i2block
        fwrite(&block_id,4,1,fd);//i2block
        int i2block_id = block_id;
        block_id ++;
        fseek(fs,3072 + tmp * block_size,SEEK_SET);//偏移到i2block
        //接着读取处理iblock
        for(j = 0; j < block_size / 4; j ++){
            int tmp1;
            fseek(fs,3072 + tmp * block_size + 4 * j,SEEK_SET);// 依次偏移到每一个iblock的序号
            fread(&tmp1,4,1,fs);//读取iblock的序号
            if(tmp1 == 0) {//文件结束，填充0
                fseek(fd,3072 + i2block_id * block_size + 4 * j, SEEK_SET);//把iblock的序号记录在i2block的对应位置
                fwrite(&con,4,1,fd);
                continue;
            }
            fseek(fd,3072 + i2block_id * block_size + 4 * j, SEEK_SET);//把iblock的序号记录在i2block的对应位置
            fwrite(&block_id,4,1,fd);
            fseek(fs,3072 + tmp1 * block_size, SEEK_SET);// 偏移到block，读出复制
            int t;
            //接下来读取block序号处理
            for(t = 0; t < block_size / 4; t ++){
                fseek(fs,3072 + tmp1 * block_size + 4 * t, SEEK_SET);
                int d;
                fread(&d,4,1,fs);
                fseek(fs,3072 + d * block_size,SEEK_SET);
                fread(buffers,block_size,1,fs);
                fseek(fd,3072 + block_id * block_size, SEEK_SET);
                fwrite(buffers,block_size,1,fd);
                block_id ++;
            }
        }
    }
    return 0;
}

int main(int argc, char *argv[]) {
        FILE *fs, *fd;
        struct inode *nodes,*noded;
        nodes = (struct inode*)malloc(100);
        noded = (struct inode*)malloc(100);
        char source_filename[50], defrag_filename[50];

        strcpy(source_filename, argv[1]);
        strcpy(defrag_filename,argv[1]);
        strcpy(defrag_filename + strlen(argv[1]) - 4, "-defrag.txt");

        fs = fopen(source_filename, "r");
        fd = fopen(defrag_filename, "w");
        fclose(fd);
        fd = fopen(defrag_filename,"r+");

        if (fd == NULL) {
            printf("打开%s失败", source_filename);
            return 0;
        }
        fseek(fs, 0, SEEK_END);
        long int l = ftell(fs) / 512;
        int tt = 0;
        char buffer[515];
        fseek(fs, 0, SEEK_SET);
        fseek(fd, 0, SEEK_SET);
        for(tt = 0; tt < l; tt ++){
            fread(buffer, 512, 1, fs);
            fwrite(buffer, 512, 1, fd);
        }

        fseek(fs, 512, SEEK_SET);
        superblk = (struct superblock *) malloc(512);
        fread(superblk, 512, 1, fs); //root block
        block_size = superblk->size;

        int inode_num = superblk->size * (superblk->data_offset - superblk->inode_offset) / sizeof(struct inode);//inode number
        int i;

        for(i = 0; i < inode_num; i ++){
            fseek(fs,1024 + sizeof(struct inode)* i, SEEK_SET);
            fseek(fd,1024 + sizeof(struct inode)* i, SEEK_SET);
            fread(nodes, sizeof(struct inode), 1,fs);
            fread(noded, sizeof(struct inode), 1,fd);
            if(nodes->nlink == 0)
                continue;
            if(copy_dblock(fs,fd,nodes,noded) != 0) {
                if(copy_iblock(fs,fd,nodes,noded) == 0) {
                    if(copy_i2block(fs,fd,nodes,noded) == 0) {
                        copy_i3block(fs,fd,nodes,noded);
                    }
                }
            }
            fseek(fd,1024 + sizeof(struct inode)* i, SEEK_SET);
            fwrite(noded, sizeof(struct inode), 1, fd);
        }
        fclose(fs);
        fclose(fd);
        free(noded);
        free(nodes);
        free(superblk);
        return 0;
}
