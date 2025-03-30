#ifndef APP_H
#define APP_H

//this function is called once at startup
extern void apptaskInitHook( void );

//this function is called once every 1 ms
extern int apptask1msHook( void );

extern void apptaskbgHook(void);
#endif
