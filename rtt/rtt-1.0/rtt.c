// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2023 Kent Gibson <warthog618@gmail.com>

/* Minimal example of toggling a single line. */

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "rtThread.h"
#include "app.h"
#include <sys/mman.h> //needed for posix shared memmory
#include <fcntl.h> //constants used by shared memory api's

#define CLOCKS_PER_NSEC (CLOCKS_PER_SEC/1000/1000/1000)
//parameters
#define GPIO_MAX_NB						27	//27 for RPI
#define ALLOW_TASK_OVERFLOW 	1		//if set to 1 then a hook will be called on task overflow; otherwise the application will end
#define GPIOS_AVAILABLE  			0		//set to 1 if the system has gpio's (rpi for example)
#define GPIO_TOGGLE_PIN 			23 	//GPIO pin to toggle at periodic task; set to 0xFF to disable toggle
#if 0 != GPIOS_AVAILABLE
#include <gpiod.h>
#endif
#define SHAREDMEM_ACTIVE			1		//set to 1 to enable logging data to shared memmory to be used by rttDataTrace program for example
#define SHMEM_BUFSIZE					256

struct timespec timT, timNow, timToggle;
#if 0 != GPIOS_AVAILABLE
static const char *const chip_path = "/dev/gpiochip0";
struct gpiod_chip *chip;
unsigned char outPinsCfg[GPIO_MAX_NB + 1];
struct gpiod_line_request* outPinLines[GPIO_MAX_NB];
#endif
//rt task variables
int quitReq;
unsigned int cpuUsageMin, cpuUsageAvg, cpuUsageAvgLastSec, cpuUsageMax;
unsigned int cpuUsageLastSecSum;
unsigned long int loopTime;

#if SHAREDMEM_ACTIVE == 1
struct shmemStruct
{
	unsigned long addr;
	unsigned char head;
	unsigned char tail;
	int buf[SHMEM_BUFSIZE];
};

unsigned long oldMappedAddr;

struct shmemStruct *mappedMemPtr;
int shmFd; //file descriptor
#endif

//application variables
unsigned int appCntMs;
unsigned int appCntSec;
unsigned int appCntMin;
unsigned int oldCntSec;



#if 0 != GPIOS_AVAILABLE
static struct gpiod_line_request* request_output_line( unsigned int offset, enum gpiod_line_value value, const char *consumer)
{
	struct gpiod_request_config *req_cfg = NULL;
	struct gpiod_line_request *request = NULL;
	struct gpiod_line_settings *settings;
	struct gpiod_line_config *line_cfg;

	int ret;

	settings = gpiod_line_settings_new();
	if (!settings)
		goto linereq_end;

	gpiod_line_settings_set_direction(settings,
					  GPIOD_LINE_DIRECTION_OUTPUT);
	gpiod_line_settings_set_output_value(settings, value);

	line_cfg = gpiod_line_config_new();
	if (!line_cfg)
		goto free_settings;

	ret = gpiod_line_config_add_line_settings(line_cfg, &offset, 1,
						  settings);
	if (ret)
		goto free_line_config;

	if (consumer) {
		req_cfg = gpiod_request_config_new();
		if (!req_cfg)
			goto free_line_config;

		gpiod_request_config_set_consumer(req_cfg, consumer);
	}

	request = gpiod_chip_request_lines(chip, req_cfg, line_cfg);

	gpiod_request_config_free(req_cfg);

free_line_config:
	gpiod_line_config_free(line_cfg);

free_settings:
	gpiod_line_settings_free(settings);

linereq_end:

	return request;
}

// static enum gpiod_line_value toggle_line_value(enum gpiod_line_value value)
// {
// 	return (value == GPIOD_LINE_VALUE_ACTIVE) ? GPIOD_LINE_VALUE_INACTIVE :
// 						    GPIOD_LINE_VALUE_ACTIVE;
// }

// static const char * value_str(enum gpiod_line_value value)
// {
// 	if (value == GPIOD_LINE_VALUE_ACTIVE)
// 		return "Active";
// 	else if (value == GPIOD_LINE_VALUE_INACTIVE) {
// 		return "Inactive";
// 	} else {
// 		return "Unknown";
// 	}
// }

