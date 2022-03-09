/*
 * Copyright 2017, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(DATA61_BSD)
 */

#include <camkes.h>
#include <stdio.h>
#include "payload.h"
#include <camkes/dataport.h>

int run(void)
{
    int operands[] = { 342, 74, 283, 37, 534 };
    int sz = sizeof(operands) / sizeof(int);
    const char *name = get_instance_name();

    printf("%s: what's the answer to ", name);
    for (int i = 0; i < sz; i++) {
        printf("%d ", operands[i]);
        if (i != sz - 1) {
            printf("+ ");
        }
    }
    printf("?\n");

    struct payload *p = (void *)d + 1024;
    p->sz = sz;
    for (int i = 0; i < sz; i++) {
        p->operands[i] = operands[i];
    }

    dataport_ptr_t ptr = a_calculate(dataport_wrap_ptr((void *)p));
    p = dataport_unwrap_ptr(ptr);

    printf("%s: result was %d\n", name, p->result);
    return 0;
}
