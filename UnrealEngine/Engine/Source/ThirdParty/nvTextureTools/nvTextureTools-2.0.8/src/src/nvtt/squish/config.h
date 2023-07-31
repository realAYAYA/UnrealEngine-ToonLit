/* -----------------------------------------------------------------------------

	Copyright (c) 2006 Simon Brown                          si@sjbrown.co.uk

	Permission is hereby granted, free of charge, to any person obtaining
	a copy of this software and associated documentation files (the 
	"Software"), to	deal in the Software without restriction, including
	without limitation the rights to use, copy, modify, merge, publish,
	distribute, sublicense, and/or sell copies of the Software, and to 
	permit persons to whom the Software is furnished to do so, subject to 
	the following conditions:

	The above copyright notice and this permission notice shall be included
	in all copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
	OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF 
	MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
	IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY 
	CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
	TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE 
	SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
	
	This code includes modifications made by Epic Games to
	improve vectorization on some compilers and also
	target different micro architectures with the goal of improving
	the performance of texture compression. All modifications are
	clearly enclosed inside defines or comments.

   -------------------------------------------------------------------------- */
   
#ifndef SQUISH_CONFIG_H
#define SQUISH_CONFIG_H

// Set to 1 when building squish to use altivec instructions.
#ifndef SQUISH_USE_ALTIVEC
#	define SQUISH_USE_ALTIVEC defined(__VEC__)
#endif

// START EPIC MOD: Epic Games Optimizations for AVX2 Instruction Set.
// 
#ifndef SQUISH_USE_AVX2
#	if defined(__AVX2__)
#		define SQUISH_USE_AVX2 1
#	else
#		define SQUISH_USE_AVX2 0
#	endif
#endif

// FMA is turned off because the gains are negligible and it breaks compatibility between the SSE2 binaries and AVX2. 
// We strongly prefer having both binaries output the exact same results.
#define SQUISH_USE_FMADD   0   // 48.306 seconds -> 47.607 seconds (Use Fused Multiply Add)
#define SQUISH_USE_FNMADD  0   // 47.607 seconds -> 50.895 seconds (Use Fused Negative Multiply Add) (Slower on AMD TR 3970X for some reason)

// Force replacement of reciprocal by a proper division to improve both performance and precision.
// This also fixes non-determinism in the output depending on the CPUs executing the reciprocal.
// Turns out _mm_rcp_ps(_mm_set1_ps(39.0f)) yields different results on AMD TR 3970X vs Intel i7-7700k.
#define SQUISH_USE_DIV     1

// Benchmark for a single-threaded 8k texture compression
//
// When activating AVX2 without FMA
// - 80s to 52s on AMD TR 3970X    => MD5SUM c48818e129f98479389ddc831cda4591
// - 81s to 59s on Intel i7-7700k  => MD5SUM 5686d6d8046371687816e14463f57859
//
// When replacing reciprocal by division on SSE2
// - 80s to 73s on AMD TR 3970X    => MD5SUM 6a89b2e8bde4b0a3bbf15c05c32fd45e
// - 81s to 76s on Intel i7-7700k  => MD5SUM 6a89b2e8bde4b0a3bbf15c05c32fd45e
//
// When replacing reciprocal by division on AVX2
// - 52s to 47s on AMD TR 3970X    => MD5SUM 6a89b2e8bde4b0a3bbf15c05c32fd45e
// - 59s to 55s on Intel i7-7700k  => MD5SUM 6a89b2e8bde4b0a3bbf15c05c32fd45e

// SQUISH_USE_DIV = 0
//
// Image size compared: 8192x8192
// Total pixels: 67108864
// Color:
//   Mean absolute error: 0.687673
//   Max absolute error: 56.000000
//   Root mean squared error: 1.825307
//   Peak signal to noise ratio in dB: 42.904087

// SQUISH_USE_DIV = 1
//
// Image size compared : 8192x8192
// Total pixels : 67108864
// Color :
//   Mean absolute error : 0.687671
//   Max absolute error : 56.000000
//   Root mean squared error : 1.825304
//   Peak signal to noise ratio in dB : 42.904102
//

// The proof of non-determinism can be corroborated by comparing locally built textures using the same nvtt_64.dll as everyone else with the ones stored on the company's DDC.
// The company DDC is filled by multiple different computers having different CPUs, highlighting the fact that the results might differ among them.

