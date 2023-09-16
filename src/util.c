#include "pch.h"
#include "util.h"

int get_bit_value(uint32_t *data, int index)
{
    return *data >> index & 1;
}

void set_bit_value(uint32_t *data, int index, int f)
{
    uint32_t t = 1 << index;
    *data = f ? *data | t : *data & ~t;
}