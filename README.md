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
|||   0x000014b8 4883c701       add rdi, 1            ; Shift our index at
|||   0x000014bc 4883ee01       sub rsi, 1            ; both ends "inward" toward the middle
|||   0x000014c0 4839f7         cmp rdi, rsi
`===< 0x000014c3 72e9           jb 0x14ae
 ``-> 0x000014c5 f3c3           ret
```

An animation of the basic algorithm:

![](/images/Naive-Byte.gif)

Our own custom implementation of this algorithm to start us off:
We'll template the element-size at compile-time and emit a pseudo-structure that fits this size in an attempt to keep this illustrative implementation as generic as possible. By having the element-size be templated we can make specific template specializations for certain sizes while all other sizes fall-back to this naive algorithm. Having this done with a template allows this look-up to resolve at compile-time rather than check a "element-size" argument against a list of available implementations at run-time.

```cpp
template< std::size_t ElementSize >
inline void qReverse(void* Array, std::size_t Count)
{
	struct PodElement
	{
		std::uint8_t u8[ElementSize];
	};
	PodElement* ArrayN = reinterpret_cast<PodElement*>(Array);
	// We're only iterating through half of the size of the 
	for( std::size_t i = 0; i < Count / 2; ++i )
	{
		// Exchange the upper and lower element as we work our
		// way down to the middle from either end
		PodElement Temp(ArrayN[i]);
		ArrayN[i] = ArrayN[Count - i - 1];
		ArrayN[Count - i - 1] = Temp;
	}
}
```

