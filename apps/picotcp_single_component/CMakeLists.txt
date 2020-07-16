#
<<<<<<< HEAD
# Copyright 2019, Data61
=======
# Copyright 2020, Data61
>>>>>>> 7efca8f (picotcp_single_component: Add picotcp example)
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

project(Ethdriver C)
includeGlobalComponents()

set(CAmkESCPP ON CACHE BOOL "" FORCE)
if("${KernelArch}" STREQUAL "x86")
    set(cpp_define -DKernelArchX86)
elseif("${KernelArch}" STREQUAL "arm")
    set(cpp_define -DKernelArchArm)
endif()
set(PICOSERVER_IP_ADDR "" CACHE STRING "IP address for the Picoserver component")

<<<<<<< HEAD
CAmkESAddCPPInclude("${CMAKE_CURRENT_LIST_DIR}/include/plat/${KernelPlatform}/")

set(ETHDRIVER_LIBS sel4utils sel4vka sel4allocman sel4vspace sel4simple sel4platsupport ethdrivers)
=======
CAmkESAddCPPInclude("${CMAKE_CURRENT_LIST_DIR}/src/")

set(libs sel4utils sel4vka sel4allocman sel4vspace sel4simple sel4platsupport ethdrivers)

set(sources src/ethdriver.c src/tcp_echo_socket.c src/udp_echo_socket.c src/utilization_socket.c)
>>>>>>> 7efca8f (picotcp_single_component: Add picotcp example)

DeclareCAmkESComponent(
    Ethdriver82574_1
    SOURCES
<<<<<<< HEAD
    src/ethdriver.c
=======
    ${sources}
>>>>>>> 7efca8f (picotcp_single_component: Add picotcp example)
    x86_64_eth_init.c
    INCLUDES
    include
    LIBS
<<<<<<< HEAD
    "${ETHDRIVER_LIBS}"
)

DeclareCAmkESComponent(
    EthdriverARMPlat_1
    SOURCES
    src/ethdriver.c
    INCLUDES
    include
    LIBS
    "${ETHDRIVER_LIBS}"
)
=======
    "${libs}"
)

DeclareCAmkESComponent(EthdriverARMPlat_1 SOURCES ${sources} INCLUDES include LIBS "${libs}")
>>>>>>> 7efca8f (picotcp_single_component: Add picotcp example)

DeclareCAmkESRootserver(
    Ethdriver.camkes
    CPP_FLAGS
    ${cpp_define}
    -DPICOSERVER_IP_ADDR=\"${PICOSERVER_IP_ADDR}\"
)
