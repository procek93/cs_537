################################################################################
# NAME:			GRAHAM NYGARD
# SECTION:		1 (MWF 9:55am - 10:45am)
# PROGRAM ASSIGNMENT:	4 (Due: Monday April 21, 2014)
# FILE NAME:		makefile
#
# FILE DESCRIPTION:	This is the makefile used to compile and test the cache
# 			simulation files cache1D, cache2Drows, and cache2Dcols.
# 			Each of these files will be tested using the 'pin'
# 			function with varying cache block sizes of 4, 32, and 
# 			64 bytes.
################################################################################

# SPECIFIED TARGETS
all: fsck.c 
	gcc fsck.c -o myfsck -lm  -Werror

recover:
	rm -f *.img
	cp ./backup_example/* .


# COMPILE fsck
fsck: fsck.c
	gcc fsck.c -o myfsck -lm  -Werror

# COMPILE lfsreader
#lfsreader: lfsreader.c
#	gcc -o lfsreader lfsreader.c -Werror

clean:
	rm -f myfsck 
