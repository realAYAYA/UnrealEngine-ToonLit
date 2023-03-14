// ======================================================================== //
// Copyright 2009-2017 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#pragma once

namespace embree
{ 
#ifndef _MM_SHUF_PERM2
#define _MM_SHUF_PERM2(e3, e2, e1, e0) \
  ((int)(((e3)<<3) | ((e2)<<2) | ((e1)<<1) | (e0)))
#endif

#ifndef _MM_SHUF_PERM3
#define _MM_SHUF_PERM3(e1, e0) \
  ((int)(((e1)<<4) | (e0)))
#endif

  /* 4-wide AVX2 64bit long type */
  template<>
    struct vlong<4>
  {
    typedef vboold4 Bool;

    enum  { size = 4 }; // number of SIMD elements
    union {             // data
      __m256i v; 
      long i[4]; 
    };
    
    ////////////////////////////////////////////////////////////////////////////////
    /// Constructors, Assignment & Cast Operators
    ////////////////////////////////////////////////////////////////////////////////
       
    __forceinline vlong() {}
    __forceinline vlong(const vlong4& t) { v = t.v; }
    __forceinline vlong4& operator=(const vlong4& f) { v = f.v; return *this; }

    __forceinline vlong(const __m256i& t) { v = t; }
    __forceinline operator __m256i () const { return v; }
    __forceinline operator __m256d () const { return _mm256_castsi256_pd(v); }


    __forceinline vlong(const long i) {
      v = _mm256_set1_epi64x(i);
    }

    __forceinline vlong(const unsigned long i) {
      v = _mm256_set1_epi64x(i);
    }
    
    __forceinline vlong(const long a, const long b, const long c, const long d) {
      v = _mm256_set_epi64x(d,c,b,a);      
    }
   
    
    ////////////////////////////////////////////////////////////////////////////////
    /// Constants
    ////////////////////////////////////////////////////////////////////////////////
    
    __forceinline vlong( ZeroTy   ) : v(_mm256_setzero_si256()) {}
    __forceinline vlong( OneTy    ) : v(_mm256_set1_epi64x(1)) {}
    __forceinline vlong( StepTy   ) : v(_mm256_set_epi64x(3,2,1,0)) {}
    __forceinline vlong( ReverseStepTy )   : v(_mm256_set_epi64x(0,1,2,3)) {}

    __forceinline static vlong4 zero()     { return _mm256_setzero_si256(); }
    __forceinline static vlong4 one ()     { return _mm256_set1_epi64x(1);  }
    __forceinline static vlong4 neg_one () { return _mm256_set1_epi64x(-1);  }

    ////////////////////////////////////////////////////////////////////////////////
    /// Loads and Stores
    ////////////////////////////////////////////////////////////////////////////////

    static __forceinline void store_nt(void *__restrict__ ptr, const vlong4& a) {
      _mm256_stream_ps((float*)ptr,_mm256_castsi256_ps(a));
    }

    static __forceinline vlong4 loadu(const void* addr)
    {
      return _mm256_loadu_si256((__m256i*)addr);
    }

    static __forceinline vlong4 load(const vlong4* addr) {
      return _mm256_load_si256((__m256i*)addr);
    }

    static __forceinline vlong4 load(const long* addr) {
      return _mm256_load_si256((__m256i*)addr);
    }

    static __forceinline void store(void* ptr, const vlong4& v) {
      _mm256_store_si256((__m256i*)ptr,v);
    }

    static __forceinline void storeu(void* ptr, const vlong4& v) {
      _mm256_storeu_si256((__m256i*)ptr,v);
    }

    static __forceinline void storeu(const vboold4& mask, long* ptr, const vlong4& f) {
#if defined(__AVX512VL__)
      _mm256_mask_storeu_epi64(ptr,mask,f);
#else
      _mm256_maskstore_pd((double*)ptr,mask,_mm256_castsi256_pd(f));
#endif
    }

    static __forceinline void store(const vboold4& mask, void* ptr, const vlong4& f) {
#if defined(__AVX512VL__)
      _mm256_mask_store_epi64(ptr,mask,f);
#else
      _mm256_maskstore_pd((double*)ptr,mask,_mm256_castsi256_pd(f));
#endif
    }

