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

#include "common.h"

int run()
{
    printf("Calling server...\n");
    a_f();
    printf("Back from server!\n");
    printf("Client is testing alignment...\n");
    test_alignment();
    printf("All is well in the universe!\n");

    return 0;
}
