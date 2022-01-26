cmake_minimum_required (VERSION 3.0)
project(xMGARD)
set (CMAKE_CXX_STANDARD 11)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_CXX_FLAGS "-g -O3")

find_library(SZ_CPP_LIB sz_cpp HINTS "$ENV{HOME}/code/meta_compressor/build/src")
set (SZ_CPP_INC "$ENV{HOME}/code/meta_compressor/include")

find_package(PkgConfig)
message(${PROJECT_NAME})
add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(
        ${PROJECT_NAME} INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
option(MGARDX_USE_BUNDLED_ZSTD "prefer the bundled version of Zstd" OFF)
pkg_search_module(ZSTD IMPORTED_TARGET libzstd)
if(ZSTD_FOUND AND NOT MGARDX_USE_BUNDLED_ZSTD)
    target_link_libraries(${PROJECT_NAME} INTERFACE PkgConfig::ZSTD)
else()
    add_subdirectory(zstd)
    target_link_libraries(${PROJECT_NAME} INTERFACE zstd)
endif()

add_subdirectory (test)
# add_subdirectory (parallel_src)
