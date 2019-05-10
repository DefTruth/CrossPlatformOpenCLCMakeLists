# MIT License
# Copyright (c) 2019 Janos Buttgereit
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.


cmake_minimum_required (VERSION 3.12)

# Using C++ 17 - You might want to change that to your needs
set (CMAKE_CXX_STANDARD 17)

# Project specific settings
project (CrossPlatformOpenCL)
set (CLKernelName "vectorAdd")

# Specify the executable ant its source files
add_executable (HostApplication ${PROJECT_SOURCE_DIR}/src/vectorAdd.cpp)
include_directories (${PROJECT_SOURCE_DIR}/src/openclhelpers)
#target_sources (HostApplication
#        PRIVATE
#        someSource.cpp
#        )

# For FPGA SoC Builds, specify the board type and the config
set (BoardType           "de10_standard")
set (BoardHardwareConfig "de10_standard_sharedonly")

# --------- You shouldn't change anything below this line unless you know what you are doing ----

# Messages to clarify which profile is used
if (OPEN_CL_INTEL_FPGA STREQUAL "CROSS_COMPILING")
    message ("Using Intel FPGA OpenCL cross compiling profile")
elseif (OPEN_CL_INTEL_FPGA STREQUAL "SIMULATION")
    message ("USING Intel FPGA OpenCL simulation profile")
else()
    message ("Using default (native) profile")
endif()

# Expecting the kernel source file to be located at the root of the source directory
set (CLKernelSourceFile "${PROJECT_SOURCE_DIR}/src/${CLKernelName}.cl")

# This part is only relevant if compiling for Intel FPGA
if (OPEN_CL_INTEL_FPGA)

    # This environment variable must be present to find all other tools
    if ("$ENV{INTELFPGAOCLSDKROOT}" STREQUAL "")
        message (FATAL_ERROR "INTELFPGAOCLSDKROOT environment variable not found. Set INTELFPGAOCLSDKROOT to the root directory of the Intel(R) FPGA SDK for OpenCL(TM) software installation")
    endif()

    # A macro that can be used to enable special options for FPGA builds in the host AND kernel code
    # (The macro for the kernel compiler is defined later in the aoc call)
    add_compile_definitions (OPEN_CL_INTEL_FPGA)

    #The Intel FPGA OpenCL SDK supports OpenCL 1.1 with some 2.0 feature additions
    add_compile_definitions (CL_HPP_MINIMUM_OPENCL_VERSION=110 CL_HPP_TARGET_OPENCL_VERSION=200)

    # According to the reverse engineering, those libraries are needed for FPGA builds
    target_link_libraries (HostApplication rt pthread)

    # The compiled kernel binary should be placed next to the host binary
    # (you might want to change this according to your needs)
    set (CLKernelBinary "${PROJECT_BINARY_DIR}/${CLKernelName}.aocx")

    # Find paths relative to the sdk root
    set (IntelFPGAOCLSDKRoot "$ENV{INTELFPGAOCLSDKROOT}")
    set (BoardPackageRoot    "${IntelFPGAOCLSDKRoot}/board/${BoardType}")
    set (aoclExecutable      "${IntelFPGAOCLSDKRoot}/bin/aocl")
    set (aocExecutable       "${IntelFPGAOCLSDKRoot}/bin/aoc")

    # Include the 2.0 OpenCL headers
    include_directories ("${IntelFPGAOCLSDKRoot}/host/include20")

    # Pahts to directories the linker should use
    if (OPEN_CL_INTEL_FPGA STREQUAL "CROSS_COMPILING")
        set (BoardLibLinkerPath "${BoardPackageRoot}/arm32/lib")
        set (HostLibLinkerPath  "${IntelFPGAOCLSDKRoot}/host/arm32/lib")
    else()
        set (BoardLibLinkerPath "${IntelFPGAOCLSDKRoot}/board/s5_ref/linux64/lib")
        set (HostLibLinkerPath  "${IntelFPGAOCLSDKRoot}/host/linux64/lib")
    endif()

    link_directories (${BoardLibLinkerPath} ${HostLibLinkerPath})

    # Get the additional compiler/linker flags for the host application as returned by the aocl tool
    if (OPEN_CL_INTEL_FPGA STREQUAL "CROSS_COMPILING")
        execute_process (COMMAND bash -c "export AOCL_BOARD_PACKAGE_ROOT=${BoardPackageRoot}; ${aoclExecutable} link-config --arm"    OUTPUT_VARIABLE AoclLinkConfig)
    else()
        execute_process (COMMAND bash -c "${aoclExecutable} link-config"    OUTPUT_VARIABLE AoclLinkConfig)
    endif()
    # Needed to remove linebreaks at the end of the string fetched by execute_process above
    string (STRIP "${AoclLinkConfig}"    AoclLinkConfig)

    # Append the strings generated to the existing flags
    set (CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} ${AoclLinkConfig}")

    # The custom command to call the open cl offline compiler with all relevant flags
    if (OPEN_CL_INTEL_FPGA STREQUAL "CROSS_COMPILING")
        add_custom_command (OUTPUT  ${CLKernelBinary}
                            COMMAND ${aocExecutable} ${CLKernelSourceFile} -o ${CLKernelBinary} -report -board-package=${BoardPackageRoot} -board=${BoardHardwareConfig} -DOPEN_CL_INTEL_FPGA
                            DEPENDS ${CLKernelSourceFile}
        )
    else() # compile for emulation
        add_custom_command (OUTPUT  ${CLKernelBinary}
                            COMMAND ${aocExecutable} ${CLKernelSourceFile} -march=emulator -o ${CLKernelBinary} -DOPEN_CL_INTEL_FPGA
                            DEPENDS ${CLKernelSourceFile}
        )
    endif()
    # Make the (re-) compilation of the kernel dependent on kernel file or output file changes
    add_custom_target (CompileCLSources DEPENDS ${CLKernelBinary} ${CLKernelSourceFile})
    add_dependencies (HostApplication CompileCLSources)
