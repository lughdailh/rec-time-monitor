# Common build dependencies module

include_guard(GLOBAL)

# _check_deps_version: Checks for obs-deps VERSION file in prefix paths
function(_check_deps_version version)
  set(found FALSE)

  foreach(path IN LISTS CMAKE_PREFIX_PATH)
    if(EXISTS "${path}/share/obs-deps/VERSION")
      if(dependency STREQUAL qt6 AND NOT EXISTS "${path}/lib/cmake/Qt6/Qt6Config.cmake")
        set(found FALSE)
        continue()
      endif()

      file(READ "${path}/share/obs-deps/VERSION" _check_version)
      string(REPLACE "\n" "" _check_version "${_check_version}")
      string(REPLACE "-" "." _check_version "${_check_version}")
      string(REPLACE "-" "." version "${version}")

      if(_check_version VERSION_EQUAL version)
        set(found TRUE)
        break()
      elseif(_check_version VERSION_LESS version)
        message(
          AUTHOR_WARNING
          "Older ${label} version detected in ${path}: \n"
          "Found ${_check_version}, require ${version}"
        )
        list(REMOVE_ITEM CMAKE_PREFIX_PATH "${path}")
        list(APPEND CMAKE_PREFIX_PATH "${path}")
        set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH})
        continue()
      else()
        message(
          AUTHOR_WARNING
          "Newer ${label} version detected in ${path}: \n"
          "Found ${_check_version}, require ${version}"
        )
        set(found TRUE)
        break()
      endif()
    endif()
  endforeach()

  return(PROPAGATE found CMAKE_PREFIX_PATH)
endfunction()

