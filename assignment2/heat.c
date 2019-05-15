#include "heat.h"

#include <stdio.h>
#include <stdlib.h>

#include "input.h"
#include "timing.h"

/*#ifndef PROFILE
#define PROFILE 1
#endif*/

//#ifdef PROFILE
#include <papi.h>


void usage(char *s) {
	fprintf(stderr, "Usage: %s <input file> [result file]\n\n", s);
}


int main(int argc, char *argv[]) {
  unsigned iter;
  FILE *infile, *resfile;
  char *resfilename;
  
	// algorithmic parameters
  algoparam_t param;
  int np,i;  
  double runtime, flop;
  double residual;
  double time[1000];
  double floprate[1000];
  int resolution[1000];
  int experiment=0;

  if (argc < 2) {
    usage(argv[0]);
    return 1;
  }

  if (!(infile = fopen(argv[1], "r"))) {
    fprintf(stderr, "\nError: Cannot open \"%s\" for reading.\n\n", argv[1]);
    usage(argv[0]);
    return 1;
  }

  // check result file
  resfilename = (argc >= 3) ? argv[2] : "heat.ppm";

  if (!(resfile = fopen(resfilename, "w"))) {
    fprintf(stderr, "\nError: Cannot open \"%s\" for writing.\n\n", resfilename);

    usage(argv[0]);
    return 1;
  }

  if (!read_input(infile, &param)) {
    fprintf(stderr, "\nError: Error parsing input file.\n\n");
    usage(argv[0]);
    return 1;
  }

  print_params(&param);

  // set the visualization resolution
  param.visres = 1024;
  
  param.u = 0;
  param.uhelp = 0;
  param.uvis = 0;
  param.act_res = param.initial_res;
  //if (param.algorithm == 0) param.maxiter++;
  while (1) {
    // free allocated memory of previous experiment
    if (param.u != 0)
      finalize(&param);
    if (!initialize(&param))  {
      fprintf(stderr, "Error in Jacobi initialization.\n\n");
      usage(argv[0]);
    }
      double l2_misses = 0, l3_misses = 0, l2_accesses = 0, l3_accesses=0;  
      fprintf(stderr, "Resolution: %5u\r", param.act_res);
      // full size (param.act_res are only the inner points)
      np = param.act_res + 2;
      // starting time
      runtime = wtime();
      residual = 999999999;
      iter = 0;

      while (1) {
	int numEvents = 4;
	long long values[4];
	//#ifdef PROFILE
	int events[4] = {PAPI_L2_TCM, PAPI_L2_TCA, PAPI_L3_TCM, PAPI_L3_TCA};
	//#endif
	int error;

	switch(param.algorithm) {

	case 0: //JACOBI
	  // u=u1, uhelp=u1
	  
	  /*jacobi_update(param.u, param.uhelp, np, np);
	    residual = jacobi_compute(param.u, param.uhelp, np, np);*/
	  //residual = jacobi_compute(param.u, param.uhelp, np, np);
	  //u=u1, uhelp=u1
	  //residual = jacobi_compute(param.u, param.uhelp, np, np);
	  if ((error = PAPI_start_counters(events, numEvents)) != PAPI_OK) {
	    printf("PAPI error: %d\n", error);
	  }
	  
	  
	  relax_jacobi(param.u, param.uhelp, np, np);
	  //relax_jacobi_fast(param.u, param.uhelp, np, np);
	  residual = residual_jacobi(param.u, np, np);

	  if ( (error = PAPI_stop_counters(values, numEvents)) != PAPI_OK) {
	    printf("PAPI error: %d\n", error);
	  }
	  l2_misses += values[0];
	  l2_accesses += values[1];
	  l3_misses += values[2];
	  l3_accesses += values[3];
	  break;

	case 1: //GAUSS
	  relax_gauss(param.u, np, np);
	  residual = residual_gauss(param.u, param.uhelp, np, np);
	  break;
	}

	iter++;
	// solution good enough ?
	if (residual < 0.000005)
	  break;
	// max. iteration reached ? (no limit with maxiter=0)
	if (param.maxiter > 0 && iter >= param.maxiter)
	  break;
	if (iter % 100 == 0)
	  fprintf(stderr, "residual %f, %d iterations\n", residual, iter);
      }

      fprintf(stderr, "L2 cache misses: %f, L2 total cache accesses: %f\n", l2_misses, l2_accesses);
      fprintf(stderr, "L3 cache misses: %f, L3 total cache accesses: %f\n", l3_misses, l3_accesses);
      fprintf(stderr, "Miss rates: L2: %f / L3: %f\n", l2_misses / l2_accesses, l3_misses / l3_accesses);

      flop = iter * 11.0 * param.act_res * param.act_res;
      // stopping time
      runtime = wtime() - runtime;
      fprintf(stderr, "Resolution: %5u, ", param.act_res);
      fprintf(stderr, "Time: %04.3f ", runtime);
      fprintf(stderr, "(%3.3f GFlop => %6.2f MFlop/s, ", flop / 1000000000.0, flop / runtime / 1000000);
      fprintf(stderr, "residual %f, %d iterations)\n\n", residual, iter);
      // for plot...
      time[experiment]=runtime;
      floprate[experiment]=flop / runtime / 1000000;
      resolution[experiment]=param.act_res;
      
      if (param.act_res + param.res_step_size > param.max_res)
	break;
      param.act_res += param.res_step_size;
      experiment++;
  }
  for (i=0;i<experiment; i++){
    printf("%5d; %5.3f; %5.3f\n", resolution[i], time[i], floprate[i]);
  }
  coarsen(param.u, np, np, param.uvis, param.visres + 2, param.visres + 2);
  write_image(resfile, param.uvis, param.visres + 2, param.visres + 2);
  finalize(&param);

  return 0;
}
