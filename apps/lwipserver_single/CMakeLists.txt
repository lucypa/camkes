#
# Copyright 2020, Data61
# Commonwealth Scientific and Industrial Research Organisation (CSIRO)
# ABN 41 687 119 230.
#
# This software may be distributed and modified according to the terms of
# the BSD 2-Clause license. Note that NO WARRANTY is provided.
# See "LICENSE_BSD2.txt" for details.
#
# @TAG(DATA61_BSD)
#

cmake_minimum_required(VERSION 3.8.2)

project(lwipserver_single C)
includeGlobalComponents()

set(CAmkESCPP ON CACHE BOOL "" FORCE)
if("${KernelArch}" STREQUAL "x86")
    set(cpp_define -DKernelArchX86)
elseif("${KernelArch}" STREQUAL "arm")
    set(cpp_define -DKernelArchArm)
endif()

include(${LWIP_HELPERS})
AddLWIPConfiguration(${CMAKE_CURRENT_LIST_DIR}/lwip_include)

set(libs sel4utils sel4vka sel4allocman sel4vspace sel4simple sel4platsupport ethdrivers lwip)

set(sources src/ethdriver.c src/tcp_echo_socket.c src/udp_echo_socket.c src/utilization_socket.c)

DeclareCAmkESComponent(
    Ethdriver82574_1
    SOURCES
    ${sources}
    src/x86_64_eth_init.c
    INCLUDES
    include
    LIBS
    ${libs}
)

DeclareCAmkESComponent(
    EthdriverARMPlat_1
    SOURCES
    ${sources}
    INCLUDES
    include
    LIBS
    ${libs}
)

DeclareCAmkESRootserver(
    lwipserver_single.camkes
    CPP_FLAGS
    ${cpp_define}
)

CAmkESAddCPPInclude("${CMAKE_CURRENT_LIST_DIR}/include/")
