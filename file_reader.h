#ifndef FILE_READER_H
#define FILE_READER_H
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
struct disk_t{
    FILE *DISC;
};

struct volume_t{
    uint8_t jump_code[3];
    char oem_name[8];
    uint16_t  bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fat_count;
    uint16_t root_dir_capacity;
    uint16_t logical_sectors16;
    uint8_t media_type;
    uint16_t sectors_per_fat;
    uint16_t chs_sectors_per_track;
    uint16_t chs_tracks_per_cylinder;
    uint32_t hidden_sectors;
    uint32_t logical_sectors32;
    uint8_t media_id;
    uint8_t chs_head;
    uint8_t ext_bpb_signature;
    uint32_t serial_number;
    char volume_label[11];
    char fsid[8];
    uint8_t boot_code[448];
    uint16_t magic;
    struct disk_t *disk;
    uint16_t fat1_position;
    uint16_t rootdir_position;
    uint16_t *fat_data;
}__attribute__((__packed__)) ;

typedef uint32_t lba;


typedef uint16_t fat_time_t;
typedef uint16_t fat_date_t;
enum fat_attributes_t {
//    0x01–tylko do odczytu (read-onlyfile)
//    0x02–plik ukryty (hiddenfile)
//    0x04–plik systemowy (system file)
//    0x08–etykieta woluminu (volumelabel)
//    0x10 –katalog (directory)
//    0x20–plik archiwalny (archived)
//    0x0F –długa nazwa pliku (LFN –longfile name)
    FAT_ATTRIB_READONLY = 0x01,
    FAT_ATTRIB_HIDDEN = 0x02,
    FAT_ATTRIB_SYSFILE = 0x04,
    FAT_ATTRIB_VOLUME = 0x08,
    FAT_ATTRIB_DIR = 0x10,
    FAT_ATTRIB_ARCH = 0x20,
    FAT_ATTRIB_LFN = 0x0F
}__attribute__ ((__packed__));

struct dir_t{
    char full_name[8 + 3];
    enum fat_attributes_t attributes;
    struct volume_t* vol;
    fat_date_t high_chain_index;
    uint16_t num;
    fat_date_t last_modification_date;
    uint16_t low_chain_index;
    uint32_t size;

}__attribute__ ((__packed__));
struct dir_entry_t{
    char* name;
    uint32_t size;
    uint8_t is_archived;
    uint8_t is_readonly;
    uint8_t is_system;
    uint8_t is_hidden;
    uint8_t is_directory;

} __attribute__ ((__packed__));

struct file_t{
    struct dir_t dir;
    uint16_t cluster;
    uint16_t position;
    struct volume_t *vol;
    uint32_t size;
    uint32_t offset;
    uint32_t global_offset;
};

struct disk_t* disk_open_from_file(const char* volume_file_name);
int disk_read(struct disk_t* pdisk, int32_t first_sector, void* buffer, int32_t sectors_to_read);
int disk_close(struct disk_t* pdisk);
struct volume_t* fat_open(struct disk_t* pdisk, uint32_t first_sector);
int fat_close(struct volume_t* pvolume);
struct file_t* file_open(struct volume_t* pvolume, const char* file_name);
int file_close(struct file_t* stream);
size_t file_read(void *ptr, size_t size, size_t nmemb, struct file_t *stream);
int32_t file_seek(struct file_t* stream, int32_t offset, int whence);
struct dir_t* dir_open(struct volume_t* pvolume, const char* dir_path);
int dir_close(struct dir_t* pdir);
int dir_read(struct dir_t* pdir, struct dir_entry_t* pentry);

#endif