else() # Building for standard CPU/GPU Platforms

    # In case the kernel is not compiled ahead of time the source file should be copied next to the host binary
    # (You might want to change this according to your needs)
    configure_file (${CLKernelSourceFile} ${CMAKE_BINARY_DIR}/${CLKernelName}.cl COPYONLY)

    # -------- OS Specific settings ----------
    if (APPLE)
        # On Apple Systems the CL runtime is bundled in the OpenCL framework
        target_link_libraries (HostApplication "-framework OpenCL")
        # Apple only supports OpenCL 1.2
        add_compile_definitions (CL_HPP_MINIMUM_OPENCL_VERSION=120 CL_HPP_TARGET_OPENCL_VERSION=120)
        # Open CL is deprecated since Mac OS Mojave, this silences compiler warnings regarding this
        add_compile_definitions (CL_SILENCE_DEPRECATION)
    else()
        add_compile_definitions (CL_HPP_MINIMUM_OPENCL_VERSION=120 CL_HPP_TARGET_OPENCL_VERSION=200)
    endif()

    if (WIN32)
        if (NOT ("$ENV{INTELOCLSDKROOT}" STREQUAL ""))
            # Use the Intel SDK
            set (WINDOWS_OPEN_CL_LIB_LOCATION "$ENV{INTELOCLSDKROOT}/lib/x64")
            include_directories ("$ENV{INTELOCLSDKROOT}/include")
            message ("Using Intel OpenCL SDK")
        elseif (NOT ("$ENV{CUDA_LIB_PATH}" STREQUAL ""))
            # Use the NVIDIA SDK
            set (WINDOWS_OPEN_CL_LIB_LOCATION $ENV{CUDA_LIB_PATH})
            include_directories ($ENV{CUDA_INC_PATH})
            message ("Using NVIDIA OpenCL SDK")
        elseif (NOT("$ENV{AMDAPPSDKROOT}" STREQUAL ""))
            # Use the AMD SDK
            set (WINDOWS_OPEN_CL_LIB_LOCATION "$ENV{AMDAPPSDKROOT}/lib/x86_64")
            include_directories ("$ENV{AMDAPPSDKROOT}/include")
            message ("Using AMD OpenCL SDK")
        elseif (NOT("$ENV{OCL_ROOT}" STREQUAL ""))
            # Use the AMD Light SDK
            set (WINDOWS_OPEN_CL_LIB_LOCATION "$ENV{OCL_ROOT}/lib/x86_64")
            include_directories ("$ENV{OCL_ROOT}/include")
            message ("Using AMD OpenCL SDK Light")
        else()
            message (FATAL_ERROR "Neither Intel nor NVIDIA nor AMD OpenCL SDK for Windows was found. Install one of those")
        endif()

        target_link_libraries (HostApplication "${WINDOWS_OPEN_CL_LIB_LOCATION}/opencl.lib")

    elseif (NOT APPLE)
        target_link_libraries (HostApplication OpenCL)
    endif()

endif()

