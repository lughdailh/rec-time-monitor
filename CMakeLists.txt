cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" OFF)
option(ENABLE_QT "Use Qt functionality" OFF)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 COMPONENTS Widgets Core)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)
  if(APPLE AND TARGET WrapOpenGL::WrapOpenGL)
    # Qt6's FindWrapOpenGL.cmake links against the AGL.framework *path* it
    # finds on disk. The .framework directory still exists on recent macOS
    # (its binary is only a broken symlink into the dyld cache), so
    # find_library "succeeds" but the linker then fails with "framework
    # 'AGL' not found". AGL is legacy/unused here, so drop it.
    get_target_property(_wrap_opengl_libs WrapOpenGL::WrapOpenGL INTERFACE_LINK_LIBRARIES)
    if(_wrap_opengl_libs)
      list(FILTER _wrap_opengl_libs EXCLUDE REGEX "AGL\\.framework")
      set_target_properties(WrapOpenGL::WrapOpenGL PROPERTIES INTERFACE_LINK_LIBRARIES "${_wrap_opengl_libs}")
    endif()
    unset(_wrap_opengl_libs)
  endif()
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
  )
endif()

target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE src/plugin-main.cpp
          src/time-tracker.cpp
          src/time-tracker.hpp
          src/program-display.cpp
          src/program-display.hpp
          src/program-monitor-dialog.cpp
          src/program-monitor-dialog.hpp
          src/badge-image.cpp
          src/badge-image.hpp
          src/settings.cpp
          src/settings.hpp
          src/settings-dialog.cpp
          src/settings-dialog.hpp
          src/time-unit-dial.cpp
          src/time-unit-dial.hpp
          src/big-time-editor.cpp
          src/big-time-editor.hpp
)

# CreateDisplay() (the bit that hands OBS a native window handle) is the only
# platform-specific piece of OBSProgramDisplay; see program-display.cpp.
if(APPLE)
  target_sources(${CMAKE_PROJECT_NAME} PRIVATE src/program-display-mac.mm)
elseif(WIN32)
  target_sources(${CMAKE_PROJECT_NAME} PRIVATE src/program-display-win.cpp)
else()
  message(FATAL_ERROR "No OBSProgramDisplay::CreateDisplay() implementation for this platform")
endif()

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
