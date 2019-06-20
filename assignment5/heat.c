#include <stdio.h>
#include <stdlib.h>
#include "input.h"
#include "heat.h"
#include "timing.h"
#include "omp.h"
#include "mmintrin.h"
#include <mpi.h>

#define ind(i, j) i * (param.cols+2) + j

double* time;

double gettime() {
	return MPI_Wtime();
}

void usage(char *s) {
	fprintf(stderr, "Usage: %s <input file> <prows> <pcols> [result file]\n\n", s);
}

int main(int argc, char *argv[]) {
	int i, j, k, ret;
	FILE *infile, *resfile;
	char resfilename[50];
	int np, iter, chkflag;
	double rnorm0, rnorm1, t0, t1;
	double tmp[8000000];

	// algorithmic parameters
	algoparam_t param;
	MPI_Comm comm;

	// timing

	double residual;

	// MPI params
	param.periods[0] = 0;
	param.periods[1] = 0;

	// set the visualization resolution
	param.visres = 100;

	// check arguments
	if (argc < 4) {
		usage(argv[0]);
		return 1;
	}
	// MPI initialization
	MPI_Init(&argc, &argv);
	// Cart grid uses x-y, we use row column
	param.dims[0] = atoi(argv[3]);
	param.dims[1] = atoi(argv[2]);
	MPI_Comm_rank(MPI_COMM_WORLD, &param.rank);
	MPI_Cart_create(MPI_COMM_WORLD, 2,
                    param.dims, param.periods,
                    param.reorder, &comm);
	MPI_Cart_coords(comm, param.rank, 2, param.coords);
	MPI_Cart_shift(comm, 0, 1, &param.west, &param.east);
  	MPI_Cart_shift(comm, 1, 1, &param.north, &param.south);

	// check input file
	if (!(infile = fopen(argv[1], "r"))) {
		fprintf(stderr, "\nError: Cannot open \"%s\" for reading.\n\n", argv[1]);

		usage(argv[0]);
		return 1;
	}

	// check result file
	sprintf(resfilename, "heat%d_%d.pgm", param.coords[0], param.coords[1]);

	if (!(resfile = fopen(resfilename, "w"))) {
		fprintf(stderr, "\nError: Cannot open \"%s\" for writing.\n\n", resfilename);

		usage(argv[0]);
		return 1;
	}

	// check input
	if (!read_input(infile, &param)) {
		fprintf(stderr, "\nError: Error parsing input file.\n\n");

		usage(argv[0]);
		return 1;
	}

	if (param.rank==0) print_params(&param);
	time = (double *) calloc(sizeof(double), (int) (param.max_res - param.initial_res + param.res_step_size) / param.res_step_size);

	int exp_number = 0;

	for (param.act_res = param.initial_res; param.act_res <= param.max_res; param.act_res = param.act_res + param.res_step_size) {
		if (!initialize(&param)) {
			fprintf(stderr, "Error in Jacobi initialization.\n\n");

			usage(argv[0]);
		}
		/*FILE *fp;
   		fp = fopen(resfilename, "w");
		for (i = 0; i < param.rows + 2; i++) {
			for (j = 0; j < param.cols + 2; j++) {
				fprintf(fp, "%f ", param.u[i * (param.cols + 2) + j]);
			}
			fprintf(fp, "\n");
		}
		fclose(fp);
		exit(0);*/
		for (i = 0; i < param.rows + 2; i++) {
			for (j = 0; j < param.cols + 2; j++) {
				param.uhelp[i * (param.cols + 2) + j] = param.u[i * (param.cols + 2) + j];
			}
		}

		// starting time
		
		residual = 999999999;
		np = param.act_res + 2;
		if (param.rank == 0) {
			time[exp_number] = wtime();
			t0 = gettime();
		}
		// Initialize rbuf for non-communicating procs
		for(i = 0; i < param.rows; i++) param.rbuf[i] = param.u[(i+1)*(param.cols+2)]; //west
		for(i = 0; i < param.rows; i++) param.rbuf[param.rows + i] = param.u[(i+1)*(param.cols+2)+param.cols+1]; //east
		for(i = 0; i < param.cols; i++) param.rbuf[2 * param.rows + i] = param.u[i+1]; //north
		for(i = 0; i < param.cols; i++) param.rbuf[param.cols + 2 * param.rows + i] = param.u[(param.rows+1)*(param.cols+2)+i+1]; //south*/
		
		for (iter = 0; iter < param.maxiter; iter++) {

			for(i = 0; i < param.rows; i++) param.sbuf[i] = param.u[(i+1)*(param.cols+2)+1]; //west
			for(i = 0; i < param.rows; i++) param.sbuf[param.rows + i] = param.u[(i+1)*(param.cols+2)+param.cols]; //east
			for(i = 0; i < param.cols; i++) param.sbuf[2 * param.rows + i] = param.u[param.cols+2+i+1]; //north
			for(i = 0; i < param.cols; i++) param.sbuf[2 * param.rows + param.cols + i] = param.u[(param.rows)*(param.cols+2)+i+1]; //south			
					
			int counts[4] = {param.rows, param.rows, param.cols, param.cols};
			int displs[4] = {0, param.rows, 2*param.rows, 2*param.rows + param.cols};
			
			MPI_Neighbor_alltoallv(param.sbuf, counts, displs, MPI_DOUBLE, param.rbuf, counts, displs, MPI_DOUBLE, comm);
			for(i = 0; i < param.rows; i++) param.u[(i+1)*(param.cols+2)] = param.rbuf[i]; //west
			for(i = 0; i < param.rows; i++) param.u[(i+1)*(param.cols+2)+param.cols+1] = param.rbuf[param.rows + i]; //east
			for(i = 0; i < param.cols; i++) param.u[i+1] = param.rbuf[2 * param.rows + i]; //north
			for(i = 0; i < param.cols; i++) param.u[(param.rows+1)*(param.cols+2)+i+1] = param.rbuf[param.cols + 2 * param.rows + i]; //south*/
			

			residual = relax_jacobi(&(param.u), &(param.uhelp), param.cols+2, param.rows+2);
			/*
			FILE *fp;
			fp = fopen(resfilename, "w");
			for (i = 0; i < param.rows + 2; i++) {
				for (j = 0; j < param.cols + 2; j++) {
					fprintf(fp, "%f ", param.u[i * (param.cols + 2) + j]);
				}
				fprintf(fp, "\n");
			}
			fclose(fp);
			MPI_Barrier(comm);
			exit(0);
			*/

		}

		double total_res;
		MPI_Reduce(&residual, &total_res, 1, MPI_DOUBLE, MPI_SUM, 0, comm);

		if (param.rank == 0) {
			t1 = gettime();
			time[exp_number] = wtime() - time[exp_number];

			printf("\n\nResolution: %u\n", param.act_res);
			printf("===================\n");
			printf("Execution time: %f\n", time[exp_number]);
			printf("Residual: %f\n\n", total_res);

			printf("megaflops:  %.1lf\n", (double) param.maxiter * (np - 2) * (np - 2) * 7 / time[exp_number] / 1000000);
			printf("  flop instructions (M):  %.3lf\n", (double) param.maxiter * (np - 2) * (np - 2) * 7 / 1000000);

			exp_number++;
		}
		
	}

	param.act_res = param.act_res - param.res_step_size;

	coarsen(param.u, param.rows + 2, param.cols + 2, param.uvis, param.visres + 2, param.visres + 2);

	write_image(resfile, param.uvis, param.visres + 2, param.visres + 2);
	finalize(&param);
	MPI_Finalize();
	return 0;
}
