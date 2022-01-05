#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include "file_reader.h"
#include <errno.h>
#include <string.h>
#include <ctype.h>


uint32_t get_next_cluster(uint32_t curr,uint16_t * fat_data){
    return fat_data[curr];
}

struct disk_t* disk_open_from_file(const char* volume_file_name){
    if(volume_file_name == NULL){
        errno =EFAULT;
        printf("Error code: %d\n", errno);
        return NULL;
    }
    FILE *f = fopen(volume_file_name,"rb");
    if(f==NULL){
        errno = ENOENT;
        printf("Error code: %d\n", errno);
        return NULL;
    }
    struct disk_t *x = malloc(sizeof(struct disk_t));
    x->DISC = f;
    return x;
}
int disk_read(struct disk_t* pdisk, int32_t first_sector, void* buffer, int32_t sectors_to_read){

    fseek(pdisk->DISC,first_sector*512,SEEK_SET);
    int read = fread(buffer,512,sectors_to_read,pdisk->DISC);
    if(read != sectors_to_read)read = 0;
    return read;

}
int disk_close(struct disk_t* pdisk){
    if(pdisk == NULL){
        errno = EFAULT;
        printf("Error code: %d\n", errno);
        return -1;
    }
    fclose(pdisk->DISC);
    free(pdisk);

    return 0;
}



struct volume_t* fat_open(struct disk_t* pdisk, uint32_t first_sector){
    if(pdisk == NULL){
        errno = EFAULT;
        return NULL;
    }
    struct volume_t *super = malloc(sizeof(struct volume_t));
    super->disk = pdisk;
    int e=disk_read(pdisk,first_sector,super,1);
    if(e != 1){
        free(super);
        errno = EFAULT;
        return NULL;
    }

    if(super->magic != 0xAA55){
        errno = EINVAL;
        free(super);
        return NULL;
    }
    super->rootdir_position = super->reserved_sectors +super->fat_count * super->sectors_per_fat;
    uint8_t *fat1_data = (uint8_t *) malloc(super->bytes_per_sector * super->sectors_per_fat);
    uint8_t *fat2_data = (uint8_t *) malloc(super->bytes_per_sector * super->sectors_per_fat);
    if (fat1_data == NULL || fat2_data == NULL) {
        free(fat1_data);
        free(fat2_data);
        errno = ENOMEM;
        return NULL;
    }
    super->fat1_position = super->reserved_sectors;
    disk_read(pdisk,super->reserved_sectors,fat1_data,super->sectors_per_fat);
    disk_read(pdisk,super->reserved_sectors + super->sectors_per_fat,fat2_data,super->sectors_per_fat);

    //12->16
    uint32_t sectors_per_rootdir = (super->root_dir_capacity * sizeof(struct dir_t)) /super->bytes_per_sector;
    if ((super->root_dir_capacity * sizeof(struct dir_t)) % super->bytes_per_sector != 0)
        sectors_per_rootdir++;
    uint32_t volume_size = super->logical_sectors16 == 0 ? super->logical_sectors32 : super->logical_sectors16;
    uint32_t user_size = volume_size - (super->fat_count * super->sectors_per_fat) - super->reserved_sectors - sectors_per_rootdir;
    uint32_t number_of_cluster_per_volume = user_size / super->sectors_per_cluster;
    uint16_t *buffer = malloc((number_of_cluster_per_volume + 2) * sizeof(int));

    for(uint32_t i = 0, j = 0; i < number_of_cluster_per_volume + 2; i += 2, j += 3) {
        uint8_t b1 = fat1_data[j];
        uint8_t b2 = fat1_data[j + 1];
        uint8_t b3 = fat1_data[j + 2];

        int c1 = ((b2 & 0x0F) << 8) | b1;
        int c2 = ((b2 & 0xF0) >> 4) | (b3 << 4);
        buffer[i] = c1;
        buffer[i + 1] = c2;
    }
    free(fat1_data);
    free(fat2_data);
    super->fat_data = buffer;
    return super;
}
int fat_close(struct volume_t* pvolume){
    if(pvolume == NULL){
        errno = EFAULT;
        printf("Error code: %d\n", errno);
        return -1;
    }
    free(pvolume->fat_data);
    free(pvolume);
    return 0;
}

