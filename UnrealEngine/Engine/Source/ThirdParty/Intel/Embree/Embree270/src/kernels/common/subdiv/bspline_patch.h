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

#include "catmullclark_patch.h"
#include "bezier_curve.h"

namespace embree
{
  class BSplineBasis
  {
  public:

    template<class T>
      static __forceinline Vec4<T>  eval(const T& u)
    {
      const T t  = u;
      const T s  = T(1.0f) - u;
      const T n0 = s*s*s;
      const T n1 = (4.0f*(s*s*s)+(t*t*t)) + (12.0f*((s*t)*s) + 6.0*((t*s)*t));
      const T n2 = (4.0f*(t*t*t)+(s*s*s)) + (12.0f*((t*s)*t) + 6.0*((s*t)*s));
      const T n3 = t*t*t;
      return Vec4<T>(n0,n1,n2,n3);
    }
    
    template<class T>
      static __forceinline Vec4<T>  derivative(const T& u)
    {
      const T t  =  u;
      const T s  =  1.0f - u;
      const T n0 = -s*s;
      const T n1 = -t*t - 4.0f*(t*s);
      const T n2 =  s*s + 4.0f*(s*t);
      const T n3 =  t*t;
      return Vec4<T>(3.0f*n0,3.0f*n1,3.0f*n2,3.0f*n3);
    }
  };

  template<typename Vertex, typename Vertex_t = Vertex>
    class __aligned(64) BSplinePatchT
    {
      typedef CatmullClark1RingT<Vertex,Vertex_t> CatmullClarkRing;
      typedef CatmullClarkPatchT<Vertex,Vertex_t> CatmullClarkPatch;
      
    public:
      
      __forceinline BSplinePatchT () {}

      __forceinline BSplinePatchT (const CatmullClarkPatch& patch) {
        init(patch);
      }

      __forceinline BSplinePatchT(const CatmullClarkPatch& patch,
                                  const BezierCurveT<Vertex>* border0,
                                  const BezierCurveT<Vertex>* border1,
                                  const BezierCurveT<Vertex>* border2,
                                  const BezierCurveT<Vertex>* border3)
      {
        init(patch);
      }

      __forceinline BSplinePatchT (const HalfEdge* edge, const char* vertices, size_t stride) {
        init(edge,vertices,stride);
      }

      __forceinline Vertex hard_corner(const                    Vertex& v01, const Vertex& v02, 
                                       const Vertex& v10, const Vertex& v11, const Vertex& v12, 
                                       const Vertex& v20, const Vertex& v21, const Vertex& v22)
      {
        return 4.0f*v11 - 2.0f*(v12+v21) + v22;
      }

      __forceinline Vertex soft_convex_corner( const                    Vertex& v01, const Vertex& v02, 
                                               const Vertex& v10, const Vertex& v11, const Vertex& v12, 
                                               const Vertex& v20, const Vertex& v21, const Vertex& v22)
      {
        return -8.0f*v11 + 4.0f*(v12+v21) + v22;
      }

      __forceinline Vertex convex_corner(const float vertex_crease_weight, 
                                         const                    Vertex& v01, const Vertex& v02, 
                                         const Vertex& v10, const Vertex& v11, const Vertex& v12, 
                                         const Vertex& v20, const Vertex& v21, const Vertex& v22)
      {
        if (std::isinf(vertex_crease_weight)) return hard_corner(v01,v02,v10,v11,v12,v20,v21,v22);
        else                                  return soft_convex_corner(v01,v02,v10,v11,v12,v20,v21,v22);
      }

      __forceinline Vertex load(const HalfEdge* edge, const char* vertices, size_t stride) {
        return Vertex_t::loadu(vertices+edge->getStartVertexIndex()*stride);
      }

      __forceinline void init_border(const CatmullClarkRing& edge0,
                                     Vertex& v01, Vertex& v02,
                                     const Vertex& v11, const Vertex& v12,
                                     const Vertex& v21, const Vertex& v22)
      {
        if (likely(edge0.has_opposite_back(0)))
        {
          v01 = edge0.back(2);
          v02 = edge0.back(1);
        } else {
          v01 = 2.0f*v11-v21;
          v02 = 2.0f*v12-v22;
        }
      }

