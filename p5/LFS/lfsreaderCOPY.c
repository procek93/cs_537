#include "LFS.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#define END_OF_FILE 0
#define BSIZE 4096

/* Included functions */
int main(int argc, char* argv[]); 

void travelDownTree(int);		/* A recursive function used to search
					   the file system tree, starting at 
					   the root and seeking out each part
					   of the specified pathname */

int getInodeType(int inum);		/* A function used for determining the
					   type of inode specificed by the 
					   inum parameter. This method returns
					   the type of the inode in question. */

char* pathname[20];			/* Used for saving the tokenized input
					   pathname of the file or directory */

int numTotal;				/* Number of tokens in full pathname */
int numFound;				/* Number of tokens currently found */

int type;				/* Differentiates between 'ls' and 'cat' command */

int handleOutput;			/* Varialbe used to handle command outputs */

int fsfd;				/* File descriptor for opening the image */
 
struct stat fileStat;			/* Structure for stating files (unnecessary) */

/* Pointers for accessing LFS structures */
checkpoint* CR;
inodeMap* iMap;
inode* currInode;
dirEnt* entry;
inode* tempInode;

int
main(int argc, char *argv[])
{

    if (argc != 4) {
	printf("Error!\n");
	exit(1);
    }

    if ( (fsfd = open(argv[3], O_RDONLY)) < 0 ) {
	printf("Error!\n");
	exit(1);
    }
    
    // Initialize some variables
    char* token;
    numTotal = 0;
    numFound = 0;
    handleOutput = 0;

    // Tokenize the input pathname
    token = strtok(argv[2], "/");
    while (token != NULL) 
    {
	pathname[numTotal] = token;
	numTotal++;
	token = strtok(NULL, "/");
    }
    pathname[numTotal] = "\0";

    /* Allocate all the shit you need right off the bat */
    CR 		= (checkpoint*)malloc(sizeof(checkpoint));
    iMap 	= (inodeMap*)malloc(sizeof(inodeMap));    
    currInode 	= (inode*)malloc(sizeof(inode));
    entry 	= (dirEnt*)malloc(sizeof(dirEnt));
    tempInode 	= (inode*)malloc(sizeof(inode));

    lseek(fsfd, 0, SEEK_SET);
    if ( read(fsfd, CR, sizeof(checkpoint)) < END_OF_FILE ) 
    {
	    printf("Error!\n");
	    exit(1);
    }	

    lseek(fsfd, (int)(*(CR->iMapPtr)), SEEK_SET);
    if ( read(fsfd, iMap, sizeof(inodeMap)) < END_OF_FILE ) 
    {
	    printf("Error!\n");
	    exit(1);
    }	

    if (strcmp(argv[1], "cat") == 0) 
    {
        type = 0;
	travelDownTree(0);

    } else if (strcmp(argv[1], "ls") == 0) 
    {
        type = 1;
	travelDownTree(0);

    } else 
    {
	printf("Error!\n");
	exit(1);
    }
    
    return 0;
}

