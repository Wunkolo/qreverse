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

#include "qreverse.hpp"

#define COUNT 1

#include <chrono>
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
			std::chrono::high_resolution_clock::now()-start
		);
	} 
};

template< typename ElmType,  std::size_t Size >
void PrintArray(const std::array<ElmType,Size>& Array)
{
	for( const ElmType& Value : Array )
	{
		std::cout << +Value << ',';
	}
	std::cout << std::endl;
}

using ElementType = std::int8_t;

int main()
{
	std::array<ElementType,82> Numbers;

	std::iota(Numbers.begin(), Numbers.end(), 0);

	std::cout << "Before:\t" << std::endl;
	PrintArray(Numbers);

	///
	std::chrono::nanoseconds Duration(0);


	printf("ElementSize: %zu bytes ElementCount: %zu\n",sizeof(ElementType),Numbers.size());


	/// std::reverse
	std::cout << "std::reverse" << std::endl;
	Duration = std::chrono::nanoseconds::zero();
	for( std::size_t i = 0; i < COUNT; i++)
	{
		Duration += Measure<>::Duration(
			std::reverse<decltype(Numbers.begin())>,
			Numbers.begin(),
			Numbers.end()
		);
	}
	Duration /= COUNT;

	std::cout << "\tAvg: " << Duration.count() << "ns" << std::endl;
	//std::cout << Duration.count() << ',';

	/// qreverse
	std::cout << "qreverse" << std::endl;
	Duration = std::chrono::nanoseconds::zero();
	for( std::size_t i = 0; i < COUNT; i++)
	{
		Duration += Measure<>::Duration(
			qreverse<ElementType>,
			Numbers.data(),
			Numbers.size()
		);
	}
	Duration /= COUNT;

	//std::cout << Duration.count() << std::endl;

	std::cout << "\tAvg: " << Duration.count() << "ns" << std::endl;

	///
	std::cout << "After:\t" << std::endl;
	PrintArray(Numbers);

	printf(
		"-----%s\033[0m-----\n",
		std::is_sorted(Numbers.begin(),Numbers.end(),std::greater<ElementType>())
		?
		"\033[1;32mReversed"
		:
		"\033[1:31mNotReversed"
	);

	return EXIT_SUCCESS;
}

