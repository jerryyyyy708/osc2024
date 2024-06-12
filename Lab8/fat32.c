#include "sd_driver.h"
#include "vfs.h"
#include "tmpfs.h"
#include "memory.h"
#include "shell.h"
#include "utils.h"
#include <stdint.h>

extern char* cpio_base;

// struct file_operations fat32_file_operations = {fat32_write,fat32_read,fat32_open,fat32_close};
// struct vnode_operations fat32_vnode_operations = {fat32_lookup,fat32_create,fat32_mkdir};

//reference: https://tw.easeus.com/data-recovery/fat32-disk-structure.html
struct PartitionEntry {
    uint8_t status;            // 磁碟區的當前狀態（00h=不活動，80h=活動）
    uint8_t start_head;        // 磁碟區開始 - 頭
    uint16_t start_sector_cylinder; // 磁碟區的開始 - 柱面/扇區
    uint8_t type;              // 磁碟區類型
    uint8_t end_head;          // 磁碟區結束 - 頭
    uint16_t end_sector_cylinder;   // 磁碟區結束 - 柱面/扇區
    uint32_t start_lba;        // MBR 與磁碟區中第一個扇區之間的扇區數
    uint32_t size_in_sectors;  // 磁碟區中的扇區數
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
    char boot_code[446];        // 可執行代碼（開機電腦）
    struct PartitionEntry partitions[4]; // 四個磁碟區條目
    uint16_t signature;            // 開機記錄簽名 (55AAh)
} __attribute__((packed));

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

// struct vnode * fat32_create_vnode(const char * name, char * data){
//     struct vnode * node = allocate_page(4096);
//     memset(node, 4096);
//     node -> f_ops = &fat32_file_operations;
//     node -> v_ops = &fat32_vnode_operations;
//     struct fat32_node * inode = allocate_page(4096);
//     memset(inode, 4096);
//     strcpy(name, inode -> name);
//     node -> internal = inode;
//     return node;
// }

void format_filename(char *dest, const char *src) {
    int i, j;
    for (i = 0, j = 0; i < 11; i++) {
        if (i == 8) {
            dest[j++] = '.';  // 插入檔案名與擴展名之間的點
        }
        if (src[i] != ' ') {
            dest[j++] = src[i];
        }
    }
    dest[j] = '\0';  // 結尾字符
}

int fat32_mount(struct filesystem *_fs, struct mount *mt){
    //initialize all entries
    mt -> fs = _fs;
    char buf[512];
    readblock(0, buf);
    struct fat32_mbr * mbr = buf;

    uart_puts("[Boot] Partition Status:\n\r");
    
    for(int i=0; i<4; i++){
        uart_puts("Partition ");
        uart_int(i+1);
        uart_puts(": ");
        uart_int(mbr -> partitions[i].status);
        newline();
    }
    
    int fat_start_block = mbr -> partitions[0].start_lba;
    readblock(fat_start_block, buf); // Read the first block of the partition
    struct FAT32BootSector *boot_sector = (struct FAT32BootSector *)buf;

    int root_dir_start_block = fat_start_block + ((boot_sector->reserved_sector_count + (boot_sector->num_fats * boot_sector->fat_size_32)) * boot_sector->sectors_per_cluster);
    uart_int(root_dir_start_block);
    newline();
    readblock(root_dir_start_block, buf);

    struct DirectoryEntry *dir = (struct DirectoryEntry *)buf;
    char formatted_name[13]; // 8 字元名稱 + 1 點 + 3 擴展名 + 1 結尾

    for (int i = 0; i < 16; i++) {  // 假定每個塊有 16 個條目
        if (dir[i].name[0] == 0x00) break; // 結束目錄讀取
        if (dir[i].name[0] == 0xE5) continue; // 跳過已刪除文件
        if (dir[i].attr & 0x08) continue; // 跳過卷標

        format_filename(formatted_name, dir[i].name);
        if (dir[i].attr & 0x10) {  // 目錄條目
            uart_puts("[DIR] ");
            uart_puts(formatted_name);
            newline();
        } else {  // 文件條目
            uart_puts("[FILE] ");
            uart_puts(formatted_name);
            newline();
        }
    }
    return 0;
}

int reg_fat32(){
    struct filesystem fs;
    fs.name = "fat32";
    fs.setup_mount = fat32_mount;
    return register_filesystem(&fs);
}

// int fat32_write(struct file *file, const void *buf, size_t len){
//     return -1;
// }

// int fat32_read(struct file *file, void *buf, size_t len){
//     struct fat32_node * internal = file -> vnode -> internal;
//     if(file -> f_pos + len > internal -> size)
//         len = internal -> size - file -> f_pos;
//     for(int i=0; i<len;i++){
//         ((char *)buf)[i] = internal -> data[i + file -> f_pos];
//     }
//     file -> f_pos += len;
//     return len;
// }

// int fat32_open(struct vnode *file_node, struct file **target){
//     (*target) -> vnode = file_node;
//     (*target) -> f_ops = file_node -> f_ops;
//     (*target) -> f_pos = 0;
//     return 0;
// }

// int fat32_close(struct file *file){
//     free_page(file);
//     return 0;
// }

// int fat32_lookup(struct vnode *dir_node, struct vnode **target, const char *component_name){
//     int idx = 0;
//     struct fat32_node * internal = dir_node -> internal;
//     while(internal -> entry[idx]){
//         struct fat32_node * entry_node = internal -> entry[idx] -> internal;
//         if(strcmp(entry_node -> name, component_name) == 0){
//             *target = internal -> entry[idx];
//             return 0;
//         }
//         idx++;
//     }
//     return -1;
// }

// int fat32_create(struct vnode *dir_node, struct vnode **target, const char *component_name){
//     return -1;
// }

// int fat32_mkdir(struct vnode *dir_node, struct vnode **target, const char *component_name){
//     return -1;
// }