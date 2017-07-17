#include <cstdint>
#include <cstddef>
#include <cstdio>

#include <immintrin.h>

// naive implementation
template< typename Type>
inline void qreverse_naive( Type* Values,std::size_t Start, std::size_t End)
{
	switch( sizeof(Type) )
	{
	case 16:
	{
		break;
	}
	case sizeof(std::uint64_t): // 8 byte elements
	{
		// 512-bit AVX2
		while( (End - Start + 1)/2 >= (sizeof(__m256i)*2/sizeof(std::uint64_t)))
		{
			printf("AVX_512 %zu %zu\n",Start,End);
			constexpr std::size_t Quantum = (sizeof(__m256i)*2/sizeof(std::uint64_t));
			__m256i A1 = _mm256_loadu_si256(reinterpret_cast<__m256i*>(Values+Start));
			__m256i A2 = _mm256_loadu_si256(reinterpret_cast<__m256i*>(Values+Start)+1);
			__m256i B1 = _mm256_loadu_si256(reinterpret_cast<__m256i*>(Values+End+1)-1);
			__m256i B2 = _mm256_loadu_si256(reinterpret_cast<__m256i*>(Values+End+1)-2);
			A1 = _mm256_permute4x64_epi64(A1,_MM_SHUFFLE(0,1,2,3));
			A2 = _mm256_permute4x64_epi64(A2,_MM_SHUFFLE(0,1,2,3));
			B1 = _mm256_permute4x64_epi64(B1,_MM_SHUFFLE(0,1,2,3));
			B2 = _mm256_permute4x64_epi64(B2,_MM_SHUFFLE(0,1,2,3));
			_mm256_storeu_si256(reinterpret_cast<__m256i*>(Values+Start),B1);
			_mm256_storeu_si256(reinterpret_cast<__m256i*>(Values+Start)+1,B2);
			_mm256_storeu_si256(reinterpret_cast<__m256i*>(Values+End+1)-1,A1);
			_mm256_storeu_si256(reinterpret_cast<__m256i*>(Values+End+1)-2,A2);
			Start += Quantum;
			End -= Quantum;
		}
		// 256-bit AVX2
		while( (End - Start + 1)/2 >= (sizeof(__m256i)/sizeof(std::uint64_t)) )
		{
			printf("AVX_256 %zu %zu\n",Start,End);
			constexpr std::size_t Quantum = sizeof(__m256i)/sizeof(std::uint64_t);
			__m256i A = _mm256_loadu_si256(reinterpret_cast<__m256i*>(Values+Start));
			__m256i B = _mm256_loadu_si256(reinterpret_cast<__m256i*>(Values+End+1)-1);
			A = _mm256_permute4x64_epi64(A,_MM_SHUFFLE(0,1,2,3));
			B = _mm256_permute4x64_epi64(B,_MM_SHUFFLE(0,1,2,3));
			_mm256_storeu_si256(reinterpret_cast<__m256i*>(Values+Start),B);
			_mm256_storeu_si256(reinterpret_cast<__m256i*>(Values+End+1)-1,A);
			Start += Quantum;
			End -= Quantum;
		}
		// 128-bit SSSe3
		while( (End - Start + 1)/2 >= (sizeof(__m128i)/sizeof(std::uint64_t)) )
		{
			printf("SSSE3_128 %zu %zu\n",Start,End);
			constexpr std::size_t Quantum = sizeof(__m128i)/sizeof(std::uint64_t);
			__m128i A = _mm_loadu_si128(reinterpret_cast<__m128i*>(Values+Start));
			__m128i B = _mm_loadu_si128(reinterpret_cast<__m128i*>(Values+End+1)-1);
			A = _mm_alignr_epi8(A,A,sizeof(std::uint64_t));// 8 byte rotate
			B = _mm_alignr_epi8(B,B,sizeof(std::uint64_t));// 8 byte rotate
			_mm_storeu_si128(reinterpret_cast<__m128i*>(Values+Start),B);
			_mm_storeu_si128(reinterpret_cast<__m128i*>(Values+End+1)-1,A);
			Start += Quantum;
			End -= Quantum;
		}
		break;
	}
	case sizeof(std::uint32_t): // Four byte elements
	{
		// 256-bit AVX2
		while( (End - Start + 1)/2 >= (sizeof(__m256i)/sizeof(std::uint32_t)) )
		{
			printf("AVX_256 %zu %zu\n",Start,End);
			const __m256i ShuffleRev = _mm256_setr_epi32(
				7,6,5,4,3,2,1,0
				);

			__m256i A = _mm256_loadu_si256(reinterpret_cast<__m256i*>(Values+Start));
			__m256i B = _mm256_loadu_si256(reinterpret_cast<__m256i*>(Values+End+1)-1);
			A = _mm256_permutevar8x32_epi32(A,ShuffleRev);
			B = _mm256_permutevar8x32_epi32(B,ShuffleRev);
			_mm256_storeu_si256(reinterpret_cast<__m256i*>(Values+Start),B);
			_mm256_storeu_si256(reinterpret_cast<__m256i*>(Values+End+1)-1,A);
			Start += sizeof(__m256i)/sizeof(std::uint32_t);
			End -= sizeof(__m256i)/sizeof(std::uint32_t);
		}
		// 128-bit SSSe3
		while( (End - Start + 1)/2 >= (sizeof(__m128i)/sizeof(std::uint32_t)) )
		{
			printf("SSSE3_128 %zu %zu\n",Start,End);

			__m128i A = _mm_loadu_si128(reinterpret_cast<__m128i*>(Values+Start));
			__m128i B = _mm_loadu_si128(reinterpret_cast<__m128i*>(Values+End+1)-1);
			A = _mm_shuffle_epi32(A,_MM_SHUFFLE(0,1,2,3));
			B = _mm_shuffle_epi32(B,_MM_SHUFFLE(0,1,2,3));
			_mm_storeu_si128(reinterpret_cast<__m128i*>(Values+Start),B);
			_mm_storeu_si128(reinterpret_cast<__m128i*>(Values+End+1)-1,A);
			Start += sizeof(__m128i)/sizeof(std::uint32_t);
			End -= sizeof(__m128i)/sizeof(std::uint32_t);
		}
		break;
	}
	case sizeof(std::uint16_t): // Two byte elements
	{
		// 256-bit AVX2
		while( (End - Start + 1)/2 >= (sizeof(__m256i)/sizeof(std::uint16_t)) )
		{
			printf("AVX_256 %zu %zu\n",Start,End);

			const __m256i ShuffleRev = _mm256_set_epi8(
				1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14,
				1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14
				);

			__m256i A = _mm256_loadu_si256(reinterpret_cast<__m256i*>(Values+Start));
			__m256i B = _mm256_loadu_si256(reinterpret_cast<__m256i*>(Values+End+1)-1);
			A = _mm256_shuffle_epi8(A,ShuffleRev);
			A = _mm256_permute2f128_si256(A,A,1);
			B = _mm256_shuffle_epi8(B,ShuffleRev);
			B = _mm256_permute2f128_si256(B,B,1);
			_mm256_storeu_si256(reinterpret_cast<__m256i*>(Values+Start),B);
			_mm256_storeu_si256(reinterpret_cast<__m256i*>(Values+End+1)-1,A);
			Start += sizeof(__m256i)/sizeof(std::uint16_t);
			End -= sizeof(__m256i)/sizeof(std::uint16_t);
		}
		// 128-bit SSSe3
		while( (End - Start + 1)/2 >= (sizeof(__m128i)/sizeof(std::uint16_t)) )
		{
			printf("SSSE3_128 %zu %zu\n",Start,End);

			const __m128i ShuffleRev = _mm_set_epi8(1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14);

			__m128i A = _mm_loadu_si128(reinterpret_cast<__m128i*>(Values+Start));
			__m128i B = _mm_loadu_si128(reinterpret_cast<__m128i*>(Values+End+1)-1);
			A = _mm_shuffle_epi8(A,ShuffleRev);
			B = _mm_shuffle_epi8(B,ShuffleRev);
			_mm_storeu_si128(reinterpret_cast<__m128i*>(Values+Start),B);
			_mm_storeu_si128(reinterpret_cast<__m128i*>(Values+End+1)-1,A);
			Start += sizeof(__m128i)/sizeof(std::uint16_t);
			End -= sizeof(__m128i)/sizeof(std::uint16_t);
		}
		break;
	}
	case sizeof(std::uint8_t): // One byte elements
	{
		// 256-bit AVX2
		while( (End - Start + 1)/2 >= (sizeof(__m256i)) )
		{
			printf("AVX_256 %zu %zu\n",Start,End);

			const __m256i ShuffleRev = _mm256_set_epi8(
				0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,
				0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15
				);

			__m256i A = _mm256_loadu_si256(reinterpret_cast<__m256i*>(Values+Start));
			__m256i B = _mm256_loadu_si256(reinterpret_cast<__m256i*>(Values+End+1)-1);
			A = _mm256_shuffle_epi8(A,ShuffleRev);
			A = _mm256_permute2f128_si256(A,A,1);
			B = _mm256_shuffle_epi8(B,ShuffleRev);
			B = _mm256_permute2f128_si256(B,B,1);
			_mm256_storeu_si256(reinterpret_cast<__m256i*>(Values+Start),B);
			_mm256_storeu_si256(reinterpret_cast<__m256i*>(Values+End+1)-1,A);
			Start += sizeof(__m256i);
			End -= sizeof(__m256i);
		}
		// 128-bit SSSe3
		while( (End - Start + 1)/2 >= (sizeof(__m128i)) )
		{
			printf("SSSE3_128 %zu %zu\n",Start,End);

			const __m128i ShuffleRev = _mm_set_epi8(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);

			__m128i A = _mm_loadu_si128(reinterpret_cast<__m128i*>(Values+Start));
			__m128i B = _mm_loadu_si128(reinterpret_cast<__m128i*>(Values+End+1)-1);
			A = _mm_shuffle_epi8(A,ShuffleRev);
			B = _mm_shuffle_epi8(B,ShuffleRev);
			_mm_storeu_si128(reinterpret_cast<__m128i*>(Values+Start),B);
			_mm_storeu_si128(reinterpret_cast<__m128i*>(Values+End+1)-1,A);
			Start += sizeof(__m128i);
			End -= sizeof(__m128i);
		}
		// 64-bit BSWAP
		while( (End - Start + 1)/2 >= (sizeof(std::uint64_t)) )
		{
			printf("bswap_64 %zu %zu\n",Start,End);
			std::uint64_t A = __builtin_bswap64(*reinterpret_cast<std::uint64_t*>(Values+Start));
			std::uint64_t B = __builtin_bswap64(*(reinterpret_cast<std::uint64_t*>(Values+End+1)-1));
			*(reinterpret_cast<std::uint64_t*>(Values+Start)) = B;
			*(reinterpret_cast<std::uint64_t*>(Values+End+1)-1) = A;
			Start += sizeof(std::uint64_t);
			End -= sizeof(std::uint64_t);
		}
		// 32-bit BSWAP
		while( (End - Start + 1)/2 >= (sizeof(std::uint32_t)) )
		{
			printf("bswap_32 %zu %zu\n",Start,End);
			std::uint32_t A = __builtin_bswap32(*reinterpret_cast<std::uint32_t*>(Values+Start));
			std::uint32_t B = __builtin_bswap32(*(reinterpret_cast<std::uint32_t*>(Values+End+1)-1));
			*reinterpret_cast<std::uint32_t*>(Values+Start) = B;
			*(reinterpret_cast<std::uint32_t*>(Values+End+1)-1) = A;
			Start += sizeof(std::uint32_t);
			End -= sizeof(std::uint32_t);
		}
		// 16-bit BSWAP
		while( (End - Start + 1)/2 >= (sizeof(std::uint16_t)) )
		{
			printf("bwap_16 %zu %zu\n",Start,End);
			std::uint16_t A = __builtin_bswap16(*reinterpret_cast<std::uint16_t*>(Values+Start));
			std::uint16_t B = __builtin_bswap16(*(reinterpret_cast<std::uint16_t*>(Values+End+1)-1));
			*reinterpret_cast<std::uint16_t*>(Values+Start) = B;
			*(reinterpret_cast<std::uint16_t*>(Values+End+1)-1) = A;
			Start += sizeof(std::uint16_t);
			End -= sizeof(std::uint16_t);
		}
		break;
	}
	}

	// Middle cases
	if( Start < End )
	{
		while( Start < End )
		{
			printf("Naive Swap %zu %zu : Diff: %zu\n",Start,End,End-Start);
			std::swap(Values[Start],Values[End]);
			++Start;
			--End;
		}
	}
	printf("");
}

template< typename Type>
inline void qreverse( Type* Values, std::size_t Size)
{
	qreverse_naive( Values,0, Size-1 );
}

