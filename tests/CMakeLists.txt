# Tests for src/core. Built from the top-level CMakeLists.txt; they need no
# third-party libraries, so -DWEBCAM_BUILD_APP=OFF builds them without OpenCV.

add_executable(core_tests
	Main.cpp
	Yuy2FrameTest.cpp
	ChangeDetectorTest.cpp
	SpriteTest.cpp
	EffectChainTest.cpp
)
target_link_libraries(core_tests PRIVATE webcam_core)
target_include_directories(core_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
webcam_warnings(core_tests)

add_test(NAME core_tests COMMAND core_tests)