    static __forceinline vlong4 broadcast64bit(size_t v) {
      return _mm256_set1_epi64x(v);
    }

    static __forceinline size_t extract64bit(const vlong4& v)
    {
      return _mm_cvtsi128_si64(_mm256_castsi256_si128(v));
    }


    ////////////////////////////////////////////////////////////////////////////////
    /// Array Access
    ////////////////////////////////////////////////////////////////////////////////
    
    __forceinline       long& operator[](const size_t index)       { assert(index < 4); return i[index]; }
    __forceinline const long& operator[](const size_t index) const { assert(index < 4); return i[index]; }

  };

  ////////////////////////////////////////////////////////////////////////////////
  /// Select
  ////////////////////////////////////////////////////////////////////////////////
  
  __forceinline const vlong4 select( const vboold4& m, const vlong4& t, const vlong4& f ) {
  #if defined(__AVX512VL__)
    return _mm256_mask_blend_epi64(m, f, t);
  #else
    return _mm256_castpd_si256(_mm256_blendv_pd(_mm256_castsi256_pd(f), _mm256_castsi256_pd(t), m));
  #endif
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// Unary Operators
  ////////////////////////////////////////////////////////////////////////////////

  __forceinline const vlong4 asLong    ( const __m256& a ) { return _mm256_castps_si256(a); }
  __forceinline const vlong4 operator +( const vlong4& a ) { return a; }
  __forceinline const vlong4 operator -( const vlong4& a ) { return _mm256_sub_epi64(_mm256_setzero_si256(), a); }

  ////////////////////////////////////////////////////////////////////////////////
  /// Binary Operators
  ////////////////////////////////////////////////////////////////////////////////

  __forceinline const vlong4 operator +( const vlong4& a, const vlong4& b ) { return _mm256_add_epi64(a, b); }
  __forceinline const vlong4 operator +( const vlong4& a, const long    b ) { return a + vlong4(b); }
  __forceinline const vlong4 operator +( const long    a, const vlong4& b ) { return vlong4(a) + b; }

  __forceinline const vlong4 operator -( const vlong4& a, const vlong4& b ) { return _mm256_sub_epi64(a, b); }
  __forceinline const vlong4 operator -( const vlong4& a, const long    b ) { return a - vlong4(b); }
  __forceinline const vlong4 operator -( const long    a, const vlong4& b ) { return vlong4(a) - b; }

  /* only low 32bit part */
  __forceinline const vlong4 operator *( const vlong4& a, const vlong4& b ) { return _mm256_mul_epi32(a, b); }
  __forceinline const vlong4 operator *( const vlong4& a, const long    b ) { return a * vlong4(b); }
  __forceinline const vlong4 operator *( const long    a, const vlong4& b ) { return vlong4(a) * b; }

  __forceinline const vlong4 operator &( const vlong4& a, const vlong4& b ) { return _mm256_and_si256(a, b); }
  __forceinline const vlong4 operator &( const vlong4& a, const long    b ) { return a & vlong4(b); }
  __forceinline const vlong4 operator &( const long    a, const vlong4& b ) { return vlong4(a) & b; }

  __forceinline const vlong4 operator |( const vlong4& a, const vlong4& b ) { return _mm256_or_si256(a, b); }
  __forceinline const vlong4 operator |( const vlong4& a, const long    b ) { return a | vlong4(b); }
  __forceinline const vlong4 operator |( const long    a, const vlong4& b ) { return vlong4(a) | b; }

  __forceinline const vlong4 operator ^( const vlong4& a, const vlong4& b ) { return _mm256_xor_si256(a, b); }
  __forceinline const vlong4 operator ^( const vlong4& a, const long    b ) { return a ^ vlong4(b); }
  __forceinline const vlong4 operator ^( const long    a, const vlong4& b ) { return vlong4(a) ^ b; }

  __forceinline const vlong4 operator <<( const vlong4& a, const long n ) { return _mm256_slli_epi64(a, n); }
  //__forceinline const vlong4 operator >>( const vlong4& a, const long n ) { return _mm256_srai_epi64(a, n); }

  __forceinline const vlong4 operator <<( const vlong4& a, const vlong4& n ) { return _mm256_sllv_epi64(a, n); }
  //__forceinline const vlong4 operator >>( const vlong4& a, const vlong4& n ) { return _mm256_srav_epi64(a, n); }
  //__forceinline const vlong4 sra ( const vlong4& a, const long b ) { return _mm256_srai_epi64(a, b); }

  __forceinline const vlong4 srl ( const vlong4& a, const long b ) { return _mm256_srli_epi64(a, b); }
  
  //__forceinline const vlong4 min( const vlong4& a, const vlong4& b ) { return _mm256_min_epi64(a, b); }
  //__forceinline const vlong4 min( const vlong4& a, const long    b ) { return min(a,vlong4(b)); }
  //__forceinline const vlong4 min( const long    a, const vlong4& b ) { return min(vlong4(a),b); }

  //__forceinline const vlong4 max( const vlong4& a, const vlong4& b ) { return _mm256_max_epi64(a, b); }
  //__forceinline const vlong4 max( const vlong4& a, const long    b ) { return max(a,vlong4(b)); }
  //__forceinline const vlong4 max( const long    a, const vlong4& b ) { return max(vlong4(a),b); }

#if defined(__AVX512VL__)
  __forceinline const vlong4 mask_and(const vboold4& m, const vlong4& c, const vlong4& a, const vlong4& b) { return _mm256_mask_and_epi64(c,m,a,b); }
  __forceinline const vlong4 mask_or (const vboold4& m, const vlong4& c, const vlong4& a, const vlong4& b) { return _mm256_mask_or_epi64(c,m,a,b); }
#else
  __forceinline const vlong4 mask_and(const vboold4& m, const vlong4& c, const vlong4& a, const vlong4& b) { return select(m, a & b, c); }
  __forceinline const vlong4 mask_or (const vboold4& m, const vlong4& c, const vlong4& a, const vlong4& b) { return select(m, a | b, c); }
#endif
  
  ////////////////////////////////////////////////////////////////////////////////
  /// Assignment Operators
  ////////////////////////////////////////////////////////////////////////////////

  __forceinline vlong4& operator +=( vlong4& a, const vlong4& b ) { return a = a + b; }
  __forceinline vlong4& operator +=( vlong4& a, const long    b ) { return a = a + b; }
  
  __forceinline vlong4& operator -=( vlong4& a, const vlong4& b ) { return a = a - b; }
  __forceinline vlong4& operator -=( vlong4& a, const long    b ) { return a = a - b; }

  __forceinline vlong4& operator *=( vlong4& a, const vlong4& b ) { return a = a * b; }
  __forceinline vlong4& operator *=( vlong4& a, const long    b ) { return a = a * b; }
  
  __forceinline vlong4& operator &=( vlong4& a, const vlong4& b ) { return a = a & b; }
  __forceinline vlong4& operator &=( vlong4& a, const long    b ) { return a = a & b; }
  
  __forceinline vlong4& operator |=( vlong4& a, const vlong4& b ) { return a = a | b; }
  __forceinline vlong4& operator |=( vlong4& a, const long    b ) { return a = a | b; }
  
  __forceinline vlong4& operator <<=( vlong4& a, const long b ) { return a = a << b; }
  //__forceinline vlong4& operator >>=( vlong4& a, const long b ) { return a = a >> b; }

  ////////////////////////////////////////////////////////////////////////////////
  /// Comparison Operators
  ////////////////////////////////////////////////////////////////////////////////

#if defined(__AVX512VL__)
  __forceinline const vboold4 operator ==( const vlong4& a, const vlong4& b ) { return _mm256_cmp_epi64_mask(a,b,_MM_CMPINT_EQ); }
  __forceinline const vboold4 operator !=( const vlong4& a, const vlong4& b ) { return _mm256_cmp_epi64_mask(a,b,_MM_CMPINT_NE); }
  __forceinline const vboold4 operator < ( const vlong4& a, const vlong4& b ) { return _mm256_cmp_epi64_mask(a,b,_MM_CMPINT_LT); }
  __forceinline const vboold4 operator >=( const vlong4& a, const vlong4& b ) { return _mm256_cmp_epi64_mask(a,b,_MM_CMPINT_GE); }
  __forceinline const vboold4 operator > ( const vlong4& a, const vlong4& b ) { return _mm256_cmp_epi64_mask(a,b,_MM_CMPINT_GT); }
  __forceinline const vboold4 operator <=( const vlong4& a, const vlong4& b ) { return _mm256_cmp_epi64_mask(a,b,_MM_CMPINT_LE); }
#else
  __forceinline const vboold4 operator ==( const vlong4& a, const vlong4& b ) { return _mm256_cmpeq_epi64(a,b); }
  __forceinline const vboold4 operator !=( const vlong4& a, const vlong4& b ) { return !(a == b); }
  __forceinline const vboold4 operator > ( const vlong4& a, const vlong4& b ) { return _mm256_cmpgt_epi64(a,b); }
  __forceinline const vboold4 operator < ( const vlong4& a, const vlong4& b ) { return _mm256_cmpgt_epi64(b,a); }
  __forceinline const vboold4 operator >=( const vlong4& a, const vlong4& b ) { return !(a < b); }
  __forceinline const vboold4 operator <=( const vlong4& a, const vlong4& b ) { return !(a > b); }
#endif

  __forceinline const vboold4 operator ==( const vlong4& a, const long    b ) { return a == vlong4(b); }
  __forceinline const vboold4 operator ==( const long    a, const vlong4& b ) { return vlong4(a) == b; }

  __forceinline const vboold4 operator !=( const vlong4& a, const long    b ) { return a != vlong4(b); }
  __forceinline const vboold4 operator !=( const long    a, const vlong4& b ) { return vlong4(a) != b; }

  __forceinline const vboold4 operator > ( const vlong4& a, const long    b ) { return a >  vlong4(b); }
  __forceinline const vboold4 operator > ( const long    a, const vlong4& b ) { return vlong4(a) >  b; }

  __forceinline const vboold4 operator < ( const vlong4& a, const long    b ) { return a <  vlong4(b); }
  __forceinline const vboold4 operator < ( const long    a, const vlong4& b ) { return vlong4(a) <  b; }

  __forceinline const vboold4 operator >=( const vlong4& a, const long    b ) { return a >= vlong4(b); }
  __forceinline const vboold4 operator >=( const long    a, const vlong4& b ) { return vlong4(a) >= b; }

  __forceinline const vboold4 operator <=( const vlong4& a, const long    b ) { return a <= vlong4(b); }
  __forceinline const vboold4 operator <=( const long    a, const vlong4& b ) { return vlong4(a) <= b; }

  __forceinline vboold4 eq(const vlong4& a, const vlong4& b) { return a == b; }
  __forceinline vboold4 ne(const vlong4& a, const vlong4& b) { return a != b; }
  __forceinline vboold4 lt(const vlong4& a, const vlong4& b) { return a <  b; }
  __forceinline vboold4 ge(const vlong4& a, const vlong4& b) { return a >= b; }
  __forceinline vboold4 gt(const vlong4& a, const vlong4& b) { return a >  b; }
  __forceinline vboold4 le(const vlong4& a, const vlong4& b) { return a <= b; }

#if defined(__AVX512VL__)
  __forceinline vboold4 eq(const vboold4& mask, const vlong4& a, const vlong4& b) { return _mm256_mask_cmp_epi64_mask(mask, a, b, _MM_CMPINT_EQ); }
  __forceinline vboold4 ne(const vboold4& mask, const vlong4& a, const vlong4& b) { return _mm256_mask_cmp_epi64_mask(mask, a, b, _MM_CMPINT_NE); }
  __forceinline vboold4 lt(const vboold4& mask, const vlong4& a, const vlong4& b) { return _mm256_mask_cmp_epi64_mask(mask, a, b, _MM_CMPINT_LT); }
  __forceinline vboold4 ge(const vboold4& mask, const vlong4& a, const vlong4& b) { return _mm256_mask_cmp_epi64_mask(mask, a, b, _MM_CMPINT_GE); }
  __forceinline vboold4 gt(const vboold4& mask, const vlong4& a, const vlong4& b) { return _mm256_mask_cmp_epi64_mask(mask, a, b, _MM_CMPINT_GT); }
  __forceinline vboold4 le(const vboold4& mask, const vlong4& a, const vlong4& b) { return _mm256_mask_cmp_epi64_mask(mask, a, b, _MM_CMPINT_LE); }
#else
  __forceinline vboold4 eq(const vboold4& mask, const vlong4& a, const vlong4& b) { return mask & (a == b); }
  __forceinline vboold4 ne(const vboold4& mask, const vlong4& a, const vlong4& b) { return mask & (a != b); }
  __forceinline vboold4 lt(const vboold4& mask, const vlong4& a, const vlong4& b) { return mask & (a <  b); }
  __forceinline vboold4 ge(const vboold4& mask, const vlong4& a, const vlong4& b) { return mask & (a >= b); }
  __forceinline vboold4 gt(const vboold4& mask, const vlong4& a, const vlong4& b) { return mask & (a >  b); }
  __forceinline vboold4 le(const vboold4& mask, const vlong4& a, const vlong4& b) { return mask & (a <= b); }
#endif

  __forceinline void xchg(const vboold4& m, vlong4& a, vlong4& b) {
    const vlong4 c = a; a = select(m,b,a); b = select(m,c,b);
  }

  __forceinline vboold4 test(const vlong4& a, const vlong4& b) {
#if defined(__AVX512VL__)
    return _mm256_test_epi64_mask(a,b);
#else
    return _mm256_testz_si256(a,b);
#endif
  }

  ////////////////////////////////////////////////////////////////////////////////
  // Movement/Shifting/Shuffling Functions
  ////////////////////////////////////////////////////////////////////////////////

  template<int B, int A> __forceinline vlong4 shuffle   (const vlong4& v) { return _mm256_castpd_si256(_mm256_permute_pd(_mm256_castsi256_pd(v),(int)_MM_SHUF_PERM2(B,A,B,A))); }
  template<int A>        __forceinline vlong4 shuffle   (const vlong4& x) { return shuffle<A,A>(x); }

  template<int B, int A> __forceinline vlong4 shuffle2   (const vlong4& v) { return _mm256_castpd_si256(_mm256_permute2f128_pd(_mm256_castsi256_pd(v),v,(int)_MM_SHUF_PERM3(B,A))); }


  __forceinline long long toScalar(const vlong4& a)
  {
    return _mm_cvtsi128_si64(_mm256_castsi256_si128(a));
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// Reductions
  ////////////////////////////////////////////////////////////////////////////////
  

  __forceinline vlong4 vreduce_and2(const vlong4& x) { return x & shuffle<0,1>(x); }
  __forceinline vlong4 vreduce_and (const vlong4& y) { const vlong4 x = vreduce_and2(y); return x & shuffle2<0,1>(x); }

  __forceinline vlong4 vreduce_or2(const vlong4& x) { return x | shuffle<0,1>(x); }
  __forceinline vlong4 vreduce_or (const vlong4& y) { const vlong4 x = vreduce_or2(y); return x | shuffle2<0,1>(x); }

  __forceinline vlong4 vreduce_add2(const vlong4& x) { return x + shuffle<0,1>(x); }
  __forceinline vlong4 vreduce_add (const vlong4& y) { const vlong4 x = vreduce_add2(y); return x + shuffle2<0,1>(x); }

  __forceinline long long reduce_add(const vlong4& a) { return toScalar(vreduce_add(a)); }
  __forceinline long long reduce_or (const vlong4& a) { return toScalar(vreduce_or(a)); }
  __forceinline long long reduce_and(const vlong4& a) { return toScalar(vreduce_and(a)); }
  
  ////////////////////////////////////////////////////////////////////////////////
  /// Output Operators
  ////////////////////////////////////////////////////////////////////////////////
  
  __forceinline std::ostream& operator<<(std::ostream& cout, const vlong4& v)
  {
    cout << "<" << v[0];
    for (long i=1; i<4; i++) cout << ", " << v[i];
    cout << ">";
    return cout;
  }
}
