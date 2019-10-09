#include "threads/mlfqs_priority.h"
#include "threads/thread.h"  /* included for PRI_MAX and PRI_MIN */

#include <debug.h>
#include <stdio.h>
#include <string.h>

/* as system boot, load_avg initialized to 0 */

my_float calc_load_avg(my_float old_load_avg, int ready_threads){
    /* (59/60) * old_load_avg + (1/60) * ready_threads; */
    return add(mult(_59div60, old_load_avg), mult_int(_1div60, ready_threads));
}

my_float calc_recent_cpu(my_float old_recent_cpu, my_float load_avg, int nice)
{
    my_float twice_load_avg;
    my_float r_cpu_coef;
    /* (2* load_avg) / (2*load_avg + 1) * recent_cpu + nice) */
    twice_load_avg = mult_int(load_avg, 2);
    r_cpu_coef = div(twice_load_avg, add_int(twice_load_avg, 1));
    return add_int(mult(r_cpu_coef, old_recent_cpu), nice);
}

/* return as PRI_MIN ~ PRI_MAX value 
   rounded down to the neartest integer */
int calc_priority(my_float recent_cpu, int nice)
{
    my_float init_priority;
    init_priority = to_int_rzero(sub(float_pri_max, add_int(div_int(recent_cpu, 4), (nice * 2))));
    if (init_priority >= PRI_MAX)
    {
        return PRI_MAX;
    }
    else if (init_priority <= PRI_MIN)
    {
        return PRI_MIN;
    }
    return init_priority;
}

int near_100x_int(my_float target_float)
{
    return to_int_rnear(mult_int(target_float, 100));
}