#include "sd_driver.h"
#include "vfs.h"
#include "tmpfs.h"
#include "memory.h"
#include "shell.h"
#include "utils.h"
#include "fat32.h"
#include <stdint.h>

extern char* cpio_base;

struct file_operations fat32_file_operations = {fat32_write,fat32_read,fat32_open,fat32_close};
struct vnode_operations fat32_vnode_operations = {fat32_lookup,fat32_create,fat32_mkdir};

//reference: https://tw.easeus.com/data-recovery/fat32-disk-structure.html
struct PartitionEntry {
    uint8_t status;            // 00 no use, 80 active
    uint8_t start_head;       
    uint16_t start_sector_cylinder;
    uint8_t type;              
    uint8_t end_head;         
    uint16_t end_sector_cylinder;  
    uint32_t start_lba;        // sectors to partition
    uint32_t size_in_sectors; 
} __attribute__((packed));

//https://lexra.pixnet.net/blog/post/303910876
struct FAT32BootSector {
    uint8_t jump_boot[3];
    uint8_t oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sector_count;
    uint8_t num_fats;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t media;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint32_t fat_size_32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fs_info;
    uint16_t backup_boot_sector;
    uint8_t reserved[12];
    uint8_t drive_number;
    uint8_t reserved1;
    uint8_t boot_signature;
    uint32_t volume_id;
    uint8_t volume_label[11];
    uint8_t fs_type[8];
} __attribute__((packed));


struct fat32_mbr {
    char boot_code[446];
    struct PartitionEntry partitions[4]; // four partitions
    uint16_t signature; // (55AAh)
} __attribute__((packed));

// https://www.codeguru.com/cplusplus/fat-root-directory-structure-on-floppy-disk-and-file-information/
struct DirectoryEntry {
    uint8_t name[11];
    uint8_t attr;
    uint8_t nt_res;
    uint8_t crt_time_tenth;
    uint16_t crt_time;
    uint16_t crt_date;
    uint16_t lst_acc_date;
    uint16_t fst_clus_hi;
    uint16_t wrt_time;
    uint16_t wrt_date;
    uint16_t fst_clus_lo;
    uint32_t file_size;
} __attribute__((packed));

struct fat32_node{
    char name[MAX_PATH];
    int type;
    struct vnode * entry[MAX_ENTRY];    
    char * data;
    uint32_t start_cluster;
    uint32_t size;
};

struct vnode * fat32_create_vnode(const char * name, int type, uint32_t start_cluster, uint32_t size){
    struct vnode * node = allocate_page(4096);
    memset(node, 4096);
    node -> f_ops = &fat32_file_operations;
    node -> v_ops = &fat32_vnode_operations;
    
    struct fat32_node * inode = allocate_page(4096);
    memset(inode, 4096);
    inode -> type = type;
    inode -> start_cluster = start_cluster;
    inode -> size = size;
    strcpy(name, inode -> name);
    node -> internal = inode;
    
    return node;
}

// https://en.wikipedia.org/wiki/8.3_filename
void format_filename(char *dest, const char *src) {
    int i, j;
    for (i = 0, j = 0; i < 11; i++) {
        if (i == 8) {
            dest[j++] = '.'; 
        }
        if (src[i] != ' ') {
            dest[j++] = src[i];
        }
    }
    dest[j] = '\0'; 
}

struct fat32_mbr * mbr;
int fat_start_block; //mbr -> partitions[0].start_lba;
struct FAT32BootSector * boot_sector; //need to use malloc
int root_dir_start_block; // 3588
uint32_t cluster_size; // 512
uint32_t entries_per_cluster; // 16
struct DirectoryEntry * rootdir; // read from start_block (need to use malloc)

