CC =  icc
CFLAGS = -O3 -xhost -qopenmp -qopt-report=5 -qopt-report-phase=par $(PAPI_INC)

MPICC = mpicc

all: heat 

heat : heat.o input.o misc.o timing.o relax_jacobi.o
	$(CC) $(CFLAGS) -o heat $+ -lm  $(PAPI_LIB)

%.o : %.c %.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o heat *~ *.ppm

remake : clean all
