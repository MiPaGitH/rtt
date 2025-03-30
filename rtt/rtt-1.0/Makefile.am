AUTOMAKE_OPTIONS = foreign

CFLAGS = -Wall -pedantic -O2
include_HEADERS = rtt.h rtThread.h app.h

LDFLAGS = -Wl,-Map=rtt.map

bin_PROGRAMS = rtt
rtt_SOURCES = rtt.c rtThread.c app.c