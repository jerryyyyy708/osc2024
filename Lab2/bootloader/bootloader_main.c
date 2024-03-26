#include "uart.h"

int strcmp(char *s1, char *s2) {
    while (*s1 != '\0' && *s1 == *s2) {
        s1++;
        s2++;
    }
    return (*(unsigned char *)s1) - (*(unsigned char *)s2);
}

void bootloader_main(void *dtb)
{
    // set up serial console
    uart_init();
    uart_puts("Bootloader Initialized!\r\n");
    int idx = 0;
    char in_char;
    while(1) {
        char buffer[1024];
        uart_puts("\r# ");
        while(1){
            in_char = uart_getc();
            uart_send(in_char);
            if(in_char == '\n'){
                buffer[idx] = '\0';
                if(strcmp(buffer, "boot")==0){
                    uart_puts("\rUse send_loader.py to load kernel\r\n");
                    unsigned int size = 0;
                    unsigned char *size_buffer = (unsigned char *) &size;
                    for(int i=0; i<4; i++) 
                        size_buffer[i] = uart_getc();
                    uart_puts("got kernel size, start receiving kernel\r\n");

                    char *kernel = (char *) 0x80000;
                    while(size--) *kernel++ = uart_getc();

                    uart_puts("kernel-loaded\r\n");
                    void (*kernel_entry)(void *) = (void (*)(void))0x80000;
                    kernel_entry(dtb);
                    // asm volatile (
                    //     "mov x30, #0x80000; mov x0, x28; ret"
                    // );
                    return;
                }
                idx = 0;
                break;
            }
            else{
                buffer[idx] = in_char;
                idx++;
            }
        }

    }
}

