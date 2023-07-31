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

#include "ray.h"

namespace embree
{
  /*! Ray structure for 4 rays. */
  struct Ray4
  {
    typedef bool4 simdb;
    typedef float4 simdf;
    typedef int4 simdi;

    /*! Default construction does nothing. */
    __forceinline Ray4() {}

    /*! Constructs a ray from origin, direction, and ray segment. Near
     *  has to be smaller than far. */
    __forceinline Ray4(const Vec3f4& org, const Vec3f4& dir, 
                       const float4& tnear = zero, const float4& tfar = inf, 
                       const float4& time = zero, const int4& mask = -1)
      : org(org), dir(dir), tnear(tnear), tfar(tfar), geomID(-1), primID(-1), instID(-1), mask(mask), time(time) {}

    /*! returns the size of the ray */
    static __forceinline size_t size() { return 4; }

    /*! Tests if we hit something. */
    __forceinline operator bool4() const { return geomID != int4(-1); }

    /*! calculates if this is a valid ray that does not cause issues during traversal */
    __forceinline bool4 valid() const {
      const bool4 vx = abs(org.x) <= float4(FLT_LARGE) & abs(dir.x) <= float4(FLT_LARGE);
      const bool4 vy = abs(org.y) <= float4(FLT_LARGE) & abs(dir.y) <= float4(FLT_LARGE);
      const bool4 vz = abs(org.z) <= float4(FLT_LARGE) & abs(dir.z) <= float4(FLT_LARGE);
      const bool4 vn = abs(tnear) <= float4(inf);
      const bool4 vf = abs(tfar) <= float4(inf);
      return vx & vy & vz & vn & vf;
    }

    /* converts ray packet to single rays */
    __forceinline void get(Ray ray[4]) const
    {
      for (size_t i=0; i<4; i++) // FIXME: use SSE transpose
      {
	ray[i].org.x = org.x[i]; ray[i].org.y = org.y[i]; ray[i].org.z = org.z[i]; 
	ray[i].dir.x = dir.x[i]; ray[i].dir.y = dir.y[i]; ray[i].dir.z = dir.z[i];
	ray[i].tnear = tnear[i]; ray[i].tfar  = tfar [i]; ray[i].time  = time[i]; ray[i].mask = mask[i];
	ray[i].Ng.x = Ng.x[i]; ray[i].Ng.y = Ng.y[i]; ray[i].Ng.z = Ng.z[i];
	ray[i].u = u[i]; ray[i].v = v[i];
	ray[i].geomID = geomID[i]; ray[i].primID = primID[i]; ray[i].instID = instID[i];
      }
    }

    /* converts single rays to ray packet */
    __forceinline void set(const Ray ray[4])
    {
      for (size_t i=0; i<4; i++)
      {
	org.x[i] = ray[i].org.x; org.y[i] = ray[i].org.y; org.z[i] = ray[i].org.z;
	dir.x[i] = ray[i].dir.x; dir.y[i] = ray[i].dir.y; dir.z[i] = ray[i].dir.z;
	tnear[i] = ray[i].tnear; tfar [i] = ray[i].tfar;  time[i] = ray[i].time; mask[i] = ray[i].mask;
	Ng.x[i] = ray[i].Ng.x; Ng.y[i] = ray[i].Ng.y; Ng.z[i] = ray[i].Ng.z;
	u[i] = ray[i].u; v[i] = ray[i].v;
	geomID[i] = ray[i].geomID; primID[i] = ray[i].primID; instID[i] = ray[i].instID;
      }
    }

  public:
    Vec3f4 org;      //!< Ray origin
    Vec3f4 dir;      //!< Ray direction
    float4 tnear;     //!< Start of ray segment 
    float4 tfar;      //!< End of ray segment   
    float4 time;      //!< Time of this ray for motion blur.
    int4 mask;      //!< used to mask out objects during traversal

  public:
    Vec3f4 Ng;       //!< Geometry normal
    float4 u;         //!< Barycentric u coordinate of hit
    float4 v;         //!< Barycentric v coordinate of hit
    int4 geomID;    //!< geometry ID
    int4 primID;    //!< primitive ID
    int4 instID;    //!< instance ID
  };

  /*! Outputs ray to stream. */
  inline std::ostream& operator<<(std::ostream& cout, const Ray4& ray) {
    return cout << "{ " 
                << "org = " << ray.org 
                << ", dir = " << ray.dir 
                << ", near = " << ray.tnear 
                << ", far = " << ray.tfar 
                << ", time = " << ray.time 
                << ", instID = " << ray.instID 
                << ", geomID = " << ray.geomID 
                << ", primID = " << ray.primID 
                << ", u = " << ray.u 
                <<  ", v = " << ray.v 
                << ", Ng = " << ray.Ng 
                << " }";
  }
}
