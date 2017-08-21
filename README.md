# qReverse [![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](https://raw.githubusercontent.com/Wunkolo/qreverse/master/LICENSE)

### qReverse is an architecture-accelerated array reversal algorithm intended as a personal study to design a fast AoS reversal algorithm utilizing SIMD.

---

Standard array reversal algorithms that you typically see involves swapping either end of the array working your way down to the middle. Note the following approach treats the array elements as "objects" and will call overloaded class operators such as `operator=` or a `copy by reference` function during the creation of the intermediate swap buffer upon each swap to make the exchange. Some implementations would internally use an intermediate temporary variable to make the exchange which would require a minimum of two calls to your object's `operator=` and at least one call to your object's `copy by reference` constructor. `std::swap` also allows a custom overload `swap` function to be used in place of the generic one and this overload will be called if defined within the same namespace as your class allowing for some speedups.

```c++
#include <algorithm>
#include <cstddef>

template< typename ElementType >
void Reverse( ElementType* Array, std::size_t Count )
{
	// We're only iterating through half of the size of the array
	for( std::size_t i = 0; i < Count/2 ; ++i)
	{
		// Exchange the upper and lower element as we work our
		// way down to the middle from either end
		std::swap(
			Array[i],             // "lower" element
			Array[Count - i - 1]  // "upper" element
		);
	}
}
```

In some situations the data that you are dealing with does not implement any overloads for `operator=` or `copy by reference` and can simply be interpreted as the raw bytes that make up the objects at run-time. Data such as the `char`s found in a string or an _Array Of Structures_ that implement no additional memory-movement logic. Most usages of `struct` are intended to be treated as "bags of data" and do not have the limitation of additional memory-movement logic for copying or swapping. They simply represent a structure of interpretation for byte-data.

For a regular 1-byte element with no overload overhead, `std::reverse` for byte elements of `std::uint8_t` emits the following assembly in gcc:

Without this limitation it is possible to accelerate the swapping of two elements of an array in an architecture-aware way. An array of one-byte elements on a 32-bit machine can load four-bytes(four elements) of data into a register at once, and use the 16/32/64 bit `bswap` instruction to rapidly reverse a small chunk of elements before placing it at the other end of the array.

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
|||   0x000014bc 4883ee01       sub rsi, 1            ; both ends "inward"
|||   0x000014c0 4839f7         cmp rdi, rsi
`===< 0x000014c3 72e9           jb 0x14ae
 ``-> 0x000014c5 f3c3           ret                   ; We can do better!
```


## bswap

The `bswap` instruction reverses the individual bytes of a register and is typically used to swap the `endian` of an integer to exchange between `host` and `network` byte-order(see `htons`,`htonl`,`ntohs`,`ntohl`). Most x86 compilers implement assembly intrinsics that you can put right into your C or C++ code to get the compiler to emit the `bswap` instruction directly:

**MSVC:**
- `_byteswap_uint64`
- `_byteswap_ulong`
- `_byteswap_ushort`

**GCC/Clang:**

- `_builtin_bswap64`
- `_builtin_bswap32`
- `_builtin_bswap16`

The x86 header `immintrin.h` also includes `_bswap` and `_bswap64`. Otherwise a more generic and portable implementation can be used.

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

**Now** we can specialize our accelerated implementation for 1-byte elements that have no swap-related overloads with our new algorithm that can swap 4-bytes at a time! Given the size of our array we can determine exactly how many times we will be using new 4-byte swap trick.

Using 32-bit `bswap`s, we can swap 4 elements from either end at a time. So we get the half-size count that we used during a naive swap(when we cut our array in half and start swapping either end), and adjust our algorithm to the fact that we can now do 4 elements at once.

Given an array of `11` bytes that we want to reverse. We divide by two to get the number of _single-element_ swaps we would have to do(using integer arithmetic): `11/2 = 5`. So we need `5` single-element swaps that we would have to do at either end of our split array. Since we have a way to do `4` elements at once too, we can integer-divide this result `5` again by `4` to know how many _four-element_ swaps we we need to do(`5/4 = 1`). So we need only one `bswap`-swap and one `naive`-swap to fully reverse an 11-element array..

