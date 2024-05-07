#include "memory.h"
#include "thread.h"
#include "uart.h"

//kernel stack: trapframe,... etc
//user stack: user variables ...

extern void switch_to();
extern thread* get_current();
int min_priority = 999;


#define MAX_TASK 64
thread * thread_pool[MAX_TASK];

void update_min_priority(){
    min_priority = 999;
    for(int i=0; i<MAX_TASK; i++){
        if(thread_pool[i] == 0 || thread_pool[i] -> state != 1)
            continue;
        if(min_priority > thread_pool[i] -> priority)
            min_priority = thread_pool[i] -> priority;
    }
    // uart_puts("Current min priority: ");
    // uart_int(min_priority);
    // newline();
}

void thread_exit(){
    thread * cur = get_current();
    cur -> state = -1; //end
    if(cur -> priority == min_priority){
        cur -> priority = 999;
        update_min_priority();
    }
    schedule();
}

void thread_execute(){
    uart_puts("In exec pid: ");
    uart_int(get_current() -> pid);
    newline();
    // unsigned long sp;
    // asm volatile ("mrs %0, sp_el1" : "=r" (sp));
    // uart_int(sp);
    // newline();
    thread * cur = get_current();
    cur -> funct();
    uart_puts("End pid: ");
    uart_int(get_current() -> pid);
    newline();
    thread_exit();
}

int create_thread(void * function, int priority){
    int pid;
    //give it a page, page minus header is free space
    thread * t = allocate_page(sizeof(thread));//malloc(sizeof(struct thread));
    for(int i=0; i<MAX_TASK; i++){
        if(thread_pool[i] == 0){
            pid = i;
            uart_puts("Create thread with PID ");
            uart_int(i);
            newline();
            break;
        }
        if(i == MAX_TASK - 1){
            uart_puts("Error, no more threads\n\r");
            return -1;
        }
    }

    for(int i = 0; i< sizeof(struct registers); i++){
        ((char*)(&(t -> regs)))[i] = 0; 
    }

    t -> pid = pid;
    t -> state = 1; //running
    t -> parent = -1;
    t -> priority = priority;
    t -> funct = function;
    t -> sp_el1 = ((unsigned long)allocate_page(4096)) + 4096;//((unsigned long)t + 4096); //sp start from bottom
    t -> sp_el0 = ((unsigned long)allocate_page(4096)) + 4096;
    t -> regs.lr = thread_execute;
    t -> regs.sp = t -> sp_el1;
    update_min_priority();
    thread_pool[pid] = t; 
    return pid;
}

int get_pid(){
    return get_current() -> pid;//no runnning thread
}

void kill_zombies(){
    for(int i=0;i<MAX_TASK;i++){
        if(thread_pool[i] == 0)
            continue;
        if(thread_pool[i] -> state == -1){//recycle thread
            free_page(thread_pool[i] -> sp_el0 - 4096);
            free_page(thread_pool[i] -> sp_el1 - 4096);
            free_page(thread_pool[i]);
            thread_pool[i] = 0;
            uart_puts("Killed zombie PID ");
            uart_int(i);
            newline();
        }
    }
}

void schedule(){
    update_min_priority();
    int next = get_current() -> pid;
    for(int i = get_current() -> pid + 1; i<MAX_TASK; i++){ //find min priority thread
        if(thread_pool[i] == 0 || thread_pool[i] -> state != 1 || thread_pool[i] -> priority > min_priority)
            continue;
        next = i;
        break;
    }

    if(next == get_current() -> pid){
        for(int i = 0; i<get_current() -> pid; i++){//find next thread with same priority if doesn't switch
            if(thread_pool[i] == 0 || thread_pool[i] -> state != 1 || thread_pool[i] -> priority > min_priority) 
                continue;
            next = i;
            break;
        }
    }

    if(get_current() -> pid != next && thread_pool[next] != 0 && thread_pool[next] -> state == 1){
        thread * from = get_current();
        thread * to = thread_pool[next];
        switch_to(from, to);
    }
    // else{
    //     uart_int(get_current() -> pid);
    //     uart_getc();
    // }
}

void idle(){
    while(1){
        kill_zombies();
        schedule();
    }
}

void do_nothing(){

}

void thread_init(){ // a nop thread for idle
    // min_priority = 10;
    // thread * t = allocate_page(sizeof(thread));
    // for(int i = 0; i< sizeof(struct registers); i++){
    //     ((char*)(&(t -> regs)))[i] = 0;
    // }
    // t -> state = 1;
    // t -> parent = -1;
    // t -> priority = 10;
    // t -> funct = 0;
    // t -> sp_el1 = 0;
    // t -> sp_el0 = 0;
    // thread_pool[0] = t;
    min_priority = 10;
    create_thread(do_nothing, 10);
    asm volatile ("msr tpidr_el1, %0"::"r"((unsigned long)thread_pool[0]));
	asm volatile ("msr sp_el0, %0"::"r"(thread_pool[0] -> sp_el0));
}
