cmake_minimum_required (VERSION 3.0)
project(xMGARD)
set (CMAKE_CXX_STANDARD 11)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_CXX_FLAGS "-g -O3")

#find_library(ZSTD_LIB zstd HINTS "/Users/xin/github/SZ_master/build/zstd")
#find_library(SZ3_LIB SZ3 HINTS "/Users/xin/github/SZ_refactory/build/src")
find_library(SZ_CPP_LIB sz_cpp HINTS "/Users/kzhao/code/metacompressor/build/lib")
#set (ZSTD_INC "/Users/xin/github/SZ_master/zstd")
#set (SZ3_INC "/Users/xin/github/SZ_refactory/include")
set (SZ_CPP_INC "/Users/kzhao/code/metacompressor/build/include")
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