# _setup_obs_studio: Create obs-studio build project, then build libobs and obs-frontend-api
function(_setup_obs_studio)
  if(NOT libobs_DIR)
    set(_is_fresh --fresh)
  endif()

  if(OS_WINDOWS)
    set(_cmake_generator "${CMAKE_GENERATOR}")
    # Deliberately NOT passing ",version=${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}"
    # here: when reconstructed into a raw `-A` command-line argument for this
    # nested execute_process (as opposed to a CMakePresets.json "architecture"
    # field, which CMake parses differently), CMAKE_VS_PLATFORM_NAME ends up
    # equal to the whole "x64,version=X" string instead of just "x64". That
    # broke the hashes lookup in obs-studio's own vendored buildspec.json
    # (which only has a plain "windows-x64" key) with "JSON member 'hashes
    # windows-x64,version=10.0.22621.0' not found". CMAKE_SYSTEM_VERSION below
    # already pins the target Windows version, so this is just letting the
    # installed SDK auto-resolve for this inner obs-studio sub-build.
    set(_cmake_arch "-A ${arch}")
    # NOTE: must be a proper CMake list (separate quoted args), not one
    # space-joined string — ${_cmake_extra} is expanded unquoted below, and
    # an unquoted space-joined string becomes a SINGLE argv element with a
    # literal space in it, which silently corrupts both -D values (see the
    # macOS branch's comment below for how this manifested).
    set(_cmake_extra "-DCMAKE_SYSTEM_VERSION=${CMAKE_SYSTEM_VERSION}" "-DCMAKE_ENABLE_SCRIPTING=OFF")
  elseif(OS_MACOS)
    set(_cmake_generator "Xcode")
    set(_cmake_arch "-DCMAKE_OSX_ARCHITECTURES:STRING='arm64;x86_64'")
    # CMAKE_OSX_SYSROOT isn't actually populated in our own top-level scope
    # either at this point (the Xcode generator resolves it lazily, later
    # than this), so forwarding "${CMAKE_OSX_SYSROOT}" just forwards empty.
    # obs-studio 30.2.3's own cmake/macos/compilerconfig.cmake assumes it's
    # already set and fails with "Your macOS SDK version () is too low"
    # otherwise. Resolve it explicitly via xcrun instead of relying on the
    # variable.
    if(CMAKE_OSX_SYSROOT)
      set(_macos_sdk_path "${CMAKE_OSX_SYSROOT}")
    else()
      execute_process(
        COMMAND xcrun --sdk macosx --show-sdk-path
        OUTPUT_VARIABLE _macos_sdk_path
        OUTPUT_STRIP_TRAILING_WHITESPACE
      )
    endif()
    # Same list-vs-space-joined-string pitfall as the Windows branch above:
    # this used to be one space-joined string with a single -D flag in it
    # (harmless while there was only one), but adding a second -D flag to
    # the same joined string made the *whole* expanded value land in a
    # single argv element (since ${_cmake_extra} is expanded unquoted at the
    # call site). CMake's -D parser then read everything after the first
    # "=" — including the literal space and the second "-DCMAKE_OSX_SYSROOT="
    # — as ONE value for CMAKE_OSX_DEPLOYMENT_TARGET, which is exactly what
    # showed up mangled into the linker's target triple ("... 12.0
    # -DCMAKE_OSX_SYSROOT=/Applications/...-ld doesn't exist").
    set(_cmake_extra "-DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}" "-DCMAKE_OSX_SYSROOT=${_macos_sdk_path}")
  endif()

  message(STATUS "Configure ${label} (${arch})")
  execute_process(
    COMMAND
      "${CMAKE_COMMAND}" -S "${dependencies_dir}/${_obs_destination}" -B
      "${dependencies_dir}/${_obs_destination}/build_${arch}" -G ${_cmake_generator} "${_cmake_arch}"
      -DOBS_CMAKE_VERSION:STRING=3.0.0 -DENABLE_PLUGINS:BOOL=OFF -DENABLE_FRONTEND:BOOL=OFF
      -DOBS_VERSION_OVERRIDE:STRING=${_obs_version} "-DCMAKE_PREFIX_PATH='${CMAKE_PREFIX_PATH}'" ${_is_fresh}
      ${_cmake_extra}
    RESULT_VARIABLE _process_result
    COMMAND_ERROR_IS_FATAL ANY
    OUTPUT_QUIET
  )
  message(STATUS "Configure ${label} (${arch}) - done")

  message(STATUS "Build ${label} (Debug - ${arch})")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" --build build_${arch} --target obs-frontend-api --config Debug --parallel
    WORKING_DIRECTORY "${dependencies_dir}/${_obs_destination}"
    RESULT_VARIABLE _process_result
    COMMAND_ERROR_IS_FATAL ANY
    OUTPUT_QUIET
  )
  message(STATUS "Build ${label} (Debug - ${arch}) - done")

  message(STATUS "Build ${label} (Release - ${arch})")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" --build build_${arch} --target obs-frontend-api --config Release --parallel
    WORKING_DIRECTORY "${dependencies_dir}/${_obs_destination}"
    RESULT_VARIABLE _process_result
    COMMAND_ERROR_IS_FATAL ANY
    OUTPUT_QUIET
  )
  message(STATUS "Build ${label} (Reelase - ${arch}) - done")

  message(STATUS "Install ${label} (${arch})")
  execute_process(
    COMMAND
      "${CMAKE_COMMAND}" --install build_${arch} --component Development --config Debug --prefix "${dependencies_dir}"
    WORKING_DIRECTORY "${dependencies_dir}/${_obs_destination}"
    RESULT_VARIABLE _process_result
    COMMAND_ERROR_IS_FATAL ANY
    OUTPUT_QUIET
  )
  execute_process(
    COMMAND
      "${CMAKE_COMMAND}" --install build_${arch} --component Development --config Release --prefix "${dependencies_dir}"
    WORKING_DIRECTORY "${dependencies_dir}/${_obs_destination}"
    RESULT_VARIABLE _process_result
    COMMAND_ERROR_IS_FATAL ANY
    OUTPUT_QUIET
  )
  message(STATUS "Install ${label} (${arch}) - done")
endfunction()