struct file_t;
struct file_t* file_open(struct volume_t* pvolume, const char* file_name){
    if(pvolume == NULL){
        errno =EFAULT;
        printf("errno: %d",errno);
        return NULL;
    }
    if(file_name == NULL){
        errno = ENOENT;
        printf("errno: %d",errno);
        return NULL;
    }
    int rootdir_size = (pvolume->root_dir_capacity * sizeof(struct dir_t))/ (int)pvolume->bytes_per_sector;
    uint16_t rootdir_position = 0  + pvolume->reserved_sectors + pvolume->fat_count * pvolume->sectors_per_fat;
    struct dir_t *dir = malloc(rootdir_size * pvolume->bytes_per_sector);
    struct file_t *fil = malloc(sizeof(struct file_t));
    if(dir == NULL || fil == NULL) {
        errno = ENOMEM;
        free(dir);
        free(fil);
        printf("errno: %d",errno);
        return NULL;
    }
    disk_read(pvolume->disk,rootdir_position ,dir,rootdir_size);

    for(int i=0;i<pvolume->root_dir_capacity;i++){

        if(dir[i].full_name[0] == '\x0')break;

        if((dir->attributes & FAT_ATTRIB_VOLUME) != 0)continue;

        char name[8+3+1] = { 0 };

        memcpy(name,dir[i].full_name,8+3);

        int cluster = ((int)dir[i].high_chain_index << 16 | dir[i].low_chain_index);
        int f=0;
        if((dir[i].attributes & FAT_ATTRIB_DIR) != 0){
            printf("%d: %s <DIRECTORY>, cluster=%d\n",
                   i,
                   name,
                   cluster
            );
        }
        else{
            printf("%d: %s size = %d, cluster=%d\n",
                   i,
                   name,
                   dir[i].size,
                   cluster
            );
            for(int i=0,j=0;i<(int)strlen(file_name);i++,j++){
                if(*(file_name+j) == '.')j++;
                while(*(name+i) == ' ')i++;
                if(*(name+i) != *(file_name+j))f =1;
            }
            if(f == 0){
                fil->offset = 0;
                fil->dir = dir[i];
                fil->size = dir[i].size;
                fil->vol = pvolume;
                fil->global_offset=0;
                fil->cluster = cluster;
                free(dir);

                return fil;
            }

        }

    }
    free(fil);
    free(dir);
    return NULL;
}
int file_close(struct file_t* stream){
    if(stream == NULL){
        errno = EFAULT;
        return -1;
    }
    free(stream);
    return 0;
}
size_t file_read(void *ptr, size_t size, size_t nmemb, struct file_t *stream){
    if(ptr == NULL){
        errno = EFAULT;
        return -1;
    }
    int data_start =  stream->vol->fat1_position + 2* stream->vol->sectors_per_fat;
    int sectors_per_root = stream->vol->root_dir_capacity * sizeof(struct dir_t) / stream->vol->bytes_per_sector;
    int d_start = data_start + sectors_per_root;
    intptr_t offset = 0;

    int curr = stream->cluster;
    int count=0;
    while(1) {
        curr =get_next_cluster(curr,stream->vol->fat_data);
        count++;
        if(curr >= 0xFF8)break;
    }

    curr = stream->cluster;
    uint8_t *buff = (uint8_t*)malloc(count * stream->vol->sectors_per_cluster * stream->vol->bytes_per_sector);
    if(buff == NULL)return 69;
    int s=0;
    if(curr >= 0xFF8)return 0;
    while(1) {
        int cluster_start =d_start + (curr -2 ) * stream->vol->sectors_per_cluster;
        s = disk_read(stream->vol->disk, cluster_start, buff+offset, stream->vol->sectors_per_cluster);
        if(s == 0){
            errno =ERANGE;
            free(buff);
            return -1;}

        offset+=stream->vol->sectors_per_cluster *stream->vol->bytes_per_sector;
        if(get_next_cluster(curr,stream->vol->fat_data) >= 0xFF8)break;
        curr=get_next_cluster(curr,stream->vol->fat_data);
    }
    int x=(int)(nmemb);
        intptr_t add = stream->global_offset;
        int num =(int) (nmemb * size);
        stream->global_offset+=nmemb*size;
        if(stream->global_offset > stream->size){

            num=(int)(num -(stream->global_offset - stream->size));
            x=num;
            if(x <(int) size)x=0;
            memcpy(ptr, (buff+add) , num);
        }
        else{
            memcpy(ptr, (buff+add) , (nmemb * size)  );
        }
    free(buff);
    return x;
}
int32_t file_seek(struct file_t* stream, int32_t offset, int whence){
    if(stream == NULL){
        errno = EFAULT;
        return -1;
    }
    if(whence == SEEK_SET){
        if(offset >(int32_t) stream->size){
            errno = ENXIO;
            return -1;
        }
        stream->global_offset = offset;
    }
    else if(whence == SEEK_CUR){
        if((int)(offset + stream->global_offset) >(int) stream->size){
            errno = ENXIO;
            return -1;
        }
        stream->global_offset += offset;
    }
    else if(whence == SEEK_END){
        if((int)(offset + stream->size) > (int)stream->size){
            errno = ENXIO;
            return -1;
        }
        stream->global_offset =stream->size + offset;
    }
    else{
        errno = EINVAL;
        return -1;
    }
    return stream->global_offset;
}

