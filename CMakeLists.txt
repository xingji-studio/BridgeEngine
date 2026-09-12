cmake_minimum_required(VERSION 3.20)

if(POLICY CMP0135)
	cmake_policy(SET CMP0135 NEW)
endif()
if(POLICY CMP0169)
	cmake_policy(SET CMP0169 OLD)
endif()

# Auto-detect vcpkg from VCPKG_ROOT environment variable
if(NOT CMAKE_TOOLCHAIN_FILE AND DEFINED ENV{VCPKG_ROOT})
	set(_vcpkg_toolchain "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
	if(EXISTS "${_vcpkg_toolchain}")
		set(CMAKE_TOOLCHAIN_FILE "${_vcpkg_toolchain}" CACHE FILEPATH "vcpkg toolchain file")
		message(STATUS "Auto-detected vcpkg from VCPKG_ROOT: $ENV{VCPKG_ROOT}")
	endif()
	unset(_vcpkg_toolchain)
endif()

project(BridgeEngine VERSION 26.0.1 LANGUAGES C CXX)

# User-visible version string (may include non-numeric suffixes like "26v1").
# Keep numeric PROJECT_VERSION for major/minor/patch macros and compatibility.
set(BRIDGEENGINE_VERSION_STRING "26v1" CACHE STRING "User-visible BridgeEngine version string")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(FetchContent)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Generate compile_commands.json" FORCE)

option(BRIDGEENGINE_BUILD_SHARED "Build the shared BridgeEngine library" ON)
option(BRIDGEENGINE_BUILD_STATIC "Build the static BridgeEngine library" ON)
option(BRIDGEENGINE_BUILD_EXAMPLES "Build BridgeEngine example executables" ON)
option(BRIDGEENGINE_BUILD_XJ380 "Enable XJ380 custom build targets" ON)
option(BRIDGEENGINE_BUILD_TESTS "Build BridgeEngine regression tests" ON)
option(BRIDGEENGINE_BUILD_EDITOR "Build the BridgeEngine UI editor (desktop, ImGui)" ON)
option(BRIDGEENGINE_FETCH_SDL "Fetch SDL3 dependencies when they are unavailable" ON)
option(BRIDGEENGINE_FETCH_XJ380 "Fetch the XJ380 SDK when XJ380_SDK is unset" ON)
option(BRIDGEENGINE_FETCH_IMGUI "Fetch Dear ImGui (docking) sources when building the editor" ON)
option(BRIDGEENGINE_ENABLE_ASAN "Enable AddressSanitizer for supported regression tests" OFF)

set(BRIDGEENGINE_FETCHCONTENT_TIMESTAMP_ARGS)
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.24)
	list(APPEND BRIDGEENGINE_FETCHCONTENT_TIMESTAMP_ARGS DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake/bridgeengine_version.h.in"
	"${CMAKE_CURRENT_BINARY_DIR}/generated/bridgeengine_version.h"
	@ONLY
)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated/xposix")
configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake/xj380_stddef.h.in"
	"${CMAKE_CURRENT_BINARY_DIR}/generated/xposix/stddef.h"
	COPYONLY
)

set(BRIDGEENGINE_PLATFORM_SOURCES
	src/platform/platform.c
	src/platform/sdl3/platform_sdl3.c
	src/platform/sdl3/keycode.c
)
set(BRIDGEENGINE_CORE_SOURCES
	src/core/render_context.c
	src/core/init.c
	src/core/version.c
	src/io.c
	src/pack.c
	src/resource.c
	src/log.c
)
set(BRIDGEENGINE_RENDER_SOURCES
	src/draw.c
	src/input/mouse_drawing.c
	src/text.c
	src/math.c
	src/texture.c
	src/input/input.c
	src/camera.c
)
set(BRIDGEENGINE_UI_SOURCES
	src/button.c
	src/scene.c
	src/level.c
	src/xml_loader.c
	src/project.c
	 src/ui.c
	 src/ui_xml.c
	 src/ui_crypt.c
)
set(BRIDGEENGINE_MEDIA_SOURCES
	src/audio.c
	src/video/video.c
)
set(BRIDGEENGINE_THIRDPARTY_SOURCES
	thirdparty/rzip/rz_lib.c
)
# rz_lib.c uses fseeko/ftello, which strict -std=c11 hides on glibc; the macro
# is a harmless no-op on Windows (_WIN32 branch uses _fseeki64/_ftelli64).
set_source_files_properties(thirdparty/rzip/rz_lib.c PROPERTIES
	COMPILE_DEFINITIONS "_POSIX_C_SOURCE=200809L"
)
set(BRIDGEENGINE_SOURCES
	${BRIDGEENGINE_PLATFORM_SOURCES}
	${BRIDGEENGINE_CORE_SOURCES}
	${BRIDGEENGINE_RENDER_SOURCES}
	${BRIDGEENGINE_UI_SOURCES}
	${BRIDGEENGINE_MEDIA_SOURCES}
	${BRIDGEENGINE_THIRDPARTY_SOURCES}
)

