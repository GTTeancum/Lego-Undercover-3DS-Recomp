# Reconstructed build integration for the Recovery-J-style generated AOT pages.
# The historical filename scheme is unknown; the recovery emitter intentionally
# uses lego_page_<guest-page>.cpp and exactly 599 text-page translation units.

function(lego_target_generated_aot target generated_dir)
    if(NOT TARGET "${target}")
        message(FATAL_ERROR "lego_target_generated_aot: target '${target}' does not exist")
    endif()

    file(GLOB lego_aot_pages CONFIGURE_DEPENDS
        "${generated_dir}/lego_page_*.cpp"
    )
    list(SORT lego_aot_pages)
    list(LENGTH lego_aot_pages lego_aot_page_count)
    if(NOT lego_aot_page_count EQUAL 599)
        message(FATAL_ERROR
            "Recovered LEGO AOT layout requires exactly 599 page translation units; "
            "found ${lego_aot_page_count} in ${generated_dir}"
        )
    endif()

    foreach(required_file
        lego_aot_registry.cpp
        lego_aot_functions.cpp
        lego_aot_generated.h
        lego_aot_pages.json
        manifest.json
    )
        if(NOT EXISTS "${generated_dir}/${required_file}")
            message(FATAL_ERROR
                "Recovered LEGO AOT support artifact missing: ${generated_dir}/${required_file}"
            )
        endif()
    endforeach()

    target_sources("${target}" PRIVATE
        "${generated_dir}/lego_aot_registry.cpp"
        "${generated_dir}/lego_aot_functions.cpp"
        ${lego_aot_pages}
    )
    target_include_directories("${target}" PRIVATE "${generated_dir}")

    set_property(TARGET "${target}" PROPERTY LEGO_AOT_PAGE_COUNT "${lego_aot_page_count}")
endfunction()
