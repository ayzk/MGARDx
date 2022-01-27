cmake_minimum_required (VERSION 3.0)
project(MGARDx VERSION 0.0.1)

include(CTest)
include(GNUInstallDirs)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_SHARED_LIBS "prefer shared libraies" ON)

find_package(SZCpp REQUIRED)
find_package(PkgConfig REQUIRED)
add_library(${PROJECT_NAME} INTERFACE)
target_compile_features( ${PROJECT_NAME} INTERFACE cxx_std_11)
target_include_directories(
        ${PROJECT_NAME} INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(${PROJECT_NAME} INTERFACE SZCpp::sz_cpp)
option(MGARDX_USE_BUNDLED_ZSTD "prefer the bundled version of Zstd" OFF)
pkg_search_module(ZSTD IMPORTED_TARGET libzstd)
if(ZSTD_FOUND AND NOT MGARDX_USE_BUNDLED_ZSTD)
    target_link_libraries(${PROJECT_NAME} INTERFACE PkgConfig::ZSTD)
else()
    add_subdirectory(zstd)
    target_link_libraries(${PROJECT_NAME} INTERFACE zstd)
endif()

install (TARGETS MGARDx EXPORT MGARDxTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/MGARDx)
install(EXPORT MGARDxTargets NAMESPACE MGARDx:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MGARDx/)
include(CMakePackageConfigHelpers)
configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/MGARDxConfig.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/MGARDxConfig.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MGARDx
)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/MGARDxConfigVersion.cmake"
  VERSION "${PROJECT_VERSION}"
  COMPATIBILITY AnyNewerVersion
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/MGARDxConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/MGARDxConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MGARDx
)


if(BUILD_TESTING)
  add_subdirectory (test)
endif()
# add_subdirectory (parallel_src)
