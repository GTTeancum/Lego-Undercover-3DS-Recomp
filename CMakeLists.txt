cmake_minimum_required(VERSION 3.20)
project(LEGOChaseRecovery LANGUAGES CXX)
include(CTest)
include(cmake/LEGOHostRuntime.cmake)
include(cmake/LEGOTeakra.cmake)
set(LEGO_AOT_DIR "" CACHE PATH "Private directory containing the 599 restored AOT pages")
set(LEGO_SUPPLEMENT_CODE_BIN "" CACHE FILEPATH "Optional verified private code.bin for observed build-time AOT additions")
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
target_link_libraries(lego_ctr PUBLIC lego_a32 lego_teakra)
if(BUILD_TESTING)
    foreach(test_name ctr_supplemental_registry_test ctr_cfg_language_test ctr_dsp_audio_file_test ctr_pica_proctex_test ctr_pica_swizzle_test ctr_cfg_sound_test ctr_dsp_address_test ctr_dsp_pipe_read_test ctr_dsp_boot_slice_test ctr_btdmp_test ctr_dsp_audio_capture_test ctr_event_notifier_test ctr_dsp_semaphore_test ctr_dsp_live_device_test ctr_dsp_live_service_test ctr_dsp_icu_test ctr_dsp_execution_test ctr_dsp_special_test ctr_dsp_boot_handshake_test ctr_dsp1_image_test ctr_dsp_inspect_test ctr_launch_header_test ctr_a32_step_test ctr_dual_core_test ctr_dual_runner_test ctr_memory_alias_test ctr_apt_cpu_limit_test ctr_cfg_stereo_test ctr_y2r_initialize_test ctr_lcd_blank_test ctr_display_event_test ctr_display_clock_test ctr_scaled_transfer_test ctr_memory_fill_test ctr_gsp_fill_test ctr_block_entry_test ctr_kernel_test ctr_runner_test ctr_ipc_test ctr_startup_test ctr_apt_test ctr_apt_enable_test ctr_apt_notify_test ctr_apt_utility_test ctr_ndm_test ctr_clock_test ctr_fs_user_test ctr_fs_archive_test ctr_fs_create_test ctr_ptm_test ctr_ptm_steps_test ctr_fs_open_test ctr_fs_write_test ctr_romfs_test ctr_sha256_stream_test ctr_fs_priority_test ctr_gsp_acquire_test ctr_gsp_relay_test ctr_shared_map_test ctr_gsp_register_test ctr_legacy_barrier_test ctr_gsp_queue_test ctr_pica_startup_test ctr_gsp_submit_test ctr_display_transfer_test ctr_gsp_transfer_test ctr_dsp_discovery_test ctr_hid_handles_test ctr_core1_budget_test)
        add_executable(${test_name} tests/${test_name}.cpp)
        target_link_libraries(${test_name} PRIVATE lego_ctr)
        add_test(NAME ${test_name} COMMAND ${test_name})
    endforeach()
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        # Test-only interception verifies flush selection and error propagation.
        target_compile_definitions(ctr_dsp_audio_file_test PRIVATE LEGO_TEST_WRAP_STDIO=1)
        target_link_options(ctr_dsp_audio_file_test PRIVATE "LINKER:--wrap=fwrite" "LINKER:--wrap=fflush" "LINKER:--wrap=fclose")
        target_compile_definitions(ctr_fs_write_test PRIVATE LEGO_TEST_WRAP_FSYNC=1)
        target_link_options(ctr_fs_write_test PRIVATE "LINKER:--wrap=fsync")
    endif()
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
    if(LEGO_SUPPLEMENT_CODE_BIN)
        get_filename_component(LEGO_SUPPLEMENT_CODE_BIN "${LEGO_SUPPLEMENT_CODE_BIN}" ABSOLUTE)
        find_package(Python3 REQUIRED COMPONENTS Interpreter)
        find_program(LEGO_SUPPLEMENT_CLANG NAMES clang REQUIRED)
        find_program(LEGO_SUPPLEMENT_OBJDUMP NAMES llvm-objdump REQUIRED)
        set(supplement "${CMAKE_CURRENT_BINARY_DIR}/private-generated/lego_aot_supplement.cpp")
        add_custom_command(OUTPUT "${supplement}"
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_aot_supplement.py"
                "${LEGO_SUPPLEMENT_CODE_BIN}" "${CMAKE_CURRENT_SOURCE_DIR}/config/observed_aot_supplements.json"
                "${supplement}" --clang "${LEGO_SUPPLEMENT_CLANG}" --llvm-objdump "${LEGO_SUPPLEMENT_OBJDUMP}"
            DEPENDS "${LEGO_SUPPLEMENT_CODE_BIN}" tools/generate_aot_supplement.py
                tools/generate_aot_pages_llvm.py config/observed_aot_supplements.json
            VERBATIM)
        target_sources(LEGOChaseNative PRIVATE "${supplement}")
        target_compile_definitions(LEGOChaseNative PRIVATE LEGO_HAVE_AOT_SUPPLEMENT=1)
    endif()
endif()
