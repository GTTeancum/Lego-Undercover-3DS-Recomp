# Screen layout and scanout decoding remain usable by ROM-free headless tests.
add_library(lego_frontend_core STATIC
    src/frontend/display_frame.cpp
    src/frontend/display_snapshot.cpp
    src/frontend/screen_layout.cpp)
target_link_libraries(lego_frontend_core PUBLIC lego_ctr)

option(LEGO_BUILD_DESKTOP "Build the optional SDL3 desktop presentation and input layer" OFF)
option(LEGO_FETCH_SDL3 "Fetch the pinned SDL3 release when desktop support needs it" ON)

if(LEGO_BUILD_DESKTOP)
    find_package(SDL3 3.2 CONFIG QUIET)
    if(NOT TARGET SDL3::SDL3)
        if(NOT LEGO_FETCH_SDL3)
            message(FATAL_ERROR "SDL3 >= 3.2 is required; set SDL3_DIR or allow LEGO_FETCH_SDL3")
        endif()
        include(FetchContent)
        if(POLICY CMP0135)
            cmake_policy(SET CMP0135 NEW)
        endif()
        set(SDL_SHARED OFF CACHE BOOL "Build shared SDL3")
        set(SDL_STATIC ON CACHE BOOL "Build static SDL3")
        set(SDL_TESTS OFF CACHE BOOL "Build SDL3 tests")
        set(SDL_TEST_LIBRARY OFF CACHE BOOL "Build the SDL3 test support library")
        set(SDL_EXAMPLES OFF CACHE BOOL "Build SDL3 examples")
        FetchContent_Declare(SDL3
            URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.18/SDL3-3.4.18.tar.gz
            URL_HASH SHA256=9c75cf16330322c217dedd2e0609f1124f1b54b8633e763467b4684d0f4334a3)
        FetchContent_MakeAvailable(SDL3)
    endif()
    add_library(lego_desktop STATIC src/frontend/desktop_window.cpp)
    target_link_libraries(lego_desktop PUBLIC lego_frontend_core SDL3::SDL3)
    if(BUILD_TESTING)
        add_executable(desktop_frontend_smoke_test tests/desktop_frontend_smoke_test.cpp)
        target_link_libraries(desktop_frontend_smoke_test PRIVATE lego_desktop)
        add_test(NAME desktop_frontend_smoke_test COMMAND desktop_frontend_smoke_test)
        set_tests_properties(desktop_frontend_smoke_test PROPERTIES
            ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software"
            TIMEOUT 30)
    endif()
endif()
