/******************************************************************************
 * FILENAME: mem.c
 * AUTHOR:   cherin@cs.wisc.edu <Cherin Joseph>
 * DATE:     20 Nov 2013
 * PROVIDES: Contains a set of library functions for memory allocation
 * MODIFIED BY: Peter S. Procek, Section #1
 * *****************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include "mem.h"

/* this structure serves as the header for each block */
typedef struct block_hd{
  /* The blocks are maintained as a linked list */
  /* The blocks are ordered in the increasing order of addresses */
  struct block_hd* next;

  /* size of the block is always a multiple of 4 */
  /* ie, last two bits are always zero - can be used to store other information*/
  /* LSB = 0 => free block */
  /* LSB = 1 => allocated/busy block */

  /* For free block, block size = size_status */
  /* For an allocated block, block size = size_status - 1 */

  /* The size of the block stored here is not the real size of the block */
  /* the size stored here = (size of block) - (size of header) */
  int size_status;

}block_header;

/* Global variable - This will always point to the first block */
/* ie, the block with the lowest address */
block_header* list_head = NULL;


/* Function used to Initialize the memory allocator */
/* Not intended to be called more than once by a program */
/* Argument - sizeOfRegion: Specifies the size of the chunk which needs to be allocated */
/* Returns 0 on success and -1 on failure */
int Mem_Init(int sizeOfRegion)
{
  int pagesize;
  int padsize;
  int fd;
  int alloc_size;
  void* space_ptr;
  static int allocated_once = 0;
  
  if(0 != allocated_once)
  {
    fprintf(stderr,"Error:mem.c: Mem_Init has allocated space during a previous call\n");
    return -1;
  }
  if(sizeOfRegion <= 0)
  {
    fprintf(stderr,"Error:mem.c: Requested block size is not positive\n");
    return -1;
  }

  /* Get the pagesize */
  pagesize = getpagesize();

  /* Calculate padsize as the padding required to round up sizeOfRegio to a multiple of pagesize */
  padsize = sizeOfRegion % pagesize;
  padsize = (pagesize - padsize) % pagesize;

  alloc_size = sizeOfRegion + padsize;

  /* Using mmap to allocate memory */
  fd = open("/dev/zero", O_RDWR);
  if(-1 == fd)
  {
    fprintf(stderr,"Error:mem.c: Cannot open /dev/zero\n");
    return -1;
  }
  space_ptr = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
  if (MAP_FAILED == space_ptr)
  {
    fprintf(stderr,"Error:mem.c: mmap cannot allocate space\n");
    allocated_once = 0;
    return -1;
  }
  
  allocated_once = 1;
  
  /* To begin with, there is only one big, free block */
  list_head = (block_header*)space_ptr;
  list_head->next = NULL;
  /* Remember that the 'size' stored in block size excludes the space for the header */
  list_head->size_status = alloc_size - (int)sizeof(block_header);
  
  return 0;
}


