#ifndef FLOAT_CUSTOM_H
#define FLOAT_CUSTOM_H

#include <debug.h>
#include <stdint.h>

//follow 17.14 format as in the document.
typedef int my_float;
#define scale_f (1 << 14)

my_float to_my_float(int);
int to_int_rzero(my_float);
int to_int_rnear(my_float);

my_float add(my_float, my_float);
my_float add_int(my_float, my_float);
my_float sub(my_float, my_float);
my_float sub_int(my_float, int);
my_float mult(my_float, my_float);
my_float mult_int(my_float, int);
my_float div(my_float, my_float);
my_float div_int(my_float, int);

#endif /* threads/float_custom.h */