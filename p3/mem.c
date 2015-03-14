/******************************************************************************
 * MODIFIED BY: Graham Nygard, Section 1
 * FILENAME: 	mem.c
 * AUTHOR:   	cherin@cs.wisc.edu <Cherin Joseph>
 * DATE:     	20 Nov 2013
 * PROVIDES: 	Contains a set of library functions for memory allocation
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
  list_head->size_status = alloc_size - ( (int)sizeof(block_header) );
  
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
  /* Your code should go in here */

  /* Local variables */ 
  char         *next_ptr;   
  block_header *curr_block;
  block_header *next_block;

  int block_size;           /* Integer variable for determing 
  			       size of block to be allocated*/
  
  int free_remaining;	    /* Integer variable for determing 
  			       size of block left over when 
			       allocation results in a block split*/

  curr_block = list_head;   /* Initialize the current block 
  			       for list traversal */

  /* Determine if the requested size is valid */
  if ( size <= 0 ) return NULL;

  /* Round size up to a multiple of 4 (for alignment) */
  while ( size % 4 != 0) {
	size++;
  }

  /* Initialize the number of bytes required for successful allocation */
  block_size = size + (int)sizeof(block_header);

  /* Traverse the linked list of memory blocks until a block large enough to
   * fulfill the requested size is found, and split the block accordingly. */
  while (curr_block != NULL) {
	
	/* If the current block is allocated, move to the next block */
  	if ( (curr_block->size_status) % 2 != 0 ) {
		curr_block = curr_block->next;
  	}

        /* If the current block is too small, move to the next block */
	else if ( curr_block->size_status < size ) {
		curr_block = curr_block->next;
	}

        /* If the current block is a perfect match, mark as allocated */
	else if ( curr_block->size_status == size ) {							  	
		curr_block->size_status |= 0x00000001;

		return curr_block;
	}

        /* If the current block is large enough for allocation, split
	 * the blocks into two, consecutive memory blocks */
  	else if ( curr_block->size_status >= block_size ) {

		/* Establish a memory block of the remaining free size
		 * after the split, and give the appropriate fields */
		free_remaining 		 = curr_block->size_status - block_size;

		next_ptr 		 = (char*) curr_block + block_size;

		next_block		 = (block_header*) next_ptr;

		next_block->next	 = curr_block->next;

		next_block->size_status  = free_remaining;
		
		/* Establish the memory block to be returned as the newly
		 * allocated block of the requested size, and give the 
		 * appropriate fields before returning */
		curr_block->next	 = next_block;

		curr_block->size_status  = size;
		
		curr_block->size_status |= 0x00000001;
		
		curr_block->next 	 = next_block;	

		return curr_block;
  	}
	
	/* Update the current block to be checked */
	else {
		curr_block = curr_block->next;
	}
  }

  return NULL; /* If this is reached, there was not a large enough
  		  memory block to fulfill the requested size */
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
  /* Your code should go in here */

  /* Local variables */
  block_header *curr_block = NULL;	/* Variable pointers to block_headers */
  block_header *next_block = NULL;
  block_header *free_block = NULL;
  block_header *prev_block = NULL;

  int block_size;			/* Integer variable for determing 
  					   size of block to be coalesced*/

  if ( ptr == NULL ) return -1;		/* Check if ptr parameter is valid,
  					 * and return appropriate value */

  /* Initialize curr_block for ptr validity testing */
  curr_block = list_head;

  /* Traverse the linked list of memory blocks to determine if ptr is a valid
   * pointer to a memory block that has been allocated by Mem_Alloc() */
  while ( curr_block != ptr ) {
	
	curr_block = curr_block->next;

	if ( curr_block == NULL ) return -1;
  }

  /* Initialize variable pointers to block_headers */

  curr_block = list_head;

  next_block = curr_block->next;

  free_block = (block_header*)ptr;

  /* Traverse the linked list of memory blocks until the desired block to be
   * freed is found, and determine if it is able to be coalesced w/ neighbors*/ 
  while ( curr_block != NULL ) {

	if (curr_block == free_block) {

		/* Check if ptr already points to a 
		 * free block, and return appropriate value */
		if (curr_block->size_status % 2 == 0) return -1;
                
		/* If the block to be freed is another block's next field
		 * determine if this block is free and coalesce accordingly */
		if (prev_block != NULL) {
			
			if (prev_block->size_status % 2 == 0) {

				block_size 	= curr_block->size_status +
				    	     	  (int)sizeof(block_header);

				prev_block->next 	 = next_block;

				prev_block->size_status += block_size;

				curr_block = prev_block;
			}
		}

		/* If the block to be freed has another block as its next field 
		 * determine if that block is free and coalesce accordingly */
		if (next_block != NULL) {

			if (next_block->size_status % 2 == 0) {
			
				block_size 	 = next_block->size_status +
			    	     	           (int)sizeof(block_header);

				curr_block->next 	 = next_block->next;

				curr_block->size_status += block_size;
			}
		}
		
		/* Mark the newly freed block from 'Busy' to 'Free' */
		curr_block->size_status &= 0xfffffffe;
		
		return 0;
	}

	else {
		/* Update the current block to be checked 
		 * and its corresponding neighbors*/
		prev_block = curr_block;
  		curr_block = next_block;
		next_block = next_block->next;
  	}
  }
  
  return -1; /* Should never reach */
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
