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
all:
	gcc -c mem.c -Wall
	gcc -shared -o libmem.so mem.o
	rm -f mem.o

# CLEAN the directory
clean:
	rm -f libmem.so