      __forceinline void init_corner(const CatmullClarkRing& edge0,
                                     Vertex& v00,       const Vertex& v01, const Vertex& v02, 
                                     const Vertex& v10, const Vertex& v11, const Vertex& v12, 
                                     const Vertex& v20, const Vertex& v21, const Vertex& v22)
      {
        const bool has_back1 = edge0.has_opposite_back(1);
        const bool has_back0 = edge0.has_opposite_back(0);
        const bool has_front1 = edge0.has_opposite_front(1);
        const bool has_front2 = edge0.has_opposite_front(2);
        
        if (likely(has_back0)) {
          if (likely(has_front1)) { assert(has_back1 && has_front2); v00 = edge0.back(3); }
          else { assert(!has_back1); v00 = 2.0f*v01-v02; }
        }
        else {
          if (likely(has_front1)) { assert(!has_front2); v00 = 2.0f*v10-v20; }
          else v00 = convex_corner(edge0.vertex_crease_weight,v01,v02,v10,v11,v12,v20,v21,v22);
        }
      }
      
      void init(const CatmullClarkPatch& patch)
      {
        /* fill inner vertices */
        const Vertex v11 = v[1][1] = patch.ring[0].vtx;
        const Vertex v12 = v[1][2] = patch.ring[1].vtx;
        const Vertex v22 = v[2][2] = patch.ring[2].vtx; 
        const Vertex v21 = v[2][1] = patch.ring[3].vtx; 
        
        /* fill border vertices */
        init_border(patch.ring[0],v[0][1],v[0][2],v11,v12,v21,v22);
        init_border(patch.ring[1],v[1][3],v[2][3],v12,v22,v11,v21);
        init_border(patch.ring[2],v[3][2],v[3][1],v22,v21,v12,v11);
        init_border(patch.ring[3],v[2][0],v[1][0],v21,v11,v22,v12);
        
        /* fill corner vertices */
        init_corner(patch.ring[0],v[0][0],v[0][1],v[0][2],v[1][0],v11,v12,v[2][0],v21,v22);
        init_corner(patch.ring[1],v[0][3],v[1][3],v[2][3],v[0][2],v12,v22,v[0][1],v11,v21);
        init_corner(patch.ring[2],v[3][3],v[3][2],v[3][1],v[2][3],v22,v21,v[1][3],v12,v11);
        init_corner(patch.ring[3],v[3][0],v[2][0],v[1][0],v[3][1],v21,v11,v[3][2],v22,v12);
      }
      
      __forceinline void init_border(const HalfEdge* edge0, const char* vertices, size_t stride,
                                     Vertex& v01, Vertex& v02,
                                     const Vertex& v11, const Vertex& v12,
                                     const Vertex& v21, const Vertex& v22)
      {
        if (likely(edge0->hasOpposite())) 
        {
          const HalfEdge* e = edge0->opposite()->next()->next(); 
          v01 = load(e,vertices,stride); 
          v02 = load(e->next(),vertices,stride);
        } else {
          v01 = 2.0f*v11-v21;
          v02 = 2.0f*v12-v22;
        }
      }
      
      __forceinline void init_corner(const HalfEdge* edge0, const char* vertices, size_t stride,
                                     Vertex& v00, const Vertex& v01, const Vertex& v02, 
                                     const Vertex& v10, const Vertex& v11, const Vertex& v12, 
                                     const Vertex& v20, const Vertex& v21, const Vertex& v22)
      {
        const bool has_back0 = edge0->hasOpposite();
        const bool has_front1 = edge0->prev()->hasOpposite();

        if (likely(has_back0))
        { 
          const HalfEdge* e = edge0->opposite()->next();
          if (likely(has_front1))
          {
            assert(e->hasOpposite());
            assert(edge0->prev()->opposite()->prev()->hasOpposite());
            v00 = load(e->opposite()->prev(),vertices,stride);
          } 
          else {
            assert(!e->hasOpposite());
            v00 = 2.0f*v01-v02;
          }
        }
        else
        {
          if (likely(has_front1)) {
            assert(!edge0->prev()->opposite()->prev()->hasOpposite());
            v00 = 2.0f*v10-v20;
          }
          else {
            assert(edge0->vertex_crease_weight == 0.0f || std::isinf(edge0->vertex_crease_weight));
            v00 = convex_corner(edge0->vertex_crease_weight,v01,v02,v10,v11,v12,v20,v21,v22);
          }
        }
      }
      
