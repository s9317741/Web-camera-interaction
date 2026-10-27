cmake_minimum_required(VERSION 3.21)
project(WebcamInteraction LANGUAGES C CXX)

option(WEBCAM_BUILD_APP "Build the OpenCV + Dear ImGui application" ON)
option(WEBCAM_BUILD_TESTS "Build the tests for src/core" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	# Background detection is far too slow unoptimized; see README.
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

function(webcam_warnings target)
	if(MSVC)
		target_compile_options(${target} PRIVATE /W4 /utf-8)
	else()
		target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
	endif()
endfunction()

# --- Frame processing, no third-party dependencies -------------------------

add_library(webcam_core STATIC
	src/core/Yuy2Frame.cpp
	src/core/ChangeDetector.cpp
	src/core/Sprite.cpp
	src/core/EffectChain.cpp
	src/core/Effects.cpp
)
target_include_directories(webcam_core PUBLIC src)
webcam_warnings(webcam_core)

if(WEBCAM_BUILD_TESTS)
	enable_testing()
	add_subdirectory(tests)
endif()

if(NOT WEBCAM_BUILD_APP)
	return()
endif()

# --- Application -------------------------------------------------------------

# Windows: the official prebuilt package unpacked into third_party/ (README).
if(NOT OpenCV_DIR AND EXISTS "${CMAKE_SOURCE_DIR}/third_party/opencv/build/OpenCVConfig.cmake")
	set(OpenCV_DIR "${CMAKE_SOURCE_DIR}/third_party/opencv/build")
endif()
# The prebuilt package's config only knows MSVC up to 19.4x; newer toolsets
# are ABI compatible with its vc16 binaries.
if(MSVC AND MSVC_VERSION GREATER_EQUAL 1950 AND NOT DEFINED OpenCV_RUNTIME)
	set(OpenCV_RUNTIME vc16)
	set(OpenCV_ARCH x64)
endif()
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs videoio dnn objdetect)

# AI models from the OpenCV Zoo: person segmentation (PP-HumanSeg, Apache-2.0)
# and face detection (YuNet, MIT). Without them the app falls back to
# learning the background and has no face stickers.
option(WEBCAM_DOWNLOAD_MODELS "Download the AI models when configuring" ON)
set(WEBCAM_MODEL_DIR "${CMAKE_BINARY_DIR}/models")
set(WEBCAM_MODELS
	"human_segmentation_pphumanseg/human_segmentation_pphumanseg_2023mar.onnx|552d8a984054e59b5d773d24b9b12022b22046ceb2bbc4c9aaeaceb36a9ddf24"
	"face_detection_yunet/face_detection_yunet_2023mar.onnx|8f2383e4dd3cfbb4553ea8718107fc0423210dc964f9f4280604804ed2552fa4")
if(WEBCAM_DOWNLOAD_MODELS)
	foreach(entry IN LISTS WEBCAM_MODELS)
		string(REPLACE "|" ";" parts "${entry}")
		list(GET parts 0 path)
		list(GET parts 1 hash)
		get_filename_component(name "${path}" NAME)
		if(NOT EXISTS "${WEBCAM_MODEL_DIR}/${name}")
			message(STATUS "Downloading ${name}")
			file(DOWNLOAD "https://media.githubusercontent.com/media/opencv/opencv_zoo/main/models/${path}"
				"${WEBCAM_MODEL_DIR}/${name}" STATUS status)
			list(GET status 0 code)
			file(SHA256 "${WEBCAM_MODEL_DIR}/${name}" actual)
			if(NOT code EQUAL 0 OR NOT actual STREQUAL hash)
				message(WARNING "Could not download ${name}; AI features will be off. (${status})")
				file(REMOVE "${WEBCAM_MODEL_DIR}/${name}")
			endif()
		endif()
	endforeach()
endif()
find_package(OpenGL REQUIRED)

include(FetchContent)
set(FETCHCONTENT_QUIET ON)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(glfw
	GIT_REPOSITORY https://github.com/glfw/glfw.git
	GIT_TAG 3.5.1
	GIT_SHALLOW TRUE)

