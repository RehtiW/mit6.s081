## 懒惰分配
系统调用fork时，未优化的情况是分配内存，复制父进程所有页，创建一个子进程页表，将页表项映射到所有复制的新页，
缺点：性能浪费，有时fork操作紧接着exec，即不需要使用大部分复制的父进程数据，白复制了
解决办法：lazy allocation
父子进程共享所有页，当遇到可写的页时，将父子进程对应的pte:设置PTE_COW，取消PTE_W，当尝试写入时，发生写时复制
```c
int

uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)

{

  pte_t *pte;

  uint64 pa, i;

  uint flags;

  

  for(i = 0; i < sz; i += PGSIZE){

    if((pte = walk(old, i, 0)) == 0)    // parent_pagetable's pte

      panic("uvmcopy: pte should exist");

    if((*pte & PTE_V) == 0)

      panic("uvmcopy: page not present");

    pa = PTE2PA(*pte);

  

    if (*pte & PTE_W) { // set writable page unwritable, PTE_COW

      *pte &= ~PTE_W;    

      *pte |= PTE_COW;    

    }

    flags = PTE_FLAGS(*pte); // update

    if(mappages(new, i, PGSIZE, pa, flags) != 0){ // 将该父亲物理页映射到子页表new的对应位置

      goto err;

    }

    incref(pa);

  }

  return 0;

  

 err:

  uvmunmap(new, 0, i / PGSIZE, 1);

  return -1;

}
```
## 写时复制在xv6流程
### 1.
当进程尝试往
设置了PTE_COW位的pte对应的物理页写入时
即该页是cow的共享页，同时也设置了unwritable
### 2.
因尝试写入未设置PTE_W的页，触发pagefault，错误号15
### 3.
user trap函数通过调用cow_alloc，分配新页给该进程，将对应pte重新映射到new_page，取消对old_page的引用
```c
int

cow_alloc(pagetable_t pagetable, uint64 va)

{

  va = PGROUNDDOWN(va);

  if(va >= MAXVA)

    return -1;

  

  pte_t *pte = walk(pagetable, va, 0);

  if(pte == 0 || (*pte & PTE_V) == 0 || (*pte & PTE_COW) == 0)

    return -1;

  

  uint64 pa = PTE2PA(*pte);

  uint64 new_page = (uint64)kalloc();

  if(new_page == 0)

    return -1;

  

  // 复制原页面内容到新页面

  memmove((void*)new_page, (void*)pa, PGSIZE);

  

  // 更新页表项，指向新页面，设置可写位，清除牛位

  // 更新：不使用mappages使pte重映射到新页，避免了remap panic的麻烦

  uint flags = (PTE_FLAGS(*pte) & ~PTE_COW) | PTE_W;

  *pte = PA2PTE(new_page) | flags;

  

  // 释放原页面的引用

  kfree((void*)pa);

  

  return 0;

}
```

因为只是修改已有的pte的映射，即受cow_alloc函数处理的页表已有pte，最好不要使用mappages函数，**mappages函数主要为未初始化的页表服务**，如果利用mappages重新映射，容易导致panic: remap的后果
更新后通过新分配物理页的地址，和经过取消cow位，设置可写处理的flags
对pte重新赋值

### 4.
 陷阱处理的后续阶段，恢复用户寄存器，satp等，从原来发生错误的位置重新执行


