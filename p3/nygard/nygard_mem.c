/******************************************************************************
 * FILENAME: mem.c
 * AUTHOR:   procek@wisc.edu <Peter Procek>, gnygard@wisc.edu <Graham Nygard>
 * DATE:     15 March 2015
 * PROVIDES: Contains a set of library functions for memory 
 * *****************************************************************************/

#include "mymem.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>

//create variable holder for slabSize request
static int g_slabSize;

//variable to hold padded allocation size
//global to do arithmetic for slab allocator location
static int alloc_size;

//flag to signal if we are to use slab allocation
//states:
//0 - slab not requested
//1 - slab requested
//2 - slab failed
//*global declaration for optional thread optimizing*
static int slab_fl = 0;

//top of slab list & NF REGION
void * slab_head = NULL;
void * nf_head = NULL;

//Headers for freelist for each region
FreeHeader * slab_head_l = NULL;
FreeHeader * nf_head_l = NULL;

//last accessible address possible (SEG_FAULT_CHECK)
void * EOL = NULL;

/* Function used to Initialize the memory allocator */
/* Not intended to be called more than once by a program */
/* Argument - sizeOfRegion: Specifies the size of the chunk which needs to be allocated */
/* Returns 0 on success and -1 on failure */
void * Mem_Init(int sizeOfRegion, int slabSize)
{
  int pagesize;
  int padding;
  void* space_ptr;

  //invoke static to retain value b/t invokations
  static int allocated_once = 0;
  
  //if this method invoked more than once or if request innapropriate size
  //then error
  if(0 != allocated_once || sizeOfRegion <= 0)
  {
    return NULL;
  }

  //save the special slabSize
  g_slabSize = slabSize;

  /*create padding in requested memory as to provide natural alignment
  /-->natural alignment incase user reqests memory not a multiple of pagesize*/

  //Get pagesize (portable)
  pagesize = sysconf(_SC_PAGESIZE);

  // pad user requested memory to be multiple of page size
  padding = sizeOfRegion % pagesize;
  padding = (pagesize - padding) % pagesize;
  alloc_size = sizeOfRegion + padding;

  // use mmap to allocate memory 
  // **non backed memory region
  space_ptr = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (MAP_FAILED == space_ptr)
  {
    //mapped memory allocation failed
    return NULL;
  }
  
  //flag that we mapped to memory once now
  allocated_once = 1;

  /*CREATE ALL MARKERS AND POINTERS TO CRITICAL SECTIONS*/
  //mark begining of each allocation type region
  slab_head = space_ptr;
  nf_head = space_ptr;

  //return begining of the large free block, which will
  //also serve as the begining of the slab block
  slab_head_l = (FreeHeader *)space_ptr;
  //only block in free list
  slab_head_l->next = NULL;
  //*SIZE STORED IN BLOCK EXCLUDES HEADER SPACE
  slab_head_l->length = (alloc_size/4) - (int)sizeof(FreeHeader);

  //first 25% of memory dedicated to slab
  //so list for next_fit allocation comes next
  nf_head_l = (FreeHeader *)(space_ptr + (alloc_size/4));
  nf_head_l->next = NULL;
  nf_head_l->length = ((3*alloc_size)/4) - (int)sizeof(FreeHeader);

  //mark end of list (final addressable memory slot)
  EOL = space_ptr + (alloc_size - 1);
  
  //return the addr of the entire piece of memory
  return space_ptr;
}
/*function is in charge of doling out memory.*/ 
/*provides ptr to the requested chunk of memory.*/
/*returns ptr to adr of requested block on success.*/
/*returns Null ptr on failure*/
void * Mem_Alloc(int size){

	int padding;
	int alloc_size;
	int req_size = size;
	
	//sanity check request size
	if(req_size < 0)
	{
		return NULL;
	}

	//check if the user requested a slab
	if(req_size == g_slabSize)
	{
		//slab requested, attempt slab allocation
		if(slab_alloc(&slab_fl) == NULL)
		{
			//error
			return NULL;
		}
	}

	//if the user requested a next fit piece of
	//memory, make it 16-byte aligned
	if(slab_fl == 0)
	{
		padding = req_size % 16;
		padding = (16 - padding) % 16;
		alloc_size = req_size + padding;
	}	

	//attempt next fit allocation if either user passed
	//in non slab request size, or slab allocation failed
	if(slab_fl == 0)
	{
		if(nf_alloc(alloc_size) == NULL)
		{
			//not enough contiguous space
			return NULL;
		}
	}

	else
	if(slab_fl == 2)
	{
		if(nf_alloc(req_size) == NULL)
		{
			//not enough contiguous space
			return NULL;
		}
	}
		
}