if(BRIDGEENGINE_BUILD_SHARED OR BRIDGEENGINE_BUILD_STATIC OR BRIDGEENGINE_BUILD_EXAMPLES)
	find_package(SDL3 CONFIG QUIET)
	find_package(SDL3_image CONFIG QUIET)
	find_package(SDL3_ttf CONFIG QUIET)
	find_package(PkgConfig QUIET)

	if(PkgConfig_FOUND)
		pkg_check_modules(SDL3 QUIET IMPORTED_TARGET sdl3)
		pkg_check_modules(SDL3_IMAGE QUIET IMPORTED_TARGET sdl3-image)
		pkg_check_modules(SDL3_TTF QUIET IMPORTED_TARGET sdl3-ttf)
	endif()

	if(TARGET SDL3::SDL3)
		set(BRIDGEENGINE_SDL3_TARGET SDL3::SDL3)
	elseif(TARGET PkgConfig::SDL3)
		add_library(SDL3::SDL3 INTERFACE IMPORTED)
		set_target_properties(SDL3::SDL3 PROPERTIES
			INTERFACE_LINK_LIBRARIES PkgConfig::SDL3
		)
		set(BRIDGEENGINE_SDL3_TARGET SDL3::SDL3)
	else()
		if(NOT BRIDGEENGINE_FETCH_SDL)
			message(FATAL_ERROR "SDL3 was not found. Install it or configure with -DBRIDGEENGINE_FETCH_SDL=ON.")
		endif()
		set(SDL_SHARED OFF CACHE BOOL "" FORCE)
		set(SDL_STATIC ON CACHE BOOL "" FORCE)
		set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
		FetchContent_Declare(sdl3
			URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.12/SDL3-3.4.12.tar.gz
			URL_HASH SHA256=f07b958a9ac5020fb7a44cadb957f658b2149c3c8abb4f63145fac9303249db7
			${BRIDGEENGINE_FETCHCONTENT_TIMESTAMP_ARGS}
		)
		FetchContent_MakeAvailable(sdl3)
		set(BRIDGEENGINE_SDL3_TARGET SDL3::SDL3)
	endif()

	if(TARGET SDL3_image::SDL3_image)
		set(BRIDGEENGINE_SDL3_IMAGE_TARGET SDL3_image::SDL3_image)
	elseif(TARGET PkgConfig::SDL3_IMAGE)
		add_library(SDL3_image::SDL3_image INTERFACE IMPORTED)
		set_target_properties(SDL3_image::SDL3_image PROPERTIES
			INTERFACE_LINK_LIBRARIES PkgConfig::SDL3_IMAGE
		)
		set(BRIDGEENGINE_SDL3_IMAGE_TARGET SDL3_image::SDL3_image)
	else()
		if(NOT BRIDGEENGINE_FETCH_SDL)
			message(FATAL_ERROR "SDL3_image was not found. Install it or configure with -DBRIDGEENGINE_FETCH_SDL=ON.")
		endif()
		set(SDLIMAGE_VENDORED ON CACHE BOOL "" FORCE)
		FetchContent_Declare(sdl3_image
			URL https://github.com/libsdl-org/SDL_image/releases/download/release-3.4.4/SDL3_image-3.4.4.tar.gz
			URL_HASH SHA256=29751304a13d25ac513f24305fa25b06a6edd9607718c90129b8350d35fc5573
			${BRIDGEENGINE_FETCHCONTENT_TIMESTAMP_ARGS}
		)
		FetchContent_MakeAvailable(sdl3_image)
		set(BRIDGEENGINE_SDL3_IMAGE_TARGET SDL3_image::SDL3_image)
	endif()

	if(TARGET SDL3_ttf::SDL3_ttf)
		set(BRIDGEENGINE_SDL3_TTF_TARGET SDL3_ttf::SDL3_ttf)
	elseif(TARGET PkgConfig::SDL3_TTF)
		add_library(SDL3_ttf::SDL3_ttf INTERFACE IMPORTED)
		set_target_properties(SDL3_ttf::SDL3_ttf PROPERTIES
			INTERFACE_LINK_LIBRARIES PkgConfig::SDL3_TTF
		)
		set(BRIDGEENGINE_SDL3_TTF_TARGET SDL3_ttf::SDL3_ttf)
	else()
		if(NOT BRIDGEENGINE_FETCH_SDL)
			message(FATAL_ERROR "SDL3_ttf was not found. Install it or configure with -DBRIDGEENGINE_FETCH_SDL=ON.")
		endif()
		set(SDLTTF_VENDORED ON CACHE BOOL "" FORCE)
		FetchContent_Declare(sdl3_ttf
			URL https://github.com/libsdl-org/SDL_ttf/releases/download/release-3.2.2/SDL3_ttf-3.2.2.tar.gz
			URL_HASH SHA256=63547d58d0185c833213885b635a2c0548201cc8f301e6587c0be1a67e1e045d
			${BRIDGEENGINE_FETCHCONTENT_TIMESTAMP_ARGS}
		)
		FetchContent_MakeAvailable(sdl3_ttf)
		set(BRIDGEENGINE_SDL3_TTF_TARGET SDL3_ttf::SDL3_ttf)
	endif()
	add_library(BridgeEngine::SDL3 INTERFACE IMPORTED)
	set_target_properties(BridgeEngine::SDL3 PROPERTIES
		INTERFACE_LINK_LIBRARIES "${BRIDGEENGINE_SDL3_TARGET}"
	)
	add_library(BridgeEngine::SDL3_image INTERFACE IMPORTED)
	set_target_properties(BridgeEngine::SDL3_image PROPERTIES
		INTERFACE_LINK_LIBRARIES "${BRIDGEENGINE_SDL3_IMAGE_TARGET}"
	)
	add_library(BridgeEngine::SDL3_ttf INTERFACE IMPORTED)
	set_target_properties(BridgeEngine::SDL3_ttf PROPERTIES
		INTERFACE_LINK_LIBRARIES "${BRIDGEENGINE_SDL3_TTF_TARGET}"
	)
	set(BRIDGEENGINE_SDL_LIBS
		BridgeEngine::SDL3
		BridgeEngine::SDL3_image
		BridgeEngine::SDL3_ttf
	)

	if(WIN32)
		find_package(FFmpeg QUIET)
		if(NOT FFmpeg_FOUND)
			message(FATAL_ERROR
				"FFmpeg development libraries are required: avcodec, avformat, avutil, swscale, swresample.\n"
				"Windows: run scripts\\bootstrap-vcpkg.ps1, then configure with\n"
				"-DCMAKE_TOOLCHAIN_FILE=<vcpkg>\\scripts\\buildsystems\\vcpkg.cmake\n"
				"-DVCPKG_TARGET_TRIPLET=x64-windows.")
		endif()
		set(BRIDGEENGINE_PLATFORM_LIBS
			${BRIDGEENGINE_SDL_LIBS}
			FFmpeg::avcodec FFmpeg::avformat FFmpeg::avutil FFmpeg::swscale FFmpeg::swresample
			opengl32 gdi32 user32 kernel32 shell32
		)
		set(BRIDGEENGINE_PLATFORM_INCLUDES ${FFmpeg_INCLUDE_DIRS})
	else()
		if(NOT PkgConfig_FOUND)
			message(FATAL_ERROR
				"FFmpeg development libraries are required: avcodec, avformat, avutil, swscale, swresample.\n"
				"Install pkg-config and FFmpeg development packages (Linux: libavcodec-dev libavformat-dev "
				"libavutil-dev libswscale-dev libswresample-dev; macOS: brew install ffmpeg pkg-config).")
		endif()
		pkg_check_modules(FFMPEG QUIET IMPORTED_TARGET
			libavcodec libavformat libavutil libswscale libswresample
		)
		if(NOT FFMPEG_FOUND)
			message(FATAL_ERROR
				"FFmpeg development libraries are required: avcodec, avformat, avutil, swscale, swresample.\n"
				"Linux (Debian/Ubuntu): sudo apt install libavcodec-dev libavformat-dev libavutil-dev "
				"libswscale-dev libswresample-dev pkg-config\n"
				"macOS: brew install ffmpeg pkg-config")
		endif()
		if(APPLE)
			find_library(BRIDGEENGINE_OPENGL_FRAMEWORK OpenGL REQUIRED)
			set(BRIDGEENGINE_PLATFORM_LIBS ${BRIDGEENGINE_SDL_LIBS} PkgConfig::FFMPEG
				${BRIDGEENGINE_OPENGL_FRAMEWORK} m)
		else()
			set(BRIDGEENGINE_PLATFORM_LIBS ${BRIDGEENGINE_SDL_LIBS} PkgConfig::FFMPEG GL m)
		endif()
	endif()
