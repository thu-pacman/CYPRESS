CC =mpicc
OPT=-O2
CFLAGS = -Wall #-I/usr/local/mpich2-1.0.5.gcc/include
DEF= -DPERF_TRACE -DMPIZIP_NO_TAG #-DMPICH2
AR = ar
ARTAG = rcv

all	: libmpit intercomp

libmpit : trace.o timer.o mpiwrapper.o mpiwrapper_f.o intrafunc.o
	$(AR) $(ARTAG) libmpit.a mpiwrapper.o mpiwrapper_f.o timer.o trace.o intrafunc.o
	$(RM) -f *.o
trace.o : trace.c	
	$(CC) $(CFLAGS) $(DEF) -c trace.c
mpiwrapper.o: mpiwrapper.c
	$(CC) $(CFLAGS) $(DEF) -c mpiwrapper.c
mpiwrapper_f.o: mpiwrapper_f.c
	$(CC) $(CFLAGS) $(DEF) -c mpiwrapper_f.c 
timer.o: timer.c	
	$(CC) $(CFLAGS) -c timer.c
intrafunc.o: intrafunc.c
	$(CC) $(CFLAGS) -c intrafunc.c
intercomp : intercomp.c
	$(CC) $(CFLAGS) intercomp.c timer.c -o compresser -lpthread
clean :
	rm -f *.o	rm -f libmpit.a rm -f parse rm -f compresser
