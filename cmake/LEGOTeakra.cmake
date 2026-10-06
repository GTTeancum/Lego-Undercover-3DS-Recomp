# Pinned MIT DSP interpreter, statically linked. No network access at build time.
# Local guard patches and exact upstream blob identities: vendor/.../UPSTREAM.json.
set(LEGO_TEAKRA_DIR "${CMAKE_CURRENT_LIST_DIR}/../vendor/teakra-3d697a1")
find_package(Threads REQUIRED)
add_library(lego_teakra STATIC
    ${LEGO_TEAKRA_DIR}/src/ahbm.cpp
    ${LEGO_TEAKRA_DIR}/src/apbp.cpp
    ${LEGO_TEAKRA_DIR}/src/btdmp.cpp
    ${LEGO_TEAKRA_DIR}/src/dma.cpp
    ${LEGO_TEAKRA_DIR}/src/timer.cpp
    ${LEGO_TEAKRA_DIR}/src/memory_interface.cpp
    ${LEGO_TEAKRA_DIR}/src/mmio.cpp
    ${LEGO_TEAKRA_DIR}/src/processor.cpp
    ${LEGO_TEAKRA_DIR}/src/teakra.cpp)
target_compile_features(lego_teakra PUBLIC cxx_std_20)
target_include_directories(lego_teakra PUBLIC "${LEGO_TEAKRA_DIR}/include"
    PRIVATE "${LEGO_TEAKRA_DIR}/include/teakra/impl")
target_link_libraries(lego_teakra PRIVATE Threads::Threads)
