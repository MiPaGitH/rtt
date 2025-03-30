

/*
 * POSIX Real Time Example
 * using a single pthread as RT thread
 */
#include <errno.h>
#include <limits.h>
#define __USE_GNU
#include "rtThread.h"
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

// defines
#define RT_KERNEL 0

// variables
pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

sCmdSet aCmdSets[] = {
        {eHelp, "help"}, 
        {eRunTime, "rt"}, 
        {eCpuUsage, "cu"}, 
        {eCpuUsageClear, "cl"},
        {eQuit, "quit"}
};

eCmds actCmd;
int task1msExitCond;
int nbOfCmds;

// imported functions
extern int task1m(void);
extern int apptaskbg(void);

// functions
eCmds getCmd(char tCmd[]) {
  unsigned char lidx = 0u;
  eCmds ccmd = eNoCommand;
//   printf("\ncommand: %s", tCmd);
  while (lidx < nbOfCmds) {
//     printf("\nchecked command: %s", aCmdSets[lidx].cmdText);
    if (NULL != strstr(tCmd, aCmdSets[lidx].cmdText)) { // cmd found
      ccmd = aCmdSets[lidx].ec;
//       printf( "\ncmd text: %s, ccmd: %d\n", aCmdSets[lidx].cmdText, ccmd);
      break; // exit from while loop
    }
    lidx++;
  }

  return ccmd;
}

void *thread_func(void *data) {
  printf("\ntask1ms pid: %d\n", getpid());
  task1msExitCond = 0;

  while (0 == task1msExitCond) {
    task1msExitCond |= task1m();
    // signal main thread that it can continue with the background task
    // function
    pthread_mutex_lock(&mut);
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&mut);
  }
#if DBG_LEVEL > 0
  printf("\nexit condition value: %d", task1msExitCond);
#endif

  return NULL;
}

void *cmdThread_func(void *data) {
  char tCmd[255] = " ";
  struct timespec timS1;
  nbOfCmds = sizeof(aCmdSets) / sizeof(aCmdSets[0]);
  char pendCh = ' ';

	timS1.tv_sec = 0;
	timS1.tv_nsec = 100/*ms*/*1000/*us*/*1000/*ns*/;

  actCmd = eNoCommand;

  sleep(1); //needed so the messages related to thread starting have the chance to be printed first

  while (eQuit != actCmd) {
    printf("\ncmd:");
    //fflush(stdout);
    fgets(tCmd, sizeof(tCmd), stdin);
    if (eNoCommand == actCmd) {
      actCmd = getCmd(tCmd);
      nanosleep(&timS1, &timS1);
#if DBG_LEVEL > 0
      printf("\ncmdno: %d  ", actCmd);
      //fflush(stdout);
#endif
    }
    else {
      printf(" ignoring this command as the previous one was not processed yet");     
    }
  }
#if DBG_LEVEL > 0
  printf("\ncmdThread end\n");
#endif
  return NULL;
}

int rtt_init(void) {
  struct sched_param param;
  pthread_attr_t attr;
  pthread_t thread, cmdThread;
  cpu_set_t cpuset;
  int ret;

  /* Lock memory */
  if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
    printf("\nmlockall failed: %m");
    exit(-2);
  }

  /* Initialize pthread attributes (default values) */
  ret = pthread_attr_init(&attr);
  if (ret) {
    printf("\ninit pthread attributes failed");
    goto out;
  }

  /* Set a specific stack size  */
  ret = pthread_attr_setstacksize(&attr, PTHREAD_STACK_MIN);
  if (ret) {
    printf("\npthread setstacksize failed");
    goto out;
  }

  /* Create the nonRtt pthread that will manage user commands from console; 
  this thread has default attributes and will be executed on a CPU different than #3 (CPU#3 is reserved for RTTasks)*/
  ret = pthread_create(&cmdThread, &attr, cmdThread_func, NULL);
  if (ret) {
    printf("\ncreate cmd pthread failed");
    goto out;
  }
#if RT_KERNEL == 1
  // Prepare parameters of the RTT thread
  /* Set scheduler policy and priority of pthread */
  ret = pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
  if (ret) {
    printf("\npthread setschedpolicy failed");
    goto out;
  }
  param.sched_priority = 98; // max is 99 but is needed by the kernel so is not recommended to be used;
  ret = pthread_attr_setschedparam(&attr, &param);
  if (ret) {
    printf("\npthread setschedparam failed");
    goto out;
  }
  /* Use scheduling parameters of attr */
  ret = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
  if (ret) {
    printf("\npthread setinheritsched failed");
    goto out;
  }

  // assign main thread to cpu 3 - all other threads spawned from here (rtt thread) will also be on cpu 3
  CPU_ZERO(&cpuset);
  CPU_SET(3, &cpuset);
  ret = sched_setaffinity(getpid(), sizeof(cpuset), &cpuset);
  if (ret) {
    printf("\nerror calling pthread_setaffinity_np");
    goto out;
  }
#endif
  printf("\nbackground task pid: %d", getpid());

  /* Create a pthread with specified attributes */
  ret = pthread_create(&thread, &attr, thread_func, NULL);
  if (ret) {
    printf("\ncreate rtt pthread failed");
    goto out;
  }

  printf("\nrtt thread started\n");

  while (0 == task1msExitCond) {
    if (0 == apptaskbg()) {
      // sleep until the next end of task1ms
      pthread_mutex_lock(&mut);
      pthread_cond_wait(&cond, &mut);
      pthread_mutex_unlock(&mut);
    } 
    else {//error processing background task
      task1msExitCond |= 1;
    }
  }
  ret = pthread_join(thread, NULL);
  ret |= pthread_join(cmdThread, NULL);

  if (ret) printf("join pthread failed: %m\n");
#if DBG_LEVEL > 0
  printf("\nthreads joined\n");
#endif
out:
  return ret;
}
