/*
 * heat.h
 *
 * Global definitions for the iterative solver
 */

#ifndef JACOBI_H_INCLUDED
#define JACOBI_H_INCLUDED

#define BLOCK_SIZEX 1000
#define BLOCK_SIZEY 8
//#define BLOCKED 1


#include <stdio.h>

// configuration

typedef struct
{
    float posx;
    float posy;
    float range;
    float temp;
}
heatsrc_t;

typedef struct
{
    unsigned maxiter;       // maximum number of iterations
    unsigned act_res;
    unsigned max_res;       // spatial resolution
    unsigned initial_res;
    unsigned res_step_size;
    unsigned visres;        // visualization resolution
  
  double *u, *uhelp, *diffs;
    double *uvis;

    unsigned   numsrcs;     // number of heat sources
    heatsrc_t *heatsrcs;
}
algoparam_t;


// function declarations

// misc.c
int initialize( algoparam_t *param );
int finalize( algoparam_t *param );
void write_image( FILE * f, double *u,
		  unsigned sizex, unsigned sizey );
int coarsen(double *uold, unsigned oldx, unsigned oldy ,
	    double *unew, unsigned newx, unsigned newy );

// Gauss-Seidel: relax_gauss.c
double residual_gauss( double *u, double *utmp,
		       unsigned sizex, unsigned sizey );
void relax_gauss( double *u, 
		  unsigned sizex, unsigned sizey  );

// Jacobi: relax_jacobi.c
double residual_jacobi( double *u,
			unsigned sizex, unsigned sizey );
double relax_jacobi( double **u, double **utmp, double *diffs,
		   unsigned sizex, unsigned sizey );
double relax_jacobi_blocked( double **u, double **utmp,
		   unsigned sizex, unsigned sizey ); 


#endif // JACOBI_H_INCLUDED