static void * slab_alloc(void * head, int * fl){

}

static void * nf_alloc(void * head, int size){

}

/* Function for freeing up a previously allocated block */
/* Argument - ptr: Address of the block to be freed up */
/* Returns 0 on success */
/* Returns -1 on failure */
/* Here is what this function should accomplish */
/* - No-op if ptr is NULL */
/* - Return -1 if ptr is not pointing to the first byte of a busy block */
/* - Mark the block as free */
/* - Coalesce if one or both of the immediate neighbours are free */
int Mem_Free(void *ptr)
{
  // Cast the pointer
  ptr = (AllocatedHeader*) ptr;

  if ( ptr == NULL ) return 0;		/* Check if ptr parameter is NULL,
  					 * and simply return */

  // It is an error (segmentation fault) to attempt a free outside 
  // of the originally allocated Mem_Init region
  if ( (ptr < slab_head) || (ptr > EOL) ) 
  {
	printf(stdout, "SEGFAULT\n");
	return -1;
  }

  // Trying to free an unallocated block is an error
  if ( ptr->magic != MAGIC )
  {
	return -1;
  }

  if ( ptr >= nf_head )
  {
	return nf_free(ptr);

  } else {
	return slab_free(ptr);
  }

///////////////////////// START OF THE CS354 STUFF //////////////////////////////

}

static int slab_free(void * ptr){

  /* Local variables */
  FreeHeader* curr = NULL;	/* Variable pointers to block_headers */
  FreeHeader* prev = NULL;
  FreeHeader* next = NULL;

  int slab_length;			/* Integer variable for determing 
  					   size of block to be coalesced */  

  /* Initialize variable pointers to block_headers */

  curr = slab_head_l;

  slab_length = ptr->length;

  ptr = (FreeHeader*) ptr;

  /* Slab insertion into the free list below */
 
  // If the free list is empty, make the specified pointer the
  // head of the free list and return
  if (curr == NULL) {
	slab_head_l = ptr;
	ptr->next = MAGIC;
	return 0;
  }

  // Traverse the free list to determine where the newly freed slab belongs
  while ( curr < ptr )
  {
  	prev = curr;
	curr = next;
	
	// If the freed slab is at the end of the list, append it
	if (curr == MAGIC) {
		curr->next = ptr;
		ptr->next = MAGIC;
		return 0;
	}
  }
  
  if ( ptr < slab_head_l ) {
		
	// Set the next open block
	ptr->next = curr;

	// Set the length of the free block
	ptr->length = slab_length;

	// Update the head of the free list
	slab_head_l = ptr;

  } else {

	// Link the slab into the free list
	prev->next = ptr;
	ptr->next = curr;
	
	// Set the slabs length (not really needed)
	ptr->length = slab_lengthl
  }
  
  return 0;

} // End of slab_free