# Dear ImGui and miniaudio are only fetched; their sources are compiled below.
FetchContent_Declare(imgui
	GIT_REPOSITORY https://github.com/ocornut/imgui.git
	GIT_TAG v1.93.0
	GIT_SHALLOW TRUE)
FetchContent_Declare(miniaudio
	GIT_REPOSITORY https://github.com/mackron/miniaudio.git
	GIT_TAG 0.11.25
	GIT_SHALLOW TRUE
	SOURCE_SUBDIR no-cmake-build)

FetchContent_MakeAvailable(glfw imgui miniaudio)

add_library(imgui STATIC
	${imgui_SOURCE_DIR}/imgui.cpp
	${imgui_SOURCE_DIR}/imgui_draw.cpp
	${imgui_SOURCE_DIR}/imgui_tables.cpp
	${imgui_SOURCE_DIR}/imgui_widgets.cpp
	${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
	${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui PUBLIC glfw OpenGL::GL)

# Compiled on its own so its warnings stay out of ours.
add_library(miniaudio STATIC ${miniaudio_SOURCE_DIR}/miniaudio.c)
target_include_directories(miniaudio PUBLIC ${miniaudio_SOURCE_DIR})
if(APPLE)
	target_link_libraries(miniaudio PUBLIC "-framework CoreFoundation" "-framework CoreAudio" "-framework AudioToolbox")
elseif(UNIX)
	target_link_libraries(miniaudio PUBLIC ${CMAKE_DL_LIBS} pthread m)
endif()

add_executable(WebcamInteraction
	src/app/main.cpp
	src/app/App.cpp
	src/app/Assets.cpp
	src/app/Camera.cpp
	src/app/Settings.cpp
	src/app/Sound.cpp
	src/app/Stickers.cpp
	src/app/Style.cpp
	src/app/Vision.cpp
	src/app/Movie.cpp
	src/app/Music.cpp
	src/app/OutputWindow.cpp
	src/app/Platform.cpp
	src/app/ScreenGrab.cpp
	src/app/Texture.cpp
)
target_link_libraries(WebcamInteraction PRIVATE webcam_core imgui miniaudio ${OpenCV_LIBS})
if(WIN32)
	target_link_libraries(WebcamInteraction PRIVATE dwmapi) # window bounds for screen grabbing
endif()
target_compile_definitions(WebcamInteraction PRIVATE
	WEBCAM_SOURCE_ASSETS="${CMAKE_SOURCE_DIR}/assets")
webcam_warnings(WebcamInteraction)
if(APPLE)
	# OpenGL is deprecated on macOS but still works.
	target_compile_definitions(WebcamInteraction PRIVATE GL_SILENCE_DEPRECATION)
endif()

# Pictures and models are read from an assets folder next to the executable.
add_custom_command(TARGET WebcamInteraction POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:WebcamInteraction>/assets
	COMMAND ${CMAKE_COMMAND} -E make_directory ${WEBCAM_MODEL_DIR}
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		${WEBCAM_MODEL_DIR} $<TARGET_FILE_DIR:WebcamInteraction>/assets/models)

# Windows needs the OpenCV DLLs beside the executable.
if(WIN32)
	add_custom_command(TARGET WebcamInteraction POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			$<TARGET_RUNTIME_DLLS:WebcamInteraction> $<TARGET_FILE_DIR:WebcamInteraction>
		COMMAND_EXPAND_LISTS)
	# OpenCV loads FFmpeg as a plugin at run time, so it is not among the
	# runtime DLLs above; without it no MP4 can be read or written.
	list(GET OpenCV_LIBS 0 _opencv_first)
	get_target_property(_opencv_dll ${_opencv_first} IMPORTED_LOCATION_RELEASE)
	if(_opencv_dll)
		get_filename_component(_opencv_bin "${_opencv_dll}" DIRECTORY)
		file(GLOB _opencv_ffmpeg "${_opencv_bin}/opencv_videoio_ffmpeg*.dll")
		if(_opencv_ffmpeg)
			add_custom_command(TARGET WebcamInteraction POST_BUILD
				COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_opencv_ffmpeg} $<TARGET_FILE_DIR:WebcamInteraction>)
		else()
			message(WARNING "OpenCV's FFmpeg plugin was not found in ${_opencv_bin}: MP4 videos will not open")
		endif()
	endif()
endif()
