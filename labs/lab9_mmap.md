# 需求:
`mmap`（Memory Mapping，内存映射）是一种将文件或设备直接映射到进程虚拟地址空间的机制。它通过将文件内容与内存地址关联，使得程序可以像访问内存一样读写文件，从而避免频繁的`read`/`write`系统调用。
# 函数：
## void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);

**addr:** 指定vma(virtual memory area)连续虚拟内存内存区域的起始地址，若为0则由内核决定
**length**: vma内存空间大小
**prot:**  指定映射的内存权限:R,W,R/W
**flag:** 修改内容是否需要写回文件
**fd**: 目标映射文件的fd
**offset**: 映射文件的偏移量
#作用：默认addr参数为0,将当前进程p->sz作为vma的起始地址,将p->sz更新为p->sz+length。sz为用户地址空间中heap顶指针。
每个进程结构体拥有一个vmas数组，作为可用的vma数量，找到一个未使用的vma
对vma各个字段赋值(基本上对应函数参数)
返回vma的起始地址。
注意: 该函数只分配了虚拟地址空间，还未将文件读入内存且与虚拟地址空间建立映射，当进程通过虚拟地址读写，触发页错误

## munmap(addr, length)
addr: 指定需要取消文件内存映射的起始地址
length: 指定取消映射的大小，
作用: 通过起始地址addr从vmas数组中 定位到vma结构体
此时通过walkaddr函数 确定addr有已经建立的虚拟地址映射
若没有则将vm->length-length，goto ret

进入从addr到addr_end的for循环,每次循环地址自增一个PGSIZE
若需要flag == 1，则在uvmunmap之前，将一个页的大小写回文件
其中 写回文件的offset 为 va - vma->addr_start
将该页通过调用uvmunmap取消映射,并清除对应文件内容在内核地址空间的内存
将vm->len减小

循环结束
```c
if(addr == vm->addr_start){ // 若unmap起始地址为vma起始地址,则修改vma起始位置

    vm->addr_start += length;;

    vm->addr_start = PGROUNDDOWN(vm->addr_start);

  }else{

    // OK

  }
ret:
  if(vm->len == 0){

    p->sz -= vm->len_origin;

    vm->used = 0;

    fileclose(vm->file);

  }
```

## trap.c: 修改usertrap()函数 
修改usertrap()函数 支持scause () = 13,15 的处理,即读写错误的情况
通过stval是否在任意vma范围内，若有则进行错误处理，若无正常跳转到报错
**错误处理:**
```c
uint off = va - vm->addr_start_origin + vm->off; // 计算文件起始偏移量

    if((addr_pmem = kalloc()) == 0)

      goto err;

    memset(addr_pmem, 0, PGSIZE); // 新分配内存清空

    if(mappages(p->pagetable, va, PGSIZE, (uint64)(addr_pmem), (vm->permission << 1) | PTE_U) != 0){

      kfree(addr_pmem);

      goto err;

    }


    ilock(vm->file->ip);

    readi(vm->file->ip, 1, va, off, PGSIZE);

    iunlock(vm->file->ip);
```
不将sepc自增，返回用户程序后重新执行错误指令


## fork修改:
```c
if(uvmcopy(p->pagetable, np->pagetable, p->sz - sz_vma) < 0){  // 复制虚拟内存内容不包括vmas,防止有些页未分配uvmcopy出现page not present错误

    freeproc(np);

    release(&np->lock);

    return -1;

  }
```
```c
for (int i = 0; i < 16; i++) {

    if (p->vmas[i].used) {

      memmove(&np->vmas[i],&p->vmas[i],sizeof(struct vm_area_t));

      filedup(np->vmas[i].file);  // 增加文件引用数

    }

  }
```

## exit修改
进程退出时，就好像调用了munmap一般，清除所有内存映射，写回文件
```c
int ptr_heap = 0; // 用于追踪原始heap指针

  for(int i = 0; i < 16; i++){

    if(p->vmas[i].used == 1){

      ptr_heap += p->vmas[i].len_origin;  

      p->vmas[i].used = 0;

      if(walkaddr(p->pagetable, p->vmas[i].addr_start) == 0) // if is not-mapped unmap

        continue;

      if(p->vmas[i].flags == MAP_SHARED){ // write back dirty

        begin_op();

        ilock(p->vmas[i].file->ip);

        int tot = 0;

        if((tot = writei(p->vmas[i].file->ip, 1, p->vmas[i].addr_start, p->vmas[i].off_write, p->vmas[i].len)) != p->vmas[i].len){ // 写回文件

          printf("tot: %d\n",tot);

          panic("exit write back error");

        }

        iunlock(p->vmas[i].file->ip);

        end_op();

      }

      fileclose(p->vmas[i].file);

      uvmunmap(p->pagetable, p->vmas[i].addr_start, p->vmas[i].len/PGSIZE, 1);

      p->vmas[i].len = 0;

    }

  }

  p->sz -= ptr_heap;  //找到heap顶
```