# _check_dependencies: Fetch and extract pre-built OBS build dependencies
function(_check_dependencies)
  file(READ "${CMAKE_CURRENT_SOURCE_DIR}/buildspec.json" buildspec)

  string(JSON dependency_data GET ${buildspec} dependencies)

  foreach(dependency IN LISTS dependencies_list)
    string(JSON data GET ${dependency_data} ${dependency})
    string(JSON version GET ${data} version)
    string(JSON hash GET ${data} hashes ${platform})
    string(JSON url GET ${data} baseUrl)
    string(JSON label GET ${data} label)
    string(JSON revision ERROR_VARIABLE error GET ${data} revision ${platform})

    message(STATUS "Setting up ${label} (${arch})")

    set(file "${${dependency}_filename}")
    set(destination "${${dependency}_destination}")
    string(REPLACE "VERSION" "${version}" file "${file}")
    string(REPLACE "VERSION" "${version}" destination "${destination}")
    string(REPLACE "ARCH" "${arch}" file "${file}")
    string(REPLACE "ARCH" "${arch}" destination "${destination}")
    if(revision)
      string(REPLACE "_REVISION" "_v${revision}" file "${file}")
      string(REPLACE "-REVISION" "-v${revision}" file "${file}")
    else()
      string(REPLACE "_REVISION" "" file "${file}")
      string(REPLACE "-REVISION" "" file "${file}")
    endif()

    if(EXISTS "${dependencies_dir}/.dependency_${dependency}_${arch}.sha256")
      file(
        READ
        "${dependencies_dir}/.dependency_${dependency}_${arch}.sha256"
        OBS_DEPENDENCY_${dependency}_${arch}_HASH
      )
    endif()

    set(skip FALSE)
    if(dependency STREQUAL prebuilt OR dependency STREQUAL qt6)
      if(OBS_DEPENDENCY_${dependency}_${arch}_HASH STREQUAL ${hash})
        _check_deps_version(${version})

        if(found)
          set(skip TRUE)
        endif()
      endif()
    endif()

    if(skip)
      message(STATUS "Setting up ${label} (${arch}) - skipped")
      continue()
    endif()

    if(dependency STREQUAL obs-studio)
      set(url ${url}/${file})
    else()
      set(url ${url}/${version}/${file})
    endif()

    if(NOT EXISTS "${dependencies_dir}/${file}")
      message(STATUS "Downloading ${url}")
      file(DOWNLOAD "${url}" "${dependencies_dir}/${file}" STATUS download_status EXPECTED_HASH SHA256=${hash})

      list(GET download_status 0 error_code)
      list(GET download_status 1 error_message)
      if(error_code GREATER 0)
        message(STATUS "Downloading ${url} - Failure")
        message(FATAL_ERROR "Unable to download ${url}, failed with error: ${error_message}")
        file(REMOVE "${dependencies_dir}/${file}")
      else()
        message(STATUS "Downloading ${url} - done")
      endif()
    endif()

    if(NOT OBS_DEPENDENCY_${dependency}_${arch}_HASH STREQUAL ${hash})
      file(REMOVE_RECURSE "${dependencies_dir}/${destination}")
    endif()

    if(NOT EXISTS "${dependencies_dir}/${destination}")
      file(MAKE_DIRECTORY "${dependencies_dir}/${destination}")
      if(dependency STREQUAL obs-studio)
        file(ARCHIVE_EXTRACT INPUT "${dependencies_dir}/${file}" DESTINATION "${dependencies_dir}")
      else()
        file(ARCHIVE_EXTRACT INPUT "${dependencies_dir}/${file}" DESTINATION "${dependencies_dir}/${destination}")
      endif()
    endif()

    file(WRITE "${dependencies_dir}/.dependency_${dependency}_${arch}.sha256" "${hash}")

    if(dependency STREQUAL prebuilt)
      list(APPEND CMAKE_PREFIX_PATH "${dependencies_dir}/${destination}")
    elseif(dependency STREQUAL qt6)
      list(APPEND CMAKE_PREFIX_PATH "${dependencies_dir}/${destination}")
    elseif(dependency STREQUAL obs-studio)
      set(_obs_version ${version})
      set(_obs_destination "${destination}")
      list(APPEND CMAKE_PREFIX_PATH "${dependencies_dir}")
    endif()

    message(STATUS "Setting up ${label} (${arch}) - done")
  endforeach()

  list(REMOVE_DUPLICATES CMAKE_PREFIX_PATH)

  set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} CACHE PATH "CMake prefix search path" FORCE)

  _setup_obs_studio()

  if(OS_WINDOWS)
    # obs-studio's own Windows install(EXPORT ...) rules put package config
    # files directly under "<prefix>/cmake/<component>/" (no "lib/" or
    # "share/" in between) — unlike its macOS layout (a libobs.framework
    # bundle) or the generic Unix "lib/cmake/<name>/" convention. That
    # doesn't match any of CMake's standard find_package Config-mode search
    # patterns, so find_package(libobs REQUIRED) / find_package(obs-frontend-api
    # REQUIRED) can't locate it via CMAKE_PREFIX_PATH alone. Hint the exact
    # locations directly (confirmed present via a one-off CI diagnostic dump).
    # libobsConfig.cmake itself internally calls find_dependency(w32-pthreads),
    # so that one needs hinting too, not just our own two direct find_package()
    # calls.
    set(libobs_DIR "${dependencies_dir}/cmake/libobs" CACHE PATH "libobs config dir" FORCE)
    set(obs-frontend-api_DIR "${dependencies_dir}/cmake/obs-frontend-api" CACHE PATH "obs-frontend-api config dir" FORCE)
    set(w32-pthreads_DIR "${dependencies_dir}/cmake/w32-pthreads" CACHE PATH "w32-pthreads config dir" FORCE)
  endif()
endfunction()
