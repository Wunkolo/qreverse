cmake_minimum_required( VERSION 3.2.2 )
project( qreversetest )

set( CMAKE_CXX_STANDARD 11 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )
set( CMAKE_CXX_EXTENSIONS OFF )

set( CMAKE_COLOR_MAKEFILE ON )
set( CMAKE_VERBOSE_MAKEFILE ON )

### Optimizations
if( MSVC )
	add_compile_options( /Oxs )
	add_compile_options( /arch:AVX2 )
	add_compile_options( /W3 )
elseif( CMAKE_COMPILER_IS_GNUCXX )
	add_compile_options( -m64 )
	add_compile_options( -march=native )
	add_compile_options( -Ofast )
	add_compile_options( -Wall )
	add_compile_options( -Wextra )
endif()

### Tests
enable_testing()

set(
	ElementSizes
	1 2 4 8 16
)

set(
	ElementCounts
	2 5 10 17 32 100
)

# Create tests for each element size
foreach( ElementSize ${ElementSizes})
	add_executable(
		"Verify${ElementSize}"
		tests/verify.cpp
	)
	target_include_directories(
		"Verify${ElementSize}"
		PRIVATE
		include
	)
	target_compile_definitions(
		"Verify${ElementSize}"
		PRIVATE
		-DELEMENTSIZE=${ElementSize}
	)
	# Add tests for each element size
	foreach( ElementCount ${ElementCounts})
		add_test(
			NAME "Verify${ElementSize}-${ElementCount}"
			COMMAND "Verify${ElementSize}" ${ElementCount}
		)
	endforeach( ElementCount )
endforeach( ElementSize )

# Benchmark
add_executable(
	Benchmark
	tests/benchmark.cpp
)
target_include_directories(
	Benchmark
	PRIVATE
	include
)

add_test(
	NAME Benchmark
	COMMAND Benchmark
)
