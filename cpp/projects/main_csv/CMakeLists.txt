cmake_minimum_required(VERSION 3.20)
project(Main_CSV VERSION 1.0.0 LANGUAGES CXX C)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # for clangd syntax highlighting

# PROFILE mode: disable aggressive optimizations for better profiling visibility
option(PROFILE_MODE "Build with profiling-friendly flags (no LTO, less inlining)" OFF)

# Compiler-specific options
if(PROFILE_MODE)
    # Tracy profiling: Moderate optimization, retain visibility
    message(STATUS "PROFILE_MODE: Tracy profiling enabled with -O2")
    set(CMAKE_CXX_FLAGS "-O2 -g -Wall -Wextra -march=native -fno-omit-frame-pointer")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DTRACY_ENABLE -DTRACY_TIMER_FALLBACK")
    # No LTO - keep function boundaries visible in Tracy
    # No -fno-inline needed - Tracy instruments at function level
else()
    # Production: Maximum performance
    message(STATUS "Production build: -O3 with LTO")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        set(CMAKE_CXX_FLAGS "-O3 -Wall -Wextra -march=native -flto -DNDEBUG -ffast-math")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld")
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(CMAKE_CXX_FLAGS "-O3 -Wall -Wextra -march=native -flto -DNDEBUG -ffast-math")
    elseif(MSVC)
        set(CMAKE_CXX_FLAGS "/O2 /W4 /EHsc /DNDEBUG /GL /Gy /Gw /arch:AVX2 /fp:fast")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /LTCG /OPT:REF /OPT:ICF")
    endif()
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Include directories
include_directories(../../include)
include_directories(../../package/zstd-1.5.7)

# Zstandard library path
set(ZSTD_LIB_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../package/zstd-1.5.7/libzstd.a)

# Source files
set(SOURCES
    ../../src/codec/json_config.cpp
    # ../../src/codec/binary_decoder_L1.cpp
    ../../src/codec/binary_decoder_L2.cpp
    ../../src/codec/binary_encoder_L2.cpp
    ../../src/misc/logging.cpp
    ../../src/main_csv.cpp
    ../../include/package/tracy/TracyClient.cpp  # Tracy implementation
)

# code specific settings ==========================================================================

# =================================================================================================

set(HEADERS
    ../../include/misc/misc.hpp
    ../../include/misc/print.hpp
    ../../include/misc/affinity.hpp
    ../../include/misc/logging.hpp
    ../../include/define/CBuffer.hpp
    ../../include/codec/json_config.hpp
    # ../../include/codec/binary_decoder_L1.hpp
    ../../include/codec/binary_decoder_L2.hpp
    ../../include/codec/binary_encoder_L2.hpp
    ../../include/codec/delta_encoding.hpp
    ../../include/codec/L2_DataType.hpp
    # ../../include/AnalysisTechnical.hpp
    ../../include/AnalysisHighFrequency.hpp
)

# Disable fast-math specifically for JSON library to avoid infinity issues
set_source_files_properties(../../src/codec/json_config.cpp PROPERTIES
    COMPILE_OPTIONS "-fno-fast-math"
)

# Create executable
add_executable(app_main_csv ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(app_main_csv PRIVATE
    ../../include
    ../../package/zstd-1.5.7
)

# Link Zstandard library
target_link_libraries(app_main_csv PRIVATE ${ZSTD_LIB_PATH})

# Link system libraries required by Tracy
target_link_libraries(app_main_csv PRIVATE dl pthread)

# Installation
install(TARGETS app_main_csv RUNTIME DESTINATION bin)

# Custom target for running
add_custom_target(run_main_csv
    COMMAND app_main_csv
    DEPENDS app_main_csv
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running app_main_csv"
)

# Print configuration info
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Output directory: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "app_main_csv sources: ${SOURCES}")
