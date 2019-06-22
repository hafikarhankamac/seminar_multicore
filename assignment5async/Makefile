CC =  gcc
CFLAGS = -fopenmp -O3 

MPICC = mpicc.mpich

all: heat 

heat : heat.o input.o misc.o timing.o relax_jacobi.o
	$(MPICC) $(CFLAGS) -o heat $+ -lm

%.o : %.c %.h
	$(MPICC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o heat *~ *.ppm

remake : clean all
