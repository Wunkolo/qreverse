#include <cstddef>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <iomanip>

#include <algorithm>
#include <numeric>
#include <array>
#include <vector>
#include <functional>

#include <chrono>

#include <qreverse.hpp>

/*
For use with cmake:
	Benchmarks qreverse against std::reverse at the designated
	compile-time element size.

	Define ELEMENTSIZE preprocessor value to adjust verified element size
*/

#ifndef ELEMENTSIZE
#define ELEMENTSIZE 1
#endif

#define TRIALCOUNT 10000

template<typename TimeT = std::chrono::nanoseconds>
struct Measure
{
	template<typename F, typename ...Args>
	static typename TimeT::rep Execute(F&& func, Args&&... args)
	{
		auto start = std::chrono::high_resolution_clock::now();
		std::forward<decltype(func)>(func)(std::forward<Args>(args)...);
		auto duration = std::chrono::duration_cast<TimeT>(
			std::chrono::high_resolution_clock::now() - start
		);
		return duration.count();
	}

	template<typename F, typename ...Args>
	static TimeT Duration(F&& func, Args&&... args)
	{
		auto start = std::chrono::high_resolution_clock::now();
		std::forward<decltype(func)>(func)(std::forward<Args>(args)...);
		return std::chrono::duration_cast<TimeT>(
			std::chrono::high_resolution_clock::now() - start
		);
	}
};

template< std::size_t ElementSize, std::size_t Count >
void Bench()
{
	std::size_t SpeedStd = 0;
	std::size_t SpeedQrev = 0;

	// Compile-time generic structure used to benchmark an AoS of this size
	struct ElementType
	{
		std::uint8_t u8[ElementSize];
	};

	// If compiler adds any padding/alignment bytes(and some do) then assert out
	static_assert(
		sizeof(ElementType) == ElementSize,
		"ElementSize is pad-aligned and does not match specified element size"
	);

	std::vector<ElementType> Array(Count);

	std::chrono::nanoseconds Duration;

	/// std::reverse
	Duration = std::chrono::nanoseconds::zero();
	for( std::size_t i = 0; i < TRIALCOUNT; i++ )
	{
		Duration += Measure<>::Duration(
			std::reverse<decltype(Array.begin())>,
			Array.begin(),
			Array.end()
		);
	}
	Duration /= TRIALCOUNT;
	SpeedStd = Duration.count();

	/// qreverse
	Duration = std::chrono::nanoseconds::zero();
	for( std::size_t i = 0; i < TRIALCOUNT; i++ )
	{
		Duration += Measure<>::Duration(
			qReverse<sizeof(ElementType)>,
			Array.data(),
			Array.size()
		);
	}
	Duration /= TRIALCOUNT;
	SpeedQrev = Duration.count();

	std::double_t SpeedUp =
		SpeedStd / static_cast<std::double_t>(SpeedQrev);

	std::cout
		<< Count << '|'
		<< SpeedStd << " ns|"
		<< SpeedQrev << " ns|"
		<< (SpeedUp > 1.0 ? "**" : "*")
		<< SpeedUp
		<< (SpeedUp > 1.0 ? "**" : "*")
		<< std::endl;

	return;
}

int main()
{
	/// Benchmark
	std::cout << std::fixed << std::setprecision(3);

	std::cout
		<< "Element Count" << '|'
		<< "std::reverse" << '|'
		<< "qReverse" << '|'
		<< "Speedup Factor"
		<< std::endl;

	std::cout
		<< "---|" // Element count
		<< "---|" // std::reverse
		<< "---|" // qReverse
		<< "---" // Speedup
		<< std::endl;

	// Powers of two
	Bench< ELEMENTSIZE, 8 >();
	Bench< ELEMENTSIZE, 16 >();
	Bench< ELEMENTSIZE, 32 >();
	Bench< ELEMENTSIZE, 64 >();
	Bench< ELEMENTSIZE, 128 >();
	Bench< ELEMENTSIZE, 256 >();
	Bench< ELEMENTSIZE, 512 >();
	Bench< ELEMENTSIZE, 1024 >();

	// Powers of ten
	Bench< ELEMENTSIZE, 100 >();
	Bench< ELEMENTSIZE, 1000 >();
	Bench< ELEMENTSIZE, 10000 >();
	Bench< ELEMENTSIZE, 100000 >();
	Bench< ELEMENTSIZE, 1000000 >();

	// Primes
	Bench< ELEMENTSIZE, 59 >();
	Bench< ELEMENTSIZE, 79 >();
	Bench< ELEMENTSIZE, 173 >();
	Bench< ELEMENTSIZE, 6133 >();
	Bench< ELEMENTSIZE, 10177 >();
	Bench< ELEMENTSIZE, 25253 >();
	Bench< ELEMENTSIZE, 31391 >();
	Bench< ELEMENTSIZE, 50432 >();

	return EXIT_SUCCESS;
}