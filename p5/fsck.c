#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdbool.h>
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
    lseek(fsfd, BLOCK_SIZE, SEEK_SET);

    if ( read(fsfd, &sb, sizeof(struct superblock)) > 0 ) {

//	printf("Superblock size: %d\n", sb.size);
//	printf("Image size: %d\n", numBlock);

	if( sb.size != numBlock )
	{
		numInodes = sb.ninodes;
		numDBlock = sb.nblocks;
		
//		fprintf(stdout, "inode block: %d\n", numInodes);
//		fprintf(stdout, "data blocks: %d\n", numDBlock);

  		int bitblocks = numBlock/(512*8) + 1;		   // Blocks consumed by bitmaps
		int usedblocks = sb.ninodes / IPB + 3 + bitblocks; // Blocks consumed by inodes and bitmaps

		int correctSize = usedblocks + numDBlock;
		
//		fprintf(stdout, "used blocks: %d\n", usedblocks);
//		fprintf(stdout, "data blocks: %d\n", numDBlock);
//		fprintf(stdout, "correct size: %d\n", numBlock);

		// Superblock sanity check
		if( correctSize != numBlock )
		{
			printf("Error!\n");
			exit(1);
		}
		else
		{	
			lseek(fsfd, BLOCK_SIZE, SEEK_SET);
			if ( write(fsfd, &correctSize, sizeof(int)) != sizeof(int) )
			{
				printf("Error!\n");
				exit(1);
			}
		}
	}
	else
	{
//		fprintf(stdout, "FS size: %d\n", numBlock);

		numInodes = sb.ninodes;
		numDBlock = sb.nblocks;

  		int bitblocks = sb.size/(512*8) + 1;		   // Blocks consumed by bitmaps
		int usedblocks = sb.ninodes / IPB + 3 + bitblocks; // Blocks consumed by inodes and bitmaps

		// Superblock sanity check
		if( usedblocks + numDBlock != numBlock )
		{
			printf("Error!\n");
			exit(1);
		}
		else
		{
//			fprintf(stdout, "inode block: %d\n", numInodes);
//			fprintf(stdout, "data blocks: %d\n", numDBlock);
		}

	}

    }

    // CHECK THE INODES. THE INODE REGION STARTS AT THE 3 BLOCK OF
    // THE DISK. FOR EACH OF THE INODES IN THIS REGION, CHECK TO
    // SEE WHETEHR OR NOT ITS DATA IS CORRUPTED. CHECKS CURRENTLY
    // IN PLACE INCLUDE '.' AND '..' DIRECTORY ENTRY VERIFICATION
    // AND VALID TYPE VERIFICATION.
    lseek(fsfd, 2*BLOCK_SIZE, SEEK_SET);
 
    int clearInode = 0;
    
    int i;
    for (i = 0 ; i < numInodes ; i++)
    {
	    if( read(fsfd, &inode, sizeof(struct dinode)) > 0 ) {

		inodes[i] = inode;	

//		fprintf(stdout, "\nReading inode: %d\n", i);

		short type = inode.type;
		short linkCount = 1;
		short freeType = 0;
		struct dirent directory;

		if ( type == T_DIR )
		{	

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
			}
*/			
		
		} else if ( type == T_FILE )
		{
		
		
		} else if ( type == T_DEV ) 
		{


		} else if ( type == T_FREE )
		{

		} else
		{	
			bzero(&inode, sizeof(struct dinode));
			lseek(fsfd, 2*BLOCK_SIZE + (i)*sizeof(struct dinode), SEEK_SET);

			if ( write(fsfd, &inode, sizeof(struct dinode)) != sizeof(struct dinode) )
			{
				printf("Error! Write failed miserably!\n");
				exit(1);
			}
		}

//		fprintf(stdout, "inode type: %u\n", inode.type);
//		fprintf(stdout, "inode links: %u\n", inode.nlink);
//		fprintf(stdout, "inode size: %d\n", inode.size);	

		// Superblock sanity check
		int j;
		for (j = 0 ; j < 13 ; j++)
		{
//			printf("Data Block %d address %x\n", j, BLOCK_SIZE*inode.addrs[j]);
		}

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
