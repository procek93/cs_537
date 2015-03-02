#include "types.h"
#include "stat.h"
#include "user.h"
#include "pstat.h"

void fork_kids(void); 

int
main(int argc, char *argv[])
{

	struct pstat *t = NULL;

	int i;

	t = (struct pstat *)malloc(sizeof(struct pstat) * 64);

	if(t == NULL)
	{
		exit();
	}

	printf(1, "parent pid %d\n", getpid());
	fork_kids();

	for(;;)
	{
		getpinfo(t);
		for(i = 0; i < 64; i++)
		{
			if(t[i].inuse == 1)
			{
				printf(1, "pid %d; ", t[i].pid);
				printf(1, "name %s; ", t[i].name);
				printf(1, "# of sched %d; ", t[i].n_schedule);
				printf(1, "tickets %d; ", t[i].tickets);
				printf(1, "stride %d; ", t[i].stride);
				printf(1, "pass %d; \n", t[i].pass);
			}
		}
		sleep(10);
	}
	
	exit();

}

void fork_kids(void)
{
	int i;
	int rc;

	for(i = 0; i < 4; i++)
	{
		rc = fork();

		if(rc == 0)
		{
			int tmp, sum = 0;
			
			if((i % 2) == 0)
			{
				if(settickets(100) < 0 )
				{
					exit();
				}
			}
			else
			{
				if(settickets(100) < 0)
				{
					exit();	
				}
			}
		
			for(tmp = 0; ; tmp++)
			{
				sum += tmp;
			}
			exit();
		}
	}
}
		