// https://wiki.osdev.org/FAT
int fat32_mount(struct filesystem *_fs, struct mount *mt){
    //initialize all entries
    mt -> fs = _fs;
    char buf[512];
    readblock(0, buf);
    mbr = buf;
    
    fat_start_block = mbr -> partitions[0].start_lba; //2048
    readblock(fat_start_block, buf); // Read the first block of the partition as boot_sector
    boot_sector = allocate_page(4096);
    for(int i=0; i<512; i++){
        ((char *)boot_sector)[i] = buf[i];
    }

    cluster_size = boot_sector -> bytes_per_sector * boot_sector -> sectors_per_cluster;
    entries_per_cluster = cluster_size / 32;
    
    root_dir_start_block = fat_start_block + ((boot_sector->reserved_sector_count + (boot_sector->num_fats * boot_sector->fat_size_32)) * boot_sector->sectors_per_cluster);
    readblock(root_dir_start_block, buf);

    //for root dir lookup
    rootdir = allocate_page(4096);
    for(int i=0; i<512; i++){
        ((char *)rootdir)[i] = buf[i];
    }
    
    mt -> root = fat32_create_vnode("fat32", 2, 0, 0);
    return 0;
}

int reg_fat32(){
    struct filesystem fs;
    fs.name = "fat32";
    fs.setup_mount = fat32_mount;
    return register_filesystem(&fs);
}

int fat32_write(struct file *file, const void *buf, size_t len){
    return -1;
}

// https://www.win.tue.nl/~aeb/linux/fs/fat/fatgen103.pdf
int fat32_read(struct file* file, void* ret, uint64_t len) {
    struct fat32_node* file_node = (struct fat32_node*)file->vnode->internal;
    if(file_node -> size < len + file -> f_pos)
        len = (file_node -> size - file->f_pos);
    int cluster_number = file_node -> start_cluster;
    uint32_t first_data_sector = boot_sector->reserved_sector_count + (boot_sector->num_fats * boot_sector->fat_size_32); // data sector in cluster
    //FAT + the place of first sector(followed by FAT) + cluster offset (start by 2)
    uint32_t first_sector_of_cluster = fat_start_block + first_data_sector + (cluster_number - 2) * boot_sector->sectors_per_cluster;

    char buffer[512];
    memset(buffer, 512);
    readblock(first_sector_of_cluster, (char*) buffer);
    for(int i = 0; i<len;i++){
        ((char *) ret)[i] = buffer[i + file -> f_pos];
    }

    file -> f_pos += len;
    return len;
}

int fat32_open(struct vnode *file_node, struct file **target){
    (*target) -> vnode = file_node;
    (*target) -> f_ops = file_node -> f_ops;
    (*target) -> f_pos = 0;
    return 0;
}

int fat32_close(struct file *file){
    free_page(file);
    return 0;
}

int fat32_lookup(struct vnode *dir_node, struct vnode **target, const char *component_name){
    // check entry, if not in entry, check sd card, and if still no, return -1
    char buf[512];
    int idx = 0;
    struct fat32_node * internal = dir_node -> internal;
    while(internal -> entry[idx]){
        struct fat32_node * entry_node = internal -> entry[idx] -> internal;
        if(strcmp(entry_node -> name, component_name) == 0){
            *target = internal -> entry[idx];
            return 0;
        }
        idx++;
    }

    struct DirectoryEntry *dir = rootdir;
    char formatted_name[13];
    int type = -1;
    //ref see dir
    //search root directory and if found create new vnode
    for (int i = 0; i < entries_per_cluster; i++) { 
        if (dir[i].name[0] == 0x00) break;
        if (dir[i].name[0] == 0xE5) continue;
        if (dir[i].attr & 0x08) continue;
    
        format_filename(formatted_name, dir[i].name);
        if(strcmp(formatted_name, component_name) == 0){
            uart_puts("[FAT32] Found ");
            uart_puts(formatted_name);
            
            if (dir[i].attr & 0x10) {
                type = 1;
            } else {
                type = 3;
            }
            
            uint32_t start_cluster = ((uint32_t)dir[i].fst_clus_hi << 16) | (uint32_t)dir[i].fst_clus_lo; //high 16 bit and low 16 bit
            uint32_t file_size = dir[i].file_size;
            uart_puts(" with size ");
            uart_int(file_size);
            uart_puts(" in start cluster ");
            uart_int(start_cluster);
            newline();
            //set new vnode and assign to internal -> idx
            internal -> entry[idx] = fat32_create_vnode(formatted_name, type, start_cluster, file_size);
            *target = internal -> entry[idx];
            return 0;
        }
    }

    return -1;
}

int fat32_create(struct vnode *dir_node, struct vnode **target, const char *component_name){
    return -1;
}

int fat32_mkdir(struct vnode *dir_node, struct vnode **target, const char *component_name){
    return -1;
}