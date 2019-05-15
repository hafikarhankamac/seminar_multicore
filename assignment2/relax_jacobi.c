/*
 * relax_jacobi.c
 *
 * Jacobi Relaxation
 *
 */

#include "heat.h"

unsigned int blocksize = 2;

/*
 * Residual (length of error vector)
 * between current solution and next after a Jacobi step
 */
double residual_jacobi(double *u, unsigned sizex, unsigned sizey) {
	unsigned i, j, ii, jj;
	double unew, diff, sum = 0.0;

	/*
	// non-blocked
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
	*/

	/*
	// 1D-blocked
	for (ii = 1; ii < sizey - 1; ii += blocksize) {
		for (j = 1; j < sizex - 1; j++) {
			for (i = ii; i < ii + blocksize; i++) {
				unew = 0.25 * (u[i * sizex + (j - 1)] +  // left
							u[i * sizex + (j + 1)] +  // right
							u[(i - 1) * sizex + j] +  // top
							u[(i + 1) * sizex + j]); // bottom

				diff = unew - u[i * sizex + j];
				sum += diff * diff;
			}
		}
	}
	*/

	// 1D-blocked with loop interchange
	for (ii = 1; ii < sizey - 1; ii += blocksize) {
		for (i = ii; i < ii + blocksize; i++) {
			for (j = 1; j < sizex - 1; j++) {
				unew = 0.25 * (u[i * sizex + (j - 1)] +  // left
							u[i * sizex + (j + 1)] +  // right
							u[(i - 1) * sizex + j] +  // top
							u[(i + 1) * sizex + j]); // bottom

				diff = unew - u[i * sizex + j];
				sum += diff * diff;
			}
		}
	}

	/*
	// 2D-blocked
	for (jj = 1; jj < sizex - 1; jj += blocksize) {
		for (ii = 1; ii < sizey - 1; ii += blocksize) {
			for (j = jj; j < jj + blocksize; j++) {
				for (i = ii; i < ii + blocksize; i++) {
					unew = 0.25 * (u[i * sizex + (j - 1)] +  // left
							u[i * sizex + (j + 1)] +  // right
							u[(i - 1) * sizex + j] +  // top
							u[(i + 1) * sizex + j]); // bottom

					diff = unew - u[i * sizex + j];
					sum += diff * diff;
				}
			}
		}
	}
	*/

	/*
	// 2D-blocked with loop interchange
	for (ii = 1; ii < sizey - 1; ii += blocksize) {
		for (jj = 1; jj < sizex - 1; jj += blocksize) {
			for (i = ii; i < ii + blocksize; i++) {
				for (j = jj; j < jj + blocksize; j++) {
					unew = 0.25 * (u[i * sizex + (j - 1)] +  // left
							u[i * sizex + (j + 1)] +  // right
							u[(i - 1) * sizex + j] +  // top
							u[(i + 1) * sizex + j]); // bottom

					diff = unew - u[i * sizex + j];
					sum += diff * diff;
				}
			}
		}
	}
	*/

	return sum;
}

/*
 * One Jacobi iteration step
 */
void relax_jacobi(double *u, double *utmp, unsigned sizex, unsigned sizey) {
	int i, j, ii, jj;

	/*
	// non-blocked
	for (j = 1; j < sizex - 1; j++) {
		for (i = 1; i < sizey - 1; i++) {
			utmp[i * sizex + j] = 0.25 * (u[i * sizex + (j - 1)] +  // left
						u[i * sizex + (j + 1)] +  // right
						u[(i - 1) * sizex + j] +  // top
						u[(i + 1) * sizex + j]); // bottom
		}
	}
	*/

	/*
	// 1D-blocked
	for (ii = 1; ii < sizey - 1; ii += blocksize) {
		for (j = 1; j < sizex - 1; j++) {
			for (i = ii; i < ii + blocksize; i++) {
				utmp[i * sizex + j] = 0.25 * (u[i * sizex + (j - 1)] +  // left
							u[i * sizex + (j + 1)] +  // right
							u[(i - 1) * sizex + j] +  // top
							u[(i + 1) * sizex + j]); // bottom
			}
		}
	}
	*/

	// 1D-blocked with loop interchange
	for (ii = 1; ii < sizey - 1; ii += blocksize) {
		for (i = ii; i < ii + blocksize; i++) {
			for (j = 1; j < sizex - 1; j++) {
				utmp[i * sizex + j] = 0.25 * (u[i * sizex + (j - 1)] +  // left
							u[i * sizex + (j + 1)] +  // right
							u[(i - 1) * sizex + j] +  // top
							u[(i + 1) * sizex + j]); // bottom
			}
		}
	}

	/*
	// 2D-blocked
	for (jj = 1; jj < sizex - 1; jj += blocksize) {
		for (ii = 1; ii < sizey - 1; ii += blocksize) {
			for (j = jj; j < jj + blocksize; j++) {
				for (i = ii; i < ii + blocksize; i++) {
					utmp[i * sizex + j] = 0.25 * (u[i * sizex + (j - 1)] +  // left
								u[i * sizex + (j + 1)] +  // right
								u[(i - 1) * sizex + j] +  // top
								u[(i + 1) * sizex + j]); // bottom
				}
			}
		}
	}
	*/

	/*
	// 2D-blocked with loop interchange
	for (ii = 1; ii < sizey - 1; ii += blocksize) {
		for (jj = 1; jj < sizex - 1; jj += blocksize) {
			for (i = ii; i < ii + blocksize; i++) {
				for (j = jj; j < jj + blocksize; j++) {
					utmp[i * sizex + j] = 0.25 * (u[i * sizex + (j - 1)] +  // left
								u[i * sizex + (j + 1)] +  // right
								u[(i - 1) * sizex + j] +  // top
								u[(i + 1) * sizex + j]); // bottom
				}
			}
		}
	}
	*/

	// copy from utmp to u

	/*
	// non-blocked
	for (j = 1; j < sizex - 1; j++) {
		for (i = 1; i < sizey - 1; i++) {
			u[i * sizex + j] = utmp[i * sizex + j];
		}
	}
	*/

	/*
	// 1D-blocked
	for (ii = 1; ii < sizey - 1; ii += blocksize) {
		for (j = 1; j < sizex - 1; j++) {
			for (i = ii; i < ii + blocksize; i++) {
				u[i * sizex + j] = utmp[i * sizex + j];
			}
		}
	}
	*/

	// 1D-blocked with loop interchange
	for (ii = 1; ii < sizey - 1; ii += blocksize) {
		for (i = ii; i < ii + blocksize; i++) {
			for (j = 1; j < sizex - 1; j++) {
				u[i * sizex + j] = utmp[i * sizex + j];
			}
		}
	}

	/*
	// 2D-blocked
	for (jj = 1; jj < sizex - 1; jj += blocksize) {
		for (ii = 1; ii < sizey - 1; ii += blocksize) {
			for (j = jj; j < jj + blocksize; j++) {
				for (i = ii; i < ii + blocksize; i++) {
					u[i * sizex + j] = utmp[i * sizex + j];
				}
			}
		}
	}
	*/

	/*
	// 2D-blocked with loop interchange
	for (ii = 1; ii < sizey - 1; ii += blocksize) {
		for (jj = 1; jj < sizex - 1; jj += blocksize) {
			for (i = ii; i < ii + blocksize; i++) {
				for (j = jj; j < jj + blocksize; j++) {
					u[i * sizex + j] = utmp[i * sizex + j];
				}
			}
		}
	}
	*/
}