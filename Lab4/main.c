#include "uart.h"
#include "shell.h"
#include "dtb.h"
#include "memory.h"

#define MEMORY_POOL_SIZE 1024

static char memory_pool[MEMORY_POOL_SIZE];
static char *next_free = memory_pool;

void *simple_alloc(int size) {
    if (next_free + size - memory_pool > MEMORY_POOL_SIZE) {
        return 0; 
    }
    void *allocated = next_free; 
    next_free += size; 
    return allocated; 
}


void main(void *dtb)
{
    // set up serial console
    uart_init();

    fdt_tranverse(dtb, "linux,initrd-start", initramfs_callback);
    // say hello
    frames_init();
    demo_page_alloc();
    init_memory();

    uart_puts("Start testing malloc and free.\n\r");
    char * test = malloc(5);
    uart_hex(test);
    test[0] = 'i';
    test[1] = 'a';
    test[2] = 'm';
    test[3] = 'j';
    test[4] = '\0';
    uart_puts("\n");
    uart_hex(test);
    uart_puts("\n");
    uart_puts(test);
    uart_puts("\n");
    free(test);

    char in_char;
    int idx = 0;

    // echo everything back
    while(1) {
        char buffer[1024];
        uart_send('\r');
        uart_puts("# ");
        while(1){
            in_char = uart_getc();
            uart_send(in_char);
            if(in_char == '\n'){
                buffer[idx] = '\0';
                shell(buffer);
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