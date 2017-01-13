#ifndef PARETO_H
#define PARETO_H

#include "util.h"

class ParetoSourceArg {
public:
	double alpha;
	double scale;

	double generateSize()
	{
		// uniform is uniformly distributed in (0, 1]
		double uniform = 1.0 - frandex();
		return scale / pow(uniform, (1.0/alpha));
	}
};

#endif // PARETO_H