```cpp
// Reverse an array of 1-byte elements(such as std::uint8_t)
// A specialization of the above implementation for 1-byte elements
// Does not call assignment or copy overloads
// Accelerated using - 32-bit bswap instruction
template<>
void qReverse<std::uint8_t>( std::uint8_t* Array, std::size_t Count )
{
	std::size_t i = 0;

	// Using a new iteration variable "j" to illustrate that we know
	// the exact amount of times we have to use our 32-bit bswap
	for( std::size_t j = i ; j < ( (Count/2) / 4 ) ; ++j)
	{
		// Get bswapped versions of our Upper and Lower 4-byte chunks
		std::uint32_t Lower = Swap32(
			*reinterpret_cast<std::uint32_t*>( &Array[i] )
		);
		std::uint32_t Upper = Swap32(
			*reinterpret_cast<std::uint32_t*>( &Array[Count - i - 4] )
		);

		// Place them at their swapped position
		*reinterpret_cast<std::uint32_t*>( &Array[i] ) = Upper;
		*reinterpret_cast<std::uint32_t*>( &Array[Count - i - 4] ) = Lower;

		// Four elements at a time
		i += 4;
	}

	// Everything else that we can not do a 4-byte bswap on, we swap normally
	// Naive swaps
	for( ; i < Count/2 ; ++i)
	{
		// Exchange the upper and lower element as we work our
		// way down to the middle from either end
		std::swap(
			Array[i],             // "lower" element
			Array[Count - i - 1]  // "upper" element
		);
	}
}
```

And now some benchmarks: on my _i3-6100_. I automated the benchmark process across several different array-sizes giving each array-size `10,000` array-reversal trials before getting an average execution time for for that specific array width. Using g++ compile flags: `-m64 -Ofast -march=skylake`.

Element Count|std::reverse|qReverse|Speedup Factor
---|---|---|---
8|21 ns|23 ns|0.913
16|22 ns|23 ns|0.957
32|26 ns|25 ns|**1.040**
64|35 ns|28 ns|**1.250**
128|50 ns|31 ns|**1.613**
256|90 ns|33 ns|**2.727**
512|158 ns|35 ns|**4.514**
1024|298 ns|44 ns|**6.773**
100|46 ns|29 ns|**1.586**
1000|291 ns|44 ns|**6.614**
10000|2737 ns|199 ns|**13.754**
100000|27547 ns|1722 ns|**15.997**
1000000|279491 ns|21477 ns|**13.014**
59|36 ns|29 ns|**1.241**
79|39 ns|31 ns|**1.258**
173|62 ns|32 ns|**1.938**
6133|1685 ns|133 ns|**12.669**
10177|2781 ns|200 ns|**13.905**
25253|6866 ns|456 ns|**15.057**
31391|8543 ns|566 ns|**15.094**
50432|13890 ns|886 ns|**15.677**


Looks like a nice speedup across the board except for the much smaller array sizes suffering some very slight overhead, more on that later. Let's augment even further with the *16* and *64* bit `bswap` to try and shear away more of our algorithm away from having to reach the slower `std::swap` loop.

We can simply edit the `bswap` routine we already made and edit it to 16-bit and 64-bit types, size, and offsets. We also should make sure to place our smaller-sized chunk-swaps to the bottom of our chunk-swap-chain and largest at the top. That way our algorithm will move the large outer chunks first before it moves down to the smaller more individual ones and eventually work its way down to the edge-case naive swaps(which it may not even have to do for some array sizes). Also since these `bswap`s work their way down and pick up right where the previous one left off they can pick up the same `i` variable and very quickly process how many 4-byte or 2-byte chunk-swaps need to happen with some simple division. Since we're dealing with power-of-two chunk sizes, this division would typically reduce down to a simple bit-shift to the right(those of you that dont know, a division by a power of 2 is the same as a bit-shift to the right).

