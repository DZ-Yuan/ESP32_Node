#ifndef UTIL
#define UTIL

#include <stdint.h>

int get_bit_value(uint32_t *data, int index);
void set_bit_value(uint32_t *data, int index, int f);

#endif