/*
This directory contains source code related to the book
"Graphics Gems" (editor, Andrew S. Glassner, published by
Academic Press, Cambridge, MA, 1990, ISBN 0-12-286165-5, 833 pgs.).

The authors and the publisher hold no copyright restrictions 
on any of these files; this source code is public domain, and 
is freely available to the entire computer graphics community 
for study, use, and modification.  We do request that the 
comment at the top of each file, identifying the original 
author and its original publication in the book Graphics 
Gems, be retained in all programs that use these files.

The compressed tar files that hold the gems source code
should be downloaded using binary transfer mode.

Additional submissions (bug fixes, skeleton programs, auxiliary
routines, etc.) may be directed to the site administrator,
Craig Kolb (cek@princeton.edu).

Thanks to Eric Haines for compiling and maintaining the
errata list.
*/

/*
 * Nice Numbers for Graph Labels
 * by Paul Heckbert
 * from "Graphics Gems", Academic Press, 1990
 */

/*
 * label.c: demonstrate nice graph labeling
 *
 * Paul Heckbert	2 Dec 88
 */

/*
  Modified by Ovidiu Mara, 2011
*/


#include "nicelabel.h"
#include <math.h>

#define MAX(a,b) (((a)>(b))?(a):(b))

double nicenum(double x, int round);

/*main(int ac, char **av)
{
	double min, max;
	int nticks;

	printf("%f\n", pow(10., 2.));

	if (ac!=4) {
		fprintf(stderr, "Usage: label <min> <max> <nticks>\n");
		exit(1);
	}
	min = atof(av[1]);
	max = atof(av[2]);
	nticks = atoi(av[3]);

	loose_label(min, max, nticks);
	sprintf(str, "%%.%df", nfrac);	// simplest axis labels

	printf("graphmin=%g graphmax=%g increment=%g\n", graphmin, graphmax, d);
	for (x=graphmin; x<graphmax+.5*d; x+=d) {
		sprintf(temp, str, x);
		printf("(%s)\n", temp);
	}
}*/

/*
 * loose_label: demonstrate loose labeling of data range from min to max.
 * (tight method is similar)
 * Params:
 * min = min value of axis
 * max = max value of axis
 * nticks = desired number of ticks (choose a good value based on text & graph width)
 * start = set to the coordinate of the first tick
 * size = set to the distance between 2 ticks
 */
void nice_loose_label(double min, double max, int nticks, double &start, double &size)
{
	double d;				/* tick mark spacing */
	double graphmin;		/* graph range min */
	double range;

	/* we expect min!=max */
	range = nicenum(max-min, 0);
	d = nicenum(range/(nticks-1), 1);
	graphmin = floor(min/d)*d;
	start = graphmin;
	size = d;
}

/*
 * nicenum: find a "nice" number approximately equal to x.
 * Round the number if round=1, take ceiling if round=0
 */
double nicenum(double x, int round)
{
	int expv;				/* exponent of x */
	double f;				/* fractional part of x */
	double nf;				/* nice, rounded fraction */

	expv = floor(log10(x));
	f = x/pow(10., expv);		/* between 1 and 10 */
	if (round)
		if (f<1.5) nf = 1.;
		else if (f<3.) nf = 2.;
		else if (f<7.) nf = 5.;
		else nf = 10.;
	else
		if (f<=1.) nf = 1.;
		else if (f<=2.) nf = 2.;
		else if (f<=5.) nf = 5.;
		else nf = 10.;
	return nf*pow(10., expv);
}