struct dir_t* dir_open(struct volume_t* pvolume, const char* dir_path){
    if(pvolume == NULL){
        errno = EFAULT;
        return NULL;
    }
    if(dir_path == NULL){
        errno = ENOENT;
        return NULL;
    }
    if(*dir_path != '\\'){
        errno = ENOENT;
        return NULL;
    }


    int rootdir_size = (pvolume->root_dir_capacity * sizeof(struct dir_t))/ (int)pvolume->bytes_per_sector;
    uint16_t rootdir_position = 0  + pvolume->reserved_sectors + pvolume->fat_count * pvolume->sectors_per_fat;
    struct dir_t *dir = malloc(rootdir_size * pvolume->bytes_per_sector);
    if(dir == NULL ) {
        errno = ENOMEM;
        free(dir);
        return NULL;
    }
    for(int i=0;i<rootdir_size;i++){
        disk_read(pvolume->disk,rootdir_position  ,(dir + i * pvolume->bytes_per_sector),1);
    }
    dir->vol = pvolume;
    dir->num=0;

    return dir;

}

int dir_read(struct dir_t* pdir, struct dir_entry_t* pentry){
    if(pdir->num > pdir->vol->root_dir_capacity)return 1;
        for(int i=pdir->num;i<pdir->vol->root_dir_capacity;i++){
            if((*(pdir+i)).full_name[0] == '\x0')break;
            if((pdir[i].attributes & FAT_ATTRIB_VOLUME) != 0)continue;
            pdir->num++;
            pentry->is_directory=1;pentry->is_system=1;pentry->is_hidden=1;pentry->is_readonly=1;pentry->is_archived=1;
            if((pdir[i].attributes & FAT_ATTRIB_DIR) == 0)pentry->is_directory=0;
            if((pdir[i].attributes & FAT_ATTRIB_ARCH) == 0)pentry->is_archived=0;
            if((pdir[i].attributes & FAT_ATTRIB_HIDDEN) == 0)pentry->is_hidden=0;
            if((pdir[i].attributes & FAT_ATTRIB_SYSFILE) == 0)pentry->is_system=0;
            if((pdir[i].attributes & FAT_ATTRIB_READONLY) == 0)pentry->is_readonly=0;


            char name[8+3+1] = { 0 };
            char end_name[13] = { 0 };
            memcpy(name,pdir[i].full_name,8+3);
            int i=0;
            if(name[8] != ' '){
                while(name[i] != ' ' && i<8){
                    if(!isalpha(name[i])){return dir_read(pdir,pentry);}
                    end_name[i] = name[i];
                    i++;
                }
                end_name[i]= '.';i++;
                for(int j=8;j<11;j++,i++){
                    if(name[j] == ' ')break;
                    end_name[i] = name[j];
                }
                i++;end_name[i] = '\0';
            }
            else{
                while(name[i] != ' '){
                    if(!isalpha(name[i])){return dir_read(pdir,pentry);}
                    end_name[i] = name[i];
                    i++;
                }
                end_name[i] = '\0';
            }
            pentry->name= end_name;
            pentry->size=pdir[i].size;
            return 0;
      }


    return 1;
}
int dir_close(struct dir_t* pdir){
    if(pdir==NULL){
        errno = EFAULT;
        return -1;
    }
    free(pdir);
    return 0;
}