**We can do better!** This "plain ol data" assumption can be made for lots of different types of data. Most usages of `struct` are intended to be treated as "bags of data" and do not have the limitation of additional memory-movement logic for copying or swapping since they are intended only to communicate a structure of interpretation of bytes. The more obvious case-study can also be having an array of `chars` found in an ASCII `string` or maybe a row of pixel data. **From this point on let's just assume that the sequence of data we are dealing with are to be these "bags of data" instances** that do not involve any kind of `operator=` or `Foo (const Foo&)` overhead so that we can just think of this data as bytes for our optimizations.

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
	// This tends to emit a 16-bit `ror` instruction
	return (
		((x & 0x00FF) <<  8) |
		((x & 0xFF00) >>  8)
	);
}
```

Most compilers are able to detect when an in-register endian-swap is being done and will emit `bswap` automatically or a similar intrinsic for your target architecture(The ARM architecture has the `REV` instruction for **armv6** or newer). Note also that `bswap16` is basically just a 16-bit rotate of 1 byte aka a `ror` instruction.

Using 32-bit `bswap`s, we can now take a 4-byte _chunk_ of bytes from either end into our registers, `bswap` the register, and then place the reversed _chunks_ at the opposite end! As we get closer to the center, we can use smaller 16-bit swaps(aka a 16-bit rotate) should we find ourselves with 2-byte chunks.

![](/images/bswap-32.gif)

and this of course can be expanded into a 64-bit `bswap` allowing for even larger chunks to be reversed at once:

![](/images/bswap-64.gif)

Given an array of `11` bytes that we want to reverse. We divide by two to get the number of _single-element_ swaps we would have to do(using integer arithmetic): `11/2 = 5`. So we need `5` single-element swaps that we would have to do at either end of our split array. Since we have a way to do `4` elements at once too, we can integer-divide this result `5` again by `4` to know how many _four-element_ swaps we we need to do(`5/4 = 1`). So we need only one `bswap`-swap and one `naive`-swap to fully reverse an 11-element array..

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

And now some benchmarks: on my _i3-6100_ with _8gb of DDR4 ram_. I automated the benchmark process across several different array-sizes giving each array-size `10,000` array-reversal trials before getting an average execution time for for that array width. Using g++ compile flags: `-m64 -Ofast -march=native` these are the results of comparing the execution time of the current bswap `qreverse` algorithm against `std::reverse`:

Element Count|std::reverse|qReverse|Speedup Factor
---|---|---|---
8|28 ns|25 ns|**1.120**
16|32 ns|25 ns|**1.280**
32|39 ns|27 ns|**1.444**
64|55 ns|34 ns|**1.618**
128|85 ns|34 ns|**2.500**
256|157 ns|38 ns|**4.132**
512|273 ns|45 ns|**6.067**
1024|515 ns|57 ns|**9.035**
100|72 ns|35 ns|**2.057**
1000|504 ns|56 ns|**9.000**
10000|5635 ns|954 ns|**5.907**
100000|67602 ns|3268 ns|**20.686**
1000000|631376 ns|43195 ns|**14.617**
59|48 ns|30 ns|**1.600**
79|57 ns|33 ns|**1.727**
173|101 ns|35 ns|**2.886**
6133|4538 ns|184 ns|**24.663**
10177|6959 ns|298 ns|**23.352**
25253|15970 ns|647 ns|**24.683**
31391|20050 ns|878 ns|**22.836**
50432|33307 ns|1658 ns|**20.089**

And so across the board we get speedups up to _**x24!**_ before dipping down again potentially due to cache line issues with larger data sizes.

# SIMD

The `bswap` instruction can reverse the byte-order of `2`, `4`, or `8` bytes, but several x86 extensions later and now it is possible to swap the byte order of `16`, `32`, even `64` bytes all at once through the use of `SIMD`. `SIMD` stands for *Single Instruction Multiple Data* and allows us to operate upon multiple lanes of data in parallel using only a single instruction. Much like our `bswap` which atomically reverses all four bytes in a register, `SIMD` provides and entire instruction set of arithmetic that allows us to manipulate multiple instances of data at once using a single instruction in parallel. These chunks of data that are operated upon tend to be called `vectors` of data. We will be able to suspend multiple bytes of data into a `vector` to reverse its order and place it on the opposite end similarly to our `bswap` implementation.

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

Some are kept around for compatibilities sake(`MMX`) and some are so recent, elusive, or so _vendor-specific_ to Intel or AMD that you're probably not likely to have a processor that features it(`SSE4a`). Some are very specific to enterprise hardware (such as `AVX512`) and are not likely to be on commercial hardware either.

At the moment (July 21, 2017) the steam hardware survey states that **94.42%** of all CPUs on Steam feature `SSSE3`([store.steampowered.com/hwsurvey/](http://store.steampowered.com/hwsurvey/)). Which is what we will be using in our first step into `SIMD` territory. `SSSE3` in particular due to its `_mm_shuffle_epi8` instruction which lets us _shuffle_ bytes within our 128-bit register with relative ease for our implementation.

# SSSE3

`SSE` stands for "Streaming SIMD Extensions" while `SSSE3` stands for "Supplemental Streaming SIMD Extensions 3" which is the _forth_ iteration of `SSE` technology. SSE introduces to us registers that allow for some `128` bit vector arithmetic. In C or C++ code we represent the intent to use these registers using types such as `__m128i` or `__m128d` which tell the compiler that any notion of _storage_ for these types should find their place within the 128-bit `SSE` registers when ever possible. We have intrinsics such as `_mm_add_epi8` which will add two `__m128i`s together, and treat them as a _vector_ of 8-bit elements. The `i` and `d` found in `__m128i` and `__m128d` are to notify intent of the 128-register's interpretation as `i`nteger and `d`ouble respectively. `__m128` is assumed to be a vector of four `floats` by default. Since we're just dealing with integer-byte data we care only for `__m128i` which gives access to the `_mm_shuffle_epi8` instruction.

Lets draft our code to get ready for some `SSSE3` byte swapping and create our own 16-bit `bswap` using `SSSE3`. First we need to `#include <tmmintrin.h>` in our C or C++ code to expose every intrinsic from `MMX` up until `SSSE3` to our code. Then use the instrinsic `_mm_loadu_si128` to `load` an `u`naligned `s`igned `i`nteger vector of `128` bits into a `__m128i` variable. At a hardware level, _unaligned_ data and _aligned_ data interfaces with the memory hardware slightly differently for some further slight speedups. We can make no assumption about the memory alignment of the data that we are operating upon so will go with the unaligned version. Once we are done with our arithmetic, we call an equivalent `_mm_storeu_si128` which stores our vector data into a memory address. Then we add an implementation right above out previous `Swap64` implementation, ensuring that our algorithm exhausts as much of the larger chunks as possible before resorting to the smaller ones:

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
// Right above our Swap64 implementation...
for( std::size_t j = i ; j < ( (Count/2) / 8 ) ; ++j)
...
```

This basically implements a beefed-up 16-byte `bswap` using `SSSE3`. The heart of it all is the `_mm_shuffle_epi8` instruction which _shuffles_ the vector in the first argument according to the vector of byte-indices found in the second argument and returns this new _shuffled_ vector. A constant vector `ShuffleRev` is declared using `_mm_set_epi8` with each byte set to the index of where it should get its byte from(Starting from least significant byte). You might read it as going from 0 to 15 in ascending order but this is actually indexing the bytes in reverse order which gives us a fully reversed 16-bit "chunk". And now for some new benchmarks:

Element Count|std::reverse|qReverse|Speedup Factor
---|---|---|---
8|28 ns|25 ns|**1.120**
16|32 ns|26 ns|**1.231**
32|39 ns|25 ns|**1.560**
64|55 ns|27 ns|**2.037**
128|86 ns|30 ns|**2.867**
256|151 ns|35 ns|**4.314**
512|273 ns|44 ns|**6.205**
1024|515 ns|64 ns|**8.047**
100|73 ns|30 ns|**2.433**
1000|504 ns|64 ns|**7.875**
10000|6124 ns|1080 ns|**5.670**
100000|63701 ns|5311 ns|**11.994**
1000000|662701 ns|59013 ns|**11.230**
59|48 ns|29 ns|**1.655**
79|58 ns|30 ns|**1.933**
173|104 ns|32 ns|**3.250**
6133|4181 ns|268 ns|**15.601**
10177|7378 ns|387 ns|**19.065**
25253|16441 ns|999 ns|**16.457**
31391|21943 ns|1162 ns|**18.884**
50432|33692 ns|2169 ns|**15.533**

# AVX2

Lets go one step further and work with the even larger 256-bit registers that the `AVX/AVX2` extension provides and reverse *32 byte chunks* at a time. Implementation is very similar to the `SSSE3` one: we load in *unaligned* data into our 256-bit registers using the `__m256i` type. The thing with `AVX` is that the `256-bit` register is actually two individual `128-bit` _lanes_ being operated in parallel as one larger `256-bit` register and overlaps in functionality with the `SSE` registers. Now here's where things get tricky, there is no `_mm256_shuffle_epi8` instruction that works like we think it would. Since we're just operating on 128-bit lanes in parallel, `AVX/AVX2` instructions introduces a limitation in which some cross-lane arithmetic requires special cross-lane attention. Some instructions will accept 256-bit `AVX` registers but only actually operates upon 128-bit lanes. The trick here is that rather than reverseing the 256-bits all in one go, instead we reverse the bytes of the 128-bit lanes, as if we were shuffling two 128-bit registers like in our `SSSE3` implementation, and then reverse the two 128-bit lanes themselves with whatever cross-lane arithmetic we _can_ do.

[_mm256_shuffle_epi8](https://software.intel.com/sites/landingpage/IntrinsicsGuide/#techs=AVX,AVX2&text=shuffle&expand=4726) is an `AVX2` instruction that shuffles the 128-bit lanes so we can get this out the way pretty easily.

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

Meaning we can pass in two big 256-bit registers, and use an 8-byte immediate value to pick how we want to build our new 256-bit vector. We can use this and pass in the same variable for both of the registers we are picking from to simulate a big 128-bit swap!

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

# AVX512 ( TODO )

# The middle chunk ( TODO )

Once we work our way down the middle and end up with something like `4` "middle" elements left then we are just one `Swap32` left from having the entire array reversed. What if we worked our way down to the middle and ended up with `5` elements though? This would not be possible actually so long as we have `Swap16`. `5` middle elements would mean we have `one middle element` with `two elements on either side`. Our `for( std::size_t j = i/2; j < ( (Count/2) / 2)` would catch that and `Swap16` the two elements on either side, getting us just `1` element left right in the middle which can stay right where it is within a reversed array(since the middle-most element in an odd-numbered array is our *pivot* and doesn't have to move anywhere).

TODO: Later we can find a way to accelerate our algorithm to have it consider these pivot-cases efficiently so that rather than calling two `Swap16`s on either half of a 4-byte case it could just call one last `Swap32` or even a bigger before it even parks itself in that situation of having to use the naive swap. Something like this could remove the use of the naive swap pretty much entirely.