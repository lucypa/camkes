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

cmake_minimum_required(VERSION 3.7.2)

project(lwipserver C)

includeGlobalComponents()

file(GLOB sources ${CMAKE_CURRENT_LIST_DIR}/components/Echo/src/*.c)

DeclareCAmkESComponent(
    Echo
    SOURCES
    ${sources}
    INCLUDES
    include
    LIBS
    lwip
    LWIPServer-client
)

CAmkESAddCPPInclude(${CMAKE_CURRENT_LIST_DIR}/include)

include(${LWIP_HELPERS})
AddLWIPConfiguration(${CMAKE_CURRENT_LIST_DIR}/lwip_include)

if(KernelSel4ArchX86_64)
    DeclareCAmkESComponent(Ethdriver82574DF SOURCES x86_64_eth_init.c LIBS "${ETHDRIVER_LIBS}")
    DeclareCAmkESRootserver(lwipserver_x86_64.camkes CPP_FLAGS ${cpp_define})
elseif(KernelPlatformTx2)
    DeclareCAmkESRootserver(lwipserver_tx2.camkes CPP_FLAGS ${cpp_define})
elseif(KernelPlatformZynq7000)
    DeclareCAmkESRootserver(lwipserver_zynq7000.camkes CPP_FLAGS ${cpp_define})
else()
    message(FATAL_ERROR "Unsupported platform.")
endif()
