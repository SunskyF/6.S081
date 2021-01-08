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
  struct buf buf[NBUF];
  struct buf table[NBUCKET];
  struct spinlock locks[NBUCKET];
} bcache;

char bucket_names[NBUCKET][16];

void
binit(void)
{
  struct buf *b;

  for (int i = 0; i < NBUCKET; i++) {
    snprintf(bucket_names[i], 16, "bcache%d", i);
    initlock(&bcache.locks[i], bucket_names[i]);
    bcache.table[i].prev = &bcache.table[i];
    bcache.table[i].next = &bcache.table[i];
  }

  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.table[0].next;
    b->prev = &bcache.table[0];
    initsleeplock(&b->lock, "buffer");
    bcache.table[0].next->prev = b;
    bcache.table[0].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  int idx = blockno % NBUCKET;

  acquire(&bcache.locks[idx]);
  // Is the block already cached?
  for(b = bcache.table[idx].next; b != &bcache.table[idx]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.locks[idx]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  for(b = bcache.table[idx].next; b != &bcache.table[idx]; b = b->next){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;

      b->next->prev = b->prev;
      b->prev->next = b->next;

      b->next = bcache.table[idx].next;
      b->prev = &bcache.table[idx];
      bcache.table[idx].next->prev = b;
      bcache.table[idx].next = b;
      release(&bcache.locks[idx]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  int next_i = (idx + 1) % NBUCKET;
  while(next_i != idx){
    acquire(&bcache.locks[next_i]);
    for (b = bcache.table[next_i].prev; b != &bcache.table[next_i]; b = b->prev) {
      if(b->refcnt == 0) {
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;

        b->next->prev = b->prev;
        b->prev->next = b->next;
        release(&bcache.locks[next_i]);

        b->next = bcache.table[idx].next;
        b->prev = &bcache.table[idx];
        bcache.table[idx].next->prev = b;
        bcache.table[idx].next = b;
        release(&bcache.locks[idx]);
        acquiresleep(&b->lock);
        return b;
      }
    }
    release(&bcache.locks[next_i]);
    next_i = (next_i + 1) % NBUCKET;
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

  int i = b->blockno % NBUCKET;

  acquire(&bcache.locks[i]);
  b->refcnt--;

  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.table[i].next;
    b->prev = &bcache.table[i];
    bcache.table[i].next->prev = b;
    bcache.table[i].next = b;
  }
  release(&bcache.locks[i]);
}

void
bpin(struct buf *b) {
  int i = b->blockno % NBUCKET;
  acquire(&bcache.locks[i]);
  b->refcnt++;
  release(&bcache.locks[i]);
}

void
bunpin(struct buf *b) {
  int i = b->blockno % NBUCKET;
  acquire(&bcache.locks[i]);
  b->refcnt--;
  release(&bcache.locks[i]);
}


