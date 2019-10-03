#include "threads/float_custom.h"
#include <debug.h>
#include <stddef.h>
#include <stdint.h>
#include <random.h>
#include <stdio.h>
#include <string.h>

my_float to_my_float(int n){
    return n*scale_f;
}
int to_int_rzero(my_float x){
    return x/scale_f;
}
int to_int_rnear(my_float x){
    if(x>=0)
    {
        return (x + scale_f / 2) / scale_f;
    }
    else
        return (x - scale_f / 2) / scale_f;
}

my_float add(my_float x, my_float y){
    return x + y;
}
my_float sub(my_float x, my_float y){
    return x - y;
}
my_float add_int(my_float x, int n){
    return x + n * scale_f;
}
my_float sub_int(my_float x, int n){
    return x - n * scale_f;
}
my_float mult(my_float x, my_float y){
    return ((int64_t) x) * y / scale_f;
}
my_float mult_int(my_float x, int n){
    return x * n;
}
my_float div(my_float x, my_float y){
    return ((int64_t) x) * scale_f / y;
}
my_float div_int(my_float x, int n){
    return x/n;
}