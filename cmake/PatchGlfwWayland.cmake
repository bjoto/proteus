# Backport GLFW's no-seat initialization fix to raylib 6.0's bundled GLFW.
# Headless Weston intentionally exposes no wl_seat; querying its version crashes.
# Upstream: https://github.com/glfw/glfw/issues/2517 (commit 3573c5a).
# Remove this backport when the pinned raylib contains the upstream fix.
function(proteus_patch_glfw_wayland source)
  file(READ "${source}" contents)
  set(original "if (wl_seat_get_version(_glfw.wl.seat) >= WL_KEYBOARD_REPEAT_INFO_SINCE_VERSION)")
  set(fixed "if (_glfw.wl.seat && wl_seat_get_version(_glfw.wl.seat) >= WL_KEYBOARD_REPEAT_INFO_SINCE_VERSION)")
  string(FIND "${contents}" "${fixed}" fixed_position)
  if(NOT fixed_position EQUAL -1)
    return()
  endif()
  string(FIND "${contents}" "${original}" original_position)
  if(original_position EQUAL -1)
    message(FATAL_ERROR "Bundled GLFW Wayland source changed; review the no-seat backport: ${source}")
  endif()
  string(REPLACE "${original}" "${fixed}" contents "${contents}")
  file(WRITE "${source}" "${contents}")
  message(STATUS "Patched bundled GLFW Wayland initialization for compositors without an input seat")
endfunction()
