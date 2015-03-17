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
  nf_head = (char *)space_ptr + (alloc_size/4));

  //return begining of the large free block, which will
  //also serve as the begining of the slab block
  slab_head_l = (FreeHeader *)space_ptr;
  //only block in free list
  slab_head_l->next = NULL;
  //*SIZE STORED IN BLOCK EXCLUDES HEADER SPACE
  slab_head_l->length = (alloc_size/4) - (int)sizeof(FreeHeader);

  //first 25% of memory dedicated to slab
  //so list for next_fit allocation comes next
  nf_head_l = ((char *)space_ptr + (alloc_size/4));
  //CREATE A CIRCULAR QUEUE
  nf_head_l->next = nf_head_l;
  nf_head_l->length = ((3*alloc_size)/4) - (int)sizeof(FreeHeader);

  //mark end of list (final addressable memory slot)
  EOL = (char *)space_ptr + (alloc_size - 1);

  //now segment the slab_space
  generate_slab();

  //set slabChunk
  slab_chunk = (int)sizeof(FreeHeader) + g_slabSize;

  //set number of slabs
  numSlabs = (alloc_size/4)/slabChunk;
  
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

	//always reset flag
	slab_fl = 0;
	
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
	if(slab_fl == 2)//slab fit failed
	{
		if(nf_alloc(req_size) == NULL)
		{
			//not enough contiguous space
			return NULL;
		}
	}
		
}

/*function segments the slab region into slabs of memory*/
/*and should only be called by mem_init*/
static void generate_slab(void){
	
	//variable used to keep track of where
	//in memory we are
	void * tracker = NULL;
	//temporary variable used to chain together
	//a free list
	FreeHeader * next = NULL;
	FreeHeader * prev = NULL;
	//start at begining of list
	prev = slab_head_l;
	tracker = slab_head;

	//do one look ahead to make sure we dont
	//segment a slab in the event we only have 
	//enough space for one slab to begin with
	//*keep from making part of NF_alloc a slab
	tracker = (char *)tracker + slab_chunk;

	//begin chaining together freespace
	while(tracker != nf_head)
	{
		next = (FreeHeader *)(tracker);
		//Though we know the slabsize, set it
		//for integrity checking
		next->length = g_slabSize;
		next->next = NULL;

		//link previous to next
		prev->next = next;

		//next node becomes previous on next iteration
		prev = next;

		//check if we have space for more slabs
		tracker = (char*)tracker + slab_chunk;
	}

	//reached end of slab region
	//point it to magic number rather than NULL
	//to avoid confusion b/t allocated node vs
	//a node that happens to be last in list
	prev->next = NULL;

	return;	

}
	
static void * slab_alloc(int * fl){

	FreeHeader * temp = NULL;
	FreeHeader * aloc_location = NULL;
	void * clear_space = NULL;

	//now do the check to see if all blocks allocated
	if(slab_head_l == NULL)
	{
		//signal that we must try next fit instead
		*fl = 2;
	}
	else//a free node exists
	{
		//the slab_header_l will always point to top of the free list
		//aka, it will always point to the first free block.
		//regardless of how free rebuilds the list, the header ptr
		//will always point to a free block (@top of list)

		if(slab_head_l->next == NULL)
		{
			//the node is the only node that exists
			aloc_location = slab_head_l;
			//signal that slabs are exhausted
			slab_head_l = NULL;
		}
		else
		{
			//there is more than one free node
			//pop off the top
			aloc_location = slab_head_l;
			slab_head_l = aloc_location->next;
		}	
			
		//succesful slab allocation
		*fl = 1;
	}

	clear_space = (void*)((char *)aloc_location + (int)sizeof(FreeHeader));
	return memset(clear_space, 0, g_slabSize);

}

static void * nf_alloc(int size){

	int request_size = size;
	void * split_loc = NULL;
	void * h_begin = NULL;
	void * begin = NULL;
	static FreeHeader * last_location = NULL;
	FreeHeader * self_catch = NULL;
	int leftover = 0;
	int space = 0;

	//kick start the searching procedure. On very first
	//run, start search from top of free list head
	//and all subsequent searches will start
	//from node in front of last allocated
	if(nf_once == 0)
	{
		last_location = nf_head_l;
		nf_once++;
	}

	//idea is to loop until you end up back where you started
	//self_catch will be NULL, then after entering the loop
	//it will grab the address of the free block following the last
	//allocated block (last_location)

	/*search through memory to find unallocated block using next fit*/
	/*allocate if possible*/
	while((last_location != self_catch) && (last_location != NULL))
	{
		//premptively calculate possible memory that'd be left if we allocate here
		leftover = (last_location->length) - request_size;
		//memory left after we'd allocate the header + request (aka the possible next split free block)
		space = leftover - (int)sizeof(FreeHeader);
		
		//we will hit only free blocks on iterations, point is to find
		//the adequate size
		if((last_location->length) >= request_size)//enough memory to meet just the request?
		{
			if(leftover > (int)sizeof(FreeHeader))//is freespace-request enough to fit header too?
			{
				if(
			

		
		

	

	
	
	
	
	
	

	

	

}



