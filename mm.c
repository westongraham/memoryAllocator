#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

#define ALIGNMENT         8         
#define WSIZE             4         
#define DSIZE             8         
#define INITSIZE          16        
#define MINBLOCKSIZE      16        

#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)
#define MAX(x, y) ((x) > (y)? (x) : (y))
#define PACK(size, alloc) ((size) | (alloc))
#define GET(p)        (*(size_t *)(p))
#define PUT(p, val)   (*(size_t *)(p) = (val))
#define GET_SIZE(p)  (GET(p) & ~0x1)
#define GET_ALLOC(p) (GET(p) & 0x1)
#define HDRP(bp)     ((void *)(bp) - WSIZE)
#define FTRP(bp)     ((void *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
#define NEXT_BLKP(bp) ((void *)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp) ((void *)(bp) - GET_SIZE(HDRP(bp) - WSIZE))
#define NEXT_FREE(bp)(*(void **)(bp))
#define PREV_FREE(bp)(*(void **)(bp + WSIZE))

// PROTOTYPES
static void *extend_heap(size_t words);
static void *find_fit(size_t size);
static void *coalesce(void *bp);
static void remove_freeblock(void *bp);
static void place(void *bp, size_t asize);

static char *heap_listp = 0;  /* Points to the start of the heap */
static char *free_listp = 0;  /* Poitns to the frist free block */

//mm_init start
int mm_init(void) {
  if ((heap_listp = mem_sbrk(INITSIZE + MINBLOCKSIZE)) == (void *)-1)
      return -1;
  PUT(heap_listp +    WSIZE,  PACK(MINBLOCKSIZE, 0));           
  PUT(heap_listp + (2*WSIZE), PACK(0,0));                       
  PUT(heap_listp + (3*WSIZE), PACK(0,0));                       
  PUT(heap_listp + (4*WSIZE), PACK(MINBLOCKSIZE, 0));           
  PUT(heap_listp + (5*WSIZE), PACK(0, 1));                      
  free_listp = heap_listp + (WSIZE);

  return 0;
}
//mm_init end

//mm_malloc start
void *mm_malloc(size_t size) {

  if (size == 0)
      return NULL;

  size_t asize;       // Adjusted block size
  size_t extendsize;  // Amount to extend heap by if no fit
  char *bp;

  asize = MAX(ALIGN(size) + DSIZE, MINBLOCKSIZE);

  if ((bp = find_fit(asize))) {
    place(bp, asize);
    return bp;
  }

  extendsize = MAX(asize, MINBLOCKSIZE);
  if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
    return NULL;

  place(bp, asize);

  return bp;
}
//mm_malloc end

//mm_heapCheck start
void *mm_heapCheck(size_t size) {
  
  void *bp;

    
  for (bp = free_listp; GET_ALLOC(HDRP(bp)) == 0; bp = NEXT_FREE(bp)) {
	    printf("%ld\n",GET_SIZE(HDRP(bp)));
            return bp;
  }
  return NULL;
}
//mm_heapCheck end

//extend_heap start
static void *extend_heap(size_t words) {
  char *bp;
  size_t asize;

  asize = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
  if (asize < MINBLOCKSIZE)
    asize = MINBLOCKSIZE;

  if ((bp = mem_sbrk(asize)) == (void *)-1)
    return NULL;

  PUT(HDRP(bp), PACK(asize, 0));
  PUT(FTRP(bp), PACK(asize, 0));
  PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

  return coalesce(bp);
}
//extend_heap end

//find_fit start
static void *find_fit(size_t size) {
  void *bp;

  for (bp = free_listp; GET_ALLOC(HDRP(bp)) == 0; bp = NEXT_FREE(bp)) {
    if (size <= GET_SIZE(HDRP(bp)))
      return bp;
  }
  return NULL;
}
//find_fit end

//remove_freeblock start
static void remove_freeblock(void *bp) {
  if(bp) {
    if (PREV_FREE(bp))
      NEXT_FREE(PREV_FREE(bp)) = NEXT_FREE(bp);
    else
      free_listp = NEXT_FREE(bp);
    if(NEXT_FREE(bp) != NULL)
      PREV_FREE(NEXT_FREE(bp)) = PREV_FREE(bp);
  }
}
//remove_freeblock end

//place start
static void place(void *bp, size_t asize) {  
  size_t fsize = GET_SIZE(HDRP(bp));

  if((fsize - asize) >= (MINBLOCKSIZE)) {

    PUT(HDRP(bp), PACK(asize, 1));
    PUT(FTRP(bp), PACK(asize, 1));
    remove_freeblock(bp);
    bp = NEXT_BLKP(bp);
    PUT(HDRP(bp), PACK(fsize-asize, 0));
    PUT(FTRP(bp), PACK(fsize-asize, 0));
    coalesce(bp);
  }

  else {

    PUT(HDRP(bp), PACK(fsize, 1));
    PUT(FTRP(bp), PACK(fsize, 1));
    remove_freeblock(bp);
  }
}
//place end

//coalesce start
static void *coalesce(void *bp) {
  size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))) || PREV_BLKP(bp) == bp;
  size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));

  size_t size = GET_SIZE(HDRP(bp));

  if (prev_alloc && !next_alloc) {          
    size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
    remove_freeblock(NEXT_BLKP(bp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
  }

  else if (!prev_alloc && next_alloc) {      
    size += GET_SIZE(HDRP(PREV_BLKP(bp)));
    bp = PREV_BLKP(bp);
    remove_freeblock(bp);
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
  }

  else if (!prev_alloc && !next_alloc) {    
    size += GET_SIZE(HDRP(PREV_BLKP(bp))) +
            GET_SIZE(HDRP(NEXT_BLKP(bp)));
    remove_freeblock(PREV_BLKP(bp));
    remove_freeblock(NEXT_BLKP(bp));
    bp = PREV_BLKP(bp);
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
  }

  NEXT_FREE(bp) = free_listp;
  PREV_FREE(free_listp) = bp;
  PREV_FREE(bp) = NULL;
  free_listp = bp;

  return bp;
}
//coalesce end

//free start
void mm_free(void *bp)
{ 
  
  if (!bp)
      return;

  size_t size = GET_SIZE(HDRP(bp));

  PUT(HDRP(bp), PACK(size, 0));
  PUT(FTRP(bp), PACK(size, 0));

  coalesce(bp);
}
//free end

//main method start
int main(int argc, char *argv[]) {

	

}
//main method end