/* Function for allocating 'size' bytes. */
/* Returns address of allocated block on success */
/* Returns NULL on failure */
/* Here is what this function should accomplish */
/* - Check for sanity of size - Return NULL when appropriate */
/* - Round up size to a multiple of 4 */
/* - Traverse the list of blocks and allocate the first free block which can accommodate the requested size */
/* -- Also, when allocating a block - split it into two blocks when possible */
/* Tips: Be careful with pointer arithmetic */
void* Mem_Alloc(int size)
{
  int request_size = size;
  char* split_spot = NULL;
  char* h_start = NULL;
  char* start = NULL;
  block_header* temp = NULL;
  int leftover = 0;
  int space = 0;

  /*request size must be greater than zero*/
  if(!(request_size > 0))
  {
    return NULL;
  }
	
  /*round request to multiple of 4*/
  while(request_size % 4 != 0)
  {
    request_size++;	
  }
	
  temp = list_head;

   /*search through memory to find unallocated block using first fit
    *and allocate if possible*/	
   while(temp != NULL)
   {
    leftover = (temp->size_status)-request_size; //possible memory left after allocation
    space = leftover - (int)sizeof(block_header); //memory left after allocation with header under concideration
		
     if((temp->size_status) % 4 == 0) //if block unallocated
     {
       if((temp->size_status) >= request_size) //if there is enough memory to meet request
       {
         if(leftover > (int)sizeof(block_header)) //if the memory left is greater than blockheader size
         {
           if(space > 4) //if there is memory left including blockheader size great than 4
           {
             /* parameters are met to split memory sufficiently*/
             while(space % 4 != 0) //round down left over memory to multiple of 4
             {
               space--;
             }
             h_start = (char*)temp;
             start = h_start + (int)sizeof(block_header);
             split_spot = start + request_size; //place in memory where new header should go
             block_header* split;
             split = (block_header*)split_spot;
             split->next = temp->next;
             temp->next = split; //hook up memory blocks
             split->size_status = space;
             temp->size_status = request_size+1; //mark space as allocated
             return(temp + 1); 
            } 
            else //if not enough memory to allocate after split, don't split, give request whats left
            {
              temp->size_status = (temp->size_status)+1;
              return(temp + 1);
            }
       }	
       else //if space left can't hold new header, give request this space 
       {
         temp->size_status = (temp->size_status)+1;
         return(temp + 1);
       } 
     }
     else //block isn't big enough, check next block for enough memory
     {
       temp = temp->next;
     }
   }
   else //block is allocated, check next block
   {
     temp = temp->next;
   }  
 }	
return NULL; //not enough contiguous memory
}

/* Function for freeing up a previously allocated block */
/* Argument - ptr: Address of the block to be freed up */
/* Returns 0 on success */
/* Returns -1 on failure */
/* Here is what this function should accomplish */
/* - Return -1 if ptr is NULL */
/* - Return -1 if ptr is not pointing to the first byte of a busy block */
/* - Mark the block as free */
/* - Coalesce if one or both of the immediate neighbours are free */
int Mem_Free(void *ptr)
{
  if(ptr == NULL) //pointer passed in is null
  {
    return -1;
  }
	


 block_header* temp = NULL;
 char* p_start = NULL;
 char* p_current = NULL;
 block_header* compare = NULL; //check that pointer passed is pointer to first byte of memory after header
 block_header* before = NULL; //memory block before block passed in
 block_header* after = NULL; //memory block after block passed in
 int blockmatch = 0; //1 if pointer points properly to allocated memory, -1 otherwise
	
 temp = list_head;
 p_start = (char*)ptr;
 p_current = p_start - (int)sizeof(block_header);
 compare = (block_header*)p_current; //sets compare to where start of block should be

/*go through memory to check that pointer is infact pointer to fist byte of memory after a block header
 * and check that the pointer passed in is pointer to an allocated piece of memory while also keeping
 * track of the memory blocks before and after the one being compared to for coalesceing purposes*/
 while(temp != NULL)
 {
   if(temp == compare)
   {
     if((temp->size_status) % 4 == 0) //block unallocated
     {
       return -1;
     }
     else//allocated block
     {
       blockmatch = 1;
       after = temp->next;
       break;
     }
   }
   else
   {
     before = temp;
     temp = temp->next;
     if(temp == NULL) //reached end of free/allocated list, pointer passed in doesn't point to memory
     {
       blockmatch = -1;
     }
   }		
 }

/*pointer passed in wasn't in free/allocated list, or if pointer pointed
 * to memory that was unallocated*/
 if(blockmatch == -1)
 {
   return -1;
 }

/*pointer passed in points to first byte of allocated memory, free the memory
 * and coalesce if adjacent memory blocks are free*/	
 if(blockmatch ==1)
 {
   compare->size_status =(compare->size_status)-1; //set block as unallocated

   /*checks previous block (if there is one) if free, if it is, coalesce. If block after is also
    *free, coalesce with the before block that was just coalesced*/
   if(before != NULL)
   {
     if((before->size_status) % 4 == 0) //previous block free
     { 
       before->size_status = (int)sizeof(block_header)+ //set total combined memory space available
       (compare->size_status)+(before->size_status);
       before->next = compare->next; //connect block to 'after' to finish coalesceing
       if(after != NULL) 
       {
         if((after->size_status) % 4 == 0) //after 'before' is coalesced, coalesce again with after if free
         {
   
           before->size_status = (int)sizeof(block_header)+
           (after->size_status)+(before->size_status);
           before->next = after->next;
         }
       }
     }	
   /*if the previous memory block wasn't free, check if the next block is free
    *if it is then coalesce*/		
   if(after != NULL)
   {
     if((after->size_status) % 4 == 0)
     {
       compare->size_status = (int)sizeof(block_header)+
       (after->size_status)+(compare->size_status);
       compare->next = (after->next);
     }
   }
  }
  else
  {	
   if(after != NULL)//if there wasn't a previous block, check if theres a block after
   {
     if((after->size_status) % 4 == 0)
     { //not allocated
        compare->size_status = (int)sizeof(block_header)+
       (compare->size_status)+(after->size_status);
       compare->next = after->next;
     }
   }
  }
 return 0;
 }
 return -1;	
}