```cpp
template<>
void qReverse<std::uint8_t>( std::uint8_t* Array, std::size_t Count )
{
	std::size_t i = 0;

	for( std::size_t j = i ; j < ( (Count/2) / 8 ) ; ++j)
	{
		// Get bswapped versions of our Upper and Lower 8-byte chunks
		std::uint64_t Lower = Swap64(
			*reinterpret_cast<std::uint64_t*>( &Array[i] )
		);
		std::uint64_t Upper = Swap64(
			*reinterpret_cast<std::uint64_t*>( &Array[Count - i - 8] )
		);

		// Place them at their swapped position
		*reinterpret_cast<std::uint64_t*>( &Array[i] ) = Upper;
		*reinterpret_cast<std::uint64_t*>( &Array[Count - i - 8] ) = Lower;

		// Eight elements at a time
		i += 8;
	}
	for( std::size_t j = i/4 ; j < ( (Count/2) / 4 ) ; ++j)
	{
		// Get bswapped versions of our Upper and Lower 4-byte chunks
		std::uint32_t Lower = Swap32(
			*reinterpret_cast<std::uint32_t*>( &Array[i] )
		);
		std::uint32_t Upper = Swap32(
			*reinterpret_cast<std::uint32_t*>( &Array[Count - i - 4] )
		);

		// Four elements at a time
		i += 4;
	}
	for( std::size_t j = i/2 ; j < ( (Count/2) / 2 ) ; ++j)
	{
		// Get bswapped versions of our Upper and Lower 2-byte chunks
		std::uint16_t Lower = Swap16(
			*reinterpret_cast<std::uint16_t*>( &Array[i] )
		);
		std::uint16_t Upper = Swap16(
			*reinterpret_cast<std::uint16_t*>( &Array[Count - i - 2] )
		);

		// Place them at their swapped position
		*reinterpret_cast<std::uint16_t*>( &Array[i] ) = Upper;
		*reinterpret_cast<std::uint16_t*>( &Array[Count - i - 2] ) = Lower;

		// Two elements at a time
		i += 2;
	}

	// Once we've depleted all of our chunk-swapping methods, we swap normally
	for( ; i < Count/2 ; ++i)
	{
		// Exchange the upper and lower element as we work our
		// way down to the middle from either end
		std::swap(
			Array[i],             // "lower" element
			Array[Count - i - 1]  // "upper" element
		);
	}
}
```

So now we have our fully `bswap`-accelerated byte reversal function to put to the test:

Element Count|std::reverse|qReverse|Speedup Factor
---|---|---|---
8|21 ns|26 ns|0.808
16|23 ns|24 ns|0.958
32|27 ns|25 ns|**1.080**
64|35 ns|27 ns|**1.296**
128|52 ns|29 ns|**1.793**
256|90 ns|32 ns|**2.812**
512|160 ns|34 ns|**4.706**
1024|299 ns|42 ns|**7.119**
100|48 ns|30 ns|**1.600**
1000|292 ns|45 ns|**6.489**
10000|2734 ns|197 ns|**13.878**
100000|27582 ns|1743 ns|**15.824**
1000000|279448 ns|21412 ns|**13.051**
59|36 ns|30 ns|**1.200**
79|39 ns|33 ns|**1.182**
173|65 ns|33 ns|**1.970**
6133|1678 ns|132 ns|**12.712**
10177|2785 ns|199 ns|**13.995**
25253|6860 ns|454 ns|**15.110**
31391|8539 ns|565 ns|**15.113**
50432|13889 ns|882 ns|**15.747**

The additional 16 and 64 bit `bswap` functions just slightly increased all of our speedups! Across the board we got a very  steep speedup over `std::reverse` using only `bswap`! These neat power-of-two numbers would mostly call our `Swap64` almost entirely and may never even have to touch the `std::swap` loop at the bottom. When we use less-factorable numbers such as `10,000` or a prime number such as `10,177` then a different mix of `Swap64`, `Swap32` and `Swap16` get called as the algorithm approaches the center which may or may not call the `std::swap` routine after all!

If we feed our algorithm array sizes of around 16 or lower though we start to reach a bit of a performance loss. Each routine in our chain of chunk-swapping loops comes with some _conditional code tax_ before finally falling back to the `std::swap` method that `std::reverse` would have already been doing right from the get-go. So long as we feed our algorithm arrays greater than about 16 characters then we're benefiting from a potentially huge speedup as our array size increases.

Can we do a little better? What about those "middle" cases.

# The middle "chunk" (TODO)

That final naive `std::swap` loop used for the middle-cases that our `bswap` doesn't handle is the bottle neck of our algorithm. We can get a bit of speedup by recognizing that once we work our way down to the middle we can potentially do just one more `Swap16`,`Swap32` or `Swap64` to flip the _middle-most-chunk_ and have a completely reversed array. Since our `bswap` instruction works only on power-of-two sized chunks of memory we can squeeze this extra inch of speed when our middle-case is that we have `8`,`4` or `2` bytes left.

