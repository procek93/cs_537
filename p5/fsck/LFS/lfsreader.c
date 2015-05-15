#include "structdefs.h"
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

int fd; /* file descriptor */
 
/* Structure for stating files */
struct stat fileStat;

struct inodeMap;
struct inode;
struct dirEnt;
struct checkpoint;

int main(int argc, char* argv[]); 
void cat(void);
void ls(void);

int
main(int argc, char *argv[])
{

    //check for proper user input
    if (argc != 4) {
	fprintf(stderr, "Error!\n");
	exit(1);
    }

    //open the provided file
    if ( (fd = open(argv[3], O_RDONLY)) < 0 ) {
	/* file did not open */
	fprintf(stderr, "Error!\n");
	exit(1);
    }
    
    //dole out the appropriate command
    if (strcmp(argv[1], "cat") == 0) {
    	cat();
    } else if (strcmp(argv[1], "ls") == 0) {
    	ls();
    } else {
	fprintf(stderr, "Error!\n");
	exit(1);
    }

}

void
cat(void)
{
     /* Generates a stat file descriptor specifying the address of 
      * the fileStat structure for struct field access */
     fstat(fd, &fileStat);

     /* Sets the number of bytes contained in input file */
     int numBytes = fileStat.st_size;
    
     //set up checkpoint pointer
     checkpoint * CR;

     /* Sets the number of integers contained in input file */
     //int fileSize = numBytes / sizeof(int) ;
     
     /* Allocates stack space for the integer storage array */
     //int *p = malloc(numBytes);
     
     /* Variable for reading integers from testfile */
     void * value;

     if ( read(fd, value, sizeof(checkpoint)) != END_OF_FILE ) {

	CR = (checkpoint*) value; /* Array index is assigned with binary integer */
     }

     //int value->size;
	
     inode * root = CR->iMapPtr[0];

     int i;
     for (i = 1 ; i < fileSize ; i++) {
	
	fprintf(1, "Root size: %d\n", root->size);
	fprintf(1, "Root type: %d\n", root->type);

     }

    /* While there are still 4 byte integer representations in the array
     * this loop will print integers to the output file in the opposite 
     * order of the input file, and in ASCII form instead of binary*/
    int j;
    for (j = fileSize - 1 ; j >= 0 ; j--) {
        
	 fprintf(fp, "%d\n", *(p + j));
    }

    /* Closes the files */
    close(fd);
    fclose(fp);

    return 0;
    
}

void
ls(void)
{

}
