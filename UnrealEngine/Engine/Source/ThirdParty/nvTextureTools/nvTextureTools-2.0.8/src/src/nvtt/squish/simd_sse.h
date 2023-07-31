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
   
#ifndef SQUISH_SIMD_SSE_H
#define SQUISH_SIMD_SSE_H

#include <xmmintrin.h>
#if ( SQUISH_USE_SSE > 1 )
#include <emmintrin.h>
#endif
// START EPIC MOD: Add AVX2 intrinsics include file
#if SQUISH_USE_AVX2
#include <immintrin.h>
#endif
// END EPIC MOD: Add AVX2 intrinsics include file
#include <cassert>

#define SQUISH_SSE_SPLAT( a )										\
	( ( a ) | ( ( a ) << 2 ) | ( ( a ) << 4 ) | ( ( a ) << 6 ) )

namespace squish {

#define VEC4_CONST( X ) Vec4( _mm_set1_ps( X ) )
// START EPIC MOD: Add zero initialization of Vec4
#define VEC4_ZERO() Vec4( _mm_setzero_ps() )
// END EPIC MOD: Add zero initialization of Vec4

class Vec4
{
// START EPIC MOD: Vec8 needs access to private __m128 value
	friend class Vec8;
// END EPIC MOD: Vec8 needs access to private __m128 value
public:
	typedef Vec4 const& Arg;

	Vec4() {}
		
	explicit Vec4( __m128 v ) : m_v( v ) {}
	
	Vec4( Vec4 const& arg ) : m_v( arg.m_v ) {}
	
	Vec4& operator=( Vec4 const& arg )
	{
		m_v = arg.m_v;
		return *this;
	}

	Vec4( const float * v )
	{
		m_v = _mm_load_ps( v );
	}

	Vec4( float x, float y, float z, float w )
	{
		m_v = _mm_setr_ps( x, y, z, w );
	}
	
	Vec3 GetVec3() const
	{
		SQUISH_ALIGN_16 float c[4];
		_mm_store_ps( c, m_v );
		return Vec3( c[0], c[1], c[2] );
	}
	
	Vec4 SplatX() const { return Vec4( _mm_shuffle_ps( m_v, m_v, SQUISH_SSE_SPLAT( 0 ) ) ); }
	Vec4 SplatY() const { return Vec4( _mm_shuffle_ps( m_v, m_v, SQUISH_SSE_SPLAT( 1 ) ) ); }
	Vec4 SplatZ() const { return Vec4( _mm_shuffle_ps( m_v, m_v, SQUISH_SSE_SPLAT( 2 ) ) ); }
	Vec4 SplatW() const { return Vec4( _mm_shuffle_ps( m_v, m_v, SQUISH_SSE_SPLAT( 3 ) ) ); }

	Vec4& operator+=( Arg v )
	{
		m_v = _mm_add_ps( m_v, v.m_v );
		return *this;
	}
	
	Vec4& operator-=( Arg v )
	{
		m_v = _mm_sub_ps( m_v, v.m_v );
		return *this;
	}
	
	Vec4& operator*=( Arg v )
	{
		m_v = _mm_mul_ps( m_v, v.m_v );
		return *this;
	}
	
	friend Vec4 operator+( Vec4::Arg left, Vec4::Arg right  )
	{
		return Vec4( _mm_add_ps( left.m_v, right.m_v ) );
	}
	
	friend Vec4 operator-( Vec4::Arg left, Vec4::Arg right  )
	{
		return Vec4( _mm_sub_ps( left.m_v, right.m_v ) );
	}
	
	friend Vec4 operator*( Vec4::Arg left, Vec4::Arg right  )
	{
		return Vec4( _mm_mul_ps( left.m_v, right.m_v ) );
	}
	
	//! Returns a*b + c
	friend Vec4 MultiplyAdd( Vec4::Arg a, Vec4::Arg b, Vec4::Arg c )
	{
// START EPIC MOD: Add proper FMA intrinsics but only if opted-in
#if SQUISH_USE_FMADD
		return Vec4( _mm_fmadd_ps(a.m_v, b.m_v, c.m_v ) );
#else
		return Vec4(_mm_add_ps(_mm_mul_ps(a.m_v, b.m_v), c.m_v));
#endif
// END EPIC MOD: Add proper FMA intrinsics but only if opted-in
	}
	
