#ifndef ISNAN_H
#define ISNAN_H

#include <math.h>
#include <cmath>

// Workaround for libc bug https://sourceware.org/bugzilla/show_bug.cgi?id=19439
#ifndef isnan
#define isnan __builtin_isnan
#endif

#endif
