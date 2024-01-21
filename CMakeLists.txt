#===============================================================================
#
# Copyright (C) 2023 Felix Jones
# For conditions of distribution and use, see copyright notice in LICENSE
#
#===============================================================================

cmake_minimum_required(VERSION 3.18)

project(xilefianlib LANGUAGES ASM CXX VERSION 1.0.0)

# cxx
list(APPEND includes cxx/include)

# gba arm
if(CMAKE_SYSTEM_NAME STREQUAL AdvancedGameBoy)
    list(APPEND sources
        arm/source/m4column.s
    )
    list(APPEND includes arm/include)

    set_source_files_properties(arm/source/m4column.s PROPERTIES COMPILE_FLAGS "-marm")
endif()

# Library
if(sources)
    add_library(xilefianlib STATIC ${sources})
    target_include_directories(xilefianlib PUBLIC ${includes})
    target_compile_features(xilefianlib PRIVATE cxx_std_20)

    target_compile_options(xilefianlib PRIVATE
        $<$<COMPILE_LANGUAGE:C>:
            -fomit-frame-pointer
            -ffunction-sections
            -fdata-sections
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wstrict-prototypes
        >
    )
else()
    add_library(xilefianlib INTERFACE)
    target_include_directories(xilefianlib INTERFACE ${includes})
endif()
