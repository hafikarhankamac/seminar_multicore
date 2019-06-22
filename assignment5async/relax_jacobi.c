/*
 * relax_jacobi.c
 *
 * Jacobi Relaxation
 *
 */

#include "heat.h"


double relax_jacobi( double **u1, double **utmp1,
         unsigned sizex, unsigned sizey )
{
  int i, j;
  double *help,*u, *utmp,factor=0.5;

  utmp=*utmp1;
  u=*u1;
  double unew, diff, sum=0.0;


  for( i=1; i<sizey-1; i++ ) {
  	int ii=i*sizex;
  	int iim1=(i-1)*sizex;
  	int iip1=(i+1)*sizex;
#pragma ivdep
    for( j=1; j<sizex-1; j++ ){
       unew = 0.25 * (u[ ii+(j-1) ]+
        		            u[ ii+(j+1) ]+
        		            u[ iim1+j ]+
        		            u[ iip1+j ]);
		    diff = unew - u[ii + j];
		    utmp[ii+j] = unew;
		    sum += diff * diff;

       }
    }

  *u1=utmp;
  *utmp1=u;
  return(sum);
}

double relax_jacobi_outer( double **u1, double **utmp1, unsigned sizex, unsigned sizey)
{
   int i, j, m;
  double *help,*u, *utmp,factor=0.5;

  utmp=*utmp1;
  u=*u1;
  double unew, diff, sum=0.0;
  int iboundaries[] = {1, sizey-2};
  int jboundaries[] = {1, sizex-2};

  for (m=0; m < 2; ++m) {
    i = iboundaries[m];
    int ii=i*sizex;
  	int iim1=(i-1)*sizex;
  	int iip1=(i+1)*sizex;
#pragma ivdep
    for( j=1; j<sizex-1; j++ ){
       unew = 0.25 * (u[ ii+(j-1) ]+
        		            u[ ii+(j+1) ]+
        		            u[ iim1+j ]+
        		            u[ iip1+j ]);
		    diff = unew - u[ii + j];
		    utmp[ii+j] = unew;
		    sum += diff * diff;

       }
    }

  for( i=2; i<sizey-2; i++ ) {
  	int ii=i*sizex;
  	int iim1=(i-1)*sizex;
  	int iip1=(i+1)*sizex;
    for (m = 0; m < 2; m++) {
      j = jboundaries[m];
      unew = 0.25 * (u[ ii+(j-1) ]+
        		            u[ ii+(j+1) ]+
        		            u[ iim1+j ]+
        		            u[ iip1+j ]);
		    diff = unew - u[ii + j];
		    utmp[ii+j] = unew;
		    sum += diff * diff;
    }
  }
  return(sum);
}

double relax_jacobi_inner( double **u1, double **utmp1, unsigned sizex, unsigned sizey)
{
  int i, j;
  double *help,*u, *utmp,factor=0.5;

  utmp=*utmp1;
  u=*u1;
  double unew, diff, sum=0.0;


  for( i=2; i<sizey-2; i++ ) {
  	int ii=i*sizex;
  	int iim1=(i-1)*sizex;
  	int iip1=(i+1)*sizex;
#pragma ivdep
    for( j=2; j<sizex-2; j++ ){
       unew = 0.25 * (u[ ii+(j-1) ]+
        		            u[ ii+(j+1) ]+
        		            u[ iim1+j ]+
        		            u[ iip1+j ]);
		    diff = unew - u[ii + j];
		    utmp[ii+j] = unew;
		    sum += diff * diff;
       }
    }

  return(sum);
}

void swap( double **u1, double **utmp1 ) {
  double *u, *utmp;
  utmp=*utmp1;
  u=*u1;
  *u1=utmp;
  *utmp1=u;
}