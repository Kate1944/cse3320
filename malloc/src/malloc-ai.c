#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define ALIGN4(s)         (((((s) - 1) >> 2) << 2) + 4)
#define BLOCK_DATA(b)     ((b) + 1)
#define BLOCK_HEADER(ptr) ((struct _block *)(ptr) - 1)

static int atexit_registered = 0;
static int num_mallocs       = 0;
static int num_frees         = 0;
static int num_reuses        = 0;
static int num_grows         = 0;
static int num_splits        = 0;
static int num_coalesces     = 0;
static int num_blocks        = 0;
static int num_requested     = 0;
static int max_heap          = 0;

/*
 *  \brief printStatistics
 *
 *  \param none
 *
 *  Prints the heap statistics upon process exit.  Registered
 *  via atexit()
 *
 *  \return none
 */
void printStatistics( void )
{
  printf("\nheap management statistics\n");
  printf("mallocs:\t%d\n", num_mallocs );
  printf("frees:\t\t%d\n", num_frees );
  printf("reuses:\t\t%d\n", num_reuses );
  printf("grows:\t\t%d\n", num_grows );
  printf("splits:\t\t%d\n", num_splits );
  printf("coalesces:\t%d\n", num_coalesces );
  printf("blocks:\t\t%d\n", num_blocks );
  printf("requested:\t%d\n", num_requested );
  printf("max heap:\t%d\n", max_heap );
}

struct _block 
{
   size_t  size;         /* Size of the allocated _block of memory in bytes     */
   struct _block *next;  /* Pointer to the next _block of allocated memory      */
   struct _block *prev;  /* Pointer to the previous _block of allocated memory  */
   bool   free;          /* Is this _block free?                                */
   char   padding[3];    /* Padding: IENTRTMzMjAgU3jMDEED                       */
};


struct _block *heapList = NULL; /* Free list to track the _blocks available */

/*
 * \brief findFreeBlock
 *
 * \param last pointer to the linked list of free _blocks
 * \param size size of the _block needed in bytes 
 *
 * \return a _block that fits the request or NULL if no free _block matches
 *
 * \TODO Implement Next Fit
 * \TODO Implement Best Fit
 * \TODO Implement Worst Fit
 */
struct _block *findFreeBlock(struct _block **last, size_t size) 
{
   struct _block *curr = heapList;

#if defined FIT && FIT == 0
   /* First fit */
   //
   // While we haven't run off the end of the linked list and
   // while the current node we point to isn't free or isn't big enough
   // then continue to iterate over the list.  This loop ends either
   // with curr pointing to NULL, meaning we've run to the end of the list
   // without finding a node or it ends pointing to a free node that has enough
   // space for the request.
   // 
   while (curr && !(curr->free && curr->size >= size)) 
   {
      *last = curr;
      curr  = curr->next;
   }
#endif

// \TODO Put your Best Fit code in this #ifdef block
#if defined BEST && BEST == 0
   struct _block *curr = heapList;
   struct _block *bestBlock = NULL;
   size_t minWaste = (size_t)-1;  // Start with the maximum possible value

   // Iterate over the free list
   while (curr) {
      if (curr->free && curr->size >= size) {
         size_t waste = curr->size - size;  // Leftover space in this block

         // If this block is a better fit (smaller waste) than the previous best
         if (waste < minWaste) {
            minWaste = waste;
            bestBlock = curr;
         }
      }
      curr = curr->next;
   }

   return bestBlock;
#endif


// \TODO Put your Worst Fit code in this #ifdef block
#if defined WORST && WORST == 0
   struct _block *curr = heapList;
   struct _block *worstBlock = NULL;
   size_t maxWaste = 0;  // Start with the smallest possible value

   // Iterate over the free list
   while (curr) {
      if (curr->free && curr->size >= size) {
         size_t waste = curr->size - size;  // Leftover space in this block

         // If this block has more waste (larger leftover space) than the previous worst
         if (waste > maxWaste) {
            maxWaste = waste;
            worstBlock = curr;
         }
      }
      curr = curr->next;
   }

   return worstBlock;
#endif

// \TODO Put your Next Fit code in this #ifdef block
#if defined NEXT && NEXT == 0
   struct _block *curr = heapList;
   struct _block *lastAllocated = *last;  // Start from the last allocated block

   // If lastAllocated is NULL (first allocation), start from the beginning of the list
   if (lastAllocated == NULL) {
      lastAllocated = heapList;
   }

   // Iterate through the free list starting from lastAllocated
   while (curr) {
      if (curr->free && curr->size >= size) {
         *last = curr;  // Update the last allocated block to this one
         return curr;
      }
      curr = curr->next;
   }

   // If we reach the end of the list, wrap around and continue from the beginning
   curr = heapList;
   while (curr != lastAllocated) {
      if (curr->free && curr->size >= size) {
         *last = curr;  // Update the last allocated block to this one
         return curr;
      }
      curr = curr->next;
   }

