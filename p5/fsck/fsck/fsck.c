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

int nblocks = 995;
int ninodes = 200;
int size = 1024;

int fsfd;
struct superblock sb;
struct dinode inode;

char zeroes[512];
uint freeblock;
uint usedblocks;
uint bitblocks;
uint freeinode = 1;
uint root_inode;

struct stat fileStat;

int
main(int argc, char *argv[])
{

    if (argc != 2) {
	fprintf(stderr, "Error!\n");
	exit(1);
    }

    if ( (fsfd = open(argv[1], O_RDONLY)) < 0 ) {
	/* file did not open */
	fprintf(stderr, "Error!\n");
	exit(1);
    }
    
    
    /* Generates a stat file descriptor specifying the address of 
     * the fileStat structure for struct field access */
    fstat(fsfd, &fileStat);
 
    /* Sets the number of bytes contained in input file */
    int numBytes = fileStat.st_size;
    int numBlock = numBytes / BLOCK_SIZE;

    lseek(fsfd, BLOCK_SIZE, SEEK_SET);

    if ( read(fsfd, &sb, sizeof(struct superblock)) > 0 ) {

	if( sb.size != numBlock )
	{
		fprintf(stderr, "Error!\n");
		exit(1);
	}
	else
	{
		fprintf(stdout, "FS size: %d\n", numBlock);

		int numInodes = sb.ninodes;
		int numDBlock = sb.nblocks;

  		int bitblocks = sb.size/(512*8) + 1;		   // Blocks consumed by bitmaps
		int usedblocks = sb.ninodes / IPB + 3 + bitblocks; // Blocks consumed by inodes and bitmaps

		// Superblock sanity check
		if( usedblocks + numDBlock != numBlock )
		{
			fprintf(stderr, "Error!\n");
			exit(1);
		}
		else
		{
			fprintf(stdout, "inode block: %d\n", numInodes);
			fprintf(stdout, "data blocks: %d\n", numDBlock);
		}

	}

    }

    lseek(fsfd, 2*BLOCK_SIZE, SEEK_SET);
 
    int i;
    for (i = 0 ; i < 13 ; i++)
    {
	    if ( read(fsfd, &inode, sizeof(struct dinode)) > 0 ) {

		fprintf(stdout, "\nReading inode: %d\n", i);

		fprintf(stdout, "inode type: %u\n", inode.type);
		fprintf(stdout, "inode links: %u\n", inode.nlink);
		fprintf(stdout, "inode size: %d\n", inode.size);	

		// Superblock sanity check
		int j;
		for (j = 0 ; j < 13 ; j++)
		{
			fprintf(stdout, "Data Block %d address %u\n", j, inode.addrs[j]);
		}

	   }
	   lseek(fsfd, 0, SEEK_CUR);
    }

}
