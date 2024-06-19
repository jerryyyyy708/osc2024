#include "uart.h"
#include "shell.h"
#include "dtb.h"
#include "memory.h"
#include "thread.h"
#include "utils.h"
#include "vfs.h"

extern void core_timer_enable();

void foo(){
    for(int i = 0; i < 10; ++i) {
        uart_puts("Thread id: ");
        uart_int(get_pid());
        uart_puts(" ");
        uart_int(i);
        newline();
        delay(1000000);
        schedule();
    }
}

void foo2(){
    uart_puts("infoooooooo");
    uart_int(get_pid());
    newline();
    while(1){}
}

void test_svc(){
    char* usr = "/initramfs/vfs2.img";
    asm volatile("mov x0, %0" : : "r" (usr));
    asm volatile("mov x8, 3");
    asm volatile("svc 0");
    // asm volatile("mov x8, 5");
    // asm volatile("svc 0");
    // asm volatile("mov x8, 0");
    // asm volatile("svc 0");
    // asm volatile("mov x8, 4");
    // asm volatile("svc 0");
    // int result;
    // asm volatile("mov %0, x0" : "=r"(result));
    // //asm volatile("mov x8, 0");
    // hin(result);
    // asm volatile("mov x8, 4");
    // asm volatile("svc 0");
    // asm volatile("mov %0, x0" : "=r"(result));
    // hin(result);
    //asm volatile("svc 0");
}

void cpu_timer_register(){
    unsigned long tmp;
    asm volatile("mrs %0, cntkctl_el1" : "=r"(tmp));
    tmp |= 1;
    asm volatile("msr cntkctl_el1, %0" : : "r"(tmp));
}

void main(void* dtb)
{
    unsigned long el;
    // set up serial console
    uart_init();
    hin(777);
    fdt_tranverse(dtb, "linux,initrd-start", initramfs_start_callback);
    fdt_tranverse(dtb, "linux,initrd-end", initramfs_end_callback);
    // say hello
    frames_init();
    init_rootfs();
    uart_puts("[BOOT] init_rootfs complete!\n\r");
    thread_init();
    uart_puts("[BOOT] thread initialized\n\r");
    // show el
    asm volatile ("mrs %0, CurrentEL" : "=r" (el));
    el = el >> 2;

    // print current exception level
    uart_puts("Booted! Current EL: ");
    uart_send('0' + el);
    uart_puts("\n");

    create_thread(test_svc, 2);
    schedule();

    // struct file * temp;
    // char buf[512];
    // vfs_open("/boot/FAT_W.TXT",0, &temp);
    // int hmn = vfs_read(temp, buf, 10);
    // uart_puts(buf);
    // newline();
    
    //exec("/initramfs/vfs1.img");
    //idle();
    int idx = 0;
    char in_char;
    // echo everything backf
    while(1) {
        char buffer[1024];
        shell_with_dir();
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