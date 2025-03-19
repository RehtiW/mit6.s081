// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"
#define NBUCKET 13
struct {
  struct spinlock lock[NBUCKET];
  struct buf buf[NBUF];
  struct buf bucket[NBUCKET];
} bcache;

uint hash(uint blockno){
  return blockno % NBUCKET;
}

void
binit(void)
{
  struct buf *b;
  for(int i = 0; i < NBUCKET; i++){   // bcache lock init
    initlock(&bcache.lock[i], "bcache.bucket");
    // Create linked list of buffers 
    bcache.bucket[i].prev = &bcache.bucket[i];
    bcache.bucket[i].next = &bcache.bucket[i];
  }

  for(b = bcache.buf; b < bcache.buf+NBUF; b++){  // 头插法
    b->next = bcache.bucket[0].next;
    b->prev = &bcache.bucket[0];
    initsleeplock(&b->lock, "buffer");
    bcache.bucket[0].next->prev = b;
    bcache.bucket[0].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
 
  // Is the block already cached?
  uint bucket_no = hash(blockno);
  acquire(&bcache.lock[bucket_no]);

  for(b = bcache.bucket[bucket_no].next; b != &bcache.bucket[bucket_no]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;              
      release(&bcache.lock[bucket_no]); 
      acquiresleep(&b->lock); 
      return b;
    }
  }
  // still hold bucket lock
  // Not cached.
  // check if current bucket has available buf
  release(&bcache.lock[bucket_no]);
  struct buf *lru_buf = 0;
  int lru_bucket_id = -1;
  uint min_ticks = 0xFFFFFFFF;
  int find_better = 0;
  int previous_id = -1;
  for(int i = 0; i < NBUCKET; i++){
    acquire(&bcache.lock[i]);
    for(b = bcache.bucket[i].next; b != &bcache.bucket[i]; b = b->next) {
      if (b->refcnt == 0 && b->timestamp < min_ticks) { 
        lru_buf = b;
        lru_bucket_id = i;
        min_ticks = b->timestamp;
        find_better = 1;
      }
    }
    if(!find_better){ // didnt find better one in this bucket
      release(&bcache.lock[i]);
    }else{         // find better one
      if(previous_id != -1)
        release(&bcache.lock[previous_id]);
      previous_id = i;
    } 
    find_better = 0;
    
  }

  if(!lru_buf)
    panic("bget: no buffers");

    
  // unlinked frome bucket[lru_bucket_id] 
  lru_buf->prev->next = lru_buf->next;
  lru_buf->next->prev = lru_buf->prev;
  release(&bcache.lock[lru_bucket_id]);

  // linked
  acquire(&bcache.lock[bucket_no]);   
  lru_buf->next = bcache.bucket[bucket_no].next;
  lru_buf->prev = &bcache.bucket[bucket_no];
  bcache.bucket[bucket_no].next = lru_buf;
  lru_buf->next->prev = lru_buf;

  for(b = bcache.bucket[bucket_no].next; b != &bcache.bucket[bucket_no]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt ++;
      release(&bcache.lock[bucket_no]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  
  lru_buf->blockno = blockno; // init
  lru_buf->dev = dev;
  lru_buf->valid = 0;
  lru_buf->refcnt = 1;
  release(&bcache.lock[bucket_no]);

  acquiresleep(&lru_buf->lock); 
  return lru_buf;
  
  
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock)) // buf lock must be held
    panic("brelse");

  releasesleep(&b->lock);
  int blockno = b->blockno;
  int bucket_no = hash(blockno);
  acquire(&bcache.lock[bucket_no]);
  b->refcnt--;
  if (b->refcnt == 0) {
    b->timestamp = ticks; // timestamp update
  }
  release(&bcache.lock[bucket_no]);
}

void
bpin(struct buf *b) {
  uint bucket_no = hash(b->blockno);
  acquire(&bcache.lock[bucket_no]);
  b->refcnt++;
  release(&bcache.lock[bucket_no]);
}

void
bunpin(struct buf *b) {
  uint bucket_no = hash(b->blockno);
  acquire(&bcache.lock[bucket_no]);
  b->refcnt--;
  release(&bcache.lock[bucket_no]);
}


