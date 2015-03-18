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
static int freed_after_empty = 0;

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
  nf_head_l = (FreeHeader *)((char *)space_ptr + (alloc_size/4));
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
	int first_loop = 0;
	int prior_head_node_found = 0;
	int size_allocated = 0;

	/*following temp headers are char * to do byte arithemetic*/
	char * split_loc = NULL;
	char * h_begin = NULL;
	char * mem_begin = NULL;

	/*last_location static to keep location between instantiations*/
	static FreeHeader * last_location;

	/*rover pointer that will search for a free block*/
	/*and pointer to free block that always preceeds it*/
	FreeHeader * previous = NULL;
	FreeHeader * self_catch = NULL;

	FreeHeader * split = NULL;
	FreeHeader * head_looper = NULL;

	//header used to return the address
	AllocatedHeader * ret = NULL;

	/*variables for space calculations & predictions*/
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

	if(freed_after_empty == 1)
	{
		last_location = nf_head_l;
		freed_after_empty == 0;
	}
		
	self_catch = NULL;

	previous = last_location;

	//idea is to loop until you end up back where you started
	//self_catch will be NULL, then after entering the loop
	//it will grab the address of the free block following the last
	//allocated block (last_location)

	/*search through memory to find unallocated block using next fit*/
	/*allocate if possible*/
	while((last_location != self_catch) && (nf_head_l != NULL))
	{
		if(first_loop == 0)
		{
			//set rover pointer to where last free location was
			self_catch = last_location; 
			//you only want self_catch to obtain last_location once per instantiation
			first_loop++;
		}

		//premptively calculate possible memory that'd be left if we allocate here
		leftover = (self_catch->length) - request_size;
		//memory left after we'd allocate the header + request (aka the possible next split free block)
		space = leftover - (int)sizeof(FreeHeader);
		
		//we will hit only free blocks on iterations, point is to find
		//the adequate size
		if((self_catch->length) >= request_size)//enough memory to meet just the request?
		{
			if(leftover > (int)sizeof(FreeHeader))//is freespace-request enough to fit header too?
			{
				if(space >= 4)//is remaining space enough for at least smallest request (4 bytes)
				{
					/**parameters for memory split sufficient**/

					//begin pointer arithmetic with starting location
					h_begin = (char *)self_catch; //cast to char to byte arithmetic
					//move into where the free mem is (past the header)
					mem_begin = h_begin + (int)sizeof(FreeHeader); 
					//give this current header exactly the request size in bytes
					//we will now point to where a new header will go (to split & link)
					split_loc = mem_begin + request_size;
					//create the new header
					split = (FreeHeader*)split_spot;
					//denote the space left
					split->length = space;
					self_catch->length = request_size;
					/*NOW LINK/CAST NODES APPROPRIATELY*/
					//new free head points to what the now allocated head pointed to

					/*SPECIAL CASE:: NODE ALLOCATING IS HEAD*/
					if(self_catch == nf_head_l)
					{
						//if the now allocated node happened to 
						//be the head node, shift the head node
						//to the now free node
						nf_head_l = split;
	
						//check if the original head pointed to itself
						if(self_catch->next == self_catch)
						{
							//means it was the only node around, what a scrub
							//new head will loop into itself too
							/*this happens w/ all mallocs and no frees*/
							nf_head_l->next = nf_head_l;
						}
						else
						{
							//means that the head node connects to another node
							//MUST FIND THE NODE THAT LINKS BACK TO THIS
							//HEADNODE THATS ABOUT TO DETACH
							while(!prior_head_node_found)
							{
								head_looper = self_catch->next;

								//check every node down the chain
								//to find which connects back to head
								if(head_looper->next == self_catch)
								{
									//found the node, break
									prior_head_node_found = 1;
								}
							}
		
							/*connect the nodes now appropriately*/
							//new head gets old heads next
							nf_head_l->next = self_catch->next;
							//make the node that looped back to head
							//loop back to updated head
							head_looper->next = nf_head_l;

							//going to start from the head again next
							//iteration
							last_location = nf_head_l; 
						}

					}
					
					/*CASE 2:: NODE ALLOCATING IS NOT HEAD*/
					//THIS MEANS WE HAVE PASSED THE HEAD, MEANING 
					//WE KNOW THIS CURRENT NODES PREVIOUS NODE, AND NEXT
					split->next = self_catch->next;
					previous->next = split;

					//preserve where we left off
					last_location = split;

					//pop allocated out of the chain 
					size_allocated = self_catch->length;
					ret = (AllocatedHeader *)self_catch;
					ret->length = size_allocated;
			
					return ret;
										
				}
				//if chose to split, there wouldnt be enough mem for both
				//head and adequate data, instead, give whatever remainder region
				//exists back to the user, allocate, and relink the lists

				/*CASE 1::WE ARE @ HEAD*/
				if(self_catch == nf_head_l)
				{
				
					/*HEAD IS THE ONLY NODE LEFT (IT LINKS BACK TO ITSELF)*/
					/**MAKE THE LIST HEADER = NULL**/
					if(self_catch->next == self_catch)
					{
						//pop allocated out of the chain 
						size_allocated = self_catch->length;
						ret = (AllocatedHeader *)self_catch;
						ret->length = size_allocated;

						//list now empty
						nf_head_l = NULL;
	
						return ret;
					}
					else
					{
						//means that the head node connects to another node
						//MUST FIND THE NODE THAT LINKS BACK TO THIS
						//HEADNODE THATS ABOUT TO DETACH
						
						//also, the node ahead of head thats popping off 
						//becomes the new head node		
						nf_head_l = self_catch->next;

						while(!prior_head_node_found)
						{
							head_looper = self_catch->next;

							//check every node down the chain
							//to find which connects back to head
							if(head_looper->next == self_catch)
							{
								//found the node, break
								prior_head_node_found = 1;
							}
						}
		
						/*connect the nodes now appropriately*/
						head_looper->next = nf_head_l;

						//going to start from the head again next
						//iteration
						last_location = nf_head_l; 

						//pop allocated out of the chain 
						size_allocated = self_catch->length;
						ret = (AllocatedHeader *)self_catch;
						ret->length = size_allocated;

						return ret;
					}

					/*CASE 2:: NODE ALLOCATING IS NOT HEAD*/
					//THIS MEANS WE HAVE PASSED THE HEAD, MEANING 
					//WE KNOW THIS CURRENT NODES PREVIOUS NODE, AND NEXT
					previous->next = self_catch->next;

					//preserve where we left off
					last_location = self_catch->next;

					//pop allocated out of the chain 
					size_allocated = self_catch->length;
					ret = (AllocatedHeader *)self_catch;
					ret->length = size_allocated;
			
					return ret;
				}
			}
			//if we chose to split here, there wouldnt even be enough mem left
			//to fit a new header, instead, just give user this entire space.

			/*CASE 1::WE ARE @ HEAD*/
			if(self_catch == nf_head_l)
			{
				
				/*HEAD IS THE ONLY NODE LEFT (IT LINKS BACK TO ITSELF)*/
				/**MAKE THE LIST HEADER = NULL**/
				if(self_catch->next == self_catch)
				{
					//pop allocated out of the chain 
					size_allocated = self_catch->length;
					ret = (AllocatedHeader *)self_catch;
					ret->length = size_allocated;

					//list now empty
					nf_head_l = NULL;
	
					return ret;
				}
				else
				{
					//means that the head node connects to another node
					//MUST FIND THE NODE THAT LINKS BACK TO THIS
					//HEADNODE THATS ABOUT TO DETACH
						
					//also, the node ahead of head thats popping off 
					//becomes the new head node		
					nf_head_l = self_catch->next;

					while(!prior_head_node_found)
					{
						head_looper = self_catch->next;

						//check every node down the chain
						//to find which connects back to head
						if(head_looper->next == self_catch)
						{
							//found the node, break
							prior_head_node_found = 1;
						}
					}
		
					/*connect the nodes now appropriately*/
					head_looper->next = nf_head_l;

					//going to start from the head again next
					//iteration
					last_location = nf_head_l; 

					//pop allocated out of the chain 
					size_allocated = self_catch->length;
					ret = (AllocatedHeader *)self_catch;
					ret->length = size_allocated;

					return ret;
				}

				/*CASE 2:: NODE ALLOCATING IS NOT HEAD*/
				//THIS MEANS WE HAVE PASSED THE HEAD, MEANING 
				//WE KNOW THIS CURRENT NODES PREVIOUS NODE, AND NEXT
				previous->next = self_catch->next;

				//preserve where we left off
				last_location = self_catch->next;

				//pop allocated out of the chain 
				size_allocated = self_catch->length;
				ret = (AllocatedHeader *)self_catch;
				ret->length = size_allocated;
			
				return ret;
			}
		}
		//block isn't big enough for our request
		else
		{
			//capture previous free node
			previous = self_catch;
			//move self_catch onto the next node
			self_catch = previous->next;
			
		}
	}
	//either no sufficient blocks found
	//or mem available to begin with
	return NULL;
}
			

		
		

	

	
	
	
	
	
	

	

	

}



