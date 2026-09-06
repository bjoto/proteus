cmake_minimum_required(VERSION 3.24)
project(proteus LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG 6.0
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(raylib)

add_executable(proteus src/main.c)
target_link_libraries(proteus PRIVATE raylib)
target_compile_definitions(proteus PRIVATE PROTEUS_BUILD_TYPE="$<CONFIG>")
target_compile_options(proteus PRIVATE
  $<$<C_COMPILER_ID:AppleClang,Clang,GNU>:-Wall;-Wextra;-Wpedantic>
)

add_custom_command(TARGET proteus POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_CURRENT_SOURCE_DIR}/assets
          $<TARGET_FILE_DIR:proteus>/assets
)

add_executable(validate_xm tools/validate_xm.c)
target_include_directories(validate_xm PRIVATE ${raylib_SOURCE_DIR}/src/external)
if(NOT MSVC)
  target_link_libraries(validate_xm PRIVATE m)
endif()

enable_testing()
add_test(NAME starter_xm_is_valid
  COMMAND validate_xm ${CMAKE_CURRENT_SOURCE_DIR}/assets/music/starter.xm
)
add_test(NAME fire_profile_report
  COMMAND proteus --profile fire --frames 10 --warmup 2
)
set_tests_properties(fire_profile_report PROPERTIES
  PASS_REGULAR_EXPRESSION "proteus_profile_end=1"
)
