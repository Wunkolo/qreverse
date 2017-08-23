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

using ElementType = std::uint8_t;

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
		"ElementSize is pad-aligned and does not match element size"
	);

	std::vector<ElementType> Array(Count);

#define TRIALCOUNT 10000

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
		<< (SpeedUp > 1.0 ? "**" : "")
		<< SpeedUp
		<< (SpeedUp > 1.0 ? "**" : "")
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
	Bench<1, 8>();
	Bench<1, 16>();
	Bench<1, 32>();
	Bench<1, 64>();
	Bench<1, 128>();
	Bench<1, 256>();
	Bench<1, 512>();
	Bench<1, 1024>();

	// Powers of ten
	Bench<1, 100>();
	Bench<1, 1000>();
	Bench<1, 10000>();
	Bench<1, 100000>();
	Bench<1, 1000000>();

	// Primes
	Bench<1, 59>();
	Bench<1, 79>();
	Bench<1, 173>();
	Bench<1, 6133>();
	Bench<1, 10177>();
	Bench<1, 25253>();
	Bench<1, 31391>();
	Bench<1, 50432>();

	return EXIT_SUCCESS;
}