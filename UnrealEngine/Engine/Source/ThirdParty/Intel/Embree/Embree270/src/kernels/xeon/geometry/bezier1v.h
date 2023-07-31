// ======================================================================== //
// Copyright 2009-2015 Intel Corporation                                    //
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

#include "primitive.h"
#include "../../common/globals.h"
#include "../../common/subdiv/bezier_curve.h"

namespace embree
{
  struct Bezier1v
  {
    struct Type : public PrimitiveType 
    {
      Type ();
      size_t size(const char* This) const;
    };
    static Type type;

  public:

    /*! Default constructor. */
    __forceinline Bezier1v () {}

    /*! Construction from vertices and IDs. */
    __forceinline Bezier1v (const Vec3fa& p0, const Vec3fa& p1, const Vec3fa& p2, const Vec3fa& p3, const float t0, const float t1,
                           const unsigned int geomID, const unsigned int primID, const bool last)
      : p0(p0), p1(p1), p2(p2), p3(p3), t0(t0), t1(t1), geom(geomID), prim(primID | (last << 31)) {}
    
    /*! returns required number of primitive blocks for N primitives */
    static __forceinline size_t blocks(size_t N) { return N; }

    /*! access hidden members */
    __forceinline unsigned int primID() const { 
      return prim;
    }
    __forceinline unsigned int geomID() const { 
      return geom; 
    }

    /*! fill from list */
    __forceinline void fill(atomic_set<PrimRefBlockT<Bezier1v> >::block_iterator_unsafe& iter, Scene* scene, const bool list) {
      *this = *iter; iter++; this->prim |= (list && !iter) << 31;
    }

    /*! fill triangle from triangle list */
    __forceinline void fill(const PrimRef* prims, size_t& i, size_t end, Scene* scene, const bool list)
    {
      const PrimRef& prim = prims[i];
      i++;
      const size_t geomID = prim.geomID();
      const size_t primID = prim.primID();
      const BezierCurves* curves = scene->getBezierCurves(geomID);
      const size_t id = curves->curve(primID);
      const Vec3fa& p0 = curves->vertex(id+0);
      const Vec3fa& p1 = curves->vertex(id+1);
      const Vec3fa& p2 = curves->vertex(id+2);
      const Vec3fa& p3 = curves->vertex(id+3);
      new (this) Bezier1v(p0,p1,p2,p3,0.0f,1.0f,geomID,primID,list && i>=end);
    }

    /*! fill triangle from triangle list */
    __forceinline void fill(const Bezier1v* prims, size_t& i, size_t end, Scene* scene, const bool list) {
      *this = prims[i];
      i++;
    }

    /*! returns size of t range */
    __forceinline float dt() const {
      return t1-t0;
    }
      
    /*! calculate the center of the curve */
    __forceinline const Vec3fa center() const { // FIXME: remove this function, use center2 intead
      return p0+p3;
    }

    /*! calculate the center of the curve */
    __forceinline const Vec3fa center(const AffineSpace3fa& space) const { // FIXME: remove this function, use center2 intead
      return xfmPoint(space,p0)+xfmPoint(space,p3);
    }

    /*! calculate the center of the curve */
    __forceinline const Vec3fa center2() const {
      return p0+p3;
    }

    /*! calculate the center of the curve */
    __forceinline const Vec3fa center2(const AffineSpace3fa& space) const {
      return xfmPoint(space,p0)+xfmPoint(space,p3);
    }


