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

	printf("Superblock size: %d\n", sb.size);
	printf("Image size: %d\n", numBlock);

	if( sb.size != numBlock )
	{
		numInodes = sb.ninodes;
		numDBlock = sb.nblocks;
		
		fprintf(stdout, "inode block: %d\n", numInodes);
		fprintf(stdout, "data blocks: %d\n", numDBlock);

  		int bitblocks = numBlock/(512*8) + 1;		   // Blocks consumed by bitmaps
		int usedblocks = sb.ninodes / IPB + 3 + bitblocks; // Blocks consumed by inodes and bitmaps

		int correctSize = usedblocks + numDBlock;
		
		fprintf(stdout, "used blocks: %d\n", usedblocks);
		fprintf(stdout, "data blocks: %d\n", numDBlock);
		fprintf(stdout, "correct size: %d\n", numBlock);

		// Superblock sanity check
		if( correctSize != numBlock )
		{
			printf("Error! Correct Size wrong!\n");
			exit(1);
		}
		else
		{	
			lseek(fsfd, BLOCK_SIZE, SEEK_SET);
			if ( write(fsfd, &correctSize, sizeof(int)) != sizeof(int) )
			{
				printf("Error! Write failed miserably!\n");
				exit(1);
			}
		}
	}
	else
	{
		fprintf(stdout, "FS size: %d\n", numBlock);

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
			fprintf(stdout, "inode block: %d\n", numInodes);
			fprintf(stdout, "data blocks: %d\n", numDBlock);
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

		fprintf(stdout, "\nReading inode: %d\n", i);

		short type = inode.type;
		struct dirent directory;

		if ( type == T_DIR )
		{		
			printf("Inode %d data block address %x\n", i, BLOCK_SIZE*inode.addrs[0]);

			lseek(fsfd, BLOCK_SIZE*inode.addrs[0], SEEK_SET);
			if (read(fsfd, &directory, sizeof(struct dirent)) > 0)
			{	
				printf("First directory entry: %s\n", directory.name);
				
				if (strcmp(directory.name, ".") != 0)
				{
					clearInode = 1;
				}
			}

			lseek(fsfd, 0, SEEK_CUR);
			if (read(fsfd, &directory, sizeof(struct dirent)) > 0)
			{
				printf("Second directory entry: %s\n", directory.name);
				if (strcmp(directory.name, "..") != 0)
				{
					clearInode = 1;
				}
			}

			if (clearInode)
			{
				bzero(&inode, sizeof(struct dinode));
				lseek(fsfd, 2*BLOCK_SIZE + (i)*sizeof(struct dinode), SEEK_SET);

				if ( write(fsfd, &inode, sizeof(struct dinode)) != sizeof(struct dinode) )
				{
					printf("Error! Write failed miserably!\n");
					exit(1);
				}
			}
		
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

		fprintf(stdout, "inode type: %u\n", inode.type);
		fprintf(stdout, "inode links: %u\n", inode.nlink);
		fprintf(stdout, "inode size: %d\n", inode.size);	

		// Superblock sanity check
		int j;
		for (j = 0 ; j < 13 ; j++)
		{
			printf("Data Block %d address %x\n", j, BLOCK_SIZE*inode.addrs[j]);
		}

	   }
	   lseek(fsfd, 2*BLOCK_SIZE + (i+1)*sizeof(struct dinode), SEEK_SET);
    }

    lseek(fsfd, 28*BLOCK_SIZE, SEEK_SET);

    char bitMap[BLOCK_SIZE];

    if ( read(fsfd, &bitMap, BLOCK_SIZE) > 0 )
    {
/*	
  int b, bi, m;
  struct buf *bp;
  struct superblock sb;

  bp = 0;
  readsb(dev, &sb);
  for(b = 0; b < sb.size; b += BPB){
    bp = bread(dev, BBLOCK(b, sb.ninodes));

    for(bi = 0; bi < BPB; bi++){
      m = 1 << (bi % 8);
      if((bp->data[bi/8] & m) == 0){  // Is block free?
        bp->data[bi/8] |= m;  // Mark block in use on disk.
        bwrite(bp);
        brelse(bp);
        return b + bi;
      }
    }
    brelse(bp);
  }
  */
 

    }
    

return 1;

}
