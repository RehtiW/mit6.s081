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
} kmem;

struct pageref{
  struct spinlock lock;
  int ref_count[(PHYSTOP - KERNBASE) / PGSIZE];
} pageref;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&pageref.lock, "pageref");
  for(int i = 0; i < (PHYSTOP - KERNBASE) / PGSIZE; i++){ // init ref_count
    pageref.ref_count[i] = 0;  
  }
  freerange(end, (void*)PHYSTOP);
  
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE){
    kfree(p);
  }
    
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

  // check reference count
  decref((uint64)pa);
  int idx = ((uint64)pa - KERNBASE) / PGSIZE;
  if(pageref.ref_count[idx] == 0){
    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);
    r = (struct run*)pa;

    // free mem
    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
  }
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r){
    kmem.freelist = r->next;  // set head of freelist to next
    acquire(&pageref.lock);
    int idx = ((uint64)r - KERNBASE) / PGSIZE; 
    pageref.ref_count[idx] = 1;
    release(&pageref.lock);
  }
  release(&kmem.lock);

  if(r){
    memset((char*)r, 5, PGSIZE); // fill with junk
  }
  return (void*)r;
}


void incref(uint64 pa) {
  acquire(&pageref.lock);
  int idx = (pa - KERNBASE) / PGSIZE;
  pageref.ref_count[idx]++;
  release(&pageref.lock);
}

void decref(uint64 pa) {
  acquire(&pageref.lock);
  int idx = (pa - KERNBASE) / PGSIZE;
  if(pageref.ref_count[idx] > 0){
    pageref.ref_count[idx]--;
  }
  release(&pageref.lock);
}

int get_refcount(uint64 pa){
  return pageref.ref_count[(pa-KERNBASE) / PGSIZE];
}