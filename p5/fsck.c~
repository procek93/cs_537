#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdbool.h>
#include <math.h>
/*
#include "fs.h"
#include "stat.h"
#include "types.h"
*/
#define stat xv6_stat  // avoid clash with host struct stat
#include "types.h"
#include "fs.h"
#include "stat.h"
#undef stat

#define BLOCK_SIZE (512)
#define END_OF_FILE 0

int numInodes;
int numDBlock;

int fsfd;
struct superblock sb;
struct dinode inode;
struct stat fileStat;

struct dinode inodes[200];
uint linkCount[200];

char zeroes[512];
uint freeblock;
uint usedblocks;
uint bitblocks;
uint freeinode = 1;
uint root_inode;

int
main(int argc, char *argv[])
{

    if (argc != 2) {
	printf("Error!\n");
	exit(1);
    }

    if ( (fsfd = open(argv[1], O_RDWR)) < 0 ) {
	/* file did not open */
	printf("Error!\n");
	exit(1);
    }
    
    /* Generates a stat file descriptor specifying the address of 
     * the fileStat structure for struct field access */
    fstat(fsfd, &fileStat);
 
    /* Sets the number of bytes contained in input file */
    int numBytes = fileStat.st_size;
    int numBlock = numBytes / BLOCK_SIZE;


    // CHECK THE SUPERBLOCK. THE SUPERBLOCK SHOULD HAVE CONSISTENT SIZE,
    // INODE NUMBER, AND DATA BLOCK NUMBER FIELDS. IF THESE FIELDS ARE
    // INCONSISTENT, DETERMINE WHETHER OR NOT THEY MAY BE FIXED SO THAT
    // THEY ARE CONSISTENT. FOR EXAMPLE, IF THE SIZE OF THE FILE SYSTEM
    // IS CORRUPTED, TRY TO RECONSTRUCT AN ACCURATE SIZE FROM THE NUMBER
    // OF INODES AND DATA BLOCKS, AFTER VERIFYING THAT THESE NUMBERS ARE
    // BELIEVABLE.

    //set ourselves to be at the superblock
    lseek(fsfd, BLOCK_SIZE, SEEK_SET);

    if ( read(fsfd, &sb, sizeof(struct superblock)) > 0 ) {

	printf("Superblock size: %d\n", sb.size);
	printf("Image size: %d\n", numBlock);

	if( sb.size != numBlock )
	{
		struct superblock fixed;
		fixed.size = numBlock;
		fixed.nblocks = sb.nblocks;
		fixed.ninodes = sb.ninodes;

		printf("fuckin %d\n", fixed.size);
		printf("writing\n");
		
		lseek(fsfd, BLOCK_SIZE, SEEK_SET);
		//write back to the image
		if ( write(fsfd, &fixed, sizeof(struct superblock)) != sizeof(struct superblock) )
		{
			printf("Error\n");
			exit(1);
		}

	    lseek(fsfd, BLOCK_SIZE, SEEK_SET);

	    if ( read(fsfd, &sb, sizeof(struct superblock)) > 0 ) {
			printf("NEW Superblock size: %d\n", sb.size);
	    }
		

	}
	
	
//		fprintf(stdout, "FS size: %d\n", numBlock);

		numInodes = sb.ninodes;
		numDBlock = sb.nblocks;

  		int bitblocks = sb.size/(512*8) + 1;		   // Blocks consumed by bitmaps
		int usedblocks = sb.ninodes / IPB + 3 + bitblocks; // Blocks consumed by inodes and bitmaps

		//Superblock sanity check
		if( usedblocks + numDBlock != numBlock )
		{
			printf("Error!\n");
			exit(1);
		}
		else
		{
			fprintf(stdout, "inode block: %d\n", numInodes);
			fprintf(stdout, "data blocks: %d\n", numDBlock);
		}



    }
//*****************************************************************************************************************************************//
//WE NOW KNOW THE SUPER BLOCK IS GUCCI, SO USE THE SUPER BLOCK STATS TO ENUMERATE THINGS
//there is BPB for each bitmap block, we will use a high range and assume we have 10 bitmap blocks (not the best approach)

    //**RESTORE OUR SUPERBLOCK**//
    //set ourselves to be at the superblock
    lseek(fsfd, BLOCK_SIZE, SEEK_SET);
    if ( read(fsfd, &sb, sizeof(struct superblock)) <= 0 ) {
	printf("error, son\n");
    }

    int free_map [BPB * 10];
    int bitblocks = sb.size/(512*8) + 1;		   // Blocks consumed by bitmaps

    //the free_map lives after the inodes
    int free_offset = BSIZE + BSIZE*(sb.ninodes/IPB + 3);
    //printf("offset into free is: %d\n", free_offset);
    
    //set ourselves to be at the start of freelist (bitmap)
    lseek(fsfd, free_offset, SEEK_SET);

    //for each map block
    int x;
    int y;
    int z;
    short capture;
    short temp; //for each block we'll take in 8 bits at a time
    int free_index = 0;
    short MASK = 0x01;
    for(x = 0; x < bitblocks; x++) //for each block...
    {
	for(z = 0; z < BPB; z += 16)//for each byte of the block
	{
		//read in 8 bits (a byte at a time)
		if ( read(fsfd, &temp, sizeof(char)) > 0 ) {

			for(y = 0; y < 16; y++)
	       		{
				capture = temp & (MASK << y);
				if(capture != 0)
				{
					capture = 1;
				}
				//printf("%hu ", capture);

				free_map[free_index] = capture;
				free_index++;
       			}
		}

		//printf("\n");
	}
	//printf("new block::::\n");
    }

    

    // CHECK THE INODES. THE INODE REGION STARTS AT THE 3 BLOCK OF
    // THE DISK. FOR EACH OF THE INODES IN THIS REGION, CHECK TO
    // SEE WHETEHR OR NOT ITS DATA IS CORRUPTED. CHECKS CURRENTLY
    // IN PLACE INCLUDE '.' AND '..' DIRECTORY ENTRY VERIFICATION
    // AND VALID TYPE VERIFICATION.

    //set up some stuff 
    //we will use a large number of bytes as a safe range
    struct dinode * alloc_ilist [2048] = {};
    struct dinode * free_ilist [2048] = {};
    
    numInodes = sb.ninodes;
    numDBlock = sb.nblocks;

	printf("num %d\n", numInodes);

    //position ourselves at the start of the inode region
    lseek(fsfd, 2*BLOCK_SIZE, SEEK_SET);
 
    int clearInode = 0;
    
    int i;
    int j;
//******************************************************************************************************************************//
    //here we originally made an assumption that our super block was correct
    //with its sizes
    for (i = 0 ; i < numInodes ; i++)
    {
		//printf("node : %d\n", i);
	    if( read(fsfd, &inode, sizeof(struct dinode)) > 0 ) {

		inodes[i] = inode;	

		//fprintf(stdout, "\nReading inode: %d\n", i);

		short type = inode.type;
		short linkCount = 1;
		short freeType = 0;
		struct dirent directory;

		if ( type == T_DIR)
		{	
			printf("TDIR type: %d\n", type);	

			lseek(fsfd, BLOCK_SIZE*inode.addrs[0], SEEK_SET);

			struct dirent* newData;

			newData = (struct dirent*) malloc(sizeof(struct dirent));

			// THIS SECTION INDUCES THE BUG I WAS TALKING ABOUT. THIS SECTION IS NOT LEGIT!
			// THE COMMENTED OUT SECTION THAT FOLLOWS THIS IS WHERE I DETERMINE IF THE
			// FIRST TWO ENTRIES IN A DIRECTORY ARE '.' AND '..' RESPECTIVELY.
			if(read(fsfd, newData, inode.size) < 0)
			{
				printf("error!\n");
				exit(1);
			}
			if (strcmp(newData->name, ".") != 0 || strcmp((newData++)->name, "..") != 0)
			{
				lseek(fsfd, (int)inode.addrs[0], SEEK_SET);
				memset(newData, 0, inode.size);
				write(fsfd, newData, inode.size);
			}



/*
			printf("Inode %d data block address %x\n", i, BLOCK_SIZE*inode.addrs[0]);

			lseek(fsfd, BLOCK_SIZE*inode.addrs[0], SEEK_SET);
			if (read(fsfd, &directory, sizeof(struct dirent)) > 0)
			{	
				printf("First directory entry: %s\n", directory.name);
				
				if (strcmp(directory.name, ".") != 0)
				{
					clearInode = 1;
					memset(&directory, 0, sizeof(struct dirent));
					
					if ( write(fsfd, &directory, sizeof(struct dirent)) != sizeof(struct dirent) )
					{
						printf("Error! Write failed miserably!\n");
						exit(1);
					}
				}
			}

			lseek(fsfd, 0, SEEK_CUR);
			if (read(fsfd, &directory, sizeof(struct dirent)) > 0)
			{
				printf("Second directory entry: %s\n", directory.name);
				
				if (strcmp(directory.name, "..") != 0)
				{
					clearInode = 1;
					memset(&directory, 0, sizeof(struct dirent));
					
					if ( write(fsfd, &directory, sizeof(struct dirent)) != sizeof(struct dirent) )
					{
						printf("Error! Write failed miserably!\n");
						exit(1);
					}
				}
			}
			
			if( (inode.nlink > linkCount) && (clearInode != 1) )
			{
	   			lseek(fsfd, 2*BLOCK_SIZE + (i)*sizeof(struct dinode), SEEK_SET);

				inode.nlink = linkCount;

				if ( write(fsfd, &inode, sizeof(struct dinode)) != sizeof(struct dinode) )
				{
					printf("Error! Write directory link failed miserably!\n");
					exit(1);
				}
				//clearInode = 1;
				printf("Error! Bad link count!\n");

			}

			if (clearInode)
			{
				lseek(fsfd, 2*BLOCK_SIZE + (i)*sizeof(struct dinode), SEEK_SET);
				memset(&inode, 0, sizeof(struct dinode));

				if ( write(fsfd, &inode, sizeof(struct dinode)) != sizeof(struct dinode) )
				{
					printf("Error! Write failed miserably!\n");
					exit(1);
				}
			}*/

		} else if ( type == T_FILE )
		{
					printf("TFILE type: %d\n", type);
		
		} else if ( type == T_DEV ) 
		{
			printf("TDEV type: %d\n", type);

		} else if ( type == T_FREE )
		{
			//now we must recursively sniff through the inodes
			printf("FREE type: %d\n", type);

		} else
		{
			printf("nog good: %d\n", type);
			printf("your shit node is %d\n", i);
			
			//WE ARE AT INODE I+1
			int bad_dir[2056] = {};
			int bad_dir_index = 0;
			//encountered a bad inode, clear it
			bzero(&inode, sizeof(struct dinode));

			lseek(fsfd, 2*BLOCK_SIZE + (i)*sizeof(struct dinode), SEEK_SET);

			if ( write(fsfd, &inode, sizeof(struct dinode)) != sizeof(struct dinode) )
			{
				printf("Error! Write failed miserably!\n");
				exit(1);
			}

			    //position ourselves at the start of the inode region
			    lseek(fsfd, 2*BLOCK_SIZE, SEEK_SET);

			    //NOW WE HAVE ZEROED OUT THIS INODE, WE MUST DO THE SAME FOR ALL PARENTS
			    //simply look for T_DIR inodes, follow the yellow brick road
			    for (j = 0 ; j < numInodes ; j++)
			    {

				    if( read(fsfd, &inode, sizeof(struct dinode)) > 0 ) {

					inodes[i] = inode;	

					//fprintf(stdout, "\nReading inode: %d\n", i);

					short type = inode.type;
					short size = inode.size;
					short dir_entries = size/sizeof(struct dirent);
					short linkCount = 1;
					short freeType = 0;
					struct dirent directory;

					if ( type == T_DIR )
					{
						printf("found an inode of directory type, its number %d\n", j);
						//we found a directory (yay)
						//follow it into the rabbit hole
						int bl;
						for(bl = 0; bl < dir_entries; bl++)
						{
							printf("directory entry: %d\n", bl);
							short addr = inode.addrs[bl];
							//index into our data block
							lseek(fsfd, BLOCK_SIZE * addr, SEEK_SET);

							if( read(fsfd, &directory, sizeof(struct dirent)) > 0 ) {
								printf("directories inum: %d\n", directory.inum);
								printf("directories entry name: %s\n", directory.name);
								
								if(directory.inum == i)
								{
									printf("we got in the incorrect zone...\n");
									bad_dir[bad_dir_index] = addr;
									bad_dir_index++;
									memset(&directory, 0, sizeof(struct dirent));
						
									
									int al;
									for(al = bl; al < dir_entries - 1; bl++)
									{
										inode.addrs[al] = inode.addrs[al + 1];
									}

									
									inode.size -= sizeof(struct dirent);

									free_map[addr] = 0;
	
									//clear the dirent entry in the data blocks
									lseek(fsfd, BLOCK_SIZE * addr, SEEK_SET);
									//write it back to image
									if ( write(fsfd, &directory, sizeof(struct dirent)) != sizeof(struct dirent) )
									{
										printf("Error! Write failed miserably!\n");
										exit(1);
									}

									//now fix the inode fields	
					  				 lseek(fsfd, 2*BLOCK_SIZE + (j)*sizeof(struct dinode), SEEK_SET);
									if ( write(fsfd, &inode, sizeof(struct dinode)) != sizeof(struct dinode) )
									{
										printf("Error! Write failed miserably!\n");
										exit(1);
									}
									
								}

							}
						}
						
					}
			  		 lseek(fsfd, 2*BLOCK_SIZE + (j+1)*sizeof(struct dinode), SEEK_SET);
				    }
				}
			
		}

//		fprintf(stdout, "inode type: %u\n", inode.type);
//		fprintf(stdout, "inode links: %u\n", inode.nlink);
//		fprintf(stdout, "inode size: %d\n", inode.size);	
/*
		// Superblock sanity check
		int j;
		for (j = 0 ; j < 13 ; j++)
		{
//			printf("Data Block %d address %x\n", j, BLOCK_SIZE*inode.addrs[j]);
		}
*/
	   }
	   lseek(fsfd, 2*BLOCK_SIZE + (i+1)*sizeof(struct dinode), SEEK_SET);
    }


    // SEEK TO THE BITMAP THAT CONTAINS ALL DATA ON THE AVAILABILITY OF
    // DATA BLOCKS. THE BITMAP GETS ITS OWN BLOCK AND COMES AFTER A BUFFER
    // BLOCK USED TO SEPARATE THE BITMAP FROM THE INODE BLOCKS. THE ACTUAL
    // DATA BLOCKS THEMSELVES START AFTER THE BITMAP BLOCK.
    lseek(fsfd, 28*BLOCK_SIZE, SEEK_SET);

    char bitMap[BLOCK_SIZE];

    if ( read(fsfd, &bitMap, BLOCK_SIZE) < 0 )
    {
    	printf("Error!\n");
	exit(1); 
    }

    // THIS BUFFER CONTAINS ALL OF THE BITS THAT COINCIDE WITH THE DATA BLOCKS.
    // BECAUSE THERE ARE 8 BITS PER BYTE, WE MULTIPLY THE BLOCK SIZE BY 8 SO THAT
    // EACH OF THEM CAN BE ACCOUNTED FOR. THIS BUFFER WILL BE USED FOR PERFORMING
    // SANITY CHECKS ON THE DATA BLOCKS (LIKE A BLOCK IS MARKED AS ALLOCATED IN 
    // BITMAP BUT NO INODES POINT TO IT).
    int buffer[BLOCK_SIZE*8];

    for (i = 0 ; i < BLOCK_SIZE*8 ; i++)
    {
	if( bitMap[i/8] & (1 << (i%8)) )
	{
		buffer[i] = 0;
	} else
	{
		buffer[i] = 1;
	}
    }
    
    // PRINT OUT THE BITMAP TO MAKE SURE ACTUAL DATA WAS READ IN. THIS JUST PRINTS
    // A BINCH OF 1'S AND 0'S AND WAS USED SOLELY FOR TESTING
    for (i = 0 ; i < BLOCK_SIZE*8 ; i++)
    {
	if (i%8 == 0 && i != 0)
	{
//		printf("%d\n", buffer[i]);
	} else
	{	
//		printf("%d", buffer[i]);
	}
    }

return 1;

}