	//! Returns -( a*b - c )
	friend Vec4 NegativeMultiplySubtract( Vec4::Arg a, Vec4::Arg b, Vec4::Arg c )
	{
// START EPIC MOD: Add proper FMA intrinsics but only if opted-in
#if SQUISH_USE_FNMADD
		// This is not a mistake _mm256_fnmadd_ps => (-a*b) + c
		return Vec4(_mm_fnmadd_ps(a.m_v, b.m_v, c.m_v));
#else
		return Vec4( _mm_sub_ps( c.m_v, _mm_mul_ps( a.m_v, b.m_v ) ) );
#endif
// END EPIC MOD: Add proper FMA intrinsics but only if opted-in
	}
	
	friend Vec4 Reciprocal( Vec4::Arg v )
	{
// START EPIC MOD: Replace reciprocal approximation by proper division resulting in both faster and more precise code (opt-in only)
#if SQUISH_USE_DIV
		return Vec4(_mm_div_ps(_mm_set1_ps(1.0f), v.m_v));
#else
		// get the reciprocal estimate
		__m128 estimate = _mm_rcp_ps( v.m_v );

		// one round of Newton-Rhaphson refinement
		__m128 diff = _mm_sub_ps( _mm_set1_ps( 1.0f ), _mm_mul_ps( estimate, v.m_v ) );
		return Vec4( _mm_add_ps( _mm_mul_ps( diff, estimate ), estimate ) );
#endif
// END EPIC MOD: Replace reciprocal approximation by proper division resulting in both faster and more precise code (opt-in only)
	}
	
	friend Vec4 Min( Vec4::Arg left, Vec4::Arg right )
	{
		return Vec4( _mm_min_ps( left.m_v, right.m_v ) );
	}
	
	friend Vec4 Max( Vec4::Arg left, Vec4::Arg right )
	{
		return Vec4( _mm_max_ps( left.m_v, right.m_v ) );
	}
	
	friend Vec4 Truncate( Vec4::Arg v )
	{
#if ( SQUISH_USE_SSE == 1 )
		// convert to ints
		__m128 input = v.m_v;
		__m64 lo = _mm_cvttps_pi32( input );
		__m64 hi = _mm_cvttps_pi32( _mm_movehl_ps( input, input ) );

		// convert to floats
		__m128 part = _mm_movelh_ps( input, _mm_cvtpi32_ps( input, hi ) );
		__m128 truncated = _mm_cvtpi32_ps( part, lo );
		
		// clear out the MMX multimedia state to allow FP calls later
		_mm_empty(); 
		return Vec4( truncated );
// START EPIC MOD: Use floor instead of float->int->float conversion.
#elif SQUISH_USE_AVX2
		// This one is enabled by default as the results are the same.
		// However, special value like NaN are preserved by floor but not by float->int. 
		// In this case, we don't really care because NaN are discarded from the optimal solution during compression anyway.
		return Vec4(_mm_floor_ps(v.m_v));
#else
// END EPIC MOD: Use floor instead of float->int->float conversion.
		// use SSE2 instructions
		return Vec4( _mm_cvtepi32_ps( _mm_cvttps_epi32( v.m_v ) ) );
#endif
	}
	
	friend Vec4 CompareEqual( Vec4::Arg left, Vec4::Arg right )
	{
		return Vec4( _mm_cmpeq_ps( left.m_v, right.m_v ) );
	}
	
	friend Vec4 Select( Vec4::Arg off, Vec4::Arg on, Vec4::Arg bits )
	{
        __m128 a = _mm_andnot_ps( bits.m_v, off.m_v );
        __m128 b = _mm_and_ps( bits.m_v, on.m_v );

        return Vec4( _mm_or_ps( a, b ) );
	}
	
	friend bool CompareAnyLessThan( Vec4::Arg left, Vec4::Arg right ) 
	{
		__m128 bits = _mm_cmplt_ps( left.m_v, right.m_v );
		int value = _mm_movemask_ps( bits );
		return value != 0;
	}
	
private:
	__m128 m_v;
};

// START EPIC MOD: Add 256-bit Vec8 support.
#if SQUISH_USE_AVX2

#define VEC8_CONST( X ) Vec8( _mm256_set1_ps( X ) )
#define VEC8_ZERO() Vec8( _mm256_setzero_ps() )

class Vec8
{
public:
	typedef const Vec8& Arg;

	Vec8() {}

	explicit Vec8(__m256 v) : m_v(v) {}

	Vec8(Arg v) : m_v(v.m_v) {}
	
	Vec8(const Vec4& lo, const Vec4& hi)
		: m_v(_mm256_set_m128(hi.m_v, lo.m_v))
	{
	}

	Vec8& operator=(Arg v)
	{
		m_v = v.m_v;
		return *this;
	}