static int nf_free(void * ptr){

  /* Local variables */
  FreeHeader* curr = NULL;	/* Variable pointers to block_headers */
  FreeHeader* next = NULL;
  FreeHeader* prev = NULL;

  int add_length;			/* Integer variable for determing 
  					   size of block to be coalesced */
  
  int ptr_length;			/* Integer variable for holding the
  					   length of the newly freed block */

  // Zero out the magic number
  ptr = (AllocatedHeader*) ptr;
  ptr->magic = NULL;

  /* Initialize variable pointers to block_headers */

  curr = nf_head_l;

  next = curr->next;

  ptr_length = ptr->length;

  ptr = (FreeHeader*) ptr;

  /* Coalescing code below */
  
  if ( ptr < curr ) {

	// If the freed block can be coalesced with the head of the free list
	if ( (ptr + ptr_length + (int)sizeof(FreeHeader)) == curr )
	{	
		// Set the next open block
		ptr->next = next;

		// Set the length of the free block
		add_length = (curr->length + (int)sizeof(FreeHeader));
		ptr->length = add_length + ptr_length;
		
	} else {
		
		// Set the next open block
		ptr->next = curr;

		// Set the length of the free block
		ptr->length = ptr_length;
	}

	// Update the head of the free list
	nf_head_l = ptr;

  } else {

	// Traverse the free list to determine where the newly freed
	// block will be placed. Coalescing will occur if the specified
	// block neighbors another free block
	while ( curr < ptr )
	{
	  	prev = curr;
		curr = next;
	}

	// Check for prev block coalescing
	if ( (prev + prev->length + (int)sizeof(FreeHeader)) == ptr )
	{
		// Set the length of the free block
		add_length = (ptr_length + (int)sizeof(FreeHeader));
		prev->length += add_length;
		ptr_length = ptr->length;

		// Update the pointer for further coalescing
		ptr = prev;

		// No need to update the next pointer
		
	} else {
		
		// Set the next open block
		prev->next = ptr;
	} 

	// Check for next block coalescing
	if ( (ptr + ptr_length + (int)sizeof(FreeHeader)) == curr )
	{	

		// Set the next open block
		ptr->next = next;

		// Set the length of the free block
		add_length = (curr->length + (int)sizeof(FreeHeader));
		ptr->length += add_length;
	
	} else {
		
		// Set the next open block
		ptr->next = curr;

		// Set the free block's length
		ptr->length = ptr_length;
	}
  }  

  // Return success
  return 0;

} // End of nf_free

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

  FreeHeader* slab_current = slab_head_l;
  FreeHeader* nf_current = nf_head_l;

  char* t_Begin = NULL;
  char* Begin = NULL;
  //int Size;
  int t_Size;
  char* End = NULL;
  
  int slab_free;
  int nf_free;
   
  int slab_size;

  //int slab_busy;
  //int nf_busy;

  //int slab_total;
  //int nf_total;

  char status[5];

  slab_free = 0;
  nf_free = 0;

  // All slabs are the same size, so this will be the appropriate size
  slab_size = slab_head_l->length;

  //slab_busy = 0;
  //nf_busy= 0;
 
  counter = 1;
  fprintf(stdout,"************************************Block list***********************************\n");
  fprintf(stdout,"No.\tStatus\tBegin\t\tEnd\t\tSize\tt_Size\tt_Begin\n");
  fprintf(stdout,"---------------------------------------------------------------------------------\n");
  
  // Count the number of free slabs
  while(NULL != slab_current)
  {
    t_Begin = (char*)slab_current;
    Begin = t_Begin + (int)sizeof(FreeHeader);
    Size = slab_current->length;
    strcpy(status,"Free");
/*
    if(current->magic == MAGIC)
    {
      strcpy(status,"Busy");
      t_Size = current->length + (int)sizeof(FreeHeader);
      slab_busy += t_Size;
    }
    else
    {
      t_Size = Size + (int)sizeof(block_header);
      free_size = free_size + t_Size;
    }
*/
    End = Begin + Size;
    fprintf(stdout,"%d\t%s\t0x%08lx\t0x%08lx\t%d\t%d\t0x%08lx\n",counter,status,(unsigned long int)Begin,(unsigned long int)End,Size,t_Size,(unsigned long int)t_Begin);
    slab_free += Size;
    slab_current = slab_current->next;
    counter = counter + 1;
  }

  // Count the number of free blocks in Next Fit
  while(NULL != nf_current)
  {
    t_Begin = (char*)nf_current;
    Begin = t_Begin + (int)sizeof(FreeHeader);
    Size = nf_current->length;
    strcpy(status,"Free");
/*
    if(current->magic == MAGIC)
    {
      strcpy(status,"Busy");
      t_Size = current->length + (int)sizeof(FreeHeader);
      slab_busy += t_Size;
    }
    else
    {
      t_Size = Size + (int)sizeof(block_header);
      free_size = free_size + t_Size;
    }
*/
    End = Begin + Size;
    fprintf(stdout,"%d\t%s\t0x%08lx\t0x%08lx\t%d\t%d\t0x%08lx\n",counter,status,(unsigned long int)Begin,(unsigned long int)End,Size,t_Size,(unsigned long int)t_Begin);
    nf_free += Size;
    nf_current = nf_current->next;
    counter = counter + 1;
  }

  fprintf(stdout,"---------------------------------------------------------------------------------\n");
  fprintf(stdout,"*********************************************************************************\n");

  //fprintf(stdout,"Total slab busy size = %d\n",busy_slab);
  //fprintf(stdout,"Total next fit busy size = %d\n",busy_nf);
  fprintf(stdout,"Total slab free size = %d\n",slab_free);
  fprintf(stdout,"Total next fit free size = %d\n",nf_free);
  //fprintf(stdout,"Total size = %d\n",busy_slab+busy_nf+free_slab+free_nf);
  fprintf(stdout,"*********************************************************************************\n");
  fflush(stdout);
  return;
}
