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
add_executable(validate_boat_audio tools/validate_boat_audio.c src/boat_audio.c)
target_include_directories(validate_boat_audio PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(validate_boat_audio PRIVATE raylib)
target_compile_options(validate_boat_audio PRIVATE
  $<$<C_COMPILER_ID:AppleClang,Clang,GNU>:-Wall;-Wextra;-Wpedantic>)
add_test(NAME boat_audio_is_valid COMMAND validate_boat_audio)

add_executable(boat_study tools/boat_study.c src/boat.c src/renderer3d.c src/profiler.c)
target_sources(boat_study PRIVATE src/boat_audio.c src/xm_audio.c src/boat_gpu.c src/renderer_gpu.c)
target_compile_definitions(boat_study PRIVATE
  PROTEUS_ASSET_DIR="$<TARGET_FILE_DIR:boat_study>/assets"
)
add_dependencies(boat_study proteus_assets)
add_executable(validate_boat tools/validate_boat.c src/boat.c src/renderer3d.c)
add_executable(validate_boat_environment tools/validate_boat_environment.c src/boat.c src/renderer3d.c)
add_executable(validate_boat_replay tools/validate_boat_replay.c src/boat.c src/renderer3d.c)
add_executable(boat_water_review tools/boat_water_review.c src/boat.c src/renderer3d.c
  src/boat_gpu.c src/renderer_gpu.c)
add_dependencies(boat_water_review proteus_assets)
foreach(target boat_study validate_boat validate_boat_environment validate_boat_replay boat_water_review)
  target_sources(${target} PRIVATE src/boat_water.c src/boat_course.c src/boat_replay.c src/boat_map.c src/waterfront.c src/waterfront_scene.c src/waterfront_corner.c)
  target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(${target} PRIVATE raylib)
  target_compile_options(${target} PRIVATE
    $<$<C_COMPILER_ID:AppleClang,Clang,GNU>:-Wall;-Wextra;-Wpedantic>
  )
endforeach()
add_test(NAME boat_handling COMMAND validate_boat)
add_test(NAME boat_environment COMMAND validate_boat_environment)
add_test(NAME boat_player_wake_screenshot COMMAND boat_study --renderer cpu --player-wake --time 9
  --screenshot ${CMAKE_CURRENT_BINARY_DIR}/boat-player-wake.png)
add_test(NAME boat_player_wake_course_rejected COMMAND boat_study --player-wake --course)
set_tests_properties(boat_player_wake_course_rejected PROPERTIES WILL_FAIL TRUE)
add_test(NAME boat_current_screenshot COMMAND boat_study --renderer cpu --current --time 5
  --screenshot ${CMAKE_CURRENT_BINARY_DIR}/boat-current.png)
add_test(NAME boat_crossing_wake_screenshot COMMAND boat_study --renderer cpu --crossing-wake --time 9
  --screenshot ${CMAKE_CURRENT_BINARY_DIR}/boat-crossing-wake.png)
add_test(NAME boat_environment_screenshot COMMAND boat_study --renderer cpu --current --crossing-wake
  --waves --time 9 --screenshot ${CMAKE_CURRENT_BINARY_DIR}/boat-environment.png)
add_test(NAME boat_environment_course_rejected COMMAND boat_study --current --course)
add_test(NAME boat_environment_eriksberg_screenshot COMMAND boat_study --renderer cpu
  --location eriksberg --current --crossing-wake --time 3
  --screenshot ${CMAKE_CURRENT_BINARY_DIR}/boat-eriksberg-environment.png)
add_test(NAME boat_environment_river_screenshot COMMAND boat_study --renderer cpu
  --location river --current --crossing-wake --time 3
  --screenshot ${CMAKE_CURRENT_BINARY_DIR}/boat-river-environment.png)
set_tests_properties(boat_environment_course_rejected PROPERTIES WILL_FAIL TRUE)
add_test(NAME boat_default_player_wake_screenshot COMMAND boat_study --renderer cpu --time 9
  --screenshot ${CMAKE_CURRENT_BINARY_DIR}/boat-default-player-wake.png)
set_tests_properties(boat_default_player_wake_screenshot boat_player_wake_screenshot
  PROPERTIES FIXTURES_SETUP player_wake_defaults)
add_test(NAME boat_default_wake_matches_explicit COMMAND ${CMAKE_COMMAND} -E compare_files
  ${CMAKE_CURRENT_BINARY_DIR}/boat-default-player-wake.png
  ${CMAKE_CURRENT_BINARY_DIR}/boat-player-wake.png)
set_tests_properties(boat_default_wake_matches_explicit PROPERTIES FIXTURES_REQUIRED player_wake_defaults)
add_test(NAME boat_water_route COMMAND boat_water_review --check)
add_test(NAME boat_water_wave_route COMMAND boat_water_review --check --waves)
add_test(NAME boat_river_route COMMAND boat_water_review --check --river)
add_test(NAME boat_river_wave_route COMMAND boat_water_review --check --river --waves)
set(BOAT_REPLAY_TEST_DIR "${CMAKE_CURRENT_BINARY_DIR}/replay-tests")
add_test(NAME boat_replay COMMAND validate_boat_replay ${BOAT_REPLAY_TEST_DIR})
set_tests_properties(boat_replay PROPERTIES FIXTURES_SETUP boat_recording)
add_test(NAME boat_replay_screenshot COMMAND boat_study --renderer cpu
  --replay ${BOAT_REPLAY_TEST_DIR}/replay-fixture.run --time 5
  --data-dir ${BOAT_REPLAY_TEST_DIR}/profile
  --screenshot ${CMAKE_CURRENT_BINARY_DIR}/boat-replay.png)
add_test(NAME boat_ghost_screenshot COMMAND boat_study --renderer cpu
  --ghost ${BOAT_REPLAY_TEST_DIR}/replay-fixture.run --time 0
  --data-dir ${BOAT_REPLAY_TEST_DIR}/profile
  --screenshot ${CMAKE_CURRENT_BINARY_DIR}/boat-ghost.png)
add_test(NAME boat_saved_best_screenshot COMMAND boat_study --renderer cpu --course --time 0
  --data-dir ${BOAT_REPLAY_TEST_DIR}
  --screenshot ${CMAKE_CURRENT_BINARY_DIR}/boat-saved-best.png)
add_test(NAME boat_ghost_wrong_water COMMAND boat_study --renderer cpu --waves
  --ghost ${BOAT_REPLAY_TEST_DIR}/replay-fixture.run --time 0
  --data-dir ${BOAT_REPLAY_TEST_DIR}/profile
  --screenshot ${CMAKE_CURRENT_BINARY_DIR}/boat-ghost-invalid.png)
set_tests_properties(boat_ghost_wrong_water PROPERTIES WILL_FAIL TRUE)
set_tests_properties(boat_replay_screenshot boat_ghost_screenshot boat_saved_best_screenshot boat_ghost_wrong_water
  PROPERTIES FIXTURES_REQUIRED boat_recording)
add_test(NAME boat_course_screenshot COMMAND boat_study --renderer cpu --course --time 0
  --data-dir ${BOAT_REPLAY_TEST_DIR}/profile
  --screenshot ${CMAKE_CURRENT_BINARY_DIR}/boat-course.png)
add_test(NAME boat_wave_screenshot COMMAND boat_study --renderer cpu --waves --time 1.8
  --screenshot ${CMAKE_CURRENT_BINARY_DIR}/boat-wave.png)
add_test(NAME boat_screenshot COMMAND boat_study --renderer cpu --time 3
  --screenshot ${CMAKE_CURRENT_BINARY_DIR}/boat-screenshot.png)
add_test(NAME boat_cpu_aa_option COMMAND boat_study --renderer cpu --gpu-aa --time 0
  --screenshot ${CMAKE_CURRENT_BINARY_DIR}/boat-cpu-aa-option.png)
add_test(NAME boat_cpu_aa_option_reversed COMMAND boat_study --gpu-aa --renderer cpu --time 0
  --screenshot ${CMAKE_CURRENT_BINARY_DIR}/boat-cpu-aa-option-reversed.png)
add_test(NAME boat_eriksberg_screenshot COMMAND boat_study --renderer cpu --location eriksberg --time 3
  --screenshot ${CMAKE_CURRENT_BINARY_DIR}/boat-eriksberg.png)
add_test(NAME boat_river_screenshot COMMAND boat_study --renderer cpu --location river --time 0
  --screenshot ${CMAKE_CURRENT_BINARY_DIR}/boat-river.png)

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
add_executable(validate_harbor tools/validate_harbor.c src/boat.c src/renderer3d.c
  src/boat_model.c src/waterfront_harbor.c src/waterfront_traffic.c src/boat_water.c src/boat_map.c
  src/boat_course.c src/boat_replay.c src/waterfront.c src/waterfront_scene.c src/waterfront_corner.c)
target_include_directories(validate_harbor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(validate_harbor PRIVATE raylib)
target_compile_options(validate_harbor PRIVATE
  $<$<C_COMPILER_ID:AppleClang,Clang,GNU>:-Wall;-Wextra;-Wpedantic>)
add_test(NAME harbor_geometry COMMAND validate_harbor)

add_executable(validate_traffic tools/validate_traffic.c src/boat.c src/renderer3d.c
  src/boat_model.c src/waterfront_harbor.c src/waterfront_traffic.c src/boat_water.c src/boat_map.c
  src/boat_course.c src/boat_replay.c src/waterfront.c src/waterfront_scene.c src/waterfront_corner.c)
target_include_directories(validate_traffic PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(validate_traffic PRIVATE raylib)
target_compile_options(validate_traffic PRIVATE
  $<$<C_COMPILER_ID:AppleClang,Clang,GNU>:-Wall;-Wextra;-Wpedantic>)
add_test(NAME harbor_traffic COMMAND validate_traffic)

add_executable(harbor_review tools/harbor_review.c src/renderer_gpu.c src/renderer3d.c
  src/waterfront.c src/waterfront_scene.c src/waterfront_corner.c)
add_dependencies(harbor_review proteus_assets)
add_test(NAME ferry_cpu_screenshot COMMAND harbor_review --renderer cpu --view ferry
  --time 40 --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ferry-cpu.png)
add_test(NAME harbor_cpu_screenshot COMMAND harbor_review --renderer cpu --view marina
  --screenshot ${CMAKE_CURRENT_BINARY_DIR}/harbor-marina-cpu.png)
target_include_directories(harbor_review PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(harbor_review PRIVATE raylib)
target_compile_options(harbor_review PRIVATE
  $<$<C_COMPILER_ID:AppleClang,Clang,GNU>:-Wall;-Wextra;-Wpedantic>)

# Shared generated boats and berth evaluation feed all waterfront consumers.
foreach(target proteus gpu_fixture boat_study validate_boat validate_boat_environment validate_boat_replay
    boat_water_review waterfront_storyboard validate_waterfront_actors validate_city_dressing
    harbor_review)
  target_sources(${target} PRIVATE src/boat_model.c src/waterfront_harbor.c src/waterfront_traffic.c)
endforeach()
foreach(target proteus gpu_fixture waterfront_storyboard validate_waterfront_actors
    validate_city_dressing harbor_review)
  target_sources(${target} PRIVATE src/boat_water.c)
endforeach()

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

# Reviewed daytime CPU reference with harbor vessels in the opening five-gull follow shot.
add_test(NAME cube_render_checksum
  COMMAND proteus --renderer cpu --profile cube --frames 1 --warmup 0
)
set_tests_properties(cube_render_checksum PROPERTIES
  PASS_REGULAR_EXPRESSION "final_framebuffer_checksum=80d44611b75ac3fb"
)

add_test(NAME cube_start_screenshot
  COMMAND proteus --renderer cpu --screenshot ${CMAKE_CURRENT_BINARY_DIR}/cube-start.png
          --scene cube --time 0
)
set_tests_properties(cube_start_screenshot PROPERTIES
  PASS_REGULAR_EXPRESSION "scene=cube"
)

if(PROTEUS_GPU_TESTS)
  add_test(NAME ferry_gpu_screenshot COMMAND harbor_review --renderer gpu --view ferry
    --waves --time 40 --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ferry-gpu.png)
  set_tests_properties(ferry_gpu_screenshot PROPERTIES LABELS "gpu" RUN_SERIAL TRUE)
  add_test(NAME harbor_gpu_screenshot COMMAND harbor_review --renderer gpu --view marina
    --waves --time 2 --screenshot ${CMAKE_CURRENT_BINARY_DIR}/harbor-marina-gpu.png)
  set_tests_properties(harbor_gpu_screenshot PROPERTIES LABELS "gpu" RUN_SERIAL TRUE)
  add_test(NAME boat_player_wake_gpu_screenshot COMMAND boat_study --player-wake --time 9
    --screenshot ${CMAKE_CURRENT_BINARY_DIR}/boat-player-wake-gpu.png)
  add_test(NAME boat_player_wake_gpu_aa_screenshot COMMAND boat_study --player-wake --current --waves
    --gpu-aa --time 9 --screenshot ${CMAKE_CURRENT_BINARY_DIR}/boat-player-wake-gpu-aa.png)
  set_tests_properties(boat_player_wake_gpu_screenshot boat_player_wake_gpu_aa_screenshot
    PROPERTIES LABELS "gpu" RUN_SERIAL TRUE)
  add_test(NAME boat_environment_gpu_screenshot COMMAND boat_study --current --crossing-wake
    --waves --gpu-aa --time 9 --screenshot ${CMAKE_CURRENT_BINARY_DIR}/boat-environment-gpu.png)
  set_tests_properties(boat_environment_gpu_screenshot PROPERTIES LABELS "gpu" RUN_SERIAL TRUE)
  add_test(NAME boat_environment_gpu COMMAND validate_boat_environment --gpu)
  set_tests_properties(boat_environment_gpu PROPERTIES LABELS "gpu" RUN_SERIAL TRUE)
  add_test(NAME gpu_water_surface COMMAND boat_water_review --check-gpu)
  set_tests_properties(gpu_water_surface PROPERTIES LABELS "gpu" RUN_SERIAL TRUE)
  add_test(NAME boat_ghost_gpu_screenshot COMMAND boat_study --gpu-aa
    --ghost ${BOAT_REPLAY_TEST_DIR}/replay-fixture.run --time 0
    --data-dir ${BOAT_REPLAY_TEST_DIR}/profile
    --screenshot ${CMAKE_CURRENT_BINARY_DIR}/boat-ghost-gpu.png)
  add_test(NAME boat_replay_gpu_screenshot COMMAND boat_study --gpu-aa
    --replay ${BOAT_REPLAY_TEST_DIR}/replay-fixture.run --time 5
    --data-dir ${BOAT_REPLAY_TEST_DIR}/profile
    --screenshot ${CMAKE_CURRENT_BINARY_DIR}/boat-replay-gpu.png)
  set_tests_properties(boat_ghost_gpu_screenshot boat_replay_gpu_screenshot
    PROPERTIES FIXTURES_REQUIRED boat_recording LABELS "gpu" RUN_SERIAL TRUE)
  add_test(NAME boat_gpu_screenshot COMMAND boat_study --location eriksberg --gpu --time 3
    --screenshot ${CMAKE_CURRENT_BINARY_DIR}/boat-gpu.png)
  add_test(NAME boat_gpu_aa_screenshot COMMAND boat_study --location eriksberg --gpu --gpu-aa --time 3
    --screenshot ${CMAKE_CURRENT_BINARY_DIR}/boat-gpu-aa.png)
  add_test(NAME boat_wave_gpu_screenshot COMMAND boat_study --location eriksberg --gpu-aa --waves
    --camera-distance 12 --camera-height 6 --time 2.9
    --screenshot ${CMAKE_CURRENT_BINARY_DIR}/boat-wave-gpu.png)
  add_test(NAME boat_basin_gpu_screenshot COMMAND boat_study --time 0
    --screenshot ${CMAKE_CURRENT_BINARY_DIR}/boat-basin-gpu.png)
  add_test(NAME boat_basin_gpu_aa_screenshot COMMAND boat_study --gpu-aa --time 0
    --screenshot ${CMAKE_CURRENT_BINARY_DIR}/boat-basin-gpu-aa.png)
  add_test(NAME boat_course_gpu_screenshot COMMAND boat_study --course --gpu-aa --time 0
    --data-dir ${BOAT_REPLAY_TEST_DIR}/profile
    --screenshot ${CMAKE_CURRENT_BINARY_DIR}/boat-course-gpu.png)
  add_test(NAME boat_basin_wave_gpu_screenshot COMMAND boat_study --waves --gpu-aa --time 1.8
    --screenshot ${CMAKE_CURRENT_BINARY_DIR}/boat-basin-wave-gpu.png)
  set_tests_properties(boat_basin_wave_gpu_screenshot PROPERTIES LABELS "gpu" RUN_SERIAL TRUE)
  set_tests_properties(boat_basin_gpu_screenshot boat_basin_gpu_aa_screenshot boat_course_gpu_screenshot
    PROPERTIES LABELS "gpu" RUN_SERIAL TRUE)
  set_tests_properties(boat_gpu_screenshot boat_gpu_aa_screenshot boat_wave_gpu_screenshot
    PROPERTIES LABELS "gpu" RUN_SERIAL TRUE)
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

add_test(NAME boating_xm_is_valid
  COMMAND validate_xm ${CMAKE_CURRENT_SOURCE_DIR}/assets/music/a-little-boat.xm --background)
