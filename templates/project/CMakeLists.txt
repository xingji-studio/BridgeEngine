cmake_minimum_required(VERSION 3.20)
project(__PROJECT_NAME__ LANGUAGES C)

# ---------------------------------------------------------------------------
# BridgeEngine 的两种引用方式：
#
#   A) 指定引擎源码目录（推荐）：把 BRIDGEENGINE_SOURCE_DIR 填成引擎源码路径
#      （或留空后在 cmake 命令行 -DBRIDGEENGINE_SOURCE_DIR=... 传入），直接
#      add_subdirectory，无需先安装引擎。
#   B) 使用已安装的引擎包：先执行 cmake --install build --prefix <前缀>，
#      再配置本工程时加 -DCMAKE_PREFIX_PATH=<前缀>。BRIDGEENGINE_SOURCE_DIR
#      为空时走 find_package。
# ---------------------------------------------------------------------------
set(BRIDGEENGINE_SOURCE_DIR "__ENGINE_DIR__" CACHE PATH "BridgeEngine source directory")
if(NOT BRIDGEENGINE_SOURCE_DIR)
	find_package(BridgeEngine CONFIG REQUIRED)
	set(BRIDGEENGINE_LINK_TARGET BridgeEngine::bridgeengine_static)
else()
	# Only the engine static library is needed here; keep its optional
	# subprojects (UI Kit, examples, tests, shared lib, XJ380) out of the build.
	set(BRIDGEENGINE_BUILD_SHARED OFF CACHE BOOL "" FORCE)
	set(BRIDGEENGINE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
	set(BRIDGEENGINE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
	set(BRIDGEENGINE_BUILD_UIKIT OFF CACHE BOOL "" FORCE)
	set(BRIDGEENGINE_BUILD_CLI OFF CACHE BOOL "" FORCE)
	set(BRIDGEENGINE_BUILD_XJ380 OFF CACHE BOOL "" FORCE)
	add_subdirectory(${BRIDGEENGINE_SOURCE_DIR} bridgeengine)
	set(BRIDGEENGINE_LINK_TARGET bridgeengine_static)
endif()

add_executable(__EXEC_NAME__ main.c)
target_link_libraries(__EXEC_NAME__ PRIVATE ${BRIDGEENGINE_LINK_TARGET})

# 运行时需要 assets/、ui/ 与 <项目名>.bep（默认字体、场景 xml 与项目描述），
# 构建后自动复制到 exe 旁，保证运行时按相对路径加载。
#
# UI 场景文档采用「源码明文、构建加密」：仓库里 ui/*.xml 保持可编辑，构建时
# 由 bridgeengine_uix_encrypt 统一加密后输出到产物目录（内容加密、文件名仍为
# *.xml，运行时按内容自动解密），因此最终发布的 exe 旁只有密文，源码树中的
# 明文不会被带上。密钥来自项目根目录的 .uixkey 文件（编辑器在密钥设置界面点
# 「应用」或「File > Save Project」时写入；不在则构建不加密，产物复制明文 xml）。
# 也可用命令行 -DBRIDGEENGINE_UI_KEY=... 显式覆盖。运行时用同一密钥加载。
set(BRIDGEENGINE_UI_KEY "" CACHE STRING "Key used to encrypt ui/*.xml at build time")
if(NOT BRIDGEENGINE_UI_KEY)
	if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.uixkey")
		file(READ "${CMAKE_CURRENT_SOURCE_DIR}/.uixkey" _uixkey_contents)
		string(STRIP "${_uixkey_contents}" BRIDGEENGINE_UI_KEY)
	endif()
endif()
if(BRIDGEENGINE_UI_KEY)
	target_compile_definitions(__EXEC_NAME__ PRIVATE
		BRIDGEENGINE_UI_KEY="${BRIDGEENGINE_UI_KEY}"
	)
endif()

add_custom_command(TARGET __EXEC_NAME__ POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		"${CMAKE_CURRENT_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:__EXEC_NAME__>/assets"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${CMAKE_CURRENT_SOURCE_DIR}/__PROJECT_NAME__.bep" "$<TARGET_FILE_DIR:__EXEC_NAME__>/__PROJECT_NAME__.bep"
)
if(BRIDGEENGINE_UI_KEY)
	# Encrypt ui/*.xml -> *.uix into the output ui/ dir (recursive).
	add_custom_command(TARGET __EXEC_NAME__ POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E rm -rf "$<TARGET_FILE_DIR:__EXEC_NAME__>/ui"
		COMMAND bridgeengine_uix_encrypt "${BRIDGEENGINE_UI_KEY}"
			"${CMAKE_CURRENT_SOURCE_DIR}/ui" "$<TARGET_FILE_DIR:__EXEC_NAME__>/ui"
		COMMENT "Encrypting UI documents with BRIDGEENGINE_UI_KEY"
	)
else()
	add_custom_command(TARGET __EXEC_NAME__ POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory
			"${CMAKE_CURRENT_SOURCE_DIR}/ui" "$<TARGET_FILE_DIR:__EXEC_NAME__>/ui"
	)
endif()
