## Buffer Cache
**原缓存实现**:将所有块缓冲区通过双向链表连接，通过LRU的规则，将最近使用的缓冲区置于链表头部，LRU则放在尾部head->prev，**使得当需要访问特定blockno的缓冲区时，能通过扫描cache链表快速的找到对应的缓冲区**
通过一个spinlock来控制整个链表的访问，
**接口**：当访问某个特定blockno的缓冲区，只能通过接口函数隐式调用bget来访问缓存链表，
当缓存链表没有对应块号的缓冲区时，且缓冲区满，则通过LRU来实现eviction(驱逐)

**问题**：块缓冲区缓存的访问竞争过大，无法实现并行地对缓冲区(buffer)的访问
**解决办法**：将缓存链表分成NBUCKET段，访问特定blockno的数据，通过哈希函数(blockno % NBUCKET)将查找分配到对应的哈希桶上，这样避免了同时对同一临界区的访问，增强了并发性能

```c
struct {

  struct spinlock lock[NBUCKET];

  struct buf buf[NBUF];

  struct buf bucket[NBUCKET];

} bcache;
```

## bget实现细节
通过在buf结构体增加timestamp字段，做到跟踪buf的最近使用时间，来达到严格的LRU，其中timestamp在系统运行的时间范围内不断递增
线程尽量一次性只获得一个锁，必要时让线程获取锁的顺序一致化

```c
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

	  release(&bcache.lock[bucket_no]);// 释放以减少死锁可能

  struct buf *lru_buf = 0;

  int lru_bucket_id = -1;

  uint min_ticks = 0xFFFFFFFF;

  int find_better = 0;

  int previous_id = -1;
	//线程尽量一次性只获得一个锁，必要时让线程获取锁的顺序一致化
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

    }else{         // find better one

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
```