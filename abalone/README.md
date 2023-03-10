## Intro

This is the submission of Group 4 / 5 for the last assginment for Master-Praktikum - Mehrkern-Systeme und Supercomputer effizient programmieren (IN2106, IN4048).

The group consists of:
- Hafik Arhan Kamac
- Can Guemeli
- Stepan Vanecek

The folder abalone_measurements is for group's internal testing while the abalone_tournament has the version which we will enter the tournament with and contains the
early submission version.

## Compiling

Currently to compile the code in superMUC, one has to have the gcc version 6.3.0
You can do this by unloading the current gcc compiler and calling:

```
module load gcc/6
```
We are have implemented a MPI version so one also has to load the intel mpi with version 19. You can achieve this by calling this sequence of commands:

```
module unload intel/17.0
module load intel/19.0
module unload mpi.ibm
module load mpi.intel
```

You can invoke 
```
make
``` 
in the directory you have put the file, the Makefile should be compatible with superMUC.

Without gcc version 6.3, our code will not compile due Intel compatibility settings.


## Running the player

Currently, we have not altered the player's command line arguments and it is still the same with
the base version of the code where you need to specify everything, including max-depth.

You can call a depth greater than 5. To see which search algorithm you want to use you can call 
```
./player -h
```
to see how each algorithm is indexed.

Example:

```
mpiexec -n 28 ./player X -s 2 -n 5
```

will create a player X with depth 5, with no changing evaluation function(-n), with our search strategy "AlphaBetaSorted/Sampling"(-s 2)
