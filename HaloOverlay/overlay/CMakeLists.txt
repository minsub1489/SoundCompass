cmake_minimum_required(VERSION 3.20)
project(HaloOverlay LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(/W4 /utf-8 /permissive-)
add_compile_definitions(UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)

# Engine-independent headers shared with the in-game halo (SoundPresentation.h, SoundImportancePolicy.h).
set(UE_SHARED ${CMAKE_CURRENT_SOURCE_DIR}/../../Source/SoundCompass/SoundVisualization)

add_library(halo_core STATIC
    src/Protocol.cpp
)
target_include_directories(halo_core PUBLIC src third_party ${UE_SHARED})

add_executable(HaloOverlay WIN32 src/main.cpp)
target_link_libraries(HaloOverlay PRIVATE halo_core)

enable_testing()
function(halo_test name)
    add_executable(${name} tests/${name}.cpp)
    target_link_libraries(${name} PRIVATE halo_core ${ARGN})
    target_include_directories(${name} PRIVATE tests)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

halo_test(test_protocol)
