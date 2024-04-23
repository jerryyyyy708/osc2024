# OSC2024 Lab4
Author: jerryyyyy708 (just to make sure my code is not copied by anyone)

## Note
* lab4-kernel8-basic+adv1: more clear to show page allocate/merge/free
* lab4-kernel8-no_reserve: for demo without reserve memory -> to show allocate split and free merge
* lab4-kernel8-all: with memory allocation and reserve

## Difference with startup allocation
* without startup allocation, the buddy system is placed in bss
* with startup allocation, the buddy system is placed in the memory allocated by simple_alloc

要在runtime才會知道總共有多少memory可以用，所以會需要動態分配如frame array等等的大小(不像現在寫的時候是一開始就把可以用的空間定義在程式裡，所以可以直接在global裡面給定array大小)

## Memory Reserve
* Startup allocator is placed in kernel, which is already reserved. The buddy system is placed right after the kernel, can be reserved together.

## TODO
1. Check the usable memory by dtb V
2. Replace Memory Start, Total and Frame_count by dtb and dynamic allocate it instead of hardcode. V

1. Add hardcode version to the vm
2. check why is the free list like that (%32 = 19 = 16 + 2 + 1)
3. why xor ... demo things
 


## Demo問
* Lab3 Task Queue，只要加了 
```
msr DAIFClr, 0xf
msr DAIFSet, 0xf
```
interrupt就會完全跑不了，我的寫法是 async 時開啟 uart_interrupt，之後就都用 async uart。