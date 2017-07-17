#include <cstddef>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <iostream>

#include <algorithm>
#include <numeric>
#include <array>
#include <vector>
#include <functional>

#define VERBOSE
#include "qreverse.hpp"

#include <chrono>

using ElementType = std::int8_t;
constexpr std::size_t ElementCount = 255;

void Benchmark(std::array<ElementType, ElementCount>& Array);
void PrintArray(const std::array<ElementType, ElementCount>& Array);

int main()
{
	std::array<ElementType,ElementCount> Numbers;
	std::iota(Numbers.begin(), Numbers.end(), 0);
	printf("ElementSize: %zu bytes ElementCount: %zu\n", sizeof(ElementType), Numbers.size());

	std::cout << "Before:\t" << std::endl;
	PrintArray(Numbers);

	qreverse<ElementType>(Numbers.data(), Numbers.size());

	std::cout << "After:\t" << std::endl;
	PrintArray(Numbers);

	printf(
		"-----%s-----\n",
		std::is_sorted(Numbers.begin(), Numbers.end(), std::greater<ElementType>())
		?
		"Reversed"
		:
		"NotReversed"
	);
	
#ifndef VERBOSE
	Benchmark(Numbers);
#endif

	std::cin.ignore();

	return EXIT_SUCCESS;
}

template<typename TimeT = std::chrono::nanoseconds>
struct Measure
{
	template<typename F, typename ...Args>
	static typename TimeT::rep Execute(F&& func, Args&&... args)
	{
		auto start = std::chrono::high_resolution_clock::now();
		std::forward<decltype(func)>(func)(std::forward<Args>(args)...);
		auto duration = std::chrono::duration_cast< TimeT>(
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

void Benchmark(std::array<ElementType, ElementCount>& Array)
{
	/// Benchmark
#define COUNT 10000

	std::chrono::nanoseconds Duration;

	/// std::reverse
	std::cout << "-----------std::reverse" << std::endl;
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

	std::cout << "\tAvg: " << Duration.count() << "ns" << std::endl;

	/// qreverse
	std::cout << "-----------qreverse" << std::endl;
	Duration = std::chrono::nanoseconds::zero();
	for( std::size_t i = 0; i < COUNT; i++ )
	{
		Duration += Measure<>::Duration(
			qreverse<ElementType>,
			Array.data(),
			Array.size()
		);
	}
	Duration /= COUNT;

	std::cout << "\tAvg: " << Duration.count() << "ns" << std::endl;
}

void PrintArray(const std::array<ElementType, ElementCount>& Array)
{
	for( const ElementType& Value : Array )
	{
		std::cout << +Value << ',';
	}
	std::cout << std::endl;
}