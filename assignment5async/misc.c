/*
 * misc.c
 *
 * Helper functions for
 * - initialization
 * - finalization,
 * - writing out a picture
 */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <omp.h>

#include "heat.h"

/*
 * Initialize the iterative solver
 * - allocate memory for matrices
 * - set boundary conditions according to configuration
 */
int initialize( algoparam_t *param )
{
    int i, j;
    double dist;

    // total number of points (including border)
    const int np = param->act_res + 2;
	param->rows = param->act_res / param->dims[1];
	param->cols = param->act_res / param->dims[0];
	
	int r = param->coords[1];
	int c = param->coords[0];
	int roffset = param->rows * r;
	int coffset = param->cols * c;
	if (param->coords[1] == (param->dims[1]-1))  param->rows += param->act_res % param->dims[1];
	if (param->coords[0] == (param->dims[0]-1))  param->cols += param->act_res % param->dims[0];
	//if (param->coords[0] > 0) coffset++;

	int nrows = param->rows + 2;
	int ncols = param->cols + 2;
    //
    // allocate memory
    //
	(param->sbuf)  = (double*)calloc( sizeof(double), 2 * (param->rows + param->cols));
	(param->rbuf)  = (double*)calloc( sizeof(double), 2 * (param->rows + param->cols));
    (param->u)     = (double*)malloc( sizeof(double)* nrows*ncols);
    (param->uhelp) = (double*)malloc( sizeof(double)* nrows*ncols );
    (param->uvis)  = (double*)calloc( sizeof(double),
				      (param->visres+2) *
				      (param->visres+2) );

    for (i=0;i<nrows;i++){
    	for (j=0;j<ncols;j++){
    		param->u[i*ncols+j]=0;
			param->uhelp[i*ncols+j]=0;
    	}
    }



    if( !(param->u) || !(param->uhelp) || !(param->uvis) )
    {
	fprintf(stderr, "Error: Cannot allocate memory\n");
	return 0;
    }

    for( i=0; i<param->numsrcs; i++ )
    {
	/* top row */
	if(r == 0) for( j=0; j<ncols; j++ )
	{
	    dist = sqrt( pow((double)(j+coffset)/(double)(np-1) -
			     param->heatsrcs[i].posx, 2)+
			 pow(param->heatsrcs[i].posy, 2));

	    if( dist <= param->heatsrcs[i].range )
	    {
		(param->u)[j] +=
		    (param->heatsrcs[i].range-dist) /
		    param->heatsrcs[i].range *
		    param->heatsrcs[i].temp;
	    }
	}

	/* bottom row */
	if(r == (param->dims[1]-1)) for( j=0; j<ncols; j++ )
	{
	    dist = sqrt( pow((double)(j+coffset)/(double)(np-1) -
			     param->heatsrcs[i].posx, 2)+
			 pow(1-param->heatsrcs[i].posy, 2));

	    if( dist <= param->heatsrcs[i].range )
	    {
		(param->u)[(nrows-1)*ncols+j]+=
		    (param->heatsrcs[i].range-dist) /
		    param->heatsrcs[i].range *
		    param->heatsrcs[i].temp;
	    }
	}

	/* leftmost column */
	if(c == 0) for( j=0; j<nrows; j++ )
	{
	    dist = sqrt( pow(param->heatsrcs[i].posx, 2)+
			 pow((double)(j+roffset)/(double)(np-1) -
			     param->heatsrcs[i].posy, 2));

	    if( dist <= param->heatsrcs[i].range )
	    {
		(param->u)[ j*ncols ]+=
		    (param->heatsrcs[i].range-dist) /
		    param->heatsrcs[i].range *
		    param->heatsrcs[i].temp;
	    }
	}

	/* rightmost column */
	if(c == (param->dims[0]-1)) for( j=0; j<nrows; j++ )
	{
	    dist = sqrt( pow(1-param->heatsrcs[i].posx, 2)+
			 pow((double)(roffset+j)/(double)(np-1) -
			     param->heatsrcs[i].posy, 2));

	    if( dist <= param->heatsrcs[i].range )
	    {
		(param->u)[ j*ncols+(ncols-1) ]+=
		    (param->heatsrcs[i].range-dist) /
		    param->heatsrcs[i].range *
		    param->heatsrcs[i].temp;
	    }
	}
    }

    return 1;
}