/* Function to be used for debug */
/* Prints out a list of all the blocks along with the following information for each block */
/* No.      : Serial number of the block */
/* Status   : free/busy */
/* Begin    : Address of the first useful byte in the block */
/* End      : Address of the last byte in the block */
/* Size     : Size of the block (excluding the header) */
/* t_Size   : Size of the block (including the header) */
/* t_Begin  : Address of the first byte in the block (this is where the header starts) */
void Mem_Dump()
{
  int counter;
  block_header* current = NULL;
  char* t_Begin = NULL;
  char* Begin = NULL;
  int Size;
  int t_Size;
  char* End = NULL;
  int free_size;
  int busy_size;
  int total_size;
  char status[5];

  free_size = 0;
  busy_size = 0;
  total_size = 0;
  current = list_head;
  counter = 1;
  fprintf(stdout,"************************************Block list***********************************\n");
  fprintf(stdout,"No.\tStatus\tBegin\t\tEnd\t\tSize\tt_Size\tt_Begin\n");
  fprintf(stdout,"---------------------------------------------------------------------------------\n");
  while(NULL != current)
  {
    t_Begin = (char*)current;
    Begin = t_Begin + (int)sizeof(block_header);
    Size = current->size_status;
    strcpy(status,"Free");
    if(Size & 1) /*LSB = 1 => busy block*/
    {
      strcpy(status,"Busy");
      Size = Size - 1; /*Minus one for ignoring status in busy block*/
      t_Size = Size + (int)sizeof(block_header);
      busy_size = busy_size + t_Size;
    }
    else
    {
      t_Size = Size + (int)sizeof(block_header);
      free_size = free_size + t_Size;
    }
    End = Begin + Size;
    fprintf(stdout,"%d\t%s\t0x%08lx\t0x%08lx\t%d\t%d\t0x%08lx\n",counter,status,(unsigned long int)Begin,(unsigned long int)End,Size,t_Size,(unsigned long int)t_Begin);
    total_size = total_size + t_Size;
    current = current->next;
    counter = counter + 1;
  }
  fprintf(stdout,"---------------------------------------------------------------------------------\n");
  fprintf(stdout,"*********************************************************************************\n");

  fprintf(stdout,"Total busy size = %d\n",busy_size);
  fprintf(stdout,"Total free size = %d\n",free_size);
  fprintf(stdout,"Total size = %d\n",busy_size+free_size);
  fprintf(stdout,"*********************************************************************************\n");
  fflush(stdout);
  return;
}
