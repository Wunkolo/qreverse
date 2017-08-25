# qReverse [![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](https://raw.githubusercontent.com/Wunkolo/qreverse/master/LICENSE)

qReverse is an architecture-accelerated array reversal algorithm intended as a personal study to design a fast AoS reversal algorithm utilizing SIMD.

---

Array reversal algorithms that you typically see involves swapping both end of the array and working your way down to the middle-most element. C++ being type-aware treat array elements as objects and will call overloaded class operators such as `operator=` or a `copy by reference` constructor where available so an. Many implementetions of a "swap" function would use an intermediate temporary variable to make the exchange which would require a minimum of two calls to your object's `operator=` and at least one call to your object's `copy by reference` constructor. Some other novel algorithms use the xor-swap technique after making some assumptions about the data being swapped(integer-only, register-bound, no special overloads, etc). `std::swap` also allows a custom overload `swap` function to be used if it is within the same namespace as your type should you want to expose your overloaded method to C++'s standard algorithm library.

```c++
// Taken from http://en.cppreference.com/w/cpp/algorithm/reverse
// Example of using std::reverse
#include <vector>
#include <iostream>
#include <iterator>
#include <algorithm>
 
int main()
{
	std::vector<int> v({1,2,3});
	std::reverse(std::begin(v), std::end(v));
	std::cout << v[0] << v[1] << v[2] << '\n';

	int a[] = {4, 5, 6, 7};
	std::reverse(std::begin(a), std::end(a));
	std::cout << a[0] << a[1] << a[2] << a[3] << '\n';
}
```

Should `std::reverse` be called upon a "plain ol data" type such as `std::uint8_t`(aka `unsigned char`) then it can safely assume that your data doesn't have any special assignment/copy overhead to worry about and can treat it as its raw bytes. It can make safe assumptions about your data to be able to optimize away any intermediate variables and keep the swap routine entirely register-bound and pretty efficient

The emitted x86 of a `std::reverse` on an array of `std::uint8_t` will look something like this.


```
      ; std::reverse for std::uint8_t
      0x000014a0 4839fe         cmp rsi, rdi
  ,=< 0x000014a3 7420           je 0x14c5
  |   0x000014a5 4883ee01       sub rsi, 1
  |   0x000014a9 4839f7         cmp rdi, rsi
 ,==< 0x000014ac 7317           jae 0x14c5                  
.---> 0x000014ae 0fb607         movzx eax, byte [rdi] ; Load two bytes from
|||   0x000014b1 0fb616         movzx edx, byte [rsi] ; each end.
|||   0x000014b4 8817           mov byte [rdi], dl    ; Write them at opposite
|||   0x000014b6 8806           mov byte [rsi], al    ; ends.
|||   0x000014b8 4883c701       add rdi, 1            ; Shift index at
|||   0x000014bc 4883ee01       sub rsi, 1            ; both ends "inward" toward the middle
|||   0x000014c0 4839f7         cmp rdi, rsi
`===< 0x000014c3 72e9           jb 0x14ae
 ``-> 0x000014c5 f3c3           ret
