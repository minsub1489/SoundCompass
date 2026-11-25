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
    src/Scene.cpp
    src/Config.cpp
    src/UdpReceiver.cpp
    src/OverlayWindow.cpp
    src/Painter.cpp
)
target_include_directories(halo_core PUBLIC src third_party ${UE_SHARED})
target_link_libraries(halo_core PUBLIC d3d11 dxgi dcomp d2d1 dwrite windowscodecs ws2_32)

add_executable(HaloOverlay WIN32 src/main.cpp)
target_link_libraries(HaloOverlay PRIVATE halo_core)
add_custom_command(TARGET HaloOverlay POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/halo_overlay.json $<TARGET_FILE_DIR:HaloOverlay>
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/icons $<TARGET_FILE_DIR:HaloOverlay>/icons)

enable_testing()
function(halo_test name)
    add_executable(${name} tests/${name}.cpp)
    target_link_libraries(${name} PRIVATE halo_core ${ARGN})
    target_include_directories(${name} PRIVATE tests)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

halo_test(test_protocol)
halo_test(test_geometry)
halo_test(test_scene)
halo_test(test_config)
halo_test(test_udp)
halo_test(test_painter windowscodecs)
target_compile_definitions(test_painter PRIVATE HALO_ICONS_DIR=L"${CMAKE_CURRENT_SOURCE_DIR}/icons")
