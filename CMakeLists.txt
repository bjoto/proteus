cmake_minimum_required(VERSION 3.24)
project(proteus LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(PROTEUS_GPU_TESTS "Enable GPU-context CTest cases and the GPU review target" OFF)

include(FetchContent)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG 6.0
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(raylib)

add_executable(proteus
  src/audio_spectrum.c
  src/main.c
  src/profiler.c
  src/renderer_gpu.c
  src/renderer3d.c
  src/scenes.c
  src/waterfront.c
  src/waterfront_corner.c
  src/waterfront_scene.c
  src/xm_audio.c
)
target_link_libraries(proteus PRIVATE raylib)
target_compile_definitions(proteus PRIVATE
  PROTEUS_BUILD_TYPE="$<CONFIG>"
  PROTEUS_ASSET_DIR="$<TARGET_FILE_DIR:proteus>/assets"
)
target_compile_options(proteus PRIVATE
  $<$<C_COMPILER_ID:AppleClang,Clang,GNU>:-Wall;-Wextra;-Wpedantic>
)

add_custom_target(proteus_assets
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_CURRENT_SOURCE_DIR}/assets
          $<TARGET_FILE_DIR:proteus>/assets
)
add_dependencies(proteus proteus_assets)

add_executable(gpu_fixture tools/gpu_fixture.c src/renderer_gpu.c src/waterfront_scene.c
  src/waterfront_corner.c)
target_include_directories(gpu_fixture PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${raylib_SOURCE_DIR}/src
)
target_link_libraries(gpu_fixture PRIVATE raylib)
target_compile_definitions(gpu_fixture PRIVATE
  PROTEUS_ASSET_DIR="$<TARGET_FILE_DIR:gpu_fixture>/assets"
)
target_compile_options(gpu_fixture PRIVATE
  $<$<C_COMPILER_ID:AppleClang,Clang,GNU>:-Wall;-Wextra;-Wpedantic>
)
add_dependencies(gpu_fixture proteus_assets)

add_executable(validate_xm
  tools/validate_xm.c
  src/xm_audio.c
)
target_include_directories(validate_xm PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${raylib_SOURCE_DIR}/src
  ${raylib_SOURCE_DIR}/src/external
)
if(NOT MSVC)
  target_link_libraries(validate_xm PRIVATE m)
endif()

add_executable(validate_audio_spectrum
  tools/validate_audio_spectrum.c
  src/audio_spectrum.c
)
target_include_directories(validate_audio_spectrum PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(validate_audio_spectrum PRIVATE raylib)
target_compile_options(validate_audio_spectrum PRIVATE
  $<$<C_COMPILER_ID:AppleClang,Clang,GNU>:-Wall;-Wextra;-Wpedantic>
)

enable_testing()
add_executable(validate_city_dressing tools/validate_city_dressing.c
  src/waterfront_corner.c src/waterfront_scene.c)
target_include_directories(validate_city_dressing PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(validate_city_dressing PRIVATE raylib)
target_compile_options(validate_city_dressing PRIVATE
  $<$<C_COMPILER_ID:AppleClang,Clang,GNU>:-Wall;-Wextra;-Wpedantic>
)
add_test(NAME city_dressing COMMAND validate_city_dressing)

add_executable(waterfront_storyboard
  tools/waterfront_storyboard.c src/waterfront.c src/waterfront_scene.c src/renderer3d.c
  src/renderer_gpu.c src/waterfront_corner.c
)
target_compile_definitions(waterfront_storyboard PRIVATE
  PROTEUS_ASSET_DIR="$<TARGET_FILE_DIR:waterfront_storyboard>/assets"
)
add_dependencies(waterfront_storyboard proteus_assets)
target_include_directories(waterfront_storyboard PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(waterfront_storyboard PRIVATE raylib)
target_compile_options(waterfront_storyboard PRIVATE
  $<$<C_COMPILER_ID:AppleClang,Clang,GNU>:-Wall;-Wextra;-Wpedantic>
)
add_test(NAME waterfront_storyboard_is_valid COMMAND waterfront_storyboard --check)
add_executable(validate_renderer3d tools/validate_renderer3d.c src/renderer3d.c)
target_include_directories(validate_renderer3d PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(validate_renderer3d PRIVATE raylib)
target_compile_options(validate_renderer3d PRIVATE
  $<$<C_COMPILER_ID:AppleClang,Clang,GNU>:-Wall;-Wextra;-Wpedantic>
)
add_test(NAME renderer3d_is_valid COMMAND validate_renderer3d)
add_test(NAME starter_xm_is_valid
  COMMAND validate_xm ${CMAKE_CURRENT_SOURCE_DIR}/assets/music/starter.xm
)
add_test(NAME audio_spectrum_is_valid
  COMMAND validate_audio_spectrum
)
add_test(NAME fire_profile_report
  COMMAND proteus --renderer cpu --profile fire --frames 10 --warmup 2
)
set_tests_properties(fire_profile_report PROPERTIES
  PASS_REGULAR_EXPRESSION "proteus_profile_end=1"
)
add_test(NAME cube_screenshot
  COMMAND proteus --renderer cpu --screenshot ${CMAKE_CURRENT_BINARY_DIR}/cube-screenshot.png
          --scene cube --time 1.5
)
set_tests_properties(cube_screenshot PROPERTIES
  PASS_REGULAR_EXPRESSION "timeline_time=47.580"
)

add_test(NAME interactive_timeline_options
  COMMAND proteus --renderer cpu --scene cube --time 2.5 --help
)
set_tests_properties(interactive_timeline_options PROPERTIES
  PASS_REGULAR_EXPRESSION "Usage:"
)

set(starscroll_checksum e7e36b86f9776e7d)
set(fire_checksum fd61ac9ce25e71a6)
set(plasma_checksum cac9c63d222163d3)
# Reviewed CPU reference with mapped building parts, planting, and expanded city context.
set(cube_checksum 5ebe525b6d2f6f83)
foreach(scene starscroll fire plasma cube)
  add_test(NAME ${scene}_render_checksum
    COMMAND proteus --renderer cpu --profile ${scene} --frames 1 --warmup 0
  )
  set_tests_properties(${scene}_render_checksum PROPERTIES
    PASS_REGULAR_EXPRESSION "final_framebuffer_checksum=${${scene}_checksum}"
  )
endforeach()

add_test(NAME cube_start_screenshot
  COMMAND proteus --renderer cpu --screenshot ${CMAKE_CURRENT_BINARY_DIR}/cube-start.png
          --scene cube --time 0
)
set_tests_properties(cube_start_screenshot PROPERTIES
  PASS_REGULAR_EXPRESSION "scene=cube"
)

if(PROTEUS_GPU_TESTS)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  set(PROTEUS_GPU_REVIEW_DIR "${CMAKE_CURRENT_BINARY_DIR}/gpu-review")
  set(PROTEUS_GPU_REVIEW_COMMAND
    ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/gpu_review.py
    --proteus $<TARGET_FILE:proteus>
    --gpu-fixture $<TARGET_FILE:gpu_fixture>
    --output ${PROTEUS_GPU_REVIEW_DIR}
    --build-directory ${CMAKE_CURRENT_BINARY_DIR}
  )
  add_custom_target(gpu_review
    COMMAND ${PROTEUS_GPU_REVIEW_COMMAND}
    DEPENDS proteus gpu_fixture
    USES_TERMINAL
    COMMENT "Capturing CPU/GPU waterfront review artifacts"
  )
  add_test(NAME gpu_review COMMAND ${PROTEUS_GPU_REVIEW_COMMAND})
  set_tests_properties(gpu_review PROPERTIES LABELS "gpu" RUN_SERIAL TRUE)
  add_test(NAME gpu_ground_materials COMMAND gpu_fixture --city --check)
  set_tests_properties(gpu_ground_materials PROPERTIES LABELS "gpu" RUN_SERIAL TRUE)
endif()
