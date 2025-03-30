#ifndef RTTHREAD_H
#define RTTHREAD_H

#define DBG_LEVEL 						0		//enable debug printf messages

typedef enum
{
  eNoCommand,
  eHelp,
  eRunTime,
  eCpuUsage,
  eCpuUsageClear,
  eQuit
}eCmds;

typedef struct
{
 eCmds ec;
 char cmdText[5];
}sCmdSet;

extern eCmds actCmd;
extern sCmdSet aCmdSets[];


extern int rtt_init(void);

#endif
