//============================================================================
// Author      : Soheil Abbasloo (ab.soheil@nyu.edu)
// Version     : V2.0
//============================================================================

/*
  MIT License
  Copyright (c) 2017 Soheil Abbasloo

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include <cstdlib>
#include "define.h"
//#define CHANGE_TARGET 1
#define MAX_CWND 1000
#define MIN_CWND 4

void SetupSharedMemory()
{
    //key=(key_t) (rand()%1000000+1);
    //key_rl=(key_t) (rand()%1000000+1);
    key=(key_t) (key1);
    key_rl=(key_t) (key2);

    // Setup shared memory, 11 is the size
    if ((shmid = shmget(key, shmem_size, IPC_CREAT | 0666)) < 0)
    {
        printf("Error getting shared memory id");
        return;
    }
        // Attached shared memory
    if ((shared_memory = (char*)shmat(shmid, NULL, 0)) == (char *) -1)
    {
        printf("Error attaching shared memory id");
        return;
    }
    // Setup shared memory, 11 is the size
    if ((shmid_rl = shmget(key_rl, shmem_size, IPC_CREAT | 0666)) < 0)
    {
        printf("Error getting shared memory id");
        return;
    }
    // Attached shared memory
    if ((shared_memory_rl = (char*)shmat(shmid_rl, NULL, 0)) == (char *) -1)
    {
        printf("Error attaching shared memory id");
        return;
    } 
}

int main(int argc, char **argv)
{
    DBGPRINT(DBGSERVER,4,"Main\n");
    if(argc!=3)
	{
        DBGERROR("argc:%d\n",argc);
        for(int i=0;i<argc;i++)
        	DBGERROR("argv[%d]:%s\n",i,argv[i]);
		usage();
		return 0;
	}
    
    srand(raw_timestamp());

	signal(SIGSEGV, handler);   // install our handler
	signal(SIGTERM, handler);   // install our handler
	signal(SIGABRT, handler);   // install our handler
	signal(SIGFPE, handler);   // install our handler
    signal(SIGKILL,handler);   // install our handler
    key1=atoi(argv[1]);
    key2=atoi(argv[2]);

	SetupSharedMemory();


    //wait until the actor-server apps are done!
    while(true)
    {
        sleep(5);
    }

    DBGMARK(DBGSERVER,5,"Removing the SharedMemories ... \n");
    shmdt(shared_memory);
    shmctl(shmid, IPC_RMID, NULL);
    shmdt(shared_memory_rl);
    shmctl(shmid_rl, IPC_RMID, NULL);
    return 0;
}

void usage()
{
	DBGMARK(0,0,"./mem_manager [key] [key_rl]\n");
}


