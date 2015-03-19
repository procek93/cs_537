
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
#include <stdio.h>

//create variable holder for slabSize request
static int g_slabSize;

//variable to hold padded allocation size
//global to do arithmetic for slab allocator location
static int alloc_size;

//variable to signal how many slabs to expect
static int numSlabs;

//variable to specify size of slab+header
static int slab_chunk;

//flag to signal if we are to use slab allocation
//states:
//0 - slab not requested
//1 - slab requested
//2 - slab failed
//*global declaration for optional thread optimizing*
static int slab_fl = 0;

static int nf_once = 0;
static int freed_after_empty = 0;

//top of slab list & NF REGION
void * slab_head = NULL;
void * nf_head = NULL;

//Headers for freelist for each region
struct FreeHeader * slab_head_l = NULL;
struct FreeHeader * nf_head_l = NULL;

//last accessible address possible (SEG_FAULT_CHECK)
void * EOL = NULL;


////////////////////////////////////////////////////////////////////////////////

int Mem_Free(void *ptr)
{
  if ( ptr == NULL ) return 0;		/* Check if ptr parameter is NULL,
  					 * and simply return */

  // It is an error (segmentation fault) to attempt a free outside 
  // of the originally allocated Mem_Init region
  if ( (ptr < slab_head) || (ptr > EOL) ) 
  {
	fprintf(stdout, "SEGFAULT\n");
	return -1;
  }

  // The pointer refers to a block within the next fit region
  if ( ptr >= nf_head )
  {

	//ptr = (struct AllocatedHeader*)ptr;
  	
        // Check if the specified block is allocated
        if ( (((struct AllocatedHeader*)ptr)->magic) == (void*)MAGIC )
        {
		return nf_free(ptr);
	
	// Trying to free an unallocated block results in error
        } else {
		return -1;
	}
 
  // The pointer refers to a block within the slab region
  } else {

	//ptr = (struct FreeHeader*) ptr;

	// Check if the specified block is a valid slab	
  	if ( (((struct FreeHeader*)ptr)->length) == g_slabSize )
	{
		return slab_free((struct FreeHeader*)ptr);

	// Trying to free an invalid slab results in error
	} else {
		return -1;
	}
  }

///////////////////////// START OF THE CS354 STUFF //////////////////////////////

}

static int slab_free(void * ptr){

  /* Local variables */
  struct FreeHeader* curr = NULL;	/* Variable pointers to block_headers */
  struct FreeHeader* prev = NULL;
  struct FreeHeader* next = NULL;

  

  /* Initialize variable pointers to block_headers */

  curr = slab_head_l;
  
  ptr = (struct FreeHeader *)ptr;
  /* Slab insertion into the free list below */
 
  // If the free list is empty, make the specified pointer the
  // head of the free list and return
  if (curr == NULL) {
	slab_head_l = (struct FreeHeader *)ptr;
	slab_head_l->next = NULL;
	return 0;
  }

  // Traverse the free list to determine where the newly freed slab belongs
  while ( curr < (struct FreeHeader*)ptr )
  {
  	prev = curr;
	curr = next;
	
	// If the freed slab is at the end of the list, append it
	if (curr == NULL) {
		curr->next = (struct FreeHeader *)ptr;
		((struct FreeHeader *)ptr)->next = NULL;
		return 0;
	}
  }
  
  if ( (struct FreeHeader *)ptr < slab_head_l ) {
		
	// Set the next open block
	((struct FreeHeader *)ptr)->next = curr;

	// Set the length of the free block
	((struct FreeHeader *)ptr)->length = g_slabSize;

	// Update the head of the free list
	slab_head_l = (struct FreeHeader *)ptr;

  } else {

	// Link the slab into the free list
	prev->next = (struct FreeHeader *)ptr;
	((struct FreeHeader *)ptr)->next = curr;
	
	// Set the slabs length (not really needed)
	((struct FreeHeader *)ptr)->length = g_slabSize;
  }
  
  return 0;

} // End of slab_free

