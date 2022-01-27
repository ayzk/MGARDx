file(GLOB cpu_source_files "${CMAKE_SOURCE_DIR}/test/test_*compress.cpp")

foreach (SRCFILE IN LISTS cpu_source_files)
    get_filename_component(EXE ${SRCFILE} NAME_WLE)
    message(STATUS "Target = ${EXE}")
    add_executable(${EXE} ${EXE}.cpp)
    target_link_libraries(${EXE} PRIVATE MGARDx)

    install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/${EXE} TYPE BIN)
endforeach ()