int setupTaskGpio( void )
{
	int retVal = 0;
	unsigned char gIdx = 0u;
	unsigned char loutPinsCfg[GPIO_MAX_NB];
	unsigned char outPinsNb = 0u;

	#if (GPIO_TOGGLE_PIN != 0xFF) && (GPIOS_AVAILABLE == 1)
		outPinsCfg[GPIO_TOGGLE_PIN] = GPIOD_LINE_DIRECTION_OUTPUT; //this will overwrite any configuration of this pin done by application
	#endif

	for (gIdx=0u; gIdx<GPIO_MAX_NB; gIdx++)
	{
		//check if the gpio is used as output
		if ( GPIOD_LINE_DIRECTION_OUTPUT == outPinsCfg[gIdx] )
		{//add gpio config to list
			loutPinsCfg[outPinsNb] = gIdx;
			outPinsNb++;
		}
		else if ( GPIOD_LINE_DIRECTION_INPUT == outPinsCfg[gIdx] )
		{
			//TODO add management of input pins
		}
	}
	if ( outPinsNb > 0u )
	{
		chip = gpiod_chip_open(chip_path);
		if (!chip)
		{
			retVal = -1;
		}
		else
		{
			gIdx=0u;
			while ( gIdx<outPinsNb)
			{
				outPinLines[loutPinsCfg[gIdx]] = request_output_line(loutPinsCfg[gIdx], GPIOD_LINE_VALUE_INACTIVE /*TODO allow configuration of initial value*/, "toggle-line-value");
				if (!outPinLines[loutPinsCfg[gIdx]])
				{
					#if DBG_LEVEL > 0
					fprintf(stderr, "\nfailed to request line: %s\n",strerror(errno));
					#endif
					retVal = EXIT_FAILURE;
					gIdx = outPinsNb; //to exit the while loop as the program will end anyway because of the error above
				}
				gIdx++;
			}

			//TODO configure also input pins
		}
	}

	return retVal;

}

int gpioSetValue( unsigned char gpioNb, enum gpiod_line_value value)
{
	int retVal = 0;

	if ( GPIOD_LINE_DIRECTION_OUTPUT == outPinsCfg[gpioNb] )
	{//gpio configured
		retVal = gpiod_line_request_set_value(outPinLines[gpioNb], gpioNb, value);
	}
	else
	{
		#if DBG_LEVEL > 0
		printf("\nerror: pin not configured");
		#endif
		retVal = -1;
	}

	return retVal;
}
#endif

int processCommands( void )
{
    int retVal = 0;
    //commands processing
    switch ( actCmd )
    {
        case eNoCommand:
        //no actions needed here
        break;
				case eHelp:
				 printf("cmd list: \n\thelp\n\trt (run time in ms)\n\tcu (cpu usage)\n\tquit\n");
				 fflush(stdout);
				 actCmd = eNoCommand;
				break;
        case eRunTime:
				 printf( "\t%d:%d:%d\n", appCntMin, appCntSec, appCntMs);
         fflush(stdout);
         actCmd = eNoCommand;
        break;
				case eCpuUsage:
				 printf( "\tmin:%d, avg:%d, avg last second:%d max:%d [us]\n", cpuUsageMin/1000, cpuUsageAvg/1000, cpuUsageAvgLastSec/1000, cpuUsageMax/1000);
         fflush(stdout);
         actCmd = eNoCommand;
				break;
				case eCpuUsageClear:
				 cpuUsageAvg = 0u;
				 cpuUsageMax = 0u;
				 cpuUsageMin = 0xFFFFFFFFu;
         actCmd = eNoCommand;
				break;
        case eQuit:
        default:
                // printf("cmd: quit\n");
								// fflush(stdout);
                retVal = 1; 
        break;
    }

    return retVal;
}

#if ALLOW_TASK_OVERFLOW == 1
void apptaskOverflowHook( void )
{

}
#endif