/*
 * free used memory
 */
int finalize( algoparam_t *param )
{
    if( param->u ) {
	free(param->u);
	param->u = 0;
    }

    if( param->uhelp ) {
	free(param->uhelp);
	param->uhelp = 0;
    }

    if( param->uvis ) {
	free(param->uvis);
	param->uvis = 0;
    }

	if( param->sbuf ) {
	free(param->sbuf);
	param->sbuf = 0;
    }

	if( param->rbuf ) {
	free(param->rbuf);
	param->rbuf = 0;
    }

    return 1;
}


/*
 * write the given temperature u matrix to rgb values
 * and write the resulting image to file f
 */
void write_image( FILE * f, double *u,
		  unsigned sizex, unsigned sizey )
{
    // RGB table
    unsigned char r[1024], g[1024], b[1024];
    int i, j, k;

    double min, max;

    j=1023;

    // prepare RGB table
    for( i=0; i<256; i++ )
    {
	r[j]=255; g[j]=i; b[j]=0;
	j--;
    }
    for( i=0; i<256; i++ )
    {
	r[j]=255-i; g[j]=255; b[j]=0;
	j--;
    }
    for( i=0; i<256; i++ )
    {
	r[j]=0; g[j]=255; b[j]=i;
	j--;
    }
    for( i=0; i<256; i++ )
    {
	r[j]=0; g[j]=255-i; b[j]=255;
	j--;
    }

    min=DBL_MAX;
    max=-DBL_MAX;

    // find minimum and maximum
    for( i=0; i<sizey; i++ )
    {
	for( j=0; j<sizex; j++ )
	{
	    if( u[i*sizex+j]>max )
		max=u[i*sizex+j];
	    if( u[i*sizex+j]<min )
		min=u[i*sizex+j];
	}
    }


    fprintf(f, "P3\n");
    fprintf(f, "%u %u\n", sizex, sizey);
    fprintf(f, "%u\n", 255);

    for( i=0; i<sizey; i++ )
    {
	for( j=0; j<sizex; j++ )
	{
	    k=(int)(1024.0*(u[i*sizex+j]-min)/(max-min));
	    fprintf(f, "%d %d %d  ", r[k], g[k], b[k]);
	}
	fprintf(f, "\n");
    }
}

int coarsen( double *uold, unsigned oldx, unsigned oldy ,
	     double *unew, unsigned newx, unsigned newy )
{
    int i, j, k, l, ii, jj;

    int stopx = newx;
    int stopy = newy;
    float temp;
    float stepx = (float) oldx/(float)newx;
    float stepy = (float)oldy/(float)newy;

    if (oldx<newx){
	 stopx=oldx;
	 stepx=1.0;
    }
    if (oldy<newy){
     stopy=oldy;
     stepy=1.0;
    }

    //printf("oldx=%d, newx=%d\n",oldx,newx);
    //printf("oldy=%d, newy=%d\n",oldy,newy);
    //printf("rx=%f, ry=%f\n",stepx,stepy);
    // NOTE: this only takes the top-left corner,
    // and doesnt' do any real coarsening

    for( i=0; i<stopy; i++ ){
       ii=stepy*i;
       for( j=0; j<stopx; j++ ){
          jj=stepx*j;
          temp = 0;
          for ( k=0; k<stepy; k++ ){
	       	for ( l=0; l<stepx; l++ ){
	       		if (ii+k<oldx && jj+l<oldy)
		           temp += uold[(ii+k)*oldx+(jj+l)] ;
	        }
	      }
	      unew[i*newx+j] = temp / (stepy*stepx);
       }
    }

  return 1;
}