    /*! calculate the bounds of the curve */
    __forceinline const BBox3fa bounds() const 
    {
#if 1
      const BezierCurve3fa curve2D(p0,p1,p2,p3,0.0f,1.0f,0);
#if defined(__AVX__)
      const avx4f pi = curve2D.eval8(coeff0[0],coeff0[1],coeff0[2],coeff0[3]);
#else
      const sse4f pi = curve2D.eval4(sse_coeff0[0],sse_coeff0[1],sse_coeff0[2],sse_coeff0[3]);
#endif
      const Vec3fa lower(reduce_min(pi.x),reduce_min(pi.y),reduce_min(pi.z));
      const Vec3fa upper(reduce_max(pi.x),reduce_max(pi.y),reduce_max(pi.z));
      const Vec3fa upper_r = Vec3fa(reduce_max(abs(pi.w)));
      return enlarge(BBox3fa(min(lower,p3),max(upper,p3)),max(upper_r,Vec3fa(p3.w)));
#else
      const BBox3fa b = merge(BBox3fa(p0),BBox3fa(p1),BBox3fa(p2),BBox3fa(p3));
      return enlarge(b,Vec3fa(b.upper.w));
#endif
    }
    
    /*! calculate bounds in specified coordinate space */
    __forceinline const BBox3fa bounds(const AffineSpace3fa& space) const 
    {
      Vec3fa b0 = xfmPoint(space,p0); b0.w = p0.w;
      Vec3fa b1 = xfmPoint(space,p1); b1.w = p1.w;
      Vec3fa b2 = xfmPoint(space,p2); b2.w = p2.w;
      Vec3fa b3 = xfmPoint(space,p3); b3.w = p3.w;
#if 1
      const BezierCurve3fa curve2D(b0,b1,b2,b3,0.0f,1.0f,0);
#if defined(__AVX__)
      const avx4f pi = curve2D.eval8(coeff0[0],coeff0[1],coeff0[2],coeff0[3]);
#else
      const sse4f pi = curve2D.eval4(sse_coeff0[0],sse_coeff0[1],sse_coeff0[2],sse_coeff0[3]);
#endif
      const Vec3fa lower(reduce_min(pi.x),reduce_min(pi.y),reduce_min(pi.z));
      const Vec3fa upper(reduce_max(pi.x),reduce_max(pi.y),reduce_max(pi.z));
      const Vec3fa upper_r = Vec3fa(reduce_max(abs(pi.w)));
      return enlarge(BBox3fa(min(lower,b3),max(upper,b3)),max(upper_r,Vec3fa(b3.w)));
#else
      const BBox3fa b = merge(BBox3fa(b0),BBox3fa(b1),BBox3fa(b2),BBox3fa(b3));
      return enlarge(b,Vec3fa(b.upper.w));
#endif
    }
    
    /*! subdivide the bezier curve */
    __forceinline void subdivide(Bezier1v& left_o, Bezier1v& right_o, const float T = 0.5f) const
    {
      const Vec3fa p00 = p0;
      const Vec3fa p01 = p1;
      const Vec3fa p02 = p2;
      const Vec3fa p03 = p3;
      
      const float T0 = 1.0f - T, T1 = T;
      const Vec3fa p10 = T0*p00 + T1*p01;
      const Vec3fa p11 = T0*p01 + T1*p02;
      const Vec3fa p12 = T0*p02 + T1*p03;
      const Vec3fa p20 = T0*p10 + T1*p11;
      const Vec3fa p21 = T0*p11 + T1*p12;
      const Vec3fa p30 = T0*p20 + T1*p21;
      
      const float t01 = T0*t0 + T1*t1;
      const unsigned int geomID = this->geomID();
      const unsigned int primID = this->primID();
      
      new (&left_o ) Bezier1v(p00,p10,p20,p30,t0,t01,geomID,primID,false);
      new (&right_o) Bezier1v(p30,p21,p12,p03,t01,t1,geomID,primID,false);
    }
    
