#ifndef SHELL_H
#define SHELL_H

struct cpio_newc_header {
    char c_magic[6];
    char c_ino[8];
    char c_mode[8];
    char c_uid[8];
    char c_gid[8];
    char c_nlink[8];
    char c_mtime[8];
    char c_filesize[8];
    char c_devmajor[8];
    char c_devminor[8];
    char c_rdevmajor[8];
    char c_rdevminor[8];
    char c_namesize[8];
    char c_check[8];
};

int strcmp(char *s1, char *s2);

int hex_to_int(char *p, int len);

void cpio_ls();

void shell(char * cmd);

void callback_initramfs(void * addr);

int get_initramfs();


#endif // SHELL_H
