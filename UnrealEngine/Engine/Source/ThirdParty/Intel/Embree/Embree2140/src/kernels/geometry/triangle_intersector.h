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

#include "triangle.h"
#include "triangle_intersector_moeller.h"

namespace embree
{
  namespace isa
  {
    /*! Intersects M triangles with 1 ray */
    template<int M, int Mx, bool filter>
      struct TriangleMIntersector1Moeller
      {
        typedef TriangleM<M> Primitive;
        typedef Intersector1Precalculations<MoellerTrumboreIntersector1<Mx>> Precalculations;
        
        /*! Intersect a ray with the M triangles and updates the hit. */
        static __forceinline void intersect(const Precalculations& pre, Ray& ray, IntersectContext* context, const TriangleM<M>& tri)
        {
          STAT3(normal.trav_prims,1,1,1);
          pre.intersect(ray,tri.v0,tri.e1,tri.e2,tri.Ng,Intersect1EpilogM<M,Mx,filter>(ray,context,tri.geomIDs,tri.primIDs));
        }
        
        /*! Test if the ray is occluded by one of M triangles. */
        static __forceinline bool occluded(const Precalculations& pre, Ray& ray, IntersectContext* context, const TriangleM<M>& tri)
        {
          STAT3(shadow.trav_prims,1,1,1);
          return pre.intersect(ray,tri.v0,tri.e1,tri.e2,tri.Ng,Occluded1EpilogM<M,Mx,filter>(ray,context,tri.geomIDs,tri.primIDs));
        }

        /*! Intersect an array of rays with an array of M primitives. */
        static __forceinline size_t intersect(Precalculations* pre, size_t valid, Ray** rays, IntersectContext* context,  size_t ty, const Primitive* prim, size_t num)
        {
          size_t valid_isec = 0;
          do {
            const size_t i = __bscf(valid);
            const float old_far = rays[i]->tfar;
            for (size_t n=0; n<num; n++)
              intersect(pre[i],*rays[i],context,prim[n]);
            valid_isec |= (rays[i]->tfar < old_far) ? ((size_t)1 << i) : 0;            
          } while(unlikely(valid));
          return valid_isec;
        }       
      };

#if defined(__AVX__)
    template<bool filter>
      struct TriangleMIntersector1Moeller<4,8,filter>
      {
        static const size_t M = 4;
        static const size_t Mx = 8;

        typedef TriangleM<M> Primitive;
        typedef Intersector1Precalculations<MoellerTrumboreIntersector1<Mx>> Precalculations;

        static __forceinline void intersect(const Precalculations& pre, Ray& ray, IntersectContext* context, const TriangleM<M>& tri)
        {
          STAT3(normal.trav_prims,1,1,1);
          pre.intersect(ray,tri.v0,tri.e1,tri.e2,tri.Ng,Intersect1EpilogM<M,Mx,filter>(ray,context,tri.geomIDs,tri.primIDs));
        }

        /*! Test if the ray is occluded by one of M triangles. */
        static __forceinline bool occluded(const Precalculations& pre, Ray& ray, IntersectContext* context, const TriangleM<M>& tri)
        {
          STAT3(shadow.trav_prims,1,1,1);
          return pre.intersect(ray,tri.v0,tri.e1,tri.e2,tri.Ng,Occluded1EpilogM<M,Mx,filter>(ray,context,tri.geomIDs,tri.primIDs));
        }


        /*! Intersect an array of rays with an array of M primitives. */
        static __forceinline size_t intersect(Precalculations* pre, size_t valid, Ray** rays, IntersectContext* context,  size_t ty, const Primitive* prim, size_t num)
        {
          size_t valid_isec = 0;
          do {
            const size_t i = __bscf(valid);
            for (size_t n=0; n<num; n++)
              intersect(pre[i],*rays[i],context,prim[n]);
            valid_isec |=  ((size_t)1 << i);
          } while(unlikely(valid));
          return valid_isec;
        }

      };
#endif

    /*! Intersects M triangles with K rays. */
    template<int M, int Mx, int K, bool filter>
      struct TriangleMIntersectorKMoeller
      {
        typedef TriangleM<M> Primitive;
        typedef IntersectorKPrecalculations<K,MoellerTrumboreIntersectorK<Mx,K>> Precalculations;
        
        /*! Intersects K rays with M triangles. */
        static __forceinline void intersect(const vbool<K>& valid_i, Precalculations& pre, RayK<K>& ray, IntersectContext* context, const TriangleM<M>& tri)
        {
          STAT_USER(0,TriangleM<M>::max_size());
          for (size_t i=0; i<TriangleM<M>::max_size(); i++)
          {
            if (!tri.valid(i)) break;
            STAT3(normal.trav_prims,1,popcnt(valid_i),K);
            const Vec3<vfloat<K>> p0 = broadcast<vfloat<K>>(tri.v0,i);
            const Vec3<vfloat<K>> e1 = broadcast<vfloat<K>>(tri.e1,i);
            const Vec3<vfloat<K>> e2 = broadcast<vfloat<K>>(tri.e2,i);
            const Vec3<vfloat<K>> Ng = broadcast<vfloat<K>>(tri.Ng,i);
            pre.intersectK(valid_i,ray,p0,e1,e2,Ng,IntersectKEpilogM<M,K,filter>(ray,context,tri.geomIDs,tri.primIDs,i));
          }
        }
        
        /*! Test for K rays if they are occluded by any of the M triangles. */
        static __forceinline vbool<K> occluded(const vbool<K>& valid_i, Precalculations& pre, RayK<K>& ray, IntersectContext* context, const TriangleM<M>& tri)
        {
          vbool<K> valid0 = valid_i;
          
          for (size_t i=0; i<TriangleM<M>::max_size(); i++)
          {
            if (!tri.valid(i)) break;
            STAT3(shadow.trav_prims,1,popcnt(valid0),K);
            const Vec3<vfloat<K>> p0 = broadcast<vfloat<K>>(tri.v0,i);
            const Vec3<vfloat<K>> e1 = broadcast<vfloat<K>>(tri.e1,i);
            const Vec3<vfloat<K>> e2 = broadcast<vfloat<K>>(tri.e2,i);
            const Vec3<vfloat<K>> Ng = broadcast<vfloat<K>>(tri.Ng,i);
            pre.intersectK(valid0,ray,p0,e1,e2,Ng,OccludedKEpilogM<M,K,filter>(valid0,ray,context,tri.geomIDs,tri.primIDs,i));
            if (none(valid0)) break;
          }
          return !valid0;
        }
        
        /*! Intersect a ray with M triangles and updates the hit. */
        static __forceinline void intersect(Precalculations& pre, RayK<K>& ray, size_t k, IntersectContext* context, const TriangleM<M>& tri)
        {
          STAT3(normal.trav_prims,1,1,1);
          pre.intersect(ray,k,tri.v0,tri.e1,tri.e2,tri.Ng,Intersect1KEpilogM<M,Mx,K,filter>(ray,k,context,tri.geomIDs,tri.primIDs));
        }
        
        /*! Test if the ray is occluded by one of the M triangles. */
        static __forceinline bool occluded(Precalculations& pre, RayK<K>& ray, size_t k, IntersectContext* context, const TriangleM<M>& tri)
        {
          STAT3(shadow.trav_prims,1,1,1);
          return pre.intersect(ray,k,tri.v0,tri.e1,tri.e2,tri.Ng,Occluded1KEpilogM<M,Mx,K,filter>(ray,k,context,tri.geomIDs,tri.primIDs));
        }
      };
  }
}