      void init(const HalfEdge* edge0, const char* vertices, size_t stride)
      {
        assert( edge0->isRegularFace() );
        
        /* fill inner vertices */
        const Vertex v11 = v[1][1] = load(edge0,vertices,stride); const HalfEdge* edge1 = edge0->next();
        const Vertex v12 = v[1][2] = load(edge1,vertices,stride); const HalfEdge* edge2 = edge1->next();
        const Vertex v22 = v[2][2] = load(edge2,vertices,stride); const HalfEdge* edge3 = edge2->next();
        const Vertex v21 = v[2][1] = load(edge3,vertices,stride); assert(edge0  == edge3->next());
        
        /* fill border vertices */
        init_border(edge0,vertices,stride,v[0][1],v[0][2],v11,v12,v21,v22);
        init_border(edge1,vertices,stride,v[1][3],v[2][3],v12,v22,v11,v21);
        init_border(edge2,vertices,stride,v[3][2],v[3][1],v22,v21,v12,v11);
        init_border(edge3,vertices,stride,v[2][0],v[1][0],v21,v11,v22,v12);
        
        /* fill corner vertices */
        init_corner(edge0,vertices,stride,v[0][0],v[0][1],v[0][2],v[1][0],v11,v12,v[2][0],v21,v22);
        init_corner(edge1,vertices,stride,v[0][3],v[1][3],v[2][3],v[0][2],v12,v22,v[0][1],v11,v21);
        init_corner(edge2,vertices,stride,v[3][3],v[3][2],v[3][1],v[2][3],v22,v21,v[1][3],v12,v11);
        init_corner(edge3,vertices,stride,v[3][0],v[2][0],v[1][0],v[3][1],v21,v11,v[3][2],v22,v12);
      }
      
      __forceinline BBox<Vertex> bounds() const
      {
        const Vertex* const cv = &v[0][0];
        BBox<Vertex> bounds (cv[0]);
        for (size_t i=1; i<16 ; i++)
          bounds.extend( cv[i] );
        return bounds;
      }
      
      __forceinline Vertex eval(const float uu, const float vv) const
      {
        const Vec4f v_n = BSplineBasis::eval(vv);
        const Vertex_t curve0 = v_n[0] * v[0][0] + v_n[1] * v[1][0] + v_n[2] * v[2][0] + v_n[3] * v[3][0];
        const Vertex_t curve1 = v_n[0] * v[0][1] + v_n[1] * v[1][1] + v_n[2] * v[2][1] + v_n[3] * v[3][1];
        const Vertex_t curve2 = v_n[0] * v[0][2] + v_n[1] * v[1][2] + v_n[2] * v[2][2] + v_n[3] * v[3][2];
        const Vertex_t curve3 = v_n[0] * v[0][3] + v_n[1] * v[1][3] + v_n[2] * v[2][3] + v_n[3] * v[3][3];
        
        const Vec4f u_n = BSplineBasis::eval(uu);
        return (u_n[0] * curve0 + u_n[1] * curve1 + u_n[2] * curve2 + u_n[3] * curve3) * (1.0f/36.0f);
      }
      
      __forceinline Vertex tangentU(const float uu, const float vv) const
      {
        const Vec4f v_n = BSplineBasis::eval(vv);
        const Vertex_t curve0 = v_n[0] * v[0][0] + v_n[1] * v[1][0] + v_n[2] * v[2][0] + v_n[3] * v[3][0];
        const Vertex_t curve1 = v_n[0] * v[0][1] + v_n[1] * v[1][1] + v_n[2] * v[2][1] + v_n[3] * v[3][1];
        const Vertex_t curve2 = v_n[0] * v[0][2] + v_n[1] * v[1][2] + v_n[2] * v[2][2] + v_n[3] * v[3][2];
        const Vertex_t curve3 = v_n[0] * v[0][3] + v_n[1] * v[1][3] + v_n[2] * v[2][3] + v_n[3] * v[3][3];
        
        const Vec4f u_n = BSplineBasis::derivative(uu);
        return (u_n[0] * curve0 + u_n[1] * curve1 + u_n[2] * curve2 + u_n[3] * curve3) * (1.0f/36.0f);
      }
      