Once we work our way down the middle and end up with something like  `4` "middle" elements left then we are just one `Swap32` left from having the entire array reversed. What if we worked our way down to the middle and ended up with `5` elements though? This would not be possible actually so long as we have `Swap16`. `5` middle elements would mean we have `one middle element` with `two elements on either side`. Our `for( std::size_t j = i/2; j < ( (Count/2) / 2)` would catch that and `Swap16` the two elements on either side, getting us just `1` element left right in the middle which can stay right where it is within a reversed array(since the middle-most element in an odd-numbered array is our *pivot* and doesn't have to move anywhere).

TODO: Later we can find a way to accelerate our algorithm to have it consider these pivot-cases efficiently so that rather than calling two `Swap16`s on either half of 4-bytes, it could just call one last `Swap32` or even a bigger `Swap64` before it even parks itself in that situation. Something like this could remove the use of the naive `std::swap` pretty much entirely.

## SIMD

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

## SSSE3

`SSE` stands for "Streaming SIMD Extensions" while `SSSE3` stands for "Supplemental Streaming SIMD Extensions 3" which is the _forth_ iteration of `SSE` technology. SSE introduces to us registers that allow for some `128` bit vector arithmetic. In C or C++ code we represent the intent to use these registers using types such as `__m128i` or `__m128d` which tell the compiler that any notion of _storage_ for these types should find their place within the 128-bit `SSE` registers when ever possible. We have intrinsics such as `_mm_add_epi8` which will add two `__m128i`s together, and treat them as a _vector_ of 8-bit elements. The `i` and `d` found in `__m128i` and `__m128d` are to notify intent of the 128-register's interpretation as `i`nteger and `d`ouble respectively. `__m128` has no notion of the type interpretation and is assumed to be a vector of four `floats` by default. Since we're just dealing with bytes we want to swap we care only for `__m128i` which gives us access to `_mm_shuffle_epi8`.

Lets draft our code to get ready for some `SSSE3` byte swapping and create our own 16-bit `bswap` using `SSSE3`. First thing's first, we need to `#include <tmmintrin.h>` in our C or C++ code to expose every intrinsic from `MMX` up until `SSSE3` to our code. Then use the instrinsic `_mm_loadu_si128` to `load` an `u`naligned `s`igned `i`nteger vector of `128` bits into a `__m128i` variable. At a hardware level, _unaligned_ data and _aligned_ data interfaces with the memory hardware slightly differently. Guarantee that you will be accessing aligned data 100% of the time provides for some slight speedups though we are dealing with generally unpredictable data and can not make any assumptions about its alignment. Once we are done with our arithmetic, we call an equivalent `_mm_storeu_si128` which stores our vector data into a memory address.

```cpp
#include <tmmintrin.h>

...
template<>
void qReverse<std::uint8_t>( std::uint8_t* Array, std::size_t Count )
{
	std::size_t i = 0;
	for( std::size_t j = i ; j < ( (Count/2) / 16 ) ; ++j)
	{
		// Load 16 elements at once into one 16-byte integer register
		__m128i Lower = _mm_loadu_si128(
			reinterpret_cast<__m128i*>( &Array[i] )
		);
		__m128i Upper = _mm_loadu_si128(
			reinterpret_cast<__m128i*>( &Array[Count - i - 16] )
		);
		// Our constant shuffle-vector
		// This vector tells _mm_shuffle_epi8 where each
		// byte should get its new byte from
		const __m128i ShuffleMap = _mm_set_epi8(
			0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
		);
		Lower = _mm_shuffle_epi8(Lower,ShuffleMap);
		Upper = _mm_shuffle_epi8(Upper,ShuffleMap);

		// Place them at their swapped position
		_mm_storeu_si128(
			reinterpret_cast<__m128i*>( &Array[i] ),
			Upper
		);
		_mm_storeu_si128(
			reinterpret_cast<__m128i*>( &Array[Count - i - 16] ),
			Lower
		);

		// Sixteen elements at a time
		i += 16;
	}
	// Right above our Swap64 implementation...
	for( std::size_t j = i ; j < ( (Count/2) / 8 ) ; ++j)
...
```

This basically implements a beefed-up 16-byte `bswap` using `SSSE3`. The heart of it all is the `_mm_shuffle_epi8` instruction which _shuffles_ the vector in the first argument according to the vector of byte-indices found in the second argument and returns this new _shuffled_ vector. A constant vector `ShuffleMap` is declared using `_mm_set_epi8` with each byte set to the index of where it should get its byte from(Starting from least significant byte). You might read it as going from 0 to 15 in ascending order but this is actually indexing the bytes in reverse order which gives us a fully reversed 16-bit "chunk". Now for the punch-line.

Element Count|std::reverse|qReverse|Speedup Factor
---|---|---|---
8|28 ns|33 ns|0.848
16|30 ns|33 ns|0.909
32|36 ns|32 ns|**1.125**
64|47 ns|33 ns|**1.424**
128|67 ns|34 ns|**1.971**
256|120 ns|37 ns|**3.243**
512|195 ns|38 ns|**5.132**
1024|341 ns|45 ns|**7.578**
100|45 ns|28 ns|**1.607**
1000|300 ns|45 ns|**6.667**
10000|2738 ns|217 ns|**12.618**
100000|27541 ns|2372 ns|**11.611**
1000000|279477 ns|27143 ns|**10.296**
59|36 ns|29 ns|**1.241**
79|40 ns|31 ns|**1.290**
173|62 ns|31 ns|**2.000**
6133|1680 ns|155 ns|**10.839**
10177|2784 ns|231 ns|**12.052**
25253|6862 ns|547 ns|**12.545**
31391|8546 ns|719 ns|**11.886**
50432|13889 ns|1203 ns|**11.545**

So it looks like by reversing chunks of 16-byte elements using SSSE3 we barely got any speed up. In fact some of our larger arrays being reversed have turned with lower speedups than without it. Though our small-array-size scores have skewed a bit for the better in which the cost of overhead and the boost in using 16-byte chunks gave us a very slight benefit over `std::reverse` (As a side note I should mention that if we removed everything _except_ our SSSE3 and the naive swap we save ourselves some of the overhead of the other swapping methods and proportionally get some very sub-fractional speedups for the larger data sets ).

## AVX/AVX2

Just to prove a point, lets go one step further and work with the even larger 256-bit registers that the `AVX/AVX2` extension provides and reverse *32 byte chunks* at a time. Implementation is very similar to the `SSSE3` one: we load in *unaligned* data into our 256-bit registers using the `__m256i` type. The thing with `AVX` is that the `256-bit` register is actually two individual `128-bit` _lanes_ being operated in parallel as one larger `256-bit` register and overlaps in functionality with the `SSE` registers. This will be a bit of a problem later on.

Access to `AVX\AVX2` intrinsics requires inclusion of the `immintrin.h` header in our source file. and we model our algorithm to read our larger spanned values much like for `SSE` and `bswap`:

```cpp
for( std::size_t j = i ; j < ( (Count/2) / 32 ) ; ++j )
{
	// Load 32 elements at once into one 32-byte register
	__m256i Lower = _mm256_loadu_si256(
		reinterpret_cast<__m256i*>( &Array[i] )
	);
	__m256i Upper = _mm256_loadu_si256(
		reinterpret_cast<__m256i*>( &Array[Count - i - 32] )
	);

	// Reverse the byte order of our 32-byte vectors
	// --- our AVX vector-reversal algorithm here ---

	// Place them at their swapped position
	_mm256_storeu_si256(
		reinterpret_cast<__m256i*>( &Array[i] ),
		Upper
	);
	_mm256_storeu_si256(
		reinterpret_cast<__m256i*>( &Array[Count - i - 32] ),
		Lower
	);

	// 32 elements at a time
	i += 32;
}
```

Now here's where things get tricky, there is no `_mm256_shuffle_epi8` instruction available for us to use. Since we're just operating on `128-bit` lanes in parallel `AVX` instructions introduces a limitation in which cross-lane arithmetic requires some special attention. So now we have to get clever in figuring out a way to simulate a big `256-bit bswap` using `AVX`. According to [software.intel.com/sites/landingpage/IntrinsicsGuide/#techs=AVX](https://software.intel.com/sites/landingpage/IntrinsicsGuide/#techs=AVX) by limiting ourselves to just `AVX` and not `AVX2` just yet, we are limited to no bit shift functions or shuffle functions that allows us to address individual bytes. We do have an instruction `_mm256_permute2f128_si256` which allows for a shuffling of `128-bit` integers across lanes. `_mm256_permute2f128_si256` takes three arguments: two `256-bit` vectors and one 8-bit integer. The instruction uses the 2-bit indices of the immediate value to _select_ `128-bit` elements from the first two arguments, and creates a new `256-bit` vector. So basically, it will get our first two arguments and pretend like it is an array of _four 128-bit integers_ and create a new `256-bit` integer by using the _2-bit indices_ of our _8bit immediate value_.
