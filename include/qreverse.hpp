#pragma once
#include <cstdint>
#include <cstddef>

#if defined(_MSC_VER)

#include <intrin.h>

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

#include <x86intrin.h>

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
	// An abstraction to treat the array elements purely as bytes
	struct ByteElement
	{
		std::uint8_t u8[ElementSize];
	};
	ByteElement* ArrayN = reinterpret_cast<ByteElement*>(Array);

	// If compiler adds any padding/alignment bytes(and some do) then assert out
	static_assert(
		sizeof(ByteElement) == ElementSize,
		"ByteElement is pad-aligned and does not match specified element size"
	);

	// We're only iterating through half of the size of the Array
	for( std::size_t i = 0; i < Count / 2; ++i )
	{
		// Exchange the upper and lower element as we work our
		// way down to the middle from either end
		ByteElement Temp(ArrayN[i]);
		ArrayN[i] = ArrayN[Count - i - 1];
		ArrayN[Count - i - 1] = Temp;
	}
}

// One byte elements
template<>
inline void qReverse<1>(void* Array, std::size_t Count)
{
	std::uint8_t* Array8 = reinterpret_cast<std::uint8_t*>(Array);
	std::size_t i = 0;
	// AVX-512BW/F
#if defined(__AVX512F__) && defined(__AVX512BW__)
	for( std::size_t j = i; j < ((Count / 2) / 64); ++j )
	{
		// Reverses the 16 bytes of the four  128-bit lanes in a 512-bit register
		const __m512i ShuffleRev8 = _mm512_set_epi32(
			0x00010203, 0x4050607, 0x8090a0b, 0xc0d0e0f,
			0x00010203, 0x4050607, 0x8090a0b, 0xc0d0e0f,
			0x00010203, 0x4050607, 0x8090a0b, 0xc0d0e0f,
			0x00010203, 0x4050607, 0x8090a0b, 0xc0d0e0f
		);

		// Reverses the four 128-bit lanes of a 512-bit register
		const __m512i ShuffleRev64 = _mm512_set_epi64(
			1,0,3,2,5,4,7,6
		);

		// Load 64 elements at once into one 64-byte register
		__m512i Lower = _mm512_loadu_si512(
			reinterpret_cast<__m512i*>(&Array8[i])
		);
		__m512i Upper = _mm512_loadu_si512(
			reinterpret_cast<__m512i*>(&Array8[Count - i - 64])
		);

		// Reverse the byte order of each 128-bit lane
		Lower = _mm512_shuffle_epi8(Lower,ShuffleRev8);
		Upper = _mm512_shuffle_epi8(Upper,ShuffleRev8);

		// Reverse the four 128-bit lanes in the 512-bit register
		Lower = _mm512_permutexvar_epi64(ShuffleRev64,Lower);
		Upper = _mm512_permutexvar_epi64(ShuffleRev64,Upper);

		// Place them at their swapped position
		_mm512_storeu_si512(
			reinterpret_cast<__m512i*>(&Array8[i]),
			Upper
		);
		_mm512_storeu_si512(
			reinterpret_cast<__m512i*>(&Array8[Count - i - 64]),
			Lower
		);

		// 64 elements at a time
		i += 64;
	}
#endif
	// AVX-2
#if defined(__AVX2__)
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

		Lower = _mm256_permute2x128_si256(Lower,Lower,1);
		Upper = _mm256_permute2x128_si256(Upper,Upper,1);

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
#endif
	// SSSE3
#if defined(__SSSE3__)
	for( std::size_t j = i; j < ((Count / 2) / 16); ++j )
	{
		const __m128i ShuffleRev = _mm_set_epi8(
			0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
		);
		// Load 16 elements at once into one 16-byte register
		__m128i Lower = _mm_loadu_si128(
			reinterpret_cast<__m128i*>(&Array8[i])
		);
		__m128i Upper = _mm_loadu_si128(
			reinterpret_cast<__m128i*>(&Array8[Count - i - 16])
		);

		// Reverse the byte order of our 16-byte vectors
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
#endif
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
		std::uint8_t Temp(Array8[i]);
		Array8[i] = Array8[Count - i - 1];
		Array8[Count - i - 1] = Temp;
	}
}


// Two byte elements
template<>
inline void qReverse<2>(void* Array, std::size_t Count)
{
	std::uint16_t* Array16 = reinterpret_cast<std::uint16_t*>(Array);
	std::size_t i = 0;

	// SSSE3
#if defined(__SSSE3__)
	for( std::size_t j = i; j < ((Count / 2) / 8); ++j )
	{
		const __m128i ShuffleRev = _mm_set_epi8(
			1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14
		);
		// Load 8 elements at once into one 16-byte register
		__m128i Lower = _mm_loadu_si128(
			reinterpret_cast<__m128i*>(&Array16[i])
		);
		__m128i Upper = _mm_loadu_si128(
			reinterpret_cast<__m128i*>(&Array16[Count - i - 8])
		);

		// Reverse the byte order of our 16-byte vectors
		Lower = _mm_shuffle_epi8(Lower, ShuffleRev);
		Upper = _mm_shuffle_epi8(Upper, ShuffleRev);

		// Place them at their swapped position
		_mm_storeu_si128(
			reinterpret_cast<__m128i*>(&Array16[i]),
			Upper
		);
		_mm_storeu_si128(
			reinterpret_cast<__m128i*>(&Array16[Count - i - 8]),
			Lower
		);

		// 8 elements at a time
		i += 8;
	}
#endif

	// Naive swaps
	for( ; i < Count / 2; ++i )
	{
		// Exchange the upper and lower element as we work our
		// way down to the middle from either end
		std::uint16_t Temp(Array16[i]);
		Array16[i] = Array16[Count - i - 1];
		Array16[Count - i - 1] = Temp;
	}
}

// Four byte elements
template<>
inline void qReverse<4>(void* Array, std::size_t Count)
{
	std::uint32_t* Array32 = reinterpret_cast<std::uint32_t*>(Array);
	std::size_t i = 0;

	// SSSE3
#if defined(__SSSE3__)
	for( std::size_t j = i; j < ((Count / 2) / 4); ++j )
	{
		// Load 4 elements at once into one 16-byte register
		__m128i Lower = _mm_loadu_si128(
			reinterpret_cast<__m128i*>(&Array32[i])
		);
		__m128i Upper = _mm_loadu_si128(
			reinterpret_cast<__m128i*>(&Array32[Count - i - 4])
		);

		// Reverse the byte order of our 16-byte vectors
		Lower = _mm_shuffle_epi32(Lower, _MM_SHUFFLE(0,1,2,3) );
		Upper = _mm_shuffle_epi32(Upper, _MM_SHUFFLE(0,1,2,3) );

		// Place them at their swapped position
		_mm_storeu_si128(
			reinterpret_cast<__m128i*>(&Array32[i]),
			Upper
		);
		_mm_storeu_si128(
			reinterpret_cast<__m128i*>(&Array32[Count - i - 4]),
			Lower
		);

		// 8 elements at a time
		i += 4;
	}
#endif

	// Naive swaps
	for( ; i < Count / 2; ++i )
	{
		// Exchange the upper and lower element as we work our
		// way down to the middle from either end
		std::uint32_t Temp(Array32[i]);
		Array32[i] = Array32[Count - i - 1];
		Array32[Count - i - 1] = Temp;
	}
}
