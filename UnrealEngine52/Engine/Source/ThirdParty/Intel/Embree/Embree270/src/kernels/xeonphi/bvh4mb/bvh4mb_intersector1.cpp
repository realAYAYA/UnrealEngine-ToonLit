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

#include "bvh4mb_intersector1.h"
#include "bvh4mb_traversal.h"
#include "bvh4mb_leaf_intersector.h"

namespace embree
{
  namespace isa
  {
    static unsigned int BVH4I_LEAF_MASK = BVH4i::leaf_mask; // needed due to compiler efficiency bug

    static __aligned(64) int zlc4[4] = {0xffffffff,0xffffffff,0xffffffff,0};
    
    template<typename LeafIntersector>
    void BVH4mbIntersector1<LeafIntersector>::intersect(BVH4mb* bvh, Ray& ray)
    {
      /* near and node stack */
      __aligned(64) float   stack_dist[3*BVH4i::maxDepth+1];
      __aligned(64) NodeRef stack_node[3*BVH4i::maxDepth+1];

      /* setup */
      const Vec3f16 rdir16     = rcp_safe(Vec3f16(float16(ray.dir.x),float16(ray.dir.y),float16(ray.dir.z)));
      const float16 inf        = float16(pos_inf);
      const float16 zero       = float16::zero();

      store16f(stack_dist,inf);

      const Node               * __restrict__ nodes = (Node     *)bvh->nodePtr();
      const BVH4mb::Triangle01 * __restrict__ accel = (BVH4mb::Triangle01 *)bvh->triPtr();

      stack_node[0] = BVH4i::invalidNode;      
      stack_node[1] = bvh->root;

      size_t sindex = 2;

      const float16 org_xyz      = loadAOS4to16f(ray.org.x,ray.org.y,ray.org.z);
      const float16 dir_xyz      = loadAOS4to16f(ray.dir.x,ray.dir.y,ray.dir.z);
      const float16 rdir_xyz     = loadAOS4to16f(rdir16.x[0],rdir16.y[0],rdir16.z[0]);
      const float16 org_rdir_xyz = org_xyz * rdir_xyz;
      const float16 min_dist_xyz = broadcast1to16f(&ray.tnear);
      float16       max_dist_xyz = broadcast1to16f(&ray.tfar);
      const float16 time         = broadcast1to16f(&ray.time);
	  
      const unsigned int leaf_mask = BVH4I_LEAF_MASK;
      const bool16 m7777 = 0x7777; 
      const bool16 m_rdir0 = lt(m7777,rdir_xyz,float16::zero());
      const bool16 m_rdir1 = ge(m7777,rdir_xyz,float16::zero());
	  
      while (1)
	{
	  NodeRef curNode = stack_node[sindex-1];
	  sindex--;
            
	  traverse_single_intersect(curNode,
				    sindex,
				    rdir_xyz,
				    org_rdir_xyz,
				    min_dist_xyz,
				    max_dist_xyz,
				    time,
				    stack_node,
				    stack_dist,
				    nodes,
				    leaf_mask);
	  
	    

	  /* return if stack is empty */
	  if (unlikely(curNode == BVH4i::invalidNode)) break;


	  /* intersect one ray against four triangles */

	  //////////////////////////////////////////////////////////////////////////////////////////////////

	  const bool hit = LeafIntersector::intersect(curNode,
						      dir_xyz,
						      org_xyz,
						      min_dist_xyz,
						      max_dist_xyz,
						      ray,
						      accel,
						      (Scene*)bvh->geometry);
									   
	  if (hit)
	    compactStack(stack_node,stack_dist,sindex,max_dist_xyz);

	  //////////////////////////////////////////////////////////////////////////////////////////////////

	}	  
    }

    template<typename LeafIntersector>
    void BVH4mbIntersector1<LeafIntersector>::occluded(BVH4mb* bvh, Ray& ray)
    {
      /* near and node stack */
      __aligned(64) NodeRef stack_node[3*BVH4i::maxDepth+1];

      /* setup */
      const Vec3f16 rdir16      = rcp_safe(Vec3f16(ray.dir.x,ray.dir.y,ray.dir.z));
      const float16 inf         = float16(pos_inf);
      const float16 zero        = float16::zero();

      const Node               * __restrict__ nodes = (Node     *)bvh->nodePtr();
      const BVH4mb::Triangle01 * __restrict__ accel = (BVH4mb::Triangle01 *)bvh->triPtr();

      stack_node[0] = BVH4i::invalidNode;
      stack_node[1] = bvh->root;
      size_t sindex = 2;

      const float16 org_xyz      = loadAOS4to16f(ray.org.x,ray.org.y,ray.org.z);
      const float16 dir_xyz      = loadAOS4to16f(ray.dir.x,ray.dir.y,ray.dir.z);
      const float16 rdir_xyz     = loadAOS4to16f(rdir16.x[0],rdir16.y[0],rdir16.z[0]);
      const float16 org_rdir_xyz = org_xyz * rdir_xyz;
      const float16 min_dist_xyz = broadcast1to16f(&ray.tnear);
      const float16 max_dist_xyz = broadcast1to16f(&ray.tfar);
      const float16 time         = broadcast1to16f(&ray.time);

      const unsigned int leaf_mask = BVH4I_LEAF_MASK;
      const bool16 m7777 = 0x7777; 
      const bool16 m_rdir0 = lt(m7777,rdir_xyz,float16::zero());
      const bool16 m_rdir1 = ge(m7777,rdir_xyz,float16::zero());
	  
      while (1)
	{
	  NodeRef curNode = stack_node[sindex-1];
	  sindex--;
          
	  traverse_single_occluded(curNode,
				   sindex,
				   rdir_xyz,
				   org_rdir_xyz,
				   min_dist_xyz,
				   max_dist_xyz,
				   time,
				   stack_node,
				   nodes,
				   leaf_mask);

	  /* return if stack is empty */
	  if (unlikely(curNode == BVH4i::invalidNode)) break;


	  /* intersect one ray against four triangles */

	  //////////////////////////////////////////////////////////////////////////////////////////////////

	  bool hit = LeafIntersector::occluded(curNode,
					       dir_xyz,
					       org_xyz,
					       min_dist_xyz,
					       max_dist_xyz,
					       ray,
					       accel,
					       (Scene*)bvh->geometry);

	  if (unlikely(hit))
	    {
	      ray.geomID = 0;
	      return;
	    }

	  //////////////////////////////////////////////////////////////////////////////////////////////////

	}
    }


    DEFINE_INTERSECTOR1    (BVH4mbTriangle1Intersector1, BVH4mbIntersector1<Triangle1mbLeafIntersector>);
    DEFINE_INTERSECTOR1    (BVH4mbVirtualIntersector1, BVH4mbIntersector1<Triangle1mbLeafIntersector>);

  }
}
