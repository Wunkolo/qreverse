cmake_minimum_required( VERSION 3.2.2 )
project( SimdRev )

set( CMAKE_CXX_STANDARD 14 )
set( CMAKE_COLOR_MAKEFILE ON )
set( CMAKE_VERBOSE_MAKEFILE ON )

add_executable(
	qreversetest
	main.cpp
)

#set( CMAKE_BUILD_TYPE Debug )

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	add_definitions( -m64 )
	add_definitions( -march=skylake )
	add_definitions( -Ofast )
	add_definitions( -Wall )
	add_definitions( -Wextra )
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
	add_definitions( /arch:AVX2 )
	add_definitions( /O2 )
	add_definitions( /MD )
endif()
