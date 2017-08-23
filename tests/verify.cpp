#include <cstdint>
#include <cstddef>
#include <climits>
#include <iostream>
#include <string>
#include <vector>

#include <qreverse.hpp>

/*
For use with cmake:
	Verifies that qreverse can properly reverse an array at the designated
	compile-time element size.

	Define ELEMENTSIZE preprocessor value to adjust verified element size
*/

#ifndef ELEMENTSIZE
#define ELEMENTSIZE 1
#endif

int main(int argc, char* argv[])
{
	if( argc < 2 )
	{
		std::cout << "Usage: Verify# (Element Count)"
			<< std::endl;
		return EXIT_FAILURE;
	}

	std::size_t ElementCount;

	ElementCount = std::strtoull(argv[1], nullptr, 10);

	if( ElementCount == 0 || ElementCount == ULLONG_MAX )
	{
		return EXIT_FAILURE;
	}

	std::vector<std::uint8_t> Array(ELEMENTSIZE * ElementCount);

	std::cout << "Original: " << std::endl;
	for( std::size_t i = 0; i < ElementCount; ++i )
	{
		std::uint8_t* Element = &Array[i * ELEMENTSIZE];
		for( std::size_t j = 0; j < ELEMENTSIZE; ++j )
		{
			Element[j] = static_cast<std::uint8_t>(i);
			std::cout << +Element[j] << ' ';
		}
	}

	std::cout << std::endl;

	std::vector<std::uint8_t> Reversed(Array);

	qReverse<ELEMENTSIZE>(Reversed.data(), ElementCount);

	std::cout << "Reversed: " << std::endl;
	for( std::size_t i = 0; i < ElementCount; ++i )
	{
		std::uint8_t* Element = &Reversed[i * ELEMENTSIZE];
		for( std::size_t j = 0; j < ELEMENTSIZE; ++j )
		{
			std::cout << +Element[j] << ' ';
		}
	}

	std::cout << std::endl;

	// Verify proper reversal
	for( std::size_t i = 0; i < ElementCount; ++i )
	{
		const std::uint8_t* OriginalElem = &Array[(ElementCount - i - 1) * ELEMENTSIZE];
		const std::uint8_t* ReversedElem = &Reversed[i * ELEMENTSIZE];
		for( std::size_t j = 0; j < ELEMENTSIZE; ++j )
		{
			if( OriginalElem[j] != ReversedElem[j] )
			{
				// Mismatch, not reversed
				std::cout << "[FAIL] Array Not Reversed" << std::endl;
				return EXIT_FAILURE;
			}
		}
	}

	// Successfully reversed
	std::cout << "[PASS] Array Reversed" << std::endl;
	return EXIT_SUCCESS;
}