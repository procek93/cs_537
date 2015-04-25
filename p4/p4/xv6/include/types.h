#ifndef _TYPES_H_
#define _TYPES_H_

// Type definitions

typedef unsigned int   uint;
typedef unsigned short ushort;
typedef unsigned char  uchar;
typedef uint pde_t;
#ifndef NULL
#define NULL (0)
#endif

//==================================================================================//
//MOD.P.2
//CREATE THE TYPEDEFINITION STRUCTURE TO USE FOR FETCH-AND-ADD LOCKS
typedef struct __lock_t {
	volatile uint ticket;
	volatile uint turn;
} lock_t;
//==================================================================================//

//==================================================================================//
// MOD.P.14

//TYPEDEF STRUCT TO BE USED IN A LINKED LIST INSIDE OF THE CONDITIONAL VARIABLE
//EACH NODE IN THE LIST CORRESPONDS TO A SLEEPING THREAD/PROCESS
typedef struct proc_node {

	//holds the pid
	volatile int pid; 

	//holds pointer to next p_node
	struct proc_node * next;

} p_node;

//CREATE THE TYPEDEF STRUCT TO USE FOR CONDITIONAL VARIABLES
typedef struct __cond_t {

	//0 if not initialized, any other value otherwise (stick to 1)
	int init_fl; 
	
	//head of the queue
	p_node * head;

} cond_t;

#endif //_TYPES_H_
