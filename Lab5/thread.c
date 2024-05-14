#include "memory.h"
#include "thread.h"
#include "uart.h"

// kernel stack: trapframe,... etc
// user stack: user variables ...

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
}

void thread_exit(){
    //set state to the end and schedule
    thread * cur = get_current();
    cur -> state = -1; //end
    if(cur -> priority == min_priority){
        cur -> priority = 999;
        update_min_priority();
    }
    schedule();
}

void thread_execute(){
    // pack the function for state management
    uart_puts("In exec pid: ");
    uart_int(get_current() -> pid);
    newline();
    thread * cur = get_current();
    cur -> funct();
    uart_puts("End pid: ");
    uart_int(get_current() -> pid);
    newline();
    thread_exit();
}

int create_thread(void * function, int priority){
    int pid;
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
    t -> priority = priority; //lower means higher in my implementation (Confusing XD)
    t -> funct = function;
    //add 4096(PAGE_SIZE) because stack grows up
    t -> sp_el1 = ((unsigned long)allocate_page(4096)) + 4096; //kernel stack for sp, trapframe
    t -> sp_el0 = ((unsigned long)allocate_page(4096)) + 4096; //user stack for user program
    t -> regs.lr = thread_execute; // ret jumps to lr
    t -> regs.sp = t -> sp_el1;
    update_min_priority();
    thread_pool[pid] = t; 
    return pid;
}

int get_pid(){
    return get_current() -> pid;//no runnning thread
}

void kill_zombies(){
    //free all alocated memory for zombie threads and set thread pid to NULL
    for(int i=0;i<MAX_TASK;i++){
        if(thread_pool[i] == 0)
            continue;
        if(thread_pool[i] -> state == -1){//recycle thread with state -1
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

    //find min priority thread closest to current thread
    for(int i = get_current() -> pid + 1; i<MAX_TASK; i++){ 
        if(thread_pool[i] == 0 || thread_pool[i] -> state != 1 || thread_pool[i] -> priority > min_priority)
            continue;
        next = i;
        break;
    }

    //start from left most if didn't find in right
    if(next == get_current() -> pid){
        for(int i = 0; i<get_current() -> pid; i++){
            if(thread_pool[i] == 0 || thread_pool[i] -> state != 1 || thread_pool[i] -> priority > min_priority) 
                continue;
            next = i;
            break;
        }
    }

    //switch to next thread
    if(get_current() -> pid != next && thread_pool[next] != 0 && thread_pool[next] -> state == 1){
        thread * from = get_current();
        thread * to = thread_pool[next];
        switch_to(from, to); //x0: from, x1: to, store regs to struct and load regs from struct
    }
}

void idle(){
    //infinite loop for scheduling
    while(1){
        kill_zombies();
        schedule();
    }
}

void thread_init(){
    //make main process as a thread
    min_priority = 10;
    thread * t = allocate_page(sizeof(thread));
    for(int i = 0; i< sizeof(thread); i++){
        ((char*)t)[i] = 0;
    }
    t -> pid = 0;
    t -> state = 1;
    t -> parent = -1;
    t -> priority = 10;
    thread_pool[0] = t;
    asm volatile ("msr tpidr_el1, %0"::"r"((unsigned long)thread_pool[0]));
	asm volatile ("msr sp_el0, %0"::"r"(thread_pool[0] -> sp_el0)); //the stack pointer for user program in el0
}
