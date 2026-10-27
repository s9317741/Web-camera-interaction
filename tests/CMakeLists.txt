cmake_minimum_required(VERSION 3.16)
project(WebcamInteractionCoreTests CXX)

# Tests for the VCL-free modules in src/core. The application itself is built
# with C++Builder from WebcamInteraction.cbproj, not with CMake.

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../src/core)

add_executable(core_tests
	Main.cpp
	Yuy2FrameTest.cpp
	ChangeDetectorTest.cpp
	SpriteTest.cpp
	EffectChainTest.cpp
	${CORE_DIR}/ColorSpace.cpp
	${CORE_DIR}/Yuy2Frame.cpp
	${CORE_DIR}/ChangeDetector.cpp
	${CORE_DIR}/Sprite.cpp
	${CORE_DIR}/EffectChain.cpp
	${CORE_DIR}/Effects.cpp
)
target_include_directories(core_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
	target_compile_options(core_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME core_tests COMMAND core_tests)
