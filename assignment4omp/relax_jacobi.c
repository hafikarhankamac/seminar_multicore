/*
 * relax_jacobi.c
 *
 * Jacobi Relaxation
 *
 */

#include <omp.h>
#include "heat.h"


double relax_jacobi( double **u1, double **utmp1, double *diffs,
         unsigned sizex, unsigned sizey )
{
  int i, j;
  double *help,*u, *utmp,factor=0.5;

  utmp=*utmp1;
  u=*u1;
  double unew, diff, sum=0.0, temp;

#pragma omp parallel for firstprivate(sizey, j, unew, diff, sizex) reduction(+:sum)
  for( i=1; i<sizey-1; i++ ) {
  	int ii=i*sizex;
  	int iim1=(i-1)*sizex;
  	int iip1=(i+1)*sizex;
#pragma ivdep
	for (j = 1; j < sizex-1; j++) {
	  unew = 0.25 * (u[ ii+(j-1) ]+
			 u[ ii+(j+1) ]+
			 u[ iim1+j ]+
			 u[ iip1+j ]);
	  diff = unew - u[ii + j];
	  utmp[ii + j] = unew;
	  sum += diff * diff;
	}	
  }
 

  *u1=utmp;
  *utmp1=u;
  return(sum);
}


double relax_jacobi_blocked(double **u1, double **utmp1,
			    unsigned sizex, unsigned sizey)
{
  double *help,*u, *utmp,factor=0.5;

  utmp=*utmp1;
  u=*u1;
  double unew, diff, sum=0.0, temp;

  const int numx = (sizex-2) / BLOCK_SIZEX;
  const int numy = (sizey-2) / BLOCK_SIZEY;
  const int xrem = (sizex-2) % BLOCK_SIZEX;
  const int yrem = (sizey-2) % BLOCK_SIZEY;
  int b;
#pragma omp parallel for firstprivate(unew, diff) reduction(+:sum)
  for (b = 0; b < numx * numy; b++) {
    int by = b / numx;
    int bx = b % numx;
    int starty = 1 + by * BLOCK_SIZEY;
    int startx = 1 + bx * BLOCK_SIZEX;
    int endx = startx + BLOCK_SIZEX + (int)(bx == (numx-1)) * xrem;
    int endy = starty + BLOCK_SIZEY + (int)(by == (numy-1)) * yrem;
    int i, j;
    for (i = starty; i < endy; i++) {
      int ii = i*sizex;
      int iim1=(i-1)*sizex;
      int iip1=(i+1)*sizex;
      for (j = startx; j < endx; j++) {
	unew = 0.25 * (u[ ii+(j-1) ]+
			 u[ ii+(j+1) ]+
			 u[ iim1+j ]+
			 u[ iip1+j ]);
	diff = unew - u[ii + j];
	utmp[ii + j] = unew;
	sum += diff * diff;
      }
    }
  }
  *u1=utmp;
  *utmp1=u;
  return(sum);
}


    