      __forceinline Vertex tangentV(const float uu, const float vv) const
      {
        const Vec4f v_n = BSplineBasis::derivative(vv);
        const Vertex_t curve0 = v_n[0] * v[0][0] + v_n[1] * v[1][0] + v_n[2] * v[2][0] + v_n[3] * v[3][0];
        const Vertex_t curve1 = v_n[0] * v[0][1] + v_n[1] * v[1][1] + v_n[2] * v[2][1] + v_n[3] * v[3][1];
        const Vertex_t curve2 = v_n[0] * v[0][2] + v_n[1] * v[1][2] + v_n[2] * v[2][2] + v_n[3] * v[3][2];
        const Vertex_t curve3 = v_n[0] * v[0][3] + v_n[1] * v[1][3] + v_n[2] * v[2][3] + v_n[3] * v[3][3];
        
        const Vec4f u_n = BSplineBasis::eval(uu);
        return (u_n[0] * curve0 + u_n[1] * curve1 + u_n[2] * curve2 + u_n[3] * curve3) * (1.0f/36.0f);
      }
      
      __forceinline Vertex normal(const float uu, const float vv) const
      {
        const Vertex tu = tangentU(uu,vv);
        const Vertex tv = tangentV(uu,vv);
        return cross(tv,tu);
      }   
      

      template<class T>
      __forceinline Vec3<T> eval(const T& uu, const T& vv, const Vec4<T>& u_n, const Vec4<T>& v_n) const
      {
        const T curve0_x = v_n[0] * T(v[0][0].x) + v_n[1] * T(v[1][0].x) + v_n[2] * T(v[2][0].x) + v_n[3] * T(v[3][0].x);
        const T curve1_x = v_n[0] * T(v[0][1].x) + v_n[1] * T(v[1][1].x) + v_n[2] * T(v[2][1].x) + v_n[3] * T(v[3][1].x);
        const T curve2_x = v_n[0] * T(v[0][2].x) + v_n[1] * T(v[1][2].x) + v_n[2] * T(v[2][2].x) + v_n[3] * T(v[3][2].x);
        const T curve3_x = v_n[0] * T(v[0][3].x) + v_n[1] * T(v[1][3].x) + v_n[2] * T(v[2][3].x) + v_n[3] * T(v[3][3].x);
        const T x = (u_n[0] * curve0_x + u_n[1] * curve1_x + u_n[2] * curve2_x + u_n[3] * curve3_x) * T(1.0f/36.0f);
                  
        const T curve0_y = v_n[0] * T(v[0][0].y) + v_n[1] * T(v[1][0].y) + v_n[2] * T(v[2][0].y) + v_n[3] * T(v[3][0].y);
        const T curve1_y = v_n[0] * T(v[0][1].y) + v_n[1] * T(v[1][1].y) + v_n[2] * T(v[2][1].y) + v_n[3] * T(v[3][1].y);
        const T curve2_y = v_n[0] * T(v[0][2].y) + v_n[1] * T(v[1][2].y) + v_n[2] * T(v[2][2].y) + v_n[3] * T(v[3][2].y);
        const T curve3_y = v_n[0] * T(v[0][3].y) + v_n[1] * T(v[1][3].y) + v_n[2] * T(v[2][3].y) + v_n[3] * T(v[3][3].y);
        const T y = (u_n[0] * curve0_y + u_n[1] * curve1_y + u_n[2] * curve2_y + u_n[3] * curve3_y) * T(1.0f/36.0f);
          
        const T curve0_z = v_n[0] * T(v[0][0].z) + v_n[1] * T(v[1][0].z) + v_n[2] * T(v[2][0].z) + v_n[3] * T(v[3][0].z);
        const T curve1_z = v_n[0] * T(v[0][1].z) + v_n[1] * T(v[1][1].z) + v_n[2] * T(v[2][1].z) + v_n[3] * T(v[3][1].z);
        const T curve2_z = v_n[0] * T(v[0][2].z) + v_n[1] * T(v[1][2].z) + v_n[2] * T(v[2][2].z) + v_n[3] * T(v[3][2].z);
        const T curve3_z = v_n[0] * T(v[0][3].z) + v_n[1] * T(v[1][3].z) + v_n[2] * T(v[2][3].z) + v_n[3] * T(v[3][3].z);
        const T z = (u_n[0] * curve0_z + u_n[1] * curve1_z + u_n[2] * curve2_z + u_n[3] * curve3_z) * T(1.0f/36.0f);
        
        return Vec3<T>(x,y,z);
      }
      
      template<typename T>
      __forceinline Vec3<T> eval(const T& uu, const T& vv) const
      {
        const Vec4<T> u_n = BSplineBasis::eval(uu);
        const Vec4<T> v_n = BSplineBasis::eval(vv);
        return eval(uu,vv,u_n,v_n);
      }

