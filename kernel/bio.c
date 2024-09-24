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

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  struct buf bucket[NBUCKET];
  struct spinlock bucket_lock[NBUCKET];
  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  // struct buf head;
} bcache;

struct buf* 
find_free(uint dev, uint bucketId)
{
  struct buf *cur = &bcache.bucket[bucketId];
  struct buf *rlu_buf = 0;
  while(cur->next != 0) {
    if(cur->next->refcnt == 0) {
      if(!rlu_buf || cur->next->timestamp < rlu_buf->timestamp ) {
        rlu_buf = cur->next;
      }
    }
    cur = cur->next;
  }
  return rlu_buf; 
}

void 
print_bucket()
{
  uint count = 0;
  for(int i = 0; i < NBUCKET; i++) {
    struct buf *cur = &bcache.bucket[i];
    while(cur->next != 0) {
      printf("count: %d bucket: %d buf_addr: %p ref: %d\n", ++count, i, cur->next, cur->next->refcnt);
      cur = cur->next;
    }
  }

  return;
}

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  //初始化每个桶对应的锁
  for(int i = 0; i < NBUCKET; i++) {
    char lockname[32];
    snprintf(lockname, 32, "bcache_bucket%d", i);
    initlock(&bcache.bucket_lock[i], lockname);
  }

  struct buf *cur = &bcache.bucket[0];
  for(b = bcache.buf; b < bcache.buf + NBUF; b++) {
    initsleeplock(&b->lock, "buffer");
    cur->next = b;
    cur = cur->next;
  }
  // print_bucket();

  // // Create linked list of buffers
  // bcache.head.prev = &bcache.head;
  // bcache.head.next = &bcache.head;
  // for(b = bcache.buf; b < bcache.buf+NBUF; b++){
  //   b->next = bcache.head.next;
  //   b->prev = &bcache.head;
  //   initsleeplock(&b->lock, "buffer");
  //   bcache.head.next->prev = b;
  //   bcache.head.next = b;
  // }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  // acquire(&bcache.lock);

  uint key = blockno % NBUCKET;
  acquire(&bcache.bucket_lock[key]);

  // Is the block already cached?
  // 先在当前桶里查找是否已经缓存
  struct buf *cur = &bcache.bucket[key];
  while(cur->next != 0) {
    if(cur->next->dev == dev && cur->next->blockno == blockno) {
      cur->next->refcnt++;
      release(&bcache.bucket_lock[key]);
      // release(&bcache.lock);
      acquiresleep(&cur->next->lock);
      return cur->next;
    } else {
      cur = cur->next;
    }
  }
  // for(b = bcache.head.next; b != &bcache.head; b = b->next){
  //   if(b->dev == dev && b->blockno == blockno){
  //     b->refcnt++;
  //     release(&bcache.lock);
  //     acquiresleep(&b->lock);
  //     return b;
  //   }
  // }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  // 在当前桶里看是否有空闲buf
  struct buf *rlu_buf = find_free(dev, key);
  // 如果找到了空闲buf 初始化设置并返回
  if(rlu_buf) {
    rlu_buf->dev = dev;
    rlu_buf->blockno = blockno;
    rlu_buf->valid = 0;
    rlu_buf->refcnt = 1;
    release(&bcache.bucket_lock[key]);
    // release(&bcache.lock);
    acquiresleep(&rlu_buf->lock);
    return rlu_buf;
  }
  
  // for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
  //   if(b->refcnt == 0) {
  //     b->dev = dev;
  //     b->blockno = blockno;
  //     b->valid = 0;
  //     b->refcnt = 1;
  //     release(&bcache.lock);
  //     acquiresleep(&b->lock);
  //     return b;
  //   }
  // }

  // 如果没找到，则需要遍历每个桶找到空闲块进行驱逐 
  acquire(&bcache.lock);
  for(int i = 0; i < NBUCKET; i++) {
    if(i != key) {
      acquire(&bcache.bucket_lock[i]);
      struct buf *eviction_buf = find_free(dev, i);
      if(!eviction_buf) { //这个桶没找到就换下一个
        release(&bcache.bucket_lock[i]);
        continue;
      } 
      // 找到了 则进行移动 先更新旧链表
      cur = &bcache.bucket[i];
      // cur 找到 eviction_buf 的前继节点
      while(cur->next != eviction_buf) {
        if(cur->next == 0)
          panic("bget: no eviction_buf");
        cur = cur->next;
      }
      cur->next = eviction_buf->next;
      // 接下来更新新链表
      cur = &bcache.bucket[key];
      eviction_buf->next = cur->next;
      bcache.bucket[key].next = eviction_buf;

      eviction_buf->blockno = blockno;
      eviction_buf->dev = dev;
      eviction_buf->valid = 0;
      eviction_buf->refcnt = 1;

      // print_bucket();

      release(&bcache.bucket_lock[i]);
      release(&bcache.lock);
      release(&bcache.bucket_lock[key]);
      acquiresleep(&eviction_buf->lock);
      return eviction_buf;
    }
  }

  panic("bget: no buffers");
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
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  // acquire(&bcache.lock);

  uint key = b->blockno % NBUCKET;
  acquire(&bcache.bucket_lock[key]);
  b->refcnt--;
  if (b->refcnt == 0) {
    b->timestamp = ticks;
    // no one is waiting for it.
    // b->next->prev = b->prev;
    // b->prev->next = b->next;
    // b->next = bcache.head.next;
    // b->prev = &bcache.head;
    // bcache.head.next->prev = b;
    // bcache.head.next = b;
  }
  release(&bcache.bucket_lock[key]);
  // release(&bcache.lock);
}

void
bpin(struct buf *b) {
  uint key = b->blockno % NBUCKET;
  acquire(&bcache.bucket_lock[key]);
  b->refcnt++;
  release(&bcache.bucket_lock[key]);
}

void
bunpin(struct buf *b) {
  uint key = b->blockno % NBUCKET;
  acquire(&bcache.bucket_lock[key]);
  b->refcnt--;
  release(&bcache.bucket_lock[key]);
}


