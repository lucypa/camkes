#
# Copyright 2014, NICTA
#
# This software may be distributed and modified according to the terms of
# the BSD 2-Clause license. Note that NO WARRANTY is provided.
# See "LICENSE_BSD2.txt" for details.
#
# @TAG(NICTA_BSD)
#

# This fragment is expected to included by other tests.

spawn make silentoldconfig
check_exit

spawn make clean
check_exit

spawn make -j
check_exit
