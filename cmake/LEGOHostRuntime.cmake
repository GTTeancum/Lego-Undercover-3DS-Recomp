# Reconstructed CTR host/kernel layer for LEGO Chase.

get_filename_component(LEGO_RECOMP_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

function(lego_add_ctr_runtime target_name)
    add_library(${target_name} STATIC
        "${LEGO_RECOMP_ROOT}/src/runtime/ctr_kernel.cpp"
        "${LEGO_RECOMP_ROOT}/src/runtime/ctr_svc_bridge.cpp"
    )
    target_compile_features(${target_name} PUBLIC cxx_std_20)
    target_include_directories(${target_name}
        PUBLIC "${LEGO_RECOMP_ROOT}/src"
        PUBLIC "${LEGO_RECOMP_ROOT}/vendor/triaevum-a9b4477"
    )
endfunction()