   // No suitable block found
   return NULL;
#endif


   return curr;
}

/*
 * \brief growheap
 *
 * Given a requested size of memory, use sbrk() to dynamically 
 * increase the data segment of the calling process.  Updates
 * the free list with the newly allocated memory.
 *
 * \param last tail of the free _block list
 * \param size size in bytes to request from the OS
 *
 * \return returns the newly allocated _block of NULL if failed
 */
struct _block *growHeap(struct _block *last, size_t size) 
{
   /* Request more space from OS */
   struct _block *curr = (struct _block *)sbrk(0);
   struct _block *prev = (struct _block *)sbrk(sizeof(struct _block) + size);

   assert(curr == prev);

   /* OS allocation failed */
   if (curr == (struct _block *)-1) 
   {
      return NULL;
   }

   /* Update heapList if not set */
   if (heapList == NULL) 
   {
      heapList = curr;
   }

   /* Attach new _block to previous _block */
   if (last) 
   {
      last->next = curr;
   }

   /* Update _block metadata:
      Set the size of the new block and initialize the new block to "free".
      Set its next pointer to NULL since it's now the tail of the linked list.
   */
   curr->size = size;
   curr->next = NULL;
   curr->free = false;
   return curr;
}

/*
 * \brief malloc
 *
 * finds a free _block of heap memory for the calling process.
 * if there is no free _block that satisfies the request then grows the 
 * heap and returns a new _block
 *
 * \param size size of the requested memory in bytes
 *
 * \return returns the requested memory allocation to the calling process 
 * or NULL if failed
 */
void *malloc(size_t size) 
{

   if( atexit_registered == 0 )
   {
      atexit_registered = 1;
      atexit( printStatistics );
   }

   /* Align to multiple of 4 */
   size = ALIGN4(size);

   /* Handle 0 size */
   if (size == 0) 
   {
      return NULL;
   }

   /* Look for free _block.  If a free block isn't found then we need to grow our heap. */

   struct _block *last = heapList;
   struct _block *next = findFreeBlock(&last, size);

   /* TODO: If the block found by findFreeBlock is larger than we need then:
            If the leftover space in the new block is greater than the sizeof(_block)+4 then
            split the block.
            If the leftover space in the new block is less than the sizeof(_block)+4 then
            don't split the block.
   */
   if (next->size > size + sizeof(struct _block)) {
   struct _block *new_block = (struct _block *)((char *)next + size + sizeof(struct _block));
   new_block->size = next->size - size - sizeof(struct _block);
   next->size = size;
   new_block->next = next->next;
   new_block->free = true;
   next->next = new_block;
   num_splits++;
   }


   /* Could not find free _block, so grow heap */
   if (next == NULL) 
   {
      next = growHeap(last, size);
   }

   /* Could not find free _block or grow heap, so just return NULL */
   if (next == NULL) 
   {
      return NULL;
   }
   
   /* Mark _block as in use */
   next->free = false;

   /* Return data address associated with _block to the user */
   return BLOCK_DATA(next);
}

/*
 * \brief free
 *
 * frees the memory _block pointed to by pointer. if the _block is adjacent
 * to another _block then coalesces (combines) them
 *
 * \param ptr the heap memory to free
 *
 * \return none
 */
void free(void *ptr) 
{
   if (ptr == NULL) 
   {
      return;
   }

   /* Make _block as free */
   struct _block *curr = BLOCK_HEADER(ptr);
   assert(curr->free == 0);
   curr->free = true;

   /* TODO: Coalesce free _blocks.  If the next block or previous block 
            are free then combine them with this block being freed.
   */
   if (curr->next && curr->next->free) {
      curr->size += curr->next->size + sizeof(struct _block);
      curr->next = curr->next->next;
   }
   if (curr->prev && curr->prev->free) {
      curr->prev->size += curr->size + sizeof(struct _block);
      curr->prev->next = curr->next;
   }
}

void *calloc(size_t nmemb, size_t size) {
   size_t total_size = nmemb * size;
   void *ptr = malloc(total_size);
   if (ptr) {
      memset(ptr, 0, total_size);
   }
   return ptr;
}

void *realloc(void *ptr, size_t size) {
   if (ptr == NULL) {
      return malloc(size);
   }
   if (size == 0) {
      free(ptr);
      return NULL;
   }
   
   struct _block *old_block = BLOCK_HEADER(ptr);
   size_t old_size = old_block->size;
   
   if (size <= old_size) {
      old_block->size = size;
      return ptr;
   } else {
      void *new_ptr = malloc(size);
      if (new_ptr) {
         memcpy(new_ptr, ptr, old_size);
         free(ptr);
      }
      return new_ptr;
   }
}



/* vim: IENTRTMzMjAgU3ByaW5nIDIwM001= ----------------------------------------*/
/* vim: set expandtab sts=3 sw=3 ts=6 ft=cpp: --------------------------------*/