int taskinit( void )
{
  int retVal = 0;

	quitReq = 0;
	cpuUsageMin = 0xFFFFFFFFu;

  if (0 != clock_gettime(CLOCK_REALTIME,&timToggle))
  {
    printf("clock_gettime error");
    retVal = -1;
  }
  else
  {
	  apptaskInitHook(); //aplication should place here it's initialization code
#if GPIOS_AVAILABLE == 1
		if ( 0 == setupTaskGpio() )
#endif
		{//ok
#if SHAREDMEM_ACTIVE == 1
			shmFd = shm_open("/shmRtt", O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
			if ( -1 == shmFd )
			{
				printf("shm_open error");
				retVal = -1;
			}
			else {
				if(-1 == ftruncate( shmFd, sizeof(struct shmemStruct) ) )
				{
					printf("\nftruncate err");
				}
				else
				{
					mappedMemPtr = mmap(NULL, sizeof(struct shmemStruct), PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0);
					if(MAP_FAILED == mappedMemPtr)
					{
						printf("\nmmap err");
					}
					else 
					{
#if DBG_LEVEL > 0
						printf("\nshared memory created successfully\n");
#endif
						/*mmap will fill the area with 0 so is guaranteed that the struct members have value 0*/
						mappedMemPtr->addr = (unsigned long)(&loopTime); // initialize address of the variable to be logged; loopTime is selected because is incremented at every periodic task and we will have by default some nice values in the shared memmory buffer
#if DBG_LEVEL > 0						
						printf("size of mapped memory is: %d",sizeof(*mappedMemPtr));
#endif
					}
				}
			}

			if ( 0 == retVal )
#endif
			{
			  loopTime = 0u;

			  #if DBG_LEVEL > 0
			  printf("\nentering loop");
			  #endif
			}
		}
#if GPIOS_AVAILABLE == 1
		else
		{//error configuring Gpio
			#if DBG_LEVEL > 0
			printf("\nerror configuring Gpio");
			#endif
			retVal = -1;
		}
#endif
  }

  return retVal;
}

int task1m(void) {
  unsigned long int oCnt = 0u;
  long mC1, nsDiff;
#if GPIOS_AVAILABLE == 1
  enum gpiod_line_value taskPinvalue = GPIOD_LINE_VALUE_INACTIVE;
#endif
  int exitCond = 0;

	timT.tv_sec = 0;
	timT.tv_nsec = 0;

  // processing
#if DBG_LEVEL > 1
  printf("\n[%d]", loopTime);
#endif
#if (GPIO_TOGGLE_PIN != 0xFF) && (GPIOS_AVAILABLE == 1)
  exitCond |= gpioSetValue(GPIO_TOGGLE_PIN, GPIOD_LINE_VALUE_ACTIVE);
#endif
#if SHAREDMEM_ACTIVE == 1
	if (oldMappedAddr != mappedMemPtr->addr)
	{
#if DBG_LEVEL > 0
		printf("\nnew traced adr 0x: %lx\n",mappedMemPtr->addr);
		fflush(stdout);
#endif
		oldMappedAddr = mappedMemPtr->addr;
	}
  mappedMemPtr->buf[mappedMemPtr->head] = (*(unsigned long *)(mappedMemPtr->addr)); //put the data in circular buffer
	if ((mappedMemPtr->tail == (mappedMemPtr->head + 1) ) || 
	    (mappedMemPtr->tail == 0 && mappedMemPtr->head == 255) )
	{//circular buffer full
		//don't update head .. new values will be written in the last place available until reader receives the data
		mappedMemPtr->buf[mappedMemPtr->head] = 0x00000000u; //pattern to show buffer overflow
		//printf(".");
	}
	else
	{
		mappedMemPtr->head++;
	}
#endif
  loopTime++;

  appCntMs++;
	// run time calculation
	if (appCntMs >= 1000u){
		appCntMs = 0u;
		appCntSec++;
		if (60 == appCntSec) {
			appCntSec = 0;
			appCntMin++;
		}
	}

	exitCond |= apptask1msHook(); // application should place all it's code in
																// this function - should return an error code

	mC1 = 1000000; // time in ns until next toggle
	if ((999999999 - timToggle.tv_nsec) > mC1) {
		timToggle.tv_nsec += mC1;
	} else { // overflow
		timToggle.tv_sec++;
		timToggle.tv_nsec = (mC1 + timToggle.tv_nsec) - 999999999;
	}
	// sleep for the remaining time to next gpio toggle
	if (0 != clock_gettime(CLOCK_REALTIME, &timNow)) {
#if DBG_LEVEL > 0
		printf("\nclock_gettime error");
#endif
		exitCond = -1; // exit with error
	} else {
#if DBG_LEVEL > 1
		printf("\nclock_gettime values: sec %d, nsec %d ", timNow.tv_sec, timNow.tv_nsec);
#endif
		if (( timToggle.tv_sec > timNow.tv_sec ) ||
				( (timToggle.tv_sec <= timNow.tv_sec) && (timToggle.tv_nsec > timNow.tv_nsec) ) ) 
		{ // there is still time remaining
			nsDiff = (timToggle.tv_sec - timNow.tv_sec /*this should be max 1*/) * 1000000000 +	timToggle.tv_nsec;
			nsDiff -= timNow.tv_nsec;

#if DBG_LEVEL > 1
			printf("\ncalc time %d", nsDiff);
#endif

#if (GPIO_TOGGLE_PIN != 0xFF) && (GPIOS_AVAILABLE == 1)
					exitCond |= gpioSetValue(GPIO_TOGGLE_PIN, GPIOD_LINE_VALUE_INACTIVE);
#endif

			timT.tv_nsec = nsDiff;
			//CPU Usage calculation
			if ( cpuUsageMin > (1000000 - nsDiff) ) cpuUsageMin = 1000000 - nsDiff;
			if (  0u == cpuUsageAvg ) cpuUsageAvg = 1000000 - nsDiff;
			if ( 0u == appCntMs)
			{
				cpuUsageAvgLastSec = cpuUsageLastSecSum / 1000u;
				cpuUsageLastSecSum = 0u;

				cpuUsageAvg = (double)cpuUsageAvg + 
							( (double)cpuUsageAvgLastSec - (double)cpuUsageAvg) / 
							((double)appCntSec + ((double)appCntMin*60));
			}
			cpuUsageLastSecSum += 1000000 - nsDiff;

			if ( cpuUsageMax < (1000000 - nsDiff) ) cpuUsageMax = 1000000 - nsDiff;


			nanosleep(&timT, &timT);

		} else { // the calculations above or thread interruptions took at least
							// the entire time to next toggle
			oCnt++;
#if DBG_LEVEL > 0
			printf("\noverflow happend %d times", oCnt);
#endif
#if ALLOW_TASK_OVERFLOW == 1
			apptaskOverflowHook();
// no sleep needed here as we anyway used more than 1 ms allready
#else
        exitCond = -1; // exit with error
#endif
		}
	}

  return exitCond;
}

int apptaskbg(void)
{
	int retVal = 0;

	if (0 != processCommands()){
		retVal = 1;
	}
	else {
	
	  //TODO remember function calls for last second
	  //TODO implement commands: ft (function call trace), pv (print variable value) 

  	apptaskbgHook();
	}

	return retVal;
}

void taskEnd( void )
{
#if GPIOS_AVAILABLE == 1
	unsigned char gIdx = 0u;

	for (gIdx=0u; gIdx<GPIO_MAX_NB; gIdx++)
	{
		//check if the gpio is used
		if ( outPinsCfg[gIdx] != 0u )
		{
			gpiod_line_request_release(outPinLines[gIdx]);
		}
	}

	//TODO release also the input pins

	gpiod_chip_close(chip);
#endif

#if SHAREDMEM_ACTIVE == 1
 	if ( -1 != shmFd ) shm_unlink("/shmRtt");
#endif

	printf("\n");
}


int main(void)
{
	int retVal = EXIT_SUCCESS;

	// enum gpiod_line_value value = GPIOD_LINE_VALUE_ACTIVE;
	// struct gpiod_line_request *request;

	printf("\nrunning init task\n");
	retVal = taskinit();

	if ( 0 == retVal )
	{

		printf("\nstarting thread for 1ms task");

		retVal = rtt_init();
		//retVal = task1m();
	
	}

	taskEnd();

	return retVal;
}