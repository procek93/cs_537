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
char zeroes[512];
uint freeblock;
uint usedblocks;
uint bitblocks;
uint freeinode = 1;
uint root_inode;

struct stat fileStat;

/*
void balloc(int);
void wsect(uint, void*);
void winode(uint, struct dinode*);
void rinode(uint inum, struct dinode *ip);
void rsect(uint sec, void *buf);
uint ialloc(ushort type);
void iappend(uint inum, void *p, int n);
*/

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

    //void* value;

    lseek(fsfd, BLOCK_SIZE, SEEK_SET);

    if ( read(fsfd, &sb, sizeof(struct superblock)) != END_OF_FILE ) {

	if( sb.size != numBlock )
	{
		fprintf(stderr, "Error!\n");
		exit(1);
	}
    }

}
