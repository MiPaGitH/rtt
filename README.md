# rtt
real time task

this linux program provides 1ms task and a background task
 it can be used on a real time rpi kernel to easily develop (in a linux environment) code intended for time sensitive functions; then these functions can be used on the designated real time target (usually a uC)
 can be added to a YOCTO configuration (see rtt_YoctoReadme.txt)

to help runtime debugging, tracing of variables is provided via posix mmap (memory mapping); the python program rttDataTrace shows trace data graphically for selectable variables from the user functions; (address of the variables is taken from the generated rtt.map)

rtt provides also a user cmd prompt; the following commands are provided:
 help - shows list of commands
 rt - shows program runtime in minutes, seconds and milliseconds
 cu - shows cpu usage (min, max, avg, avgLastSecond) based on the idle task run time
 cl - clears the cpu usage values to reinitialize the calculation
 quit - exit the program

