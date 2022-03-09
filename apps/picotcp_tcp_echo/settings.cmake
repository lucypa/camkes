#
# Copyright 2019, Data61
# Commonwealth Scientific and Industrial Research Organisation (CSIRO)
# ABN 41 687 119 230.
#
# This software may be distributed and modified according to the terms of
# the BSD 2-Clause license. Note that NO WARRANTY is provided.
# See "LICENSE_BSD2.txt" for details.
#
# @TAG(DATA61_BSD)
#

set(LibPicotcp ON CACHE BOOL "" FORCE)
set(LibPicotcpBsd OFF CACHE BOOL "" FORCE)
set(LibEthdriverNumPreallocatedBuffers 32 CACHE STRING "" FORCE)

# For x86, we map DMA frames into the IOMMU to use as buffers for the
# Ethernet device. The VKA and VSpace libraries do not like pages that
# are not 4K in size.
set(CAmkESDMALargeFramePromotion OFF CACHE BOOL "" FORCE)

# The app has only been tested on hardware, and not on QEMU
set(SIMULATION OFF CACHE BOOL "" FORCE)
if("${KernelArch}" STREQUAL "x86")
    # The IOMMU is required for the Ethdriver component on x86
    set(KernelIOMMU ON CACHE BOOL "" FORCE)
endif()

set(LibEthdriverRXDescCount 256 CACHE STRING "" FORCE)
set(LibEthdriverTXDescCount 512 CACHE STRING "" FORCE)
<<<<<<< HEAD

# Print at runtime
set(KernelPrinting ON CACHE BOOL "" FORCE)
set(KernelDebugBuild ON CACHE BOOL "" FORCE)
=======
set(CAmkESNoFPUByDefault ON CACHE BOOL "" FORCE)
<<<<<<< HEAD

# Print at runtime
# set(KernelPrinting ON CACHE BOOL "" FORCE)
# set(KernelDebugBuild ON CACHE BOOL "" FORCE)
>>>>>>> f713a07 (Move picotcp CMake settings to settings.cmake)
=======
>>>>>>> c25c1db (picotcp_tcp_echo: Use async socket interfaces)
