#include "uart.h"
#include "shell.h"
#include "dtb.h"
#include "memory.h"
#include "thread.h"

void foo(){
    for(int i = 0; i < 10; ++i) {
        uart_puts("Thread id: ");
        uart_int(get_pid());
        uart_puts(" ");
        uart_int(i);
        newline();
        int r = 1000000;
        while(r--) { asm volatile("nop"); }
        schedule();
    }
}

void main(void* dtb)
{
    unsigned long el;
    // set up serial console
    uart_init();
    fdt_tranverse(dtb, "linux,initrd-start", initramfs_start_callback);
    fdt_tranverse(dtb, "linux,initrd-end", initramfs_end_callback);

    // say hello
    frames_init();
    // show el
    asm volatile ("mrs %0, CurrentEL" : "=r" (el));
    el = el >> 2;
    // thread_init();
    // for(int i=0;i<5;i++){
    //     create_thread(foo);
    // }
    // uart_getc();
    // //schedule();
    // idle();
    
    //print current exception level
    uart_puts("Booted! Current EL: ");
    uart_send('0' + el);
    uart_puts("\n");
    //core_timer_enable();
    int idx = 0;
    char in_char;
    // echo everything backf
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