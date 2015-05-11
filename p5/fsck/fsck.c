#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <dirent.h>
#include <stdbool.h>

#define stat xv6_stat  // avoid clash with host struct stat
#define dirent xv6_dirent  // avoid clash with host struct stat
#include "fs.h"
#undef stat
#undef dirent

#define BLOCK_SIZE (512)
#define END_OF_FILE 0

//int nblocks = 995;
//int ninodes = 200;
//int size = 1024;

int numInodes;
int numDBlock;

int fsfd;
struct superblock sb;
struct dinode inode;
struct dirent* directory;

char zeroes[512];
uint freeblock;
uint usedblocks;
uint bitblocks;
uint freeinode = 1;
uint root_inode;

//dinode dirInodes[200];
//dinode fileInodes[200];

struct stat fileStat;

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

    //printf("Bout to stat dat file!\n");
    
    /* Generates a stat file descriptor specifying the address of 
     * the fileStat structure for struct field access */
    fstat(fsfd, &fileStat);
 
    /* Sets the number of bytes contained in input file */
    int numBytes = fileStat.st_size;
    int numBlock = numBytes / BLOCK_SIZE;

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

    // Check the inodes
    lseek(fsfd, 2*BLOCK_SIZE, SEEK_SET);
 
    //dirent* dataAddress;

    int i;
    for (i = 0 ; i < 13 /*numInodes*/ ; i++)
    {
	    if( read(fsfd, &inode, sizeof(struct dinode)) > 0 ) {

		fprintf(stdout, "\nReading inode: %d\n", i);

		short type = inode.type;
		directory = (struct dirent*)(inode.addrs[0]*BLOCK_SIZE);

		if ( (type != (ushort)0) || (type != (ushort)1) || (type != (ushort)2) || (type != (ushort)3) ) {
			printf("Error! Invalid type!\n");
		 	exit(1);	
		} 
		
		else if ( type == (ushort)1 ) {
			
//			lseek(fsfd, dataAddress, SEEK_SET);

//			if ( read(fsfd, &directory, sizeof(struct dirent)) > 0) {
/*
			printf("Name: %c\n", directory.name[0]);

				if (strcmp(directory.name[0], ".") != 0)
				{
					printf("Error!\n");
					exit(1);

				} else if (strcmp(directory.name[1], "..") != 0)
				{
					printf("Error!\n");
					exit(1);
				}
				*/
				printf("Read some shyte\n");
		//	}
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
	   lseek(fsfd, 0, SEEK_CUR);
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
    



}
