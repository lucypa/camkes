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

set(LibPicotcp ON CACHE BOOL "" FORCE)
set(LibPicotcpBsd OFF CACHE BOOL "" FORCE)

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
<<<<<<< HEAD
set(LibEthdriverTXDescCount 512 CACHE STRING "" FORCE)

# Print at runtime
set(KernelPrinting ON CACHE BOOL "" FORCE)
set(KernelDebugBuild ON CACHE BOOL "" FORCE)
=======
set(LibEthdriverTXDescCount 256 CACHE STRING "" FORCE)
set(CAmkESNoFPUByDefault ON CACHE BOOL "" FORCE)
>>>>>>> 7efca8f (picotcp_single_component: Add picotcp example)
