#pragma once

#include <immintrin.h>

#if defined(_MSC_VER)

inline std::uint64_t Swap64(std::uint64_t x)
{
	return _byteswap_uint64(x);
}

inline std::uint32_t Swap32(std::uint32_t x)
{
	return _byteswap_ulong(x);
}

inline std::uint16_t Swap16(std::uint16_t x)
{
	return _byteswap_ushort(x);
}

#elif defined(__GNUC__) || defined(__clang__)

inline std::uint64_t Swap64(std::uint64_t x)
{
	return __builtin_bswap64(x);
}

inline std::uint32_t Swap32(std::uint32_t x)
{
	return __builtin_bswap32(x);
}

inline std::uint16_t Swap16(std::uint16_t x)
{
	return __builtin_bswap16(x);
}

#else

inline std::uint64_t Swap64(std::uint64_t x)
{
	return (
		((x & 0x00000000000000FF) << 56) |
		((x & 0x000000000000FF00) << 40) |
		((x & 0x0000000000FF0000) << 24) |
		((x & 0x00000000FF000000) << 8) |
		((x & 0x000000FF00000000) >> 8) |
		((x & 0x0000FF0000000000) >> 24) |
		((x & 0x00FF000000000000) >> 40) |
		((x & 0xFF00000000000000) >> 56)
		);
}

inline std::uint32_t Swap32(std::uint32_t x)
{
	return(
		((x & 0x000000FF) << 24) |
		((x & 0x0000FF00) << 8) |
		((x & 0x00FF0000) >> 8) |
		((x & 0xFF000000) >> 24)
		);
}

inline std::uint16_t Swap16(std::uint16_t x)
{
	return (
		((x & 0x00FF) << 8) |
		((x & 0xFF00) >> 8)
		);
}

#endif


template< std::size_t ElementSize >
inline void qReverse(void* Array, std::size_t Count)
{
	union PodElement
	{
		std::uint8_t u8[ElementSize];
	};
	PodElement* ArrayN
		= reinterpret_cast<PodElement*>(Array);
	// We're only iterating through half of the size of the Array8
	for( std::size_t i = 0; i < Count / 2; ++i )
	{
		// Exchange the upper and lower element as we work our
		// way down to the middle from either end
		std::swap(
			ArrayN[i],             // "lower" element
			ArrayN[Count - i - 1]  // "upper" element
		);
	}
}

// One byte elements
template<>
inline void qReverse<1>(void* Array, std::size_t Count)
{
	std::uint8_t* Array8 = reinterpret_cast<std::uint8_t*>(Array);
	std::size_t i = 0;
	// AVX2
	for( std::size_t j = i; j < ((Count / 2) / 32); ++j )
	{
		const __m256i ShuffleRev = _mm256_set_epi8(
			0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,
			0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15
		);
		// Load 32 elements at once into one 32-byte register
		__m256i Lower = _mm256_loadu_si256(
			reinterpret_cast<__m256i*>(&Array8[i])
		);
		__m256i Upper = _mm256_loadu_si256(
			reinterpret_cast<__m256i*>(&Array8[Count - i - 32])
		);

		// Reverse the byte order of our 32-byte vectors
		Lower = _mm256_shuffle_epi8(Lower,ShuffleRev);
		Upper = _mm256_shuffle_epi8(Upper,ShuffleRev);

		Lower = _mm256_permute2f128_si256(Lower,Lower,1);
		Upper = _mm256_permute2f128_si256(Upper,Upper,1);

		// Place them at their swapped position
		_mm256_storeu_si256(
			reinterpret_cast<__m256i*>(&Array8[i]),
			Upper
		);
		_mm256_storeu_si256(
			reinterpret_cast<__m256i*>(&Array8[Count - i - 32]),
			Lower
		);

		// 32 elements at a time
		i += 32;
	}
	// SSSE3
	for( std::size_t j = i; j < ((Count / 2) / 16); ++j )
	{
		// Load 16 elements at once into one 16-byte register
		__m128i Lower = _mm_loadu_si128(
			reinterpret_cast<__m128i*>(&Array8[i])
		);
		__m128i Upper = _mm_loadu_si128(
			reinterpret_cast<__m128i*>(&Array8[Count - i - 16])
		);

		// Reverse the byte order of our 16-byte vectors

		const __m128i ShuffleRev = _mm_set_epi8(
			0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
		);
		Lower = _mm_shuffle_epi8(Lower, ShuffleRev);
		Upper = _mm_shuffle_epi8(Upper, ShuffleRev);

		// Place them at their swapped position
		_mm_storeu_si128(
			reinterpret_cast<__m128i*>(&Array8[i]),
			Upper
		);
		_mm_storeu_si128(
			reinterpret_cast<__m128i*>(&Array8[Count - i - 16]),
			Lower
		);

		// 16 elements at a time
		i += 16;
	}
	// BSWAP 64
	for( std::size_t j = i; j < ((Count / 2) / 8); ++j )
	{
		// Get bswapped versions of our Upper and Lower 8-byte chunks
		std::uint64_t Lower = Swap64(
			*reinterpret_cast<std::uint64_t*>(&Array8[i])
		);
		std::uint64_t Upper = Swap64(
			*reinterpret_cast<std::uint64_t*>(&Array8[Count - i - 8])
		);

		// Place them at their swapped position
		*reinterpret_cast<std::uint64_t*>(&Array8[i]) = Upper;
		*reinterpret_cast<std::uint64_t*>(&Array8[Count - i - 8]) = Lower;

		// Eight elements at a time
		i += 8;
	}
	// BSWAP 32
	for( std::size_t j = i / 4; j < ((Count / 2) / 4); ++j )
	{
		// Get bswapped versions of our Upper and Lower 4-byte chunks
		std::uint32_t Lower = Swap32(
			*reinterpret_cast<std::uint32_t*>(&Array8[i])
		);
		std::uint32_t Upper = Swap32(
			*reinterpret_cast<std::uint32_t*>(&Array8[Count - i - 4])
		);

		// Place them at their swapped position
		*reinterpret_cast<std::uint32_t*>(&Array8[i]) = Upper;
		*reinterpret_cast<std::uint32_t*>(&Array8[Count - i - 4]) = Lower;

		// Four elements at a time
		i += 4;
	}
	// BSWAP 16
	for( std::size_t j = i / 2; j < ((Count / 2) / 2); ++j )
	{
		// Get bswapped versions of our Upper and Lower 4-byte chunks
		std::uint16_t Lower = Swap16(
			*reinterpret_cast<std::uint16_t*>(&Array8[i])
		);
		std::uint16_t Upper = Swap16(
			*reinterpret_cast<std::uint16_t*>(&Array8[Count - i - 2])
		);

		// Place them at their swapped position
		*reinterpret_cast<std::uint16_t*>(&Array8[i]) = Upper;
		*reinterpret_cast<std::uint16_t*>(&Array8[Count - i - 2]) = Lower;

		// Two elements at a time
		i += 2;
	}

	// Everything else that we can not do a bswap on, we swap normally
	// Naive swaps
	for( ; i < Count / 2; ++i )
	{
		// Exchange the upper and lower element as we work our
		// way down to the middle from either end
		//printf("Naive\n");
		std::swap<std::uint8_t>(
			Array8[i],             // "lower" element
			Array8[Count - i - 1]  // "upper" element
		);
	}
}
