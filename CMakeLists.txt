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
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${CMAKE_CURRENT_SOURCE_DIR}/soundtrack/alternate/fasttracker/v01/below-the-surface.xm
          $<TARGET_FILE_DIR:proteus>/assets/music/below-the-surface.xm
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
add_executable(boat_study tools/boat_study.c src/boat.c src/renderer3d.c)
add_executable(validate_boat tools/validate_boat.c src/boat.c src/renderer3d.c)
foreach(target boat_study validate_boat)
  target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(${target} PRIVATE raylib)
  target_compile_options(${target} PRIVATE
    $<$<C_COMPILER_ID:AppleClang,Clang,GNU>:-Wall;-Wextra;-Wpedantic>
  )
endforeach()
add_test(NAME boat_handling COMMAND validate_boat)
add_test(NAME boat_screenshot COMMAND boat_study --time 3
  --screenshot ${CMAKE_CURRENT_BINARY_DIR}/boat-screenshot.png)

add_executable(dance_study tools/dance_study.c src/dance.c src/choreography.c src/renderer3d.c src/xm_audio.c)
target_include_directories(dance_study PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(dance_study PRIVATE raylib)
target_compile_definitions(dance_study PRIVATE
  PROTEUS_ASSET_DIR="$<TARGET_FILE_DIR:dance_study>/assets"
)
target_compile_options(dance_study PRIVATE
  $<$<C_COMPILER_ID:AppleClang,Clang,GNU>:-Wall;-Wextra;-Wpedantic>
)
add_dependencies(dance_study proteus_assets)

add_executable(validate_dance tools/validate_dance.c src/dance.c src/choreography.c src/renderer3d.c)
target_include_directories(validate_dance PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(validate_dance PRIVATE raylib)
target_compile_options(validate_dance PRIVATE
  $<$<C_COMPILER_ID:AppleClang,Clang,GNU>:-Wall;-Wextra;-Wpedantic>
)
add_test(NAME dance_constraints COMMAND validate_dance)
add_test(NAME house_dance_constraints COMMAND validate_dance ${CMAKE_CURRENT_SOURCE_DIR}/assets/dance/house.dance)
add_test(NAME house_groove_constraints COMMAND validate_dance ${CMAKE_CURRENT_SOURCE_DIR}/assets/dance/house-groove.dance)
add_executable(validate_choreography tools/validate_choreography.c src/choreography.c
  src/dance.c src/renderer3d.c)
target_include_directories(validate_choreography PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(validate_choreography PRIVATE raylib)
target_compile_options(validate_choreography PRIVATE
  $<$<C_COMPILER_ID:AppleClang,Clang,GNU>:-Wall;-Wextra;-Wpedantic>
)
add_test(NAME choreography_language COMMAND validate_choreography
  ${CMAKE_CURRENT_BINARY_DIR}/choreography-validation.dance)
add_test(NAME dance_screenshot
  COMMAND dance_study --time 8.8 --screenshot ${CMAKE_CURRENT_BINARY_DIR}/dance-screenshot.png
)

add_executable(validate_waterfront_actors tools/validate_waterfront_actors.c src/waterfront_scene.c)
target_include_directories(validate_waterfront_actors PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(validate_waterfront_actors PRIVATE raylib)
target_compile_options(validate_waterfront_actors PRIVATE
  $<$<C_COMPILER_ID:AppleClang,Clang,GNU>:-Wall;-Wextra;-Wpedantic>
)
add_test(NAME waterfront_actors COMMAND validate_waterfront_actors)

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
add_test(NAME soundtrack_xm_is_valid
  COMMAND validate_xm ${CMAKE_CURRENT_SOURCE_DIR}/soundtrack/alternate/fasttracker/v01/below-the-surface.xm --soundtrack
)
add_test(NAME audio_spectrum_is_valid
  COMMAND validate_audio_spectrum
)
add_test(NAME cube_profile_report
  COMMAND proteus --renderer cpu --profile cube --frames 10 --warmup 2
)
set_tests_properties(cube_profile_report PROPERTIES
  PASS_REGULAR_EXPRESSION "proteus_profile_end=1"
)
add_test(NAME cube_screenshot
  COMMAND proteus --renderer cpu --screenshot ${CMAKE_CURRENT_BINARY_DIR}/cube-screenshot.png
          --scene cube --time 1.5
)
set_tests_properties(cube_screenshot PROPERTIES
  PASS_REGULAR_EXPRESSION "timeline_time=1.500"
)

add_test(NAME interactive_timeline_options
  COMMAND proteus --renderer cpu --scene cube --time 2.5 --help
)
set_tests_properties(interactive_timeline_options PROPERTIES
  PASS_REGULAR_EXPRESSION "Usage:"
)

# Reviewed CPU reference for the opening five-gull follow shot.
add_test(NAME cube_render_checksum
  COMMAND proteus --renderer cpu --profile cube --frames 1 --warmup 0
)
set_tests_properties(cube_render_checksum PROPERTIES
  PASS_REGULAR_EXPRESSION "final_framebuffer_checksum=735e6a86373c4347"
)

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
  add_test(NAME gpu_surface_materials COMMAND gpu_fixture --city --check)
  set_tests_properties(gpu_surface_materials PROPERTIES LABELS "gpu" RUN_SERIAL TRUE)
endif()
