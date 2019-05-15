/*
 * relax_jacobi.c
 *
 * Jacobi Relaxation
 *
 */

#include "heat.h"


/*
 * Residual (length of error vector)
 * between current solution and next after a Jacobi step
 */
double residual_jacobi(double *u, unsigned sizex, unsigned sizey) {
	unsigned i, j;
	double unew, diff, sum = 0.0;
	
	for (j = 1; j < sizex - 1; j++) {
		for (i = 1; i < sizey - 1; i++) {
			unew = 0.25 * (u[i * sizex + (j - 1)] +  // left
						u[i * sizex + (j + 1)] +  // right
						u[(i - 1) * sizex + j] +  // top
						u[(i + 1) * sizex + j]); // bottom

			diff = unew - u[i * sizex + j];
			sum += diff * diff;
		}
	}

	return sum;
}


double residual_jacobi_fast(double *u, double *uahead, unsigned sizex, unsigned sizey) {
  unsigned i, j;
  double unew, diff, sum = 0.0;
  for (i = 1; i < sizey - 1; i++) {
    for (j = 1; j < sizex - 1; j++) {
      uahead[i * sizex + j] = (u[i * sizex + (j - 1)] +  // left
			       u[i * sizex + (j + 1)]);  // right
    }
  }
  for (i = 1; i < sizey - 1; i++) {
    for (j = 1; j < sizex - 1; j++) {
      uahead[i * sizex + j] += u[(i - 1) * sizex + j];
    }
  }
  for (i = 1; i < sizey - 1; i++) {
    for (j = 1; j < sizex - 1; j++) {
      uahead[i * sizex + j] += u[(i + 1) * sizex + j];
    }
  }
  for (i = 1; i < sizey - 1; i++) {
    for (j = 1; j < sizex - 1; j++) {
      uahead[i * sizex + j] *= 0.25;
    }
  }
  for (i = 1; i < sizey - 1; i++) {
    for (j = 1; j < sizex - 1; j++) {
      diff = uahead[i * sizex + j] - u[i * sizex + j];
      sum += diff * diff;
    }
  }
  return sum;
}


void relax_jacobi(double *u, double *utmp, unsigned sizex, unsigned sizey) {
  int i, j;
  for (j = 1; j < sizex - 1; j++) {
    for (i = 1; i < sizey - 1; i++) {
      utmp[i * sizex + j] = 0.25 * (u[i * sizex + (j - 1)] +  // left
				    u[i * sizex + (j + 1)] +  // right
				    u[(i - 1) * sizex + j] +  // top
				    u[(i + 1) * sizex + j]); // bottom
    }
  }

    // copy from utmp to u
  
    for (j = 1; j < sizex - 1; j++) {
      for (i = 1; i < sizey - 1; i++) {
	u[i * sizex + j] = utmp[i * sizex + j];
      }
    }
}


/*double residual_jacobi_fast(double *u, double *diffs,
			    unsigned sizex, unsigned sizey) {
  unsigned i, j;
  double unew, diff, sum = 0.0;
  for (i = 1; i < sizey - 1; i++) {
    for (j = 1; j < sizex - 1; j++) {
      unew = 0.25 * (u[i * sizex + (j - 1)] +  // left
		     u[i * sizex + (j + 1)]);  // right
      diffs[i * sizex + j] = unew;
    }
  }
  for (i = 1; i < sizey - 1; i++) {
    for (j = 1; j < sizex - 1; j++) {
      
    }
  }
*/

  
//}
/*
 * One Jacobi iteration step
 */
void relax_jacobi_fast(double *u, double *utmp, unsigned sizex, unsigned sizey) {
  int i, j;
  for (i = 1; i < sizey - 1; i++) {
# pragma ivdep
    for (j = 1; j < sizex - 1; j++) {
      utmp[i * sizex + j] = (u[i * sizex + (j - 1)] +  // left
			     u[i * sizex + (j + 1)]);  // right
    }
  }
  for (i = 1; i < sizey - 1; i++) {
# pragma ivdep
    for (j = 1; j < sizex - 1; j++) {
      utmp[i * sizex + j] += u[(i - 1) * sizex + j];
    }
  }
  for (i = 1; i < sizey - 1; i++) {
# pragma ivdep
    for (j = 1; j < sizex - 1; j++) {
      utmp[i * sizex + j] += u[(i + 1) * sizex + j];
    }
  }
  for (i = 1; i < sizey - 1; i++) {
    # pragma ivdep
    for (j = 1; j < sizex - 1; j++) {
      utmp[i * sizex + j] *= 0.25;
    }
  }
  // copy from utmp to u
  for (i = 1; i < sizey - 1; i++) {
    #pragma ivdep
    for (j = 1; j < sizex - 1; j++) {
      u[i * sizex + j] = utmp[i * sizex + j];
    }
  }
}