	Vec8(const float* v)
	{
		m_v = _mm256_load_ps(v);
	}

	Vec8(float x1, float y1, float z1, float w1, float x2, float y2, float z2, float w2)
	{
		m_v = _mm256_setr_ps(x1, y1, z1, w1, x2, y2, z2, w2);
	}

	Vec8 SplatX() const { return Vec8(_mm256_shuffle_ps(m_v, m_v, SQUISH_SSE_SPLAT(0))); }
	Vec8 SplatY() const { return Vec8(_mm256_shuffle_ps(m_v, m_v, SQUISH_SSE_SPLAT(1))); }
	Vec8 SplatZ() const { return Vec8(_mm256_shuffle_ps(m_v, m_v, SQUISH_SSE_SPLAT(2))); }
	Vec8 SplatW() const { return Vec8(_mm256_shuffle_ps(m_v, m_v, SQUISH_SSE_SPLAT(3))); }

	Vec4 GetLoPart() const { return Vec4(_mm256_extractf128_ps(m_v, 0)); }
	Vec4 GetHiPart() const { return Vec4(_mm256_extractf128_ps(m_v, 1)); }

	Vec8& operator+=(Arg v)
	{
		m_v = _mm256_add_ps(m_v, v.m_v);
		return *this;
	}

	Vec8& operator-=(Arg v)
	{
		m_v = _mm256_sub_ps(m_v, v.m_v);
		return *this;
	}

	Vec8& operator*=(Arg v)
	{
		m_v = _mm256_mul_ps(m_v, v.m_v);
		return *this;
	}

	friend Vec8 Dot(Vec8::Arg left, Vec8::Arg right)
	{
		Vec8 m = left * right;
		return Vec8(m.SplatX() + m.SplatY() + m.SplatZ());
	}

	friend Vec8 operator+(Vec8::Arg left, Vec8::Arg right)
	{
		return Vec8(_mm256_add_ps(left.m_v, right.m_v));
	}

	friend Vec8 operator-(Vec8::Arg left, Vec8::Arg right)
	{
		return Vec8(_mm256_sub_ps(left.m_v, right.m_v));
	}

	friend Vec8 operator*(Vec8::Arg left, Vec8::Arg right)
	{
		return Vec8(_mm256_mul_ps(left.m_v, right.m_v));
	}
	
	//! Returns a*b + c
	friend Vec8 MultiplyAdd(Vec8::Arg a, Vec8::Arg b, Vec8::Arg c)
	{
#if SQUISH_USE_FMADD
		return Vec8(_mm256_fmadd_ps(a.m_v, b.m_v, c.m_v));
#else
		return Vec8(_mm256_add_ps(_mm256_mul_ps(a.m_v, b.m_v), c.m_v));
#endif
	}

	//! Returns -( a*b - c )
	friend Vec8 NegativeMultiplySubtract(Vec8::Arg a, Vec8::Arg b, Vec8::Arg c)
	{
#if SQUISH_USE_FNMADD
		// This is not a mistake _mm256_fnmadd_ps => (-a*b) + c
		return Vec8(_mm256_fnmadd_ps(a.m_v, b.m_v, c.m_v));
#else
		return Vec8(_mm256_sub_ps(c.m_v, _mm256_mul_ps(a.m_v, b.m_v)));
#endif
	}
	
	friend Vec8 Reciprocal(Vec8::Arg v)
	{
#if SQUISH_USE_DIV
		return Vec8(_mm256_div_ps(_mm256_set1_ps(1.0f), v.m_v));
#else
		__m256 estimate = _mm256_rcp_ps(v.m_v);

		// one round of Newton-Rhaphson refinement
		__m256 diff = _mm256_sub_ps(_mm256_set1_ps(1.0f), _mm256_mul_ps(estimate, v.m_v));
		return Vec8(_mm256_add_ps(_mm256_mul_ps(diff, estimate), estimate));
#endif
	}

	friend Vec8 Min(Vec8::Arg left, Vec8::Arg right)
	{
		return Vec8(_mm256_min_ps(left.m_v, right.m_v));
	}

	friend Vec8 Max(Vec8::Arg left, Vec8::Arg right)
	{
		return Vec8(_mm256_max_ps(left.m_v, right.m_v));
	}

	friend Vec8 Truncate(Vec8::Arg v)
	{
		return Vec8(_mm256_floor_ps(v.m_v));
	}

private:
	__m256 m_v;
};

#endif
// END EPIC MOD: Add 256-bit Vec8 support.

} // namespace squish

#endif // ndef SQUISH_SIMD_SSE_H