endif()

function(bridgeengine_configure_target target_name)
	target_include_directories(${target_name}
		PUBLIC
			$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
			$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>
			$<INSTALL_INTERFACE:include>
		PRIVATE
			${CMAKE_CURRENT_SOURCE_DIR}/src
			${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/rzip
			${BRIDGEENGINE_PLATFORM_INCLUDES}
	)
	target_compile_definitions(${target_name} PRIVATE BAPI_LOG_ENABLED)
	target_compile_definitions(${target_name} PRIVATE
		$<$<COMPILE_LANG_AND_ID:C,MSVC>:_CRT_SECURE_NO_WARNINGS>
	)
	target_compile_options(${target_name} PRIVATE
		$<$<AND:$<COMPILE_LANG_AND_ID:C,Clang,GNU>,$<COMPILE_LANGUAGE:C>>:-Wall -Wextra>
		$<$<AND:$<COMPILE_LANG_AND_ID:CXX,Clang,GNU>,$<COMPILE_LANGUAGE:CXX>>:-Wall -Wextra>
		$<$<COMPILE_LANG_AND_ID:C,MSVC>:/W4>
		$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/W4>
	)
	if(BRIDGEENGINE_PLATFORM_LINK_DIRS)
		target_link_directories(${target_name} PRIVATE ${BRIDGEENGINE_PLATFORM_LINK_DIRS})
	endif()
	target_link_libraries(${target_name} PRIVATE ${BRIDGEENGINE_PLATFORM_LIBS})
endfunction()

function(bridgeengine_copy_runtime_assets target_name)
	if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/assets")
		add_custom_command(TARGET ${target_name} POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_directory
				"${CMAKE_CURRENT_SOURCE_DIR}/examples/assets"
				"$<TARGET_FILE_DIR:${target_name}>/assets"
		)
	endif()
endfunction()

if(BRIDGEENGINE_BUILD_TESTS)
	enable_testing()
	add_executable(bridgeengine_public_header_c_test tests/public_header_c_test.c)
	target_include_directories(bridgeengine_public_header_c_test PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
		"${CMAKE_CURRENT_BINARY_DIR}/generated"
	)
	add_test(NAME bridgeengine.public_header_c COMMAND bridgeengine_public_header_c_test)

	add_executable(bridgeengine_public_header_cpp_test tests/public_header_cpp_test.cpp)
	target_include_directories(bridgeengine_public_header_cpp_test PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
		"${CMAKE_CURRENT_BINARY_DIR}/generated"
	)
	add_test(NAME bridgeengine.public_header_cpp COMMAND bridgeengine_public_header_cpp_test)

	add_executable(bridgeengine_legacy_header_paths_test tests/legacy_header_paths_test.cpp)
	target_include_directories(bridgeengine_legacy_header_paths_test PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
		"${CMAKE_CURRENT_SOURCE_DIR}/include/legacy"
		"${CMAKE_CURRENT_BINARY_DIR}/generated"
	)
	add_test(NAME bridgeengine.legacy_header_paths COMMAND bridgeengine_legacy_header_paths_test)

	add_executable(bridgeengine_scene_level_xml_test
		tests/scene_level_xml_test.c
		src/scene.c
		src/level.c
		src/xml_loader.c
	)
	target_include_directories(bridgeengine_scene_level_xml_test PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
		"${CMAKE_CURRENT_BINARY_DIR}/generated"
		"${CMAKE_CURRENT_SOURCE_DIR}/src"
	)
	add_test(NAME bridgeengine.scene_level_xml
		COMMAND bridgeengine_scene_level_xml_test "${CMAKE_CURRENT_BINARY_DIR}"
	)

	add_executable(bridgeengine_ui_xml_test
		tests/ui_xml_test.c
		src/ui.c
		src/ui_xml.c
		src/ui_crypt.c
		src/button.c
	)
	target_include_directories(bridgeengine_ui_xml_test PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
		"${CMAKE_CURRENT_BINARY_DIR}/generated"
		"${CMAKE_CURRENT_SOURCE_DIR}/src"
	)
	add_test(NAME bridgeengine.ui_xml COMMAND bridgeengine_ui_xml_test "${CMAKE_CURRENT_BINARY_DIR}")

	add_executable(bridgeengine_project_bep_test
		tests/project_bep_test.c
		src/project.c
	)
	target_include_directories(bridgeengine_project_bep_test PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
		"${CMAKE_CURRENT_BINARY_DIR}/generated"
		"${CMAKE_CURRENT_SOURCE_DIR}/src"
	)
	add_test(NAME bridgeengine.project_bep COMMAND bridgeengine_project_bep_test "${CMAKE_CURRENT_BINARY_DIR}")

	add_executable(bridgeengine_ui_save_test
		tests/ui_save_test.c
		src/ui.c
		src/ui_xml.c
		src/ui_crypt.c
		src/button.c
	)
	target_include_directories(bridgeengine_ui_save_test PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
		"${CMAKE_CURRENT_BINARY_DIR}/generated"
		"${CMAKE_CURRENT_SOURCE_DIR}/src"
	)
	add_test(NAME bridgeengine.ui_save COMMAND bridgeengine_ui_save_test "${CMAKE_CURRENT_BINARY_DIR}")

	add_executable(bridgeengine_ui_uix_test
		tests/ui_uix_test.c
		src/ui.c
		src/ui_xml.c
		src/ui_crypt.c
		src/button.c
	)
	target_include_directories(bridgeengine_ui_uix_test PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
		"${CMAKE_CURRENT_BINARY_DIR}/generated"
		"${CMAKE_CURRENT_SOURCE_DIR}/src"
	)
	add_test(NAME bridgeengine.ui_uix COMMAND bridgeengine_ui_uix_test "${CMAKE_CURRENT_BINARY_DIR}")


	add_executable(bridgeengine_ui_attributes_test
		tests/ui_attributes_test.c
		src/ui.c
		src/ui_xml.c
		src/ui_crypt.c
		src/button.c
	)
	target_include_directories(bridgeengine_ui_attributes_test PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
		"${CMAKE_CURRENT_BINARY_DIR}/generated"
		"${CMAKE_CURRENT_SOURCE_DIR}/src"
	)
	add_test(NAME bridgeengine.ui_attributes COMMAND bridgeengine_ui_attributes_test "${CMAKE_CURRENT_BINARY_DIR}")

	add_executable(bridgeengine_ui_tree_edit_test
		tests/ui_tree_edit_test.c
		src/ui.c
		src/ui_xml.c
		src/ui_crypt.c
		src/button.c
	)
	target_include_directories(bridgeengine_ui_tree_edit_test PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
		"${CMAKE_CURRENT_BINARY_DIR}/generated"
		"${CMAKE_CURRENT_SOURCE_DIR}/src"
	)
	add_test(NAME bridgeengine.ui_tree_edit COMMAND bridgeengine_ui_tree_edit_test "${CMAKE_CURRENT_BINARY_DIR}")

	add_executable(bridgeengine_ui_src_test
		tests/ui_src_test.c
		src/ui.c
		src/ui_xml.c
		src/ui_crypt.c
		src/button.c
	)
	target_include_directories(bridgeengine_ui_src_test PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
		"${CMAKE_CURRENT_BINARY_DIR}/generated"
		"${CMAKE_CURRENT_SOURCE_DIR}/src"
	)
	add_test(NAME bridgeengine.ui_src COMMAND bridgeengine_ui_src_test "${CMAKE_CURRENT_BINARY_DIR}")

	add_executable(bridgeengine_ui_interaction_test
		tests/ui_interaction_test.c
		src/ui.c
		src/ui_xml.c
		src/ui_crypt.c
		src/button.c
	)
	target_include_directories(bridgeengine_ui_interaction_test PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
		"${CMAKE_CURRENT_BINARY_DIR}/generated"
		"${CMAKE_CURRENT_SOURCE_DIR}/src"
	)
	add_test(NAME bridgeengine.ui_interaction COMMAND bridgeengine_ui_interaction_test "${CMAKE_CURRENT_BINARY_DIR}")

	add_executable(bridgeengine_texture_test
		tests/texture_test.c
		src/texture.c
		src/io.c
		src/platform/platform.c
	)
	target_include_directories(bridgeengine_texture_test PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
		"${CMAKE_CURRENT_BINARY_DIR}/generated"
		"${CMAKE_CURRENT_SOURCE_DIR}/src"
	)
	add_test(NAME bridgeengine.texture COMMAND bridgeengine_texture_test)

	add_executable(bridgeengine_math_geometry_test
		tests/math_geometry_test.c
		src/math.c
	)
	target_include_directories(bridgeengine_math_geometry_test PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
		"${CMAKE_CURRENT_BINARY_DIR}/generated"
	)
	target_link_libraries(bridgeengine_math_geometry_test PRIVATE m)
	add_test(NAME bridgeengine.math_geometry COMMAND bridgeengine_math_geometry_test)

	add_executable(bridgeengine_sdl_keycode_test
		tests/sdl_keycode_test.c
		src/platform/sdl3/keycode.c
	)
	target_include_directories(bridgeengine_sdl_keycode_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
	add_test(NAME bridgeengine.sdl_keycode COMMAND bridgeengine_sdl_keycode_test)

	add_executable(bridgeengine_input_unknown_key_test
		tests/input_unknown_key_test.c
		src/input/input.c
	)
	target_include_directories(bridgeengine_input_unknown_key_test PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
		"${CMAKE_CURRENT_BINARY_DIR}/generated"
		"${CMAKE_CURRENT_SOURCE_DIR}/src"
	)
	add_test(NAME bridgeengine.input_unknown_key COMMAND bridgeengine_input_unknown_key_test)

	add_executable(bridgeengine_lifecycle_test
		tests/lifecycle_test.c
		src/core/render_context.c
		src/platform/platform.c
	)
	target_include_directories(bridgeengine_lifecycle_test PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
		"${CMAKE_CURRENT_BINARY_DIR}/generated"
		"${CMAKE_CURRENT_SOURCE_DIR}/src"
	)
	add_test(NAME bridgeengine.lifecycle COMMAND bridgeengine_lifecycle_test)

	add_executable(bridgeengine_platform_capabilities_test
		tests/platform_capabilities_test.c
		src/audio.c
		src/io.c
		src/video/video_stub.c
		src/core/init.c
		src/core/render_context.c
		src/platform/platform.c
	)
	target_include_directories(bridgeengine_platform_capabilities_test PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
		"${CMAKE_CURRENT_BINARY_DIR}/generated"
		"${CMAKE_CURRENT_SOURCE_DIR}/src"
	)
	target_link_libraries(bridgeengine_platform_capabilities_test PRIVATE m)
	add_test(NAME bridgeengine.platform_capabilities COMMAND bridgeengine_platform_capabilities_test)

	add_executable(bridgeengine_public_lifecycle_test
		tests/public_lifecycle_test.c
		src/core/init.c
		src/core/render_context.c
		src/platform/platform.c
	)
	target_include_directories(bridgeengine_public_lifecycle_test PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
		"${CMAKE_CURRENT_BINARY_DIR}/generated"
		"${CMAKE_CURRENT_SOURCE_DIR}/src"
	)
	target_link_libraries(bridgeengine_public_lifecycle_test PRIVATE m)
	add_test(NAME bridgeengine.public_lifecycle COMMAND bridgeengine_public_lifecycle_test)

	if(BRIDGEENGINE_BUILD_STATIC)
		set(BRIDGEENGINE_COMPAT_LINK_TARGET bridgeengine_static)
	elseif(BRIDGEENGINE_BUILD_SHARED)
		set(BRIDGEENGINE_COMPAT_LINK_TARGET bridgeengine_shared)
	else()
		message(STATUS "Skipping aggregate compatibility tests: no BridgeEngine library target is enabled")
	endif()
	if(BRIDGEENGINE_COMPAT_LINK_TARGET)
		add_executable(bridgeengine_master_aggregate_bapi_compat_test
			tests/master_aggregate_bapi_compat_test.c
		)
		target_link_libraries(bridgeengine_master_aggregate_bapi_compat_test PRIVATE
			${BRIDGEENGINE_COMPAT_LINK_TARGET}
		)
		add_test(NAME bridgeengine.master_aggregate_bapi_compat
			COMMAND bridgeengine_master_aggregate_bapi_compat_test
		)
		add_executable(bridgeengine_master_aggregate_bapi_compat_cpp_test
			tests/master_aggregate_bapi_compat_test.cpp
		)
		target_link_libraries(bridgeengine_master_aggregate_bapi_compat_cpp_test PRIVATE
			${BRIDGEENGINE_COMPAT_LINK_TARGET}
		)
		add_test(NAME bridgeengine.master_aggregate_bapi_compat_cpp
			COMMAND bridgeengine_master_aggregate_bapi_compat_cpp_test
		)
		if(BRIDGEENGINE_BUILD_SHARED OR BRIDGEENGINE_BUILD_STATIC)
			if(BRIDGEENGINE_BUILD_SHARED AND BRIDGEENGINE_BUILD_STATIC)
				set(BRIDGEENGINE_CONSUMER_DEPENDENCY_MODE shared-static)
			elseif(BRIDGEENGINE_BUILD_SHARED)
				set(BRIDGEENGINE_CONSUMER_DEPENDENCY_MODE shared-only)
			else()
				set(BRIDGEENGINE_CONSUMER_DEPENDENCY_MODE static-only)
			endif()
			set(BRIDGEENGINE_INSTALL_SMOKE_ARGS
				-DBUILD_DIR=${CMAKE_BINARY_DIR}
				-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
				-DCONSUMER_DEPENDENCY_MODE=${BRIDGEENGINE_CONSUMER_DEPENDENCY_MODE}
			)
			if(DEFINED ENV{VCPKG_ROOT} AND EXISTS "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
				if(NOT DEFINED VCPKG_TARGET_TRIPLET)
					set(BRIDGEENGINE_VCPKG_TRIPLET x64-windows)
				else()
					set(BRIDGEENGINE_VCPKG_TRIPLET "${VCPKG_TARGET_TRIPLET}")
				endif()
				list(APPEND BRIDGEENGINE_INSTALL_SMOKE_ARGS
					-DTOOLCHAIN_FILE=$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake
					-DVCPKG_BIN_DIR=$ENV{VCPKG_ROOT}/installed/${BRIDGEENGINE_VCPKG_TRIPLET}/bin
				)
			endif()
			list(APPEND BRIDGEENGINE_INSTALL_SMOKE_ARGS -DGENERATOR=${CMAKE_GENERATOR})
			add_test(NAME bridgeengine.install_tree_smoke
				COMMAND ${CMAKE_COMMAND}
					${BRIDGEENGINE_INSTALL_SMOKE_ARGS}
					-P ${CMAKE_CURRENT_SOURCE_DIR}/tests/install_tree_smoke.cmake
			)
		endif()
	endif()

	add_executable(bridgeengine_resource_shutdown_test
		tests/resource_shutdown_test.c
		src/core/render_context.c
		src/texture.c
		src/audio.c
		src/io.c
		src/video/video_stub.c
		src/platform/platform.c
	)
	target_include_directories(bridgeengine_resource_shutdown_test PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
		"${CMAKE_CURRENT_BINARY_DIR}/generated"
		"${CMAKE_CURRENT_SOURCE_DIR}/src"
	)
	add_test(NAME bridgeengine.resource_shutdown
		COMMAND bridgeengine_resource_shutdown_test
	)

	add_executable(bridgeengine_text_asset_test
		tests/text_asset_test.c
		src/text.c
		src/platform/platform.c
	)
	target_include_directories(bridgeengine_text_asset_test PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
		"${CMAKE_CURRENT_BINARY_DIR}/generated"
		"${CMAKE_CURRENT_SOURCE_DIR}/src"
	)
	add_test(NAME bridgeengine.text_assets COMMAND bridgeengine_text_asset_test)

	add_executable(bridgeengine_text_cache_test
		tests/text_cache_test.c
		src/text.c
		src/core/render_context.c
		src/platform/platform.c
	)
	target_include_directories(bridgeengine_text_cache_test PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
		"${CMAKE_CURRENT_BINARY_DIR}/generated"
		"${CMAKE_CURRENT_SOURCE_DIR}/src"
	)
	# Small limits exercise both eviction paths without allocating large textures.
	target_compile_definitions(bridgeengine_text_cache_test PRIVATE
		BAPI_TEXT_CACHE_CAPACITY=8 BAPI_TEXT_CACHE_BYTES=4096
	)
	if(BRIDGEENGINE_ENABLE_ASAN AND CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
		target_compile_options(bridgeengine_text_cache_test PRIVATE -fsanitize=address -fno-omit-frame-pointer)
		target_link_options(bridgeengine_text_cache_test PRIVATE -fsanitize=address)
	endif()
	add_test(NAME bridgeengine.text_cache COMMAND bridgeengine_text_cache_test)

	add_executable(bridgeengine_log_test
		tests/log_test.c
		src/log.c
		src/platform/platform.c
	)
	target_include_directories(bridgeengine_log_test PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
		"${CMAKE_CURRENT_BINARY_DIR}/generated"
		"${CMAKE_CURRENT_SOURCE_DIR}/src"
	)
	if(BRIDGEENGINE_ENABLE_ASAN AND CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
		target_compile_options(bridgeengine_log_test PRIVATE -fsanitize=address -fno-omit-frame-pointer)
		target_link_options(bridgeengine_log_test PRIVATE -fsanitize=address)
	endif()
	add_test(NAME bridgeengine.log COMMAND bridgeengine_log_test "${CMAKE_CURRENT_BINARY_DIR}/bridgeengine.log")

	add_executable(bridgeengine_file_io_test
		tests/file_io_test.c
		src/io.c
		src/platform/platform.c
	)
	target_include_directories(bridgeengine_file_io_test PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
		"${CMAKE_CURRENT_BINARY_DIR}/generated"
		"${CMAKE_CURRENT_SOURCE_DIR}/src"
	)
	add_test(NAME bridgeengine.file_io COMMAND bridgeengine_file_io_test)

	add_executable(bridgeengine_pack_test
		tests/pack_test.c
		src/pack.c
		thirdparty/rzip/rz_lib.c
	)
	target_include_directories(bridgeengine_pack_test PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
		"${CMAKE_CURRENT_BINARY_DIR}/generated"
		"${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/rzip"
	)
	add_test(NAME bridgeengine.pack COMMAND bridgeengine_pack_test "${CMAKE_CURRENT_BINARY_DIR}")

	add_executable(bridgeengine_pack_index_test
		tests/pack_index_test.c
		src/pack.c
		thirdparty/rzip/rz_lib.c
	)
	target_include_directories(bridgeengine_pack_index_test PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
		"${CMAKE_CURRENT_BINARY_DIR}/generated"
		"${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/rzip"
	)
	if(BRIDGEENGINE_ENABLE_ASAN AND CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
		target_compile_options(bridgeengine_pack_index_test PRIVATE -fsanitize=address -fno-omit-frame-pointer)
		target_link_options(bridgeengine_pack_index_test PRIVATE -fsanitize=address)
	endif()
	add_test(NAME bridgeengine.pack_index
		COMMAND bridgeengine_pack_index_test "${CMAKE_CURRENT_BINARY_DIR}/pack_index.rz"
	)

	add_executable(bridgeengine_resource_mem_test
		tests/resource_mem_test.c
		src/io.c
		src/pack.c
		src/resource.c
		src/audio.c
		src/texture.c
		src/video/video_stub.c
		src/platform/platform.c
		thirdparty/rzip/rz_lib.c
	)
	target_include_directories(bridgeengine_resource_mem_test PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
		"${CMAKE_CURRENT_BINARY_DIR}/generated"
		"${CMAKE_CURRENT_SOURCE_DIR}/src"
		"${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/rzip"
	)
	add_test(NAME bridgeengine.resource_mem
		COMMAND bridgeengine_resource_mem_test "${CMAKE_CURRENT_BINARY_DIR}"
	)

	if(BRIDGEENGINE_BUILD_SHARED OR BRIDGEENGINE_BUILD_STATIC OR BRIDGEENGINE_BUILD_EXAMPLES)
		add_executable(bridgeengine_media_pack_backend_test
			tests/media_pack_backend_test.c
			src/platform/platform.c
			src/platform/sdl3/platform_sdl3.c
			src/platform/sdl3/keycode.c
			src/core/init.c
			src/core/render_context.c
			src/audio.c
			src/texture.c
			src/io.c
			src/resource.c
			src/pack.c
			src/video/video.c
			thirdparty/rzip/rz_lib.c
		)
		bridgeengine_configure_target(bridgeengine_media_pack_backend_test)
		add_test(NAME bridgeengine.media_pack_backend
			COMMAND bridgeengine_media_pack_backend_test
				"${CMAKE_CURRENT_BINARY_DIR}"
				"${CMAKE_CURRENT_SOURCE_DIR}/examples/assets"
		)
	endif()
endif()

if(BRIDGEENGINE_BUILD_SHARED)
	add_library(bridgeengine_shared SHARED ${BRIDGEENGINE_SOURCES})
	set_target_properties(bridgeengine_shared PROPERTIES OUTPUT_NAME bridgeengine)
	if(WIN32)
		set_target_properties(bridgeengine_shared PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
	endif()
	bridgeengine_configure_target(bridgeengine_shared)
	add_library(BridgeEngine::shared ALIAS bridgeengine_shared)
endif()

if(BRIDGEENGINE_BUILD_STATIC)
	add_library(bridgeengine_static STATIC ${BRIDGEENGINE_SOURCES})
	set_target_properties(bridgeengine_static PROPERTIES OUTPUT_NAME bridgeengine)
	if(WIN32 AND BRIDGEENGINE_BUILD_SHARED)
		set_target_properties(bridgeengine_static PROPERTIES
			ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib/static"
		)
	endif()
	bridgeengine_configure_target(bridgeengine_static)
	add_library(BridgeEngine::static ALIAS bridgeengine_static)

	# Build-time .uix encryptor, used by project templates to encrypt ui/*.xml
	# into *.uix when building. Links only the crypto source, not the runtime.
	add_executable(bridgeengine_uix_encrypt
		tools/uix_encrypt.c
		src/ui_crypt.c
	)
	target_include_directories(bridgeengine_uix_encrypt PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
		"${CMAKE_CURRENT_BINARY_DIR}/generated"
	)
endif()

if(BRIDGEENGINE_BUILD_EXAMPLES)
	set(BRIDGEENGINE_EXAMPLE_LINK_TARGET bridgeengine_static)
	if(NOT TARGET bridgeengine_static)
		set(BRIDGEENGINE_EXAMPLE_LINK_TARGET bridgeengine_shared)
	endif()

	add_executable(bridgeengine_desktop_example examples/desktop/main.c)
	target_link_libraries(bridgeengine_desktop_example PRIVATE ${BRIDGEENGINE_EXAMPLE_LINK_TARGET})
	bridgeengine_copy_runtime_assets(bridgeengine_desktop_example)

	add_executable(bridgeengine_text_example examples/text/text_example.c)
	target_link_libraries(bridgeengine_text_example PRIVATE ${BRIDGEENGINE_EXAMPLE_LINK_TARGET})
	bridgeengine_copy_runtime_assets(bridgeengine_text_example)

	add_executable(bridgeengine_log_example examples/log/log_test.c)
	target_link_libraries(bridgeengine_log_example PRIVATE ${BRIDGEENGINE_EXAMPLE_LINK_TARGET})
	bridgeengine_copy_runtime_assets(bridgeengine_log_example)
endif()

if(BRIDGEENGINE_BUILD_EDITOR AND TARGET bridgeengine_static)
	if(NOT TARGET imgui::imgui)
		set(BRIDGEENGINE_IMGUI_SOURCE_DIR "" CACHE PATH "Path to a local Dear ImGui (docking branch) source tree")
		if(BRIDGEENGINE_IMGUI_SOURCE_DIR)
			if(NOT EXISTS "${BRIDGEENGINE_IMGUI_SOURCE_DIR}/imgui.cpp")
				message(FATAL_ERROR
					"BRIDGEENGINE_IMGUI_SOURCE_DIR is set but does not contain imgui.cpp: "
					"${BRIDGEENGINE_IMGUI_SOURCE_DIR}")
			endif()
			set(imgui_SOURCE_DIR "${BRIDGEENGINE_IMGUI_SOURCE_DIR}")
		else()
			if(NOT BRIDGEENGINE_FETCH_IMGUI)
				message(FATAL_ERROR
					"ImGui was not found. Configure with -DBRIDGEENGINE_FETCH_IMGUI=ON to fetch the docking branch.")
			endif()
			FetchContent_Declare(imgui
				GIT_REPOSITORY https://github.com/ocornut/imgui.git
				GIT_TAG v1.92.8-docking
				GIT_SHALLOW TRUE
				${BRIDGEENGINE_FETCHCONTENT_TIMESTAMP_ARGS}
			)
			FetchContent_GetProperties(imgui)
			if(NOT imgui_POPULATED)
				FetchContent_Populate(imgui)
			endif()
		endif()
		# imgui's own CMakeLists calls find_package(SDL3 CONFIG REQUIRED), which breaks
		# when SDL3 comes from FetchContent. Build the sources directly instead.
		add_library(imgui STATIC
			"${imgui_SOURCE_DIR}/imgui.cpp"
			"${imgui_SOURCE_DIR}/imgui_draw.cpp"
			"${imgui_SOURCE_DIR}/imgui_tables.cpp"
			"${imgui_SOURCE_DIR}/imgui_widgets.cpp"
			"${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp"
			"${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp"
		)
		add_library(imgui::imgui ALIAS imgui)
		target_include_directories(imgui PUBLIC
			"${imgui_SOURCE_DIR}"
			"${imgui_SOURCE_DIR}/backends"
		)
		target_link_libraries(imgui PRIVATE "${BRIDGEENGINE_SDL3_TARGET}")
		target_compile_features(imgui PUBLIC cxx_std_11)
	endif()

	set(BRIDGEENGINE_EDITOR_SOURCES
		editor/main.cpp
		editor/document_model.cpp
		editor/commands.cpp
		editor/platform_dialogs.cpp
		editor/project_templates.cpp
		editor/project_file.cpp
		editor/i18n.cpp
		editor/build_run.cpp
		editor/panels/menu.cpp
		editor/panels/documents.cpp
		editor/panels/toolbar.cpp
		editor/panels/viewport.cpp
		editor/panels/palette.cpp
		editor/panels/preview.cpp
		editor/panels/tree.cpp
		editor/panels/properties.cpp
		editor/panels/scenes.cpp
		editor/panels/welcome.cpp
		editor/panels/build_output.cpp
		editor/panels/context_menu.cpp
		editor/panels/history.cpp
		editor/templates.cpp
		editor/titlebar.cpp
	)
	add_executable(bridgeengine_editor ${BRIDGEENGINE_EDITOR_SOURCES})
	target_include_directories(bridgeengine_editor PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
		"${CMAKE_CURRENT_SOURCE_DIR}/src"
		"${CMAKE_CURRENT_BINARY_DIR}/generated"
	)
	target_compile_definitions(bridgeengine_editor PRIVATE BAPI_LOG_ENABLED)
	string(REPLACE "\\" "/" BRIDGEENGINE_SOURCE_DIR_FORWARD "${CMAKE_CURRENT_SOURCE_DIR}")
	target_compile_definitions(bridgeengine_editor PRIVATE
		BRIDGEENGINE_SOURCE_DIR="${BRIDGEENGINE_SOURCE_DIR_FORWARD}"
	)
	target_compile_features(bridgeengine_editor PRIVATE cxx_std_14)
	if(MSVC)
		target_compile_options(bridgeengine_editor PRIVATE /utf-8 /W4)
	endif()
	if(WIN32)
		target_link_libraries(bridgeengine_editor PRIVATE comdlg32 shell32)
	endif()
	target_link_libraries(bridgeengine_editor PRIVATE
		bridgeengine_static
		imgui::imgui
		${BRIDGEENGINE_PLATFORM_LIBS}
	)
	bridgeengine_copy_runtime_assets(bridgeengine_editor)
endif()

if(BRIDGEENGINE_BUILD_XJ380)
	set(XJ380_SDK "${XJ380_SDK}" CACHE PATH "Path to XJ380_CPP_API_Depend_1_3")
	if(NOT XJ380_SDK)
		if(NOT BRIDGEENGINE_FETCH_XJ380)
			message(FATAL_ERROR
				"XJ380_SDK is not set. Set it to XJ380_CPP_API_Depend_1_3 or configure with "
				"-DBRIDGEENGINE_FETCH_XJ380=ON.")
		endif()
		FetchContent_Declare(xj380_sdk
			URL https://github.com/xingji-studio/XXCC-suite/releases/download/2026v4/XJ380_CPP_API_Depend_1_3.zip
			URL_HASH SHA256=5c3ba1c31393c19ddc5a3ad52618474c156924895c32edc9729743b2ea9fff28
			${BRIDGEENGINE_FETCHCONTENT_TIMESTAMP_ARGS}
		)
		FetchContent_GetProperties(xj380_sdk)
		if(NOT xj380_sdk_POPULATED)
			FetchContent_Populate(xj380_sdk)
		endif()
		set(XJ380_SDK "${xj380_sdk_SOURCE_DIR}" CACHE PATH
			"Path to XJ380_CPP_API_Depend_1_3" FORCE)
	endif()
	set(XJ380_CC clang CACHE FILEPATH "XJ380 C compiler")
	set(XJ380_CXX clang++ CACHE FILEPATH "XJ380 C++ compiler")
	set(XJ380_LD ld CACHE FILEPATH "XJ380 linker")

	set(XJ380_C_SOURCES
		src/platform/platform.c
		src/core/render_context.c
		src/core/init.c
		src/core/version.c
		src/io.c
		src/pack_stub.c
		src/resource.c
		src/log.c
		src/draw.c
		src/input/mouse_drawing.c
		src/text.c
		src/math.c
		src/texture.c
		src/input/input.c
		src/camera.c
		src/button.c
		src/scene.c
		src/level.c
		src/xml_loader.c
		src/project.c
		src/ui.c
		src/ui_xml.c
		src/audio.c
		src/video/video_stub.c
	)
	set(XJ380_CPP_SOURCES src/platform/xj380/platform_xj380.cpp)
	set(XJ380_OBJECTS)

	set(XJ380_COMMON_FLAGS
		-ffreestanding -fno-builtin -m64 -fno-stack-protector -fshort-wchar
		-nostdinc -O -g
		-I${CMAKE_CURRENT_SOURCE_DIR}/include
		-I${CMAKE_CURRENT_BINARY_DIR}/generated
		-I${CMAKE_CURRENT_SOURCE_DIR}/src
		-I${XJ380_SDK}/include
		-I${XJ380_SDK}/include/xposix
		-DUSE_BACKEND_XJ380 -DBAPI_LOG_ENABLED -D__XJ380_OS__ -DXJ380_OS
		-Dcosf=cos -Dsinf=sin -Dsqrtf=sqrt
		-Dabs=__xj380_abs
	)

	add_custom_target(check_xj380_sdk
		COMMAND ${CMAKE_COMMAND} -DXJ380_SDK=${XJ380_SDK} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckXJ380Sdk.cmake
	)

	foreach(source IN LISTS XJ380_C_SOURCES)
		set(object "${CMAKE_CURRENT_BINARY_DIR}/xj380/${source}.xj380.o")
		get_filename_component(object_dir "${object}" DIRECTORY)
		add_custom_command(
			OUTPUT "${object}"
			COMMAND ${CMAKE_COMMAND} -E make_directory "${object_dir}"
			COMMAND ${XJ380_CC} ${XJ380_COMMON_FLAGS} -std=c11 -c -o "${object}" "${CMAKE_CURRENT_SOURCE_DIR}/${source}"
			DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${source}" check_xj380_sdk
			VERBATIM
		)
		list(APPEND XJ380_OBJECTS "${object}")
	endforeach()

	foreach(source IN LISTS XJ380_CPP_SOURCES)
		set(object "${CMAKE_CURRENT_BINARY_DIR}/xj380/${source}.xj380.o")
		get_filename_component(object_dir "${object}" DIRECTORY)
		add_custom_command(
			OUTPUT "${object}"
			COMMAND ${CMAKE_COMMAND} -E make_directory "${object_dir}"
			COMMAND ${XJ380_CXX} ${XJ380_COMMON_FLAGS} -std=c++11 -fno-exceptions -c -o "${object}" "${CMAKE_CURRENT_SOURCE_DIR}/${source}"
			DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${source}" check_xj380_sdk
			VERBATIM
		)
		list(APPEND XJ380_OBJECTS "${object}")
	endforeach()

	add_custom_command(
		OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/libbridgeengine_xj380.a"
		COMMAND ${CMAKE_AR} cr "${CMAKE_CURRENT_BINARY_DIR}/libbridgeengine_xj380.a" ${XJ380_OBJECTS}
		DEPENDS ${XJ380_OBJECTS}
		VERBATIM
	)
	add_custom_target(xj380_staticlib DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/libbridgeengine_xj380.a")

	set(XJ380_PACKAGE_DIR "${CMAKE_CURRENT_BINARY_DIR}/package")
	set(XJ380_PACKAGE_HEADERS
		"${XJ380_PACKAGE_DIR}/include/BridgeEngine.h"
		"${XJ380_PACKAGE_DIR}/include/bridgeengine_version.h"
		"${XJ380_PACKAGE_DIR}/include/xposix/stddef.h"
	)
	set(XJ380_PACKAGE_LIBRARY "${XJ380_PACKAGE_DIR}/lib/libbridgeengine.a")
	add_custom_command(
		OUTPUT ${XJ380_PACKAGE_HEADERS} "${XJ380_PACKAGE_LIBRARY}"
		COMMAND ${CMAKE_COMMAND} -E rm -rf "${XJ380_PACKAGE_DIR}/include"
		COMMAND ${CMAKE_COMMAND} -E make_directory "${XJ380_PACKAGE_DIR}/include"
		COMMAND ${CMAKE_COMMAND} -E make_directory "${XJ380_PACKAGE_DIR}/include/xposix"
		COMMAND ${CMAKE_COMMAND} -E make_directory "${XJ380_PACKAGE_DIR}/lib"
		COMMAND ${CMAKE_COMMAND} -E copy_directory
			"${CMAKE_CURRENT_SOURCE_DIR}/include"
			"${XJ380_PACKAGE_DIR}/include"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			"${CMAKE_CURRENT_SOURCE_DIR}/include/BridgeEngine.h"
			"${XJ380_PACKAGE_DIR}/include/BridgeEngine.h"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			"${CMAKE_CURRENT_BINARY_DIR}/generated/bridgeengine_version.h"
			"${XJ380_PACKAGE_DIR}/include/bridgeengine_version.h"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			"${CMAKE_CURRENT_BINARY_DIR}/generated/xposix/stddef.h"
			"${XJ380_PACKAGE_DIR}/include/xposix/stddef.h"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			"${CMAKE_CURRENT_BINARY_DIR}/libbridgeengine_xj380.a"
			"${XJ380_PACKAGE_LIBRARY}"
		DEPENDS xj380_staticlib
			"${CMAKE_CURRENT_SOURCE_DIR}/include/BridgeEngine.h"
			"${CMAKE_CURRENT_BINARY_DIR}/generated/bridgeengine_version.h"
			"${CMAKE_CURRENT_BINARY_DIR}/generated/xposix/stddef.h"
			"${CMAKE_CURRENT_BINARY_DIR}/libbridgeengine_xj380.a"
		VERBATIM
	)
	add_custom_target(xj380_package DEPENDS ${XJ380_PACKAGE_HEADERS} "${XJ380_PACKAGE_LIBRARY}")

	file(GLOB_RECURSE XJ380_XAPI_OBJS
		"${XJ380_SDK}/obj-gui/*.o"
		"${XJ380_SDK}/obj-gui/*/*.o"
		"${XJ380_SDK}/obj-gui/*/*/*.o"
	)
	list(APPEND XJ380_XAPI_OBJS "${XJ380_SDK}/obj-gui/liballoc-x86_64.a")

	add_custom_command(
		OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xj380/xj380_main.xj380.o"
		COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/xj380"
		COMMAND ${XJ380_CC} ${XJ380_COMMON_FLAGS} -std=c11 -c -o "${CMAKE_CURRENT_BINARY_DIR}/xj380/xj380_main.xj380.o" "${CMAKE_CURRENT_SOURCE_DIR}/examples/xj380/xj380_main.c"
		DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/examples/xj380/xj380_main.c" check_xj380_sdk
		VERBATIM
	)
	add_custom_command(
		OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/bridgeengine_demo.epf"
		COMMAND ${XJ380_LD} -Ttext=0x200000 ${XJ380_XAPI_OBJS} "${CMAKE_CURRENT_BINARY_DIR}/xj380/xj380_main.xj380.o" "${CMAKE_CURRENT_BINARY_DIR}/libbridgeengine_xj380.a" -o "${CMAKE_CURRENT_BINARY_DIR}/bridgeengine_demo.epf"
		DEPENDS xj380_staticlib "${CMAKE_CURRENT_BINARY_DIR}/xj380/xj380_main.xj380.o"
		VERBATIM
	)
	add_custom_target(xj380_epf DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/bridgeengine_demo.epf")
endif()

set(BRIDGEENGINE_INSTALL_TARGETS)
if(TARGET bridgeengine_shared)
	list(APPEND BRIDGEENGINE_INSTALL_TARGETS bridgeengine_shared)
endif()
if(TARGET bridgeengine_static)
	list(APPEND BRIDGEENGINE_INSTALL_TARGETS bridgeengine_static)
endif()
if(BRIDGEENGINE_INSTALL_TARGETS)
	if(TARGET bridgeengine_shared)
		install(TARGETS bridgeengine_shared
			EXPORT BridgeEngineTargets
			ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
			LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
			RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
		)
	endif()
	if(TARGET bridgeengine_static)
		set(BRIDGEENGINE_STATIC_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR})
		if(WIN32 AND TARGET bridgeengine_shared)
			set(BRIDGEENGINE_STATIC_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR}/static)
		endif()
		install(TARGETS bridgeengine_static
		EXPORT BridgeEngineTargets
			ARCHIVE DESTINATION ${BRIDGEENGINE_STATIC_INSTALL_DIR}
		)
	endif()
	configure_package_config_file(
		"${CMAKE_CURRENT_SOURCE_DIR}/cmake/BridgeEngineConfig.cmake.in"
		"${CMAKE_CURRENT_BINARY_DIR}/BridgeEngineConfig.cmake"
		INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/BridgeEngine"
	)
	write_basic_package_version_file(
		"${CMAKE_CURRENT_BINARY_DIR}/BridgeEngineConfigVersion.cmake"
		VERSION ${PROJECT_VERSION}
		COMPATIBILITY SameMajorVersion
	)
	install(EXPORT BridgeEngineTargets
		NAMESPACE BridgeEngine::
		DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/BridgeEngine"
	)
	install(FILES
		"${CMAKE_CURRENT_BINARY_DIR}/BridgeEngineConfig.cmake"
		"${CMAKE_CURRENT_BINARY_DIR}/BridgeEngineConfigVersion.cmake"
		"${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindFFmpeg.cmake"
		DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/BridgeEngine"
	)
endif()
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/" DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
	FILES_MATCHING PATTERN "*.h"
)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/generated/bridgeengine_version.h" DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
