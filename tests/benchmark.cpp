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
constexpr std::size_t ElementCount = 255;

void PrintArray(const std::array<ElementType, ElementCount>& Array);

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
	static auto Duration(F&& func, Args&&... args)
	{
		auto start = std::chrono::high_resolution_clock::now();
		std::forward<decltype(func)>(func)(std::forward<Args>(args)...);
		return std::chrono::duration_cast<TimeT>(
			std::chrono::high_resolution_clock::now() - start
		);
	}
};

template< typename ElementType, std::size_t Count >
void Bench()
{
	std::size_t SpeedStd = 0;
	std::size_t SpeedQrev = 0;

	std::vector<ElementType> Array(Count);

#define COUNT 10'000

	std::chrono::nanoseconds Duration;

	/// std::reverse
	Duration = std::chrono::nanoseconds::zero();
	for( std::size_t i = 0; i < COUNT; i++ )
	{
		Duration += Measure<>::Duration(
			std::reverse<decltype(Array.begin())>,
			Array.begin(),
			Array.end()
		);
	}
	Duration /= COUNT;

	SpeedStd = Duration.count();

	/// qreverse
	Duration = std::chrono::nanoseconds::zero();
	for( std::size_t i = 0; i < COUNT; i++ )
	{
		Duration += Measure<>::Duration(
			qReverse<sizeof(ElementType)>,
			Array.data(),
			Array.size()
		);
	}
	Duration /= COUNT;

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
	Bench<std::uint8_t, 8>();
	Bench<std::uint8_t, 16>();
	Bench<std::uint8_t, 32>();
	Bench<std::uint8_t, 64>();
	Bench<std::uint8_t, 128>();
	Bench<std::uint8_t, 256>();
	Bench<std::uint8_t, 512>();
	Bench<std::uint8_t, 1024>();

	// Powers of ten
	Bench<std::uint8_t, 100>();
	Bench<std::uint8_t, 1'000>();
	Bench<std::uint8_t, 10'000>();
	Bench<std::uint8_t, 100'000>();
	Bench<std::uint8_t, 1'000'000>();

	// Primes
	Bench<std::uint8_t, 59>();
	Bench<std::uint8_t, 79>();
	Bench<std::uint8_t, 173>();
	Bench<std::uint8_t, 6'133>();
	Bench<std::uint8_t, 10'177>();
	Bench<std::uint8_t, 25'253>();
	Bench<std::uint8_t, 31'391>();
	Bench<std::uint8_t, 50'432>();
	
	return EXIT_SUCCESS;
}

void PrintArray(const std::array<ElementType, ElementCount>& Array)
{
	for( const ElementType& Value : Array )
	{
		std::cout << +Value << ',';
	}
	std::cout << std::endl;
}
