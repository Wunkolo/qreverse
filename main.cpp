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
#include <tuple>

using ElementType = std::uint8_t;
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
	Benchmark();
#endif

	std::cin.ignore();

	return EXIT_SUCCESS;
}

template<typename TimeRes = std::chrono::nanoseconds>
struct Measure
{
	template<typename FuncType, typename ...ArgTypes>
	static typename TimeRes::rep Execute(FuncType&& Func, ArgTypes&&... Arguments)
	{
		auto Start = std::chrono::high_resolution_clock::now();
		std::forward<decltype(Func)>(Func)(std::forward<ArgTypes>(Arguments)...);
		auto Duration = std::chrono::duration_cast<TimeRes>(
			std::chrono::high_resolution_clock::now() - Start
		);
		return Duration.count();
	}

	template<typename FuncType, typename ...ArgTypes>
	static auto Duration(FuncType&& Func, ArgTypes&&... Arguments)
	{
		auto Start = std::chrono::high_resolution_clock::now();
		std::forward<decltype(Func)>(Func)(std::forward<ArgTypes>(Arguments)...);
		return std::chrono::duration_cast<TimeRes>(
			std::chrono::high_resolution_clock::now() - Start
		);
	}
};

template< typename ElementType, std::size_t Count >
void Bench()
{
	std::size_t SpeedStd = 0;
	std::size_t SpeedQrev = 0;

	std::vector<ElementType> Array(Count);

#define COUNT 10000

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
			qreverse<ElementType>,
			Array.data(),
			Array.size()
		);
	}
	Duration /= COUNT;

	SpeedQrev = Duration.count();

	std::cout << Count << '|' << SpeedStd << '|' << SpeedQrev << '|' << SpeedQrev/static_cast<std::double_t>(SpeedStd);

	return;
}

void Benchmark()
{
	/// Benchmark

	// Powers of two
	Bench<std::uint8_t,8>();
	Bench<std::uint8_t,16>();
	Bench<std::uint8_t,32>();
	Bench<std::uint8_t,64>();
	Bench<std::uint8_t,128>();
	Bench<std::uint8_t,256>();
	Bench<std::uint8_t,512>();
	Bench<std::uint8_t,1024>();

	// Powers of ten
	Bench<std::uint8_t,100>();
	Bench<std::uint8_t,1'000>();
	Bench<std::uint8_t,10'000>();
	Bench<std::uint8_t,100'000>();
	Bench<std::uint8_t,1'000'000>();

	// Primes
	Bench<std::uint8_t,59>();
	Bench<std::uint8_t,79>();
	Bench<std::uint8_t,173>();
	Bench<std::uint8_t,6'133>();
	Bench<std::uint8_t,10'177>();
	Bench<std::uint8_t,25'253>();
	Bench<std::uint8_t,31'391>();
	Bench<std::uint8_t,50'432>();
}

void PrintArray(const std::array<ElementType, ElementCount>& Array)
{
	for( const ElementType& Value : Array )
	{
		std::cout << +Value << ',';
	}
	std::cout << std::endl;
}