```

An animation of the basic algorithm:

![](/images/Naive-Byte.gif)

Our own custom implementation of this algorithm to start us off:
We'll template the element-size at compile-time and emit a pseudo-structure that fits this size in an attempt to keep this illustrative implementation as generic as possible for an element of _any_ size in bytes. By having the element-size be templated we can make specific template specializations for certain element-sizes while all other sizes fall-back to this naive algorithm. Having this done with a template allows the proper specialization to be instanced at compile-time rather than compare a runtime "element-size" argument against a list of available implementations.

```cpp
template< std::size_t ElementSize >
inline void qReverse(void* Array, std::size_t Count)
{
	// An abstraction to treat the array elements as raw bytes
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
```

**We can do better!** This "plain ol data" assumption can be made for lots of different types of data. Most usages of `struct` are intended to be treated as "bags of data" and do not have the limitation of additional memory-movement logic for copying or swapping since they are intended only to communicate a structure of interpretation of bytes. The more obvious case-study can also be having an array of `chars` found in an ASCII `string` or maybe a row of pixel data. **From this point on let's just assume that the sequence of data we are dealing with are to be these "bags of data" instances** that do not involve any kind of `operator=` or `Foo (const Foo&)` overhead so the data may be safely interpreted as literal bytes.

Most of us are running 64-bit or 32-bit machines or have register sizes that are easily much bigger than just 1 byte(the animation above had register sizes that are 8 bytes, which is the size of a single 64-bit register). One way we can speed this up is by loading in a full register-sized chunk of bytes, flipping this chunk of bytes within the register, and then placing it on the other end! Swapping all the bytes in the registers is a popular operation in networking called an `endian swap` and x86 happens to have just the instruction to do this!

# bswap

The `bswap` instruction reverses the individual bytes of a register and is typically used to swap the `endian` of an integer to exchange between `host` and `network` byte-order(see `htons`,`htonl`,`ntohs`,`ntohl`). Most x86 compilers implement assembly intrinsics that you can put right into your C or C++ code to get the compiler to emit the `bswap` instruction directly:

**MSVC:**
- `_byteswap_uint64`
- `_byteswap_ulong`
- `_byteswap_ushort`

**GCC/Clang:**

- `_builtin_bswap64`
- `_builtin_bswap32`
- `_builtin_bswap16`

The x86 header `immintrin.h` also includes `_bswap` and `_bswap64`. Otherwise a more generic and portable implementation can be used as well to be more architecture-generic.

```cpp
inline std::uint64_t Swap64(std::uint64_t x)
{
	return (
		((x & 0x00000000000000FF) << 56) |
		((x & 0x000000000000FF00) << 40) |
		((x & 0x0000000000FF0000) << 24) |
		((x & 0x00000000FF000000) <<  8) |
		((x & 0x000000FF00000000) >>  8) |
		((x & 0x0000FF0000000000) >> 24) |
		((x & 0x00FF000000000000) >> 40) |
		((x & 0xFF00000000000000) >> 56)
	);
}

inline std::uint32_t Swap32(std::uint32_t x)
{
	return(
		((x & 0x000000FF) << 24) |
		((x & 0x0000FF00) <<  8) |
		((x & 0x00FF0000) >>  8) |
		((x & 0xFF000000) >> 24)
	);
}

inline std::uint16_t Swap16(std::uint16_t x)
{
	// This tends to emit a 16-bit `rol` or `ror` instruction
	return (
		((x & 0x00FF) <<  8) |
		((x & 0xFF00) >>  8)
	);
}
```

Most compilers are able to detect when an in-register endian-swap is being done like above and will emit `bswap` automatically or a similar intrinsic for your target architecture(The ARM architecture has the `rev` instruction for **armv6** or newer). Note also that `bswap16` is basically just a 16-bit rotate of 1 byte aka a `rol` or `ror` instruction.

x86_64 (gcc):
```
Swap64(unsigned long):
  mov rax, rdi
  bswap rax
  ret
Swap32(unsigned int):
  mov eax, edi
  bswap eax
  ret
Swap16(unsigned short):
  mov eax, edi
  rol ax, 8
  ret
```

x86_64 (clang):
```
Swap64(unsigned long): # @Swap64(unsigned long)
  bswap rdi
  mov rax, rdi
  ret

Swap32(unsigned int): # @Swap32(unsigned int)
  bswap edi
  mov eax, edi
  ret

Swap16(unsigned short): # @Swap16(unsigned short)
  rol di, 8
  mov eax, edi
  ret
```

ARM64 (gcc):
```
Swap64(unsigned long):
  rev x0, x0
  ret
Swap32(unsigned int):
  rev w0, w0
  ret
Swap16(unsigned short):
  rev16 w0, w0
  ret
```


Using 32-bit `bswap`s, the algorithm can take a 4-byte _chunk_ of bytes from either end into registers, `bswap` the register, and then place the reversed _chunks_ at the opposite ends. As the algorithm gets closer to the center it can use smaller 16-bit swaps(aka a 16-bit rotate) should it encounter 2-byte chunks.

![](/images/bswap-32.gif)

and this of course can be expanded into a 64-bit `bswap` allowing for even larger chunks to be reversed at once:

![](/images/bswap-64.gif)

Given an array of `11` bytes to be reversed: divide the array size by two to get the number of _single-element_ swaps to do(using integer arithmetic): `11/2 = 5`. So `5` single-element swaps are needed that we would have to do at either end of our split array. Now that there is a way to do `4` element chunks at once too, integer-divide this result `5` again by `4` to know how many _four-element_ swaps needed(`5/4 = 1`). So only one `bswap`-swap and one `naive`-swap is needed to fully reverse an 11-element array.

```cpp
// Reverse an array of 1-byte elements(such as std::uint8_t)

// A specialization of the above implementation for 1-byte elements
// Does not call assignment or copy overloads
// Accelerated using - 32-bit bswap instruction
template<>
inline void qReverse<1>(void* Array, std::size_t Count)
{
	std::uint8_t* Array8 = reinterpret_cast<std::uint8_t*>(Array);
	std::size_t i = 0;

	// Using a new iteration variable "j" to illustrate that we know
	// the exact amount of times we have to use our chunk-swaps
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
```

And now some benchmarks: on a _i3-6100_ with _8gb of DDR4 ram_. I automated the benchmark process across several different array-sizes giving each array-size `10,000` array-reversal trials before getting an average execution time for the given array-size. Using g++ compile flags: `-m64 -Ofast -march=native` these are the results of comparing the execution time of the current bswap `qreverse` algorithm against `std::reverse`:

Element Count|std::reverse|qReverse|Speedup Factor
---|---|---|---
8|28 ns|25 ns|**1.120**
16|32 ns|25 ns|**1.280**
32|39 ns|27 ns|**1.444**
64|55 ns|30 ns|**1.833**
128|85 ns|35 ns|**2.429**
256|151 ns|38 ns|**3.974**
512|273 ns|44 ns|**6.205**
1024|491 ns|54 ns|**9.093**
100|68 ns|33 ns|**2.061**
1000|489 ns|53 ns|**9.226**
10000|8105 ns|295 ns|**27.475**
100000|64744 ns|4248 ns|**15.241**
1000000|652875 ns|41942 ns|**15.566**
59|48 ns|30 ns|**1.600**
79|57 ns|33 ns|**1.727**
173|97 ns|35 ns|**2.771**
6133|3557 ns|195 ns|**18.241**
10177|6853 ns|965 ns|**7.102**
25253|16670 ns|651 ns|**25.607**
31391|18548 ns|1206 ns|**15.380**
50432|32916 ns|1419 ns|**23.197**


And so across the board there are speedups up to _**x27!**_ before dipping down a bit for the more larger array sizes potentially due to the accumulation of cache misses with such large amounts of data. The algorithm reaches out to either end of a potentially massive array which lends itself to an accumulation of cache misses at some point. Still a _very_ large and significant speedup over `std::reverse` consistantly without trying to do some `_mm_prefetch` arithmetic to get the cache to behave.

# SIMD

The `bswap` instruction can reverse the byte-order of `2`, `4`, or `8` bytes, but several x86 extensions later and now it is possible to swap the byte order of `16`, `32`, even `64` bytes all at once through the use of `SIMD`. `SIMD` stands for *Single Instruction Multiple Data* and allows operation upon multiple lanes of data in parallel using only a single instruction. Much like `bswap` which atomically reverses all four bytes in a register, `SIMD` provides an entire instruction set of arithmetic that allows manipulation of multiple instances of data at once using a single instruction in parallel. These chunks of data that are operated upon tend to be called `vectors` of data. Multiple bytes of data can then be elevated into a `vector` register to reverse its order and place it on the opposite end similarly to our `bswap` implementation but with even larger chunks.

These additions to our algorithm will span higher-width chunks of bytes and will be append above our chain of `bswap` accelerated swap-loops. Over the years the x86 architecture has seen many generations of `SIMD` implementations, improvements, and instruciton sets:

- `MMX` (1996)
- `SSE` (1999)
- `SSE2` (2001)
- `SSE3` (2004)
- `SSSE3` (2006)
- `SSE4 a/1/2` (2006)
- `AVX` (2008)
- `AVX2` (2013)
- `AVX512` (2015)

Some are kept around for compatibilities sake(`MMX`) and some are so recent, elusive, or so _vendor-specific_ to Intel or AMD that you're probably not likely to have a processor that features it(`SSE4a`). Some are very specific to enterprise hardware (such as `AVX512`) and are not likely to be on consumer hardware either.

At the moment (July 21, 2017) the steam hardware survey states that **94.42%** of all CPUs on Steam feature `SSSE3`([store.steampowered.com/hwsurvey/](http://store.steampowered.com/hwsurvey/)). `SSSE3` is what will be used in a first step into `SIMD` territory. `SSSE3` in particular due to its `_mm_shuffle_epi8` instruction which lets us _shuffle_ bytes within our 128-bit register with relative ease for this implementation.

# SSSE3

`SSE` stands for "Streaming SIMD Extensions" while `SSSE3` stands for "Supplemental Streaming SIMD Extensions 3" which is the _forth_ iteration of `SSE` technology. SSE introduces registers that allow for some `128` bit vector arithmetic. In C or C++ code the intent to use these registers is represented using types such as `__m128i` or `__m128d` which tell the compiler that any notion of _storage_ for these types should find their place within the 128-bit `SSE` registers when ever possible. Intrinsics such as `_mm_add_epi8` which will add two `__m128i`s together, and treat them as a _vector_ of 8-bit elements are now available within C and C++ code. The `i` and `d` found in `__m128i` and `__m128d` are to notify intent of the 128-register's interpretation as `i`nteger and `d`ouble respectively. `__m128` is assumed to be a vector of four `floats` by default. Since integer-data is what is being operated upon, the `__m128i` data type will be used as our data representation which gives access to the `_mm_shuffle_epi8` instruction.

Now to draft a `SSSE3` byte swapping implementation and create a simulated 16-byte `bswap` using `SSSE3`. First, use `#include <tmmintrin.h>` in C or C++ code to expose every intrinsic from `MMX` up until `SSSE3`. Then, use the instrinsic `_mm_loadu_si128` to `load` an `u`naligned `s`igned `i`nteger vector of `128` bits into a `__m128i` variable. At a hardware level, _unaligned_ data and _aligned_ data interfaces with the memory hardware slightly differently and can provide for some further slight speedups should data-alignment be guarenteed. No assumption about the memory alignment of the data that we are operating upon can be assumed so unaligned memory access will be used. When done with the vector-arithmetic, call an equivalent `_mm_storeu_si128` which stores our vector data into an unaligned memory address. This `SSSE3` implementation will go right above the previous `Swap64` implementation, ensuring that our algorithm exhausts as much of the larger chunks as possible before resorting to the smaller ones:

```cpp
#include <tmmintrin.h>

...
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
// Right above the Swap64 implementation...
for( std::size_t j = i ; j < ( (Count/2) / 8 ) ; ++j)
...
```

This basically implements a beefed-up 16-byte `bswap` using `SSSE3`. The heart of it all is the `_mm_shuffle_epi8` instruction which _shuffles_ the vector in the first argument according to the vector of byte-indices found in the second argument and returns this new _shuffled_ vector. A constant vector `ShuffleRev` is declared using `_mm_set_epi8` with each byte set to the index of where it should get its byte from(starting from least significant byte). You might read it as going from 0 to 15 in ascending order but this is actually indexing the bytes in reverse order which gives us a fully reversed 16-bit "chunk". Now for some speed tests.

Element Count|std::reverse|qReverse|Speedup Factor
---|---|---|---
8|26 ns|24 ns|**1.083**
16|30 ns|24 ns|**1.250**
32|37 ns|24 ns|**1.542**
64|52 ns|26 ns|**2.000**
128|79 ns|28 ns|**2.821**
256|139 ns|32 ns|**4.344**
512|264 ns|42 ns|**6.286**
1024|1135 ns|59 ns|**19.237**
100|70 ns|28 ns|**2.500**
1000|458 ns|59 ns|**7.763**
10000|7687 ns|370 ns|**20.776**
100000|63227 ns|5796 ns|**10.909**
1000000|671709 ns|61417 ns|**10.937**
59|52 ns|30 ns|**1.733**
79|61 ns|31 ns|**1.968**
173|106 ns|34 ns|**3.118**
6133|3602 ns|602 ns|**5.983**
10177|7440 ns|419 ns|**17.757**
25253|15913 ns|929 ns|**17.129**
31391|19867 ns|1839 ns|**10.803**
50432|32826 ns|3170 ns|**10.355**

Speedups of up to _**x20**_!.. but this is lower than the `bswap` version which reached up to _**x27**_? Maybe some prefetching might help this algorithm play nice with the cache(Todo)

# AVX2

The implementation can go even further to work with the even larger 256-bit registers that the `AVX/AVX2` extension provides and reverse *32 byte chunks* at a time. Implementation is very similar to the `SSSE3` one: load in *unaligned* data into a 256-bit register using the `__m256i` type. The issue with `AVX/AVX2` is that the `256-bit` register is actually two individual `128-bit` _lanes_ being operated in parallel as one larger `256-bit` register and overlaps in functionality with the `SSE` register almost as an additional layer of abstraction added upon `SSE`. Now here's where things get tricky, there is no `_mm256_shuffle_epi8` instruction that works like we think it would. Since it's just operating on two 128-bit lanes in parallel, `AVX/AVX2` instructions introduces a limitation in which some cross-lane arithmetic requires special cross-lane attention. Some instructions will accept 256-bit `AVX` registers but only actually operates upon 128-bit lanes. The trick here is that rather than trying to verse a 256-bit register atomically all in one go, instead reverse the bytes within the two 128-bit lanes, as if shuffling two 128-bit registers like in the `SSSE3` implementation, and then reverse the two 128-bit lanes themselves with whatever cross-lane arithmetic that _is_ available.

[_mm256_shuffle_epi8](https://software.intel.com/sites/landingpage/IntrinsicsGuide/#techs=AVX,AVX2&text=shuffle&expand=4726) is an `AVX2` instruction that shuffles the 128-bit lanes much like the `SSSE3` intrinsic so this can be taken care of first.

```cpp
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

// Reverse each the bytes in each 128-bit lane
Lower = _mm256_shuffle_epi8(Lower,ShuffleRev);
Upper = _mm256_shuffle_epi8(Upper,ShuffleRev);
```

[_mm256_permute2x128_si256](https://software.intel.com/sites/landingpage/IntrinsicsGuide/#techs=AVX,AVX2&text=shuffle&expand=3896) is another `AVX2` instruction that permutes the 128-bit lanes of two 256-bit registers:

![](/images/_mm256_permute2x128_si256.jpg)

Given two big 256-bit registers and an 8-byte immediate value it can select how the new 256-bit vector is going to be built. Pass in the same variable for both of the registers and "pick" from to simulate a big 128-bit cross-lane swap. This is pretty much the same algorithm as the "generic" `Swap64`/`Swap32`/`Swap16` functions above.

```cpp
...
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
// Right above the SSSE3 implementation
...
```

Benchmarks:

Element Count|std::reverse|qReverse|Speedup Factor
---|---|---|---
8|28 ns|25 ns|**1.120**
16|32 ns|26 ns|**1.231**
32|39 ns|25 ns|**1.560**
64|55 ns|26 ns|**2.115**
128|85 ns|28 ns|**3.036**
256|151 ns|31 ns|**4.871**
512|273 ns|37 ns|**7.378**
1024|515 ns|49 ns|**10.510**
100|72 ns|33 ns|**2.182**
1000|504 ns|54 ns|**9.333**
10000|8454 ns|268 ns|**31.545**
100000|69353 ns|4801 ns|**14.446**
1000000|637676 ns|49960 ns|**12.764**
59|54 ns|30 ns|**1.800**
79|63 ns|31 ns|**2.032**
173|106 ns|36 ns|**2.944**
6133|4237 ns|178 ns|**23.803**
10177|6738 ns|261 ns|**25.816**
25253|16497 ns|1327 ns|**12.432**
31391|18951 ns|1460 ns|**12.980**
50432|33806 ns|2155 ns|**15.687**

A speedup of up to _**x31**_!

# AVX512

`AVX512` is particularly rare out in the commercial world. Even so, we can take the algorithm that much more of a step forward and operate upon massive 512-bit bit registers. This will allow us to swap `64` bytes of data at once. At the moment, C and C++ compiler implementations of the `AVX512` instruction set are spotty at best. There is the benefit of the [_mm512_permutexvar_epi8](https://software.intel.com/sites/landingpage/IntrinsicsGuide/#=undefined&avx512techs=AVX512F,AVX512BW,AVX512CD,AVX512DQ,AVX512ER,AVX512IFMA52,AVX512PF,AVX512VBMI,AVX512VL&expand=4726,4038,4594,4594,4038&text=_mm512_permutexvar_epi8) instruction that will allow us to shuffle the 512-bit register with 8-bit indexes though there is not a confident implementation of [_mm512_set_epi8](https://software.intel.com/sites/landingpage/IntrinsicsGuide/#=undefined&avx512techs=AVX512F,AVX512BW,AVX512CD,AVX512DQ,AVX512ER,AVX512IFMA52,AVX512PF,AVX512VBMI,AVX512VL&expand=4726,4038,4594,4594&text=_mm512_set_epi8) to be found in MSVC or GCC. There is [_mm512_set_epi32](https://software.intel.com/sites/landingpage/IntrinsicsGuide/#=undefined&avx512techs=AVX512F,AVX512BW,AVX512CD,AVX512DQ,AVX512ER,AVX512IFMA52,AVX512PF,AVX512VBMI,AVX512VL&expand=4726,4038,4594,4594,4038,4587&text=_mm512_set_epi32) which will require generation of the `ShuffleRev` constant to use 32-bit integers rather than 8-bit integers.

```cpp
// Coulld have done:
const __m512i ShuffleRev = _mm512_set_epi8(
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
	16,17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
	32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
	48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63
);
// but instead have to do the more awkward:
const __m512i ShuffleRev = _mm512_set_epi32(
	0x00010203, 0x04050607, 0x08090a0b, 0x0c0d0e0f,
	...
);
```

This can be done manually or generated with a quick python one-liner:

```python
", ".join([hex(word[3] | word[2] << 8 | word[1] << 16 | word[0] << 24) for word in [(idx,idx+1,idx+2,idx+3) for idx in range(0,64,4)]])
```

The full `AVX512` implementation:

```cpp
...
	for( std::size_t j = i; j < ((Count / 2) / 64); ++j )
	{
		// no _mm512_set_epi8 despite intel pretending there is
		// _mm512_set_epi32 for now

		// Quick python script to generate this array
		//", ".join([hex(word[3] | word[2] << 8 | word[1] << 16 | word[0] << 24) for word in [(idx,idx+1,idx+2,idx+3) for idx in range(0,64,4)]])
		const __m512i ShuffleRev = _mm512_set_epi32(
			0x00010203, 0x04050607, 0x08090a0b, 0x0c0d0e0f,
			0x10111213, 0x14151617, 0x18191a1b, 0x1c1d1e1f,
			0x20212223, 0x24252627, 0x28292a2b, 0x2c2d2e2f,
			0x30313233, 0x34353637, 0x38393a3b,	0x3c3d3e3f
		);
		// Load 64 elements at once into one 64-byte register
		__m512i Lower = _mm512_loadu_si512(
			reinterpret_cast<__m512i*>(&Array8[i])
		);
		__m512i Upper = _mm512_loadu_si512(
			reinterpret_cast<__m512i*>(&Array8[Count - i - 64])
		);

		// Reverse the byte order of our 64-byte vectors
		Lower = _mm512_permutexvar_epi8(ShuffleRev,Lower);
		Upper = _mm512_permutexvar_epi8(ShuffleRev,Upper);

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
	// Above the AVX2 implementation
...
```

Since `AVX512` is pretty rare on consumer hardware at the moment: [Intel provides an emulator](https://software.intel.com/en-us/articles/intel-software-development-emulator) that can provide for some verification that the algorithm properly reverses the array. The emulator is no grounds for a proper hardware benchmark though. After creating a simple test program to verify that the array has been reversed it can be ran through the emulator and verified:

`sde64 -mix -no_shared_libs  -- ./Verify1 128`

```
Original:
0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47 48 49 50 51 52 53 54 55 56 57 58 59 60 61 62 63 64 65 66 67 68 69 70 71 72 73 74 75 76 77 78 79 80 81 82 83 84 85 86 87 88 89 90 91 92 93 94 95 96 97 98 99 100 101 102 103 104 105 106 107 108 109 110 111 112 113 114 115 116 117 118 119 120 121 122 123 124 125 126 127
Reversed:
127 126 125 124 123 122 121 120 119 118 117 116 115 114 113 112 111 110 109 108 107 106 105 104 103 102 101 100 99 98 97 96 95 94 93 92 91 90 89 88 87 86 85 84 83 82 81 80 79 78 77 76 75 74 73 72 71 70 69 68 67 66 65 64 63 62 61 60 59 58 57 56 55 54 53 52 51 50 49 48 47 46 45 44 43 42 41 40 39 38 37 36 35 34 33 32 31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
[PASS] Array Reversed
```

The `-mix` will cause the software development emulator to audit the execution of the program and the instructions it encounters into a `sde-mix-out.txt` file. This file is massive by default so `-no_shared_libs` removes the auditing of shared libraries(such as the standard libraries) from the report. With this the execution summery of `qreverse<1>` can be examined:

```
# $dynamic-counts-for-function: void qReverse<1ul>(void*, unsigned long)  IMG: Verify1 at [0x496f10d0a0, 0x496f10ddde)   1.347%
#
# TID 0
#       opcode                 count
#
...
*isa-ext-BASE                                                         65
*isa-set-AVX512F_512                                                   3
*isa-set-AVX512_VBMI_512                                               2
...
*category-AVX512_VBMI                                                  2
...
*avx512                                                                5

...

ADD                                                                    1
AND                                                                    1
CMP                                                                    7
JBE                                                                    4
JMP                                                                    2
JNB                                                                    2
JNZ                                                                    1
JZ                                                                     1
LEA                                                                    3
MOV                                                                   14
NOP                                                                    1
POP                                                                    7
PUSH                                                                   8
RET_NEAR                                                               1
SHL                                                                    1
SHR                                                                    9
SUB                                                                    2
VMOVDQA64                                                              1
VMOVDQU64                                                              2
VPERMB                                                                 2
*total                                                                70

```

Not only is AVX512 verified to have ran and worked but the entire reversal of `128` elements took only `70` instructions in total.

Todo: `AVX512` hardware benchmarks

# The "middle chunk" ( TODO )

Once we work our way down the middle and end up with something like `4` "middle" elements left then we are just one `Swap32` left from having the entire array reversed. What if we worked our way down to the middle and ended up with `5` elements though? This would not be possible actually so long as we have `Swap16`. `5` middle elements would mean we have `one middle element` with `two elements on either side`. Our `for( std::size_t j = i/2; j < ( (Count/2) / 2)` would catch that and `Swap16` the two elements on either side, getting us just `1` element left right in the middle which can stay right where it is within a reversed array(since the middle-most element in an odd-numbered array is our *pivot* and doesn't have to move anywhere).

TODO: Later we can find a way to accelerate our algorithm to have it consider these pivot-cases efficiently so that rather than calling two `Swap16`s on either half of a 4-byte case it could just call one last `Swap32` or even a bigger before it even parks itself in that situation of having to use the naive swap. Something like this could remove the use of the naive swap pretty much entirely.