void
travelDownTree(int inum)
{
	if (inum > 15)
	{
	//	printf("Error! Could not access the pathname.\n");
		printf("Error!\n");
		exit(1);
	}

	
	if (inum >= 0) 
	{
		lseek(fsfd, (int)(*(iMap->inodePtr + inum)), SEEK_SET);
		if ( read(fsfd, currInode, sizeof(inode)) < END_OF_FILE )
		{
			printf("Error!\n");
			exit(1);
		}
	}

	// If it is time to output the results to the console.
	// - handleOutput: Set when the complete pathname is found
	// - numTotal = 0: 'ls' has been issued on the root directory
	if(handleOutput || (numTotal == 0))
	{
		if ( currInode->type == MFS_DIRECTORY )
		{		
			if (!type)
			{
				// Can't use 'cat' command on a directory
				printf("Error!\n");
				exit(1);
			} else
			{	
				/* This is used as the address reference for accessing
				   each of the entries in a directory */
				int ref = (*(currInode->ptr));
				
				int i, checkedInode;

				for (i = 0 ; i < 14 ; i++)
				{						
					lseek(fsfd, ref, SEEK_SET);
					
					if ( read(fsfd, entry, sizeof(dirEnt)) < END_OF_FILE )
					{
						printf("Error!\n");
						exit(1);
					}

					if( entry->inum < 0 )
					{
						break;
					}

					checkedInode = getInodeType(entry->inum);
					
					if (tempInode->type == 0)
					{
						printf("%s/\n", entry->name);

					} else if (tempInode->type == 1)
					{
						printf("%s\n", entry->name);
					
					} else
					{
						continue;
					}

					/* Update the reference to access the next directory entry */
					ref += sizeof(dirEnt);
				}
				
				/* Free all of the LFS structs */
				free(CR);
				free(iMap);
				free(currInode);
				free(tempInode);
				free(entry);
				exit(1);
			}

		} else if ( currInode->type == MFS_REGULAR_FILE )
		{
			if (type)
			{
				// Cannot perform 'ls' on a file
				printf("Error!\n");
				exit(1);
			} else
			{
				int j;
				
				char buf[currInode->size];
	
				for (j = 0 ; j < 14 ; j++)
				{
					// Break once the inode's referenced block is invalid
					if( (*(currInode->ptr + j)) < 0 )
					{
						break;
					}

					lseek(fsfd, (int)(*(currInode->ptr + j)), SEEK_SET);
					if ( read(fsfd, buf, sizeof(buf)) < END_OF_FILE )
					{
						printf("Error!\n");
						exit(1);
					}
					
					write(1, buf, sizeof(buf));					
				}

				/* Free all of the LFS structs */
				free(CR);
				free(iMap);
				free(currInode);
				free(tempInode);
				free(entry);
				exit(1);
			}
		}


	} else
	{

		/* RECURSIVE PRINT STATEMENT TESTS */

//		printf("\nCurrently investigating inum: %d\n", inum);
//		printf("Type: %d\n", currInode->type);
//		printf("Size: %d\n", currInode->size);
//		printf("\nLooking for -> pathname[%d] = %s\n", numFound, pathname[numFound]);
//		printf("numTotal: %d, numFound: %d\n\n", numTotal, numFound);

		if ( currInode->type == MFS_DIRECTORY )
		{
			int k;
			for (k = 0 ; k < 14 ; k++)
			{
				lseek(fsfd, (int)(*(currInode->ptr + k)), SEEK_SET);
				if ( read(fsfd, entry, sizeof(dirEnt)) < END_OF_FILE )
				{
					printf("Error!\n");
					exit(1);
				}

				if (strcmp(entry->name, pathname[numFound]) == 0)
				{	
					if (numTotal == numFound)
					{
//						printf("\nFull pathname found!\n");
//						printf("Found: %s\n", entry->name);
//						printf("Goto inum: %d to handle output.\n", entry->inum);

						handleOutput = 1;
						travelDownTree(entry->inum);

					} else
					{
//						printf("\nPart of pathname found!\n");
//						printf("Found: %s\n", entry->name);
//						printf("Goto inum: %d to continue.\n", entry->inum);

						numFound++;	
						travelDownTree(entry->inum);
					}

				} else
				{
					continue;
				}

			}
			travelDownTree(inum + 1);

		} else // Current inode is not a directory
		{
			if (numFound == numTotal)
			{
//				printf("\nFull pathname found!\n");
//				printf("Found: %s\n", entry->name);
//				printf("Goto inum: %d\n", entry->inum);

				handleOutput = 1;
				travelDownTree(entry->inum);

			} else
			{
				travelDownTree(inum + 1);
			}
		}
	}
}

int
getInodeType(int inum)
{
        lseek(fsfd, (int)(*(iMap->inodePtr + inum)), SEEK_SET);
	if ( read(fsfd, tempInode, sizeof(inode)) < END_OF_FILE )
	{
		printf("Error!\n");
		exit(1);
	}
	
	return (tempInode->type);
}

