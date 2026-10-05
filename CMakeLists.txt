cmake_minimum_required(VERSION 3.20)
project(LEGOChaseRecovery LANGUAGES CXX)
include(CTest)
include(cmake/LEGOHostRuntime.cmake)
set(LEGO_AOT_DIR "" CACHE PATH "Private directory containing the 599 restored AOT pages")
set(A32_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/triaevum-a9b4477")
add_library(lego_a32 STATIC
    ${A32_DIR}/recomp/a32_runtime.cpp
    ${A32_DIR}/recomp/a32_core.cpp
    ${A32_DIR}/recomp/a32_core_alu.cpp
    ${A32_DIR}/recomp/a32_core_memory.cpp
    ${A32_DIR}/recomp/a32_vfp_binary64.cpp
    ${A32_DIR}/recomp/a32_vfp_scalar.cpp
    ${A32_DIR}/recomp/a32_vfp_transport.cpp)
target_compile_features(lego_a32 PUBLIC cxx_std_20)
target_include_directories(lego_a32 PUBLIC "${A32_DIR}")
lego_add_ctr_runtime(lego_ctr)
target_link_libraries(lego_ctr PUBLIC lego_a32)
if(BUILD_TESTING)
    foreach(test_name ctr_kernel_test ctr_runner_test ctr_ipc_test ctr_startup_test ctr_apt_test ctr_apt_enable_test ctr_apt_notify_test ctr_apt_utility_test ctr_ndm_test ctr_clock_test ctr_fs_user_test)
        add_executable(${test_name} tests/${test_name}.cpp)
        target_link_libraries(${test_name} PRIVATE lego_ctr)
        add_test(NAME ${test_name} COMMAND ${test_name})
    endforeach()
endif()
if(LEGO_AOT_DIR)
    get_filename_component(LEGO_AOT_DIR "${LEGO_AOT_DIR}" ABSOLUTE)
    file(GLOB aot_pages CONFIGURE_DEPENDS "${LEGO_AOT_DIR}/lego_page_*.cpp")
    list(SORT aot_pages)
    list(LENGTH aot_pages page_count)
    if(NOT page_count EQUAL 599)
        message(FATAL_ERROR "Expected 599 private AOT pages; found ${page_count}")
    endif()
    # Saved archive: 603 regular files, not the earlier wrapper's 604.
    # Require the actual C++ ABI files; do not manufacture missing manifests.
    foreach(support lego_aot_generated.h lego_aot_registry.cpp lego_aot_functions.cpp)
        if(NOT EXISTS "${LEGO_AOT_DIR}/${support}")
            message(FATAL_ERROR "Missing private AOT support file: ${support}")
        endif()
    endforeach()
    add_library(lego_game_aot STATIC ${aot_pages}
        "${LEGO_AOT_DIR}/lego_aot_registry.cpp"
        "${LEGO_AOT_DIR}/lego_aot_functions.cpp")
    target_link_libraries(lego_game_aot PUBLIC lego_a32)
    target_include_directories(lego_game_aot PUBLIC "${LEGO_AOT_DIR}")
    add_executable(LEGOChaseNative src/host/main.cpp)
    target_link_libraries(LEGOChaseNative PRIVATE lego_ctr lego_game_aot)
endif()