    /*! split the hair using splitting plane */
    bool split(const Vec3fa& plane, Bezier1v& left_o, Bezier1v& right_o) const
    {
      /*! test if start and end points lie on different sides of plane */
      const float p0p = dot(p0,plane)+plane.w;
      const float p3p = dot(p3,plane)+plane.w;
      if (p0p == 0.0f || p3p == 0.0f) return false;
      if (p0p < 0.0f && p3p < 0.0f) return false;
      if (p0p > 0.0f && p3p > 0.0f) return false;
      
      /*! search for the t-value that splits the curve into one part
       *  left and right of the plane */
      float u0 = 0.0f, u1 = 1.0f;
      while (u1-u0 > 0.01f) 
      //while (u1-u0 > 0.0001f) 
      {
        const float tc = 0.5f*(u0+u1);
        Bezier1v left,right; subdivide(left,right,tc);
        const float lp0p = dot(left.p0,plane)+plane.w;
        const float lp3p = dot(left.p3,plane)+plane.w;
        if (lp0p <= 0.0f && lp3p >= 0.0f) { u1 = tc; continue; }
        if (lp0p >= 0.0f && lp3p <= 0.0f) { u1 = tc; continue; }
        u0 = tc; 
      }
      
      /*! return the split curve */
      if (p0p < 0.0f) subdivide(left_o,right_o,0.5f*(u0+u1));
      else            subdivide(right_o,left_o,0.5f*(u0+u1));
      return true;
    }

    /*! split the hair using splitting plane */
    bool split(const int dim, const float pos, Bezier1v& left_o, Bezier1v& right_o) const
    {
      /*! test if start and end points lie on different sides of plane */
      const float p0p = p0[dim];
      const float p3p = p3[dim];
      if (p0p == pos || p3p == pos) return false;
      if (p0p < pos && p3p < pos) return false;
      if (p0p > pos && p3p > pos) return false;
      
      /*! search for the t-value that splits the curve into one part
       *  left and right of the plane */
      float u0 = 0.0f, u1 = 1.0f;
      while (u1-u0 > 0.01f) 
      //while (u1-u0 > 0.0001f) 
      {
        const float tc = 0.5f*(u0+u1);
        Bezier1v left,right; subdivide(left,right,tc);
        const float lp0p = left.p0[dim];
        const float lp3p = left.p3[dim];
        if (lp0p <= pos && lp3p >= pos) { u1 = tc; continue; }
        if (lp0p >= pos && lp3p <= pos) { u1 = tc; continue; }
        u0 = tc; 
      }
      
      /*! return the split curve */
      if (p0p < pos) subdivide(left_o,right_o,0.5f*(u0+u1));
      else           subdivide(right_o,left_o,0.5f*(u0+u1));
      return true;
    }

    __forceinline uint64_t id64() const {
      return (((uint64_t)prim) << 32) + (uint64_t)geom;
    }

    friend __forceinline bool operator<(const Bezier1v& p0, const Bezier1v& p1) {
      return p0.id64() < p1.id64();
    }

    friend std::ostream& operator<<(std::ostream& cout, const Bezier1v& b) 
    {
      return std::cout << "Bezier1v { " << std::endl << 
        " p0 = " << b.p0 << ", " << std::endl <<
        " p1 = " << b.p1 << ", " << std::endl <<
        " p2 = " << b.p2 << ", " << std::endl <<
        " p3 = " << b.p3 << ",  " << std::endl <<
        " t0 = " << b.t0 << ",  t1 = " << b.t1 << ", " << std::endl <<
        " geomID = " << b.geomID() << ", primID = " << b.primID() << std::endl << 
      "}";
    }
    
  public:
    Vec3fa p0;            //!< 1st control point (x,y,z,r)
    Vec3fa p1;            //!< 2nd control point (x,y,z,r)
    Vec3fa p2;            //!< 3rd control point (x,y,z,r)
    Vec3fa p3;            //!< 4th control point (x,y,z,r)
    float t0,t1;          //!< t range of this sub-curve // FIXME: required?
    unsigned geom;      //!< geometry ID
    unsigned prim;      //!< primitive ID
  };

  typedef Bezier1v BezierPrim; // FIXME: rename to BezierRef
}
