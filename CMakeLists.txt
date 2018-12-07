cmake_minimum_required(VERSION 3.12)
project(sdtEncoder)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

configure_file(
        "${PROJECT_SOURCE_DIR}/config.h.in"
        "${PROJECT_SOURCE_DIR}/config.h"
)

find_package(Boost REQUIRED COMPONENTS system)
if (NOT Boost_FOUND)
    MESSAGE(FATAL_ERROR "NOT FOUND BOOST")
endif ()

include_directories(${Boost_INCLUDE_DIRS})

include_directories(${PROJECT_SOURCE_DIR}/include)

aux_source_directory(${PROJECT_SOURCE_DIR}/src DIR_SRCS)


option(USE_INTEL "option if use intel media sdk" ON)
option(SAVE_RAW "option if save raw file" OFF)
option(SAVE_CODECED "option if save 264/265 file" OFF)

if (${USE_INTEL} STREQUAL "ON")

    include_directories(/opt/intel/mediasdk/include)

    FILE(GLOB INTEL_COMMON version.h common/cmd_options.h common/cmd_options.cpp
            common/common_utils.h common/common_utils.cpp common/common_utils_linux.cpp common/common_vaapi.h common/common_vaapi.cpp)

    link_directories(/opt/intel/mediasdk/lib/lin_x64)

    add_executable(sdtEncoder main.cpp ${INTEL_COMMON} ${DIR_SRCS})
    target_link_libraries(sdtEncoder libmfx.a libva.so libva-drm.so dl jpeg rt ${Boost_LIBRARIES} pthread)
else (${USE_INTEL} STREQUAL "ON")


endif (${USE_INTEL} STREQUAL "ON")

