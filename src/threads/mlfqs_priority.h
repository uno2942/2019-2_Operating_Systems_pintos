#ifndef MLFQS_PRIORITY_H
#define MLFQS_PRIORITY_H

#include "threads/float_custom.h"

/* make 59/60, 1/60 as constant. */
#define _59div60 16110
#define _1div60 273

#define float_pri_max PRI_MAX << 14

my_float calc_load_avg(my_float, int);
my_float calc_recent_cpu(my_float, my_float, int);
int calc_priority(my_float, int);
int near_100x_int(my_float);

#endif /* threads/mlfqs_priority.h */