static int nf_free(void * ptr){

  /* Local variables */
  struct FreeHeader* curr = NULL;	/* Variable pointers to block_headers */
  struct FreeHeader* next = NULL;
  struct FreeHeader* prev = NULL;

  struct FreeHeader* list_end = NULL;
  struct FreeHeader* list_end_next = NULL;

  int one_block;			/* Variable used for circular buffer
  					   cycle detection with one free block */

  int add_length;			/* Integer variable for determing 
  					   size of block to be coalesced */
  
  int ptr_length;			/* Integer variable for holding the
  					   length of the newly freed block */

  char* nextPtr;			/* Arithmetic pointer */

  /* Initialize variable pointers to block_headers */
 
  one_block = 0;

  curr = nf_head_l;

  next = curr->next;

  ptr_length = ((struct AllocatedHeader *)ptr)->length;

  // Traverse the free list to get the 'end' of the circular queue.
  // That is, find the block that precededs the head of the free list
  // so that its next field may be updated to the new free list head
  
  if ( curr == NULL )
  {
  	// Make the ptr the new head of the free list if the free
	// list was empty, and loop it back to itself
	((struct AllocatedHeader *)ptr)->length = ptr_length;
	((struct AllocatedHeader *)ptr)->next = ((struct AllocatedHeader *)ptr);
	nf_head_l = ((struct FreeHeader *)ptr);

	// Set a flag for the allocation code
	freed_after_empty = 1;
	
	// Return success
	return 0;
  }

  list_end = nf_head_l;
  list_end_next = list_end->next;
	
  while ( list_end_next != nf_head_l )
  {
	list_end = list_end_next;
	list_end_next = list_end->next;
  }

  /* Coalescing code below */ 
  if ( (struct FreeHeader *)ptr < curr ) {
	
	// Pointer arithmetic swag
	nextPtr = (char*) ptr + ptr_length + (int)sizeof(struct FreeHeader);

	// If the freed block can be coalesced with the head of the free list
	if ( nextPtr == (char*) curr )
	{	
		// Set the next open block
		((struct AllocatedHeader *)ptr)->next = next;

		// Set the length of the free block
		add_length = (curr->length + (int)sizeof(struct FreeHeader));
		((struct AllocatedHeader *)ptr->length = add_length + ptr_length;

		// Update loopback of circular queue
		list_end->next = ptr;
		
	} else {
		
		// Set the next open block
		ptr->next = curr;

		// Set the length of the free block
		ptr->length = ptr_length;

		// Update loopback of circular queue
		list_end->next = ptr;
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

		// If there is only one open block in the free list
		// that points back to itself, break to avoid an
		// infinite loop and set the cycle check variable
		if (prev == curr)
		{
			one_block = 1;
			break;
		}
	}

	// If there is one free block left in the free list that
	// is found at a smaller address than the specified pointer
	if ( one_block == 1 )
	{	
		// Pointer arithmetic swag
		next_ptr = (char*) prev + prev->length + (int)sizeof(struct FreeHeader);

		// Check for prev block coalescing
		if ( next_ptr == ptr )
		{
			// Set the length of the free block
			add_length = (ptr_length + (int)sizeof(struct FreeHeader));
			prev->length += add_length;

			// No need to update the next pointer
			
		} else {
			
			// Update loopback of circular queue
			ptr->next = prev;

			// Set the next open block
			prev->next = ptr;
		} 

	// There are multiple free blocks within the free list
	} else {

		// Pointer arithmetic swag
		next_ptr = (char*) prev + prev->length + (int)sizeof(FreeHeader);

		// Check for prev block coalescing
		if ( next_ptr == ptr )
		{
			// Set the length of the free block
			add_length = (ptr_length + (int)sizeof(struct FreeHeader));
			prev->length += add_length;
			ptr_length = ptr->length;

			// Update the pointer for further coalescing
			ptr = prev;

			// No need to update the next pointer
			
		} else {
			
			// Set the next open block
			prev->next = ptr;
		} 
		
		// Pointer arithmetic swag
		next_ptr = (char*) ptr + ptr_length + (int)sizeof(struct FreeHeader);

		// Check for next block coalescing
		if ( next_ptr == curr )
		{	
			// Set the next open block
			ptr->next = next;

			// Set the length of the free block
			add_length = (curr->length + (int)sizeof(struct FreeHeader));
			ptr->length += add_length;
		
		} else {
			
			// Set the next open block
			ptr->next = curr;

			// Set the free block's length
			ptr->length = ptr_length;
		}
	}
  }  

  // Return success
  return 0;

} // End of nf_free
