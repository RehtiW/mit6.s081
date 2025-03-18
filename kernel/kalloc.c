// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];

void
kinit()
{
  for(int i = 0; i < NCPU; i++)
    initlock(&kmem[i].lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off();   // turn off interrupts
  int id_cpu = cpuid();
  pop_off();

  acquire(&kmem[id_cpu].lock);
  r->next = kmem[id_cpu].freelist;
  kmem[id_cpu].freelist = r;
  release(&kmem[id_cpu].lock);
}
struct run* steal_one_page(int id_cpu){
  for(int i = id_cpu + 1; i % NCPU != id_cpu; i++){
    int id_other_cpu = i % NCPU;
    acquire(&kmem[id_other_cpu].lock);  // acquire then check
    if(kmem[id_other_cpu].freelist){  // steal
      struct run *ret = kmem[id_other_cpu].freelist;
      kmem[id_other_cpu].freelist = kmem[id_other_cpu].freelist->next;
      release(&kmem[id_other_cpu].lock);
      return ret;
    }
    release(&kmem[id_other_cpu].lock);
  }
  return 0;
}
struct run* steal_half(int id_cpu){
  for(int i = id_cpu + 1; i % NCPU != id_cpu; i++){
    int id_other_cpu = i % NCPU;

    acquire(&kmem[id_other_cpu].lock);  // lock other's list
    if(kmem[id_other_cpu].freelist){  
      struct run *slow;
      struct run *fast;
      slow = fast = kmem[id_other_cpu].freelist;
      if(!slow->next || !slow->next->next){  // other cpu has only one or tow free mem page
        return steal_one_page(id_cpu);
      }
      while(fast && fast->next){
        slow = slow->next;
        fast = fast->next->next;
      }
      kmem[id_cpu].freelist = slow->next;
      struct run *ret = kmem[id_cpu].freelist;
      kmem[id_cpu].freelist =  kmem[id_cpu].freelist->next;
      slow->next = 0;
    
      release(&kmem[id_other_cpu].lock);  // release
      return ret;
    }
    release(&kmem[id_other_cpu].lock);  // release
  }
  return 0;
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.

void *
kalloc(void)
{
  struct run *r;

  push_off(); // turn off interrupts
  int id_cpu = cpuid();
  pop_off();

  acquire(&kmem[id_cpu].lock);  // 
  r = kmem[id_cpu].freelist;
  if(r){    // if current cpu's freelist has free mem
    kmem[id_cpu].freelist = r->next;
    release(&kmem[id_cpu].lock);  // 

  }else{    // steal frome other cpu
    release(&kmem[id_cpu].lock);  // avoid dead lock
    r = steal_one_page(id_cpu);
  }

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