// E:\LOCAL_DDC>find . | grep TEXTURE | sort > e:\texture_list.txt
// E:\LOCAL_DDC>head -n 10 e:\texture_list.txt | xargs -n 1 md5sum
// 1c5721416027dec78e11d4ecef123cd9 *./0/0/8/TEXTURE_5690602C18CA41E2B75D51939D40F224_AUTODXT_C55865344733442FBAD7D29E0B657__671F0346C1155109E3C719848EC48096CCE707A9.udd
// 1602fcecf6645332e2ab9f445990e185 *./0/1/0/TEXTURE_5690602C18CA41E2B75D51939D40F224_BGRA8_0DDBB0A4478D1C3B495F6DAAC809F66__8BA44617FC701C807E6E5679FAF0CDFA2B51BBFF.udd
// e923970f1030448b59b553c5bf1d776a *./0/1/2/TEXTURE_5690602C18CA41E2B75D51939D40F224_AUTODXT_47CBF2084A6C13E819FFCFBA1903D__BA9DBBB87785B7674E1165908D664D8FA182A9CC.udd
// 7682a4173d0c88cedc468e7fbff68fa9 *./0/1/3/TEXTURE_5690602C18CA41E2B75D51939D40F224_AUTODXT_CCC0A2B142A76632BAA6D68232E71__5C2B94EB1FF48E3BE06CEF42DF13BA5F4796B69B.udd
// 079ed17c2596b6516e0ccd4f8aef1568 *./0/1/4/TEXTURE_5690602C18CA41E2B75D51939D40F224_AUTODXT_3844E1CA48EE4C371C379B9B1A5CD__9E47D1B8B3C9D4EB27BB66E178A89284FC68C21F.udd
// be0711fe3b232b35e3a5b45595f9adeb *./0/1/4/TEXTURE_5690602C18CA41E2B75D51939D40F224_AUTODXT_A730B3A04AE8B61986F397A6D3680__E9D3B0F7A8E883EE17F08CF914F576A67EDA9407.udd
// 70cceb37461a00f1d15edddc0798fa2f *./0/2/0/TEXTURE_5690602C18CA41E2B75D51939D40F224_AUTODXT_B21C1AFF42ACD5ADC8C382B3A090A__836F15C139B2E9FCAD589C7D8027B9CE568D87DD.udd
// bc9df048264970f1712e197a57489ecb *./0/2/4/TEXTURE_5690602C18CA41E2B75D51939D40F224_AUTODXT_AB88C9A54C0E211DD5D0F2B2ED704__26B78E4F39EF80975F2DF6B13737823299F5FB0D.udd
// a32319a58acdc5df73b15142cf04692a *./0/2/8/TEXTURE_5690602C18CA41E2B75D51939D40F224_BGRA8_A1F5913543DF9135D2D3B9B6D434494__F76A1130C25FF86610489CA3E948ED4749EAA435.udd
// 8080fb3406c05e2a941b7c7eaf914bef *./0/2/8/TEXTURE_5690602C18CA41E2B75D51939D40F224_BGRA8_EADAB3F04B17D7146C021C968EB3C5A__84EA5C528A12061E16525A437D450F4226DA025F.udd

// Y:\EPIC_DDC>head -n 10 e:\texture_list.txt | xargs -n 1 md5sum
// 1c5721416027dec78e11d4ecef123cd9 *./0/0/8/TEXTURE_5690602C18CA41E2B75D51939D40F224_AUTODXT_C55865344733442FBAD7D29E0B657__671F0346C1155109E3C719848EC48096CCE707A9.udd
// 89dab92e8869f01d6def95c4b66ec653 *./0/1/0/TEXTURE_5690602C18CA41E2B75D51939D40F224_BGRA8_0DDBB0A4478D1C3B495F6DAAC809F66__8BA44617FC701C807E6E5679FAF0CDFA2B51BBFF.udd
// a4113e71fb7f1d9b3283d8a3b03f562f *./0/1/2/TEXTURE_5690602C18CA41E2B75D51939D40F224_AUTODXT_47CBF2084A6C13E819FFCFBA1903D__BA9DBBB87785B7674E1165908D664D8FA182A9CC.udd
// 134b7b3d6d87e4b5f2a95313a7d1108d *./0/1/3/TEXTURE_5690602C18CA41E2B75D51939D40F224_AUTODXT_CCC0A2B142A76632BAA6D68232E71__5C2B94EB1FF48E3BE06CEF42DF13BA5F4796B69B.udd
// a63210df6b4c0a97940ee07f97c66941 *./0/1/4/TEXTURE_5690602C18CA41E2B75D51939D40F224_AUTODXT_3844E1CA48EE4C371C379B9B1A5CD__9E47D1B8B3C9D4EB27BB66E178A89284FC68C21F.udd
// 97bc62dee58dcf7ddb98d7d2d1516d22 *./0/1/4/TEXTURE_5690602C18CA41E2B75D51939D40F224_AUTODXT_A730B3A04AE8B61986F397A6D3680__E9D3B0F7A8E883EE17F08CF914F576A67EDA9407.udd
// 12972e639761dcbbe68a93ad980b782e *./0/2/0/TEXTURE_5690602C18CA41E2B75D51939D40F224_AUTODXT_B21C1AFF42ACD5ADC8C382B3A090A__836F15C139B2E9FCAD589C7D8027B9CE568D87DD.udd
// cc4d2aba7b1cbac601be7faa36cc258a *./0/2/4/TEXTURE_5690602C18CA41E2B75D51939D40F224_AUTODXT_AB88C9A54C0E211DD5D0F2B2ED704__26B78E4F39EF80975F2DF6B13737823299F5FB0D.udd
// af314126492fc0e58cd19078abfd15df *./0/2/8/TEXTURE_5690602C18CA41E2B75D51939D40F224_BGRA8_A1F5913543DF9135D2D3B9B6D434494__F76A1130C25FF86610489CA3E948ED4749EAA435.udd
// d8f4d76889c26548652f6c8e57b3994e *./0/2/8/TEXTURE_5690602C18CA41E2B75D51939D40F224_BGRA8_EADAB3F04B17D7146C021C968EB3C5A__84EA5C528A12061E16525A437D450F4226DA025F.udd

// END EPIC MOD: Epic Games Optimizations for AVX2 Instruction Set.

// Set to 1 when building squish to use sse instructions.
#ifndef SQUISH_USE_SSE
#	if defined(__SSE2__)
#		define SQUISH_USE_SSE 2
#	elif defined(__SSE__)
#		define SQUISH_USE_SSE 1
#	else
#		define SQUISH_USE_SSE 0
#	endif
#endif

// Internally et SQUISH_USE_SIMD when either altivec or sse is available.
#if SQUISH_USE_ALTIVEC && SQUISH_USE_SSE
#	error "Cannot enable both altivec and sse!"
#endif
#if SQUISH_USE_ALTIVEC || SQUISH_USE_SSE
#	define SQUISH_USE_SIMD 1
#else
#	define SQUISH_USE_SIMD 0
#endif

#endif // ndef SQUISH_CONFIG_H