      template<typename T>
      __forceinline Vec3<T> tangentU(const T& uu, const T& vv) const
      {
        const Vec4<T> u_n = BSplineBasis::derivative(uu); 
        const Vec4<T> v_n = BSplineBasis::eval(vv); 
        return eval(uu,vv,u_n,v_n);      
      }
      
      template<typename T>
      __forceinline Vec3<T> tangentV(const T& uu, const T& vv) const
      {
        const Vec4<T> u_n = BSplineBasis::eval(uu); 
        const Vec4<T> v_n = BSplineBasis::derivative(vv); 
        return eval(uu,vv,u_n,v_n);      
      }
      
      template<typename T>
      __forceinline Vec3<T> normal(const T& uu, const T& vv) const
      {
        return cross(tangentV(uu,vv),tangentU(uu,vv));
      }

      __forceinline void eval(const float u, const float v, Vertex* P, Vertex* dPdu, Vertex* dPdv, const float dscale = 1.0f) const
      {
        if (P)    *P    = eval(u,v); 
        if (dPdu) *dPdu = tangentU(u,v)*dscale; 
        if (dPdv) *dPdv = tangentV(u,v)*dscale; 
      }

      template<class vfloat>
      __forceinline vfloat eval(const size_t i, const vfloat& uu, const vfloat& vv, const Vec4<vfloat>& u_n, const Vec4<vfloat>& v_n) const
      {
        const vfloat curve0_x = v_n[0] * vfloat(v[0][0][i]) + v_n[1] * vfloat(v[1][0][i]) + v_n[2] * vfloat(v[2][0][i]) + v_n[3] * vfloat(v[3][0][i]);
        const vfloat curve1_x = v_n[0] * vfloat(v[0][1][i]) + v_n[1] * vfloat(v[1][1][i]) + v_n[2] * vfloat(v[2][1][i]) + v_n[3] * vfloat(v[3][1][i]);
        const vfloat curve2_x = v_n[0] * vfloat(v[0][2][i]) + v_n[1] * vfloat(v[1][2][i]) + v_n[2] * vfloat(v[2][2][i]) + v_n[3] * vfloat(v[3][2][i]);
        const vfloat curve3_x = v_n[0] * vfloat(v[0][3][i]) + v_n[1] * vfloat(v[1][3][i]) + v_n[2] * vfloat(v[2][3][i]) + v_n[3] * vfloat(v[3][3][i]);
        return (u_n[0] * curve0_x + u_n[1] * curve1_x + u_n[2] * curve2_x + u_n[3] * curve3_x) * vfloat(1.0f/36.0f);
      }
        
      template<typename vbool, typename vfloat>
      __forceinline void eval(const vbool& valid, const vfloat& uu, const vfloat& vv, float* P, float* dPdu, float* dPdv, const float dscale, const size_t dstride, const size_t N) const
      {
        if (P) {
          const Vec4<vfloat> u_n = BSplineBasis::eval(uu); 
          const Vec4<vfloat> v_n = BSplineBasis::eval(vv); 
          for (size_t i=0; i<N; i++) vfloat::store(valid,P+i*dstride,eval(i,uu,vv,u_n,v_n));
        }
        if (dPdu) {
          const Vec4<vfloat> u_n = BSplineBasis::derivative(uu); 
          const Vec4<vfloat> v_n = BSplineBasis::eval(vv);
          for (size_t i=0; i<N; i++) vfloat::store(valid,dPdu+i*dstride,eval(i,uu,vv,u_n,v_n)*dscale);
        }
        if (dPdv) {
          const Vec4<vfloat> u_n = BSplineBasis::eval(uu); 
          const Vec4<vfloat> v_n = BSplineBasis::derivative(vv);
          for (size_t i=0; i<N; i++) vfloat::store(valid,dPdv+i*dstride,eval(i,uu,vv,u_n,v_n)*dscale);
        }
      }

      friend __forceinline std::ostream& operator<<(std::ostream& o, const BSplinePatchT& p)
      {
        for (size_t y=0; y<4; y++)
          for (size_t x=0; x<4; x++)
            o << "[" << y << "][" << x << "] " << p.v[y][x] << std::endl;
        return o;
      } 

    public:
      Vertex v[4][4];
    };
  
  typedef BSplinePatchT<Vec3fa,Vec3fa_t> BSplinePatch3fa;
}
