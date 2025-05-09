## 空闲列表、全局内存管理kmem、内存共享引用管理
```c
struct run {

  struct run *next;  

};
struct {

  struct spinlock lock;

  struct run *freelist;

} kmem;
struct {

  struct spinlock lock;

  int ref_count[(PHYSTOP - KERNBASE) / PGSIZE];

} pageref;
```

## kfree函数
```c
void kfree(void *pa) {
  // ... 检查地址合法性 ...

  // 将物理页的首地址强制转换为 struct run* 类型的指针
  struct run *r = (struct run*)pa;

  // 更新引用计数逻辑（此处省略）...

  // 将当前页插入空闲链表头部
  acquire(&kmem.lock);
  r->next = kmem.freelist;  // 使用 r->next 存储链表指针
  kmem.freelist = r;        // 更新链表头
  release(&kmem.lock);
}
```
kfree函数将每个释放的物理页地址强制转化为struct run* 类型指针，将指向下一个空闲页的struct run指针存放在该物理页(以pa为起始地址)的第一个字节处
**本质:将物理页的内存空间复用为链表节点，无需额外分配内存。**

## kalloc
```c
void *kalloc(void) {
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;       // 获取链表头节点（struct run* 类型）
  if (r) {
    kmem.freelist = r->next; // 更新链表头
  }
  release(&kmem.lock);

  if (r) {
    memset((char*)r, 5, PGSIZE); // 初始化页内容
  }
  return (void*)r; // 将 struct run* 转换为 void* 返回
}
```

### **总结**

通过强制类型转换 `struct run*`，代码将物理页的首地址复用为链表节点，实现高效的内存管理。这一设计的关键在于：

1. **零额外内存开销**：链表指针直接存储在空闲页自身内存中。
    
2. **地址对齐保证**：物理页按 `PGSIZE` 对齐，确保指针操作有效。
    
3. **并发安全**：通过自旋锁（`kmem.lock`）保护链表操作。