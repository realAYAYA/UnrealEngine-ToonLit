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

#include "bvh4mb_intersector16_chunk.h"
#include "bvh4mb_traversal.h"
#include "bvh4mb_leaf_intersector.h"

namespace embree
{
  namespace isa
  {
    static unsigned int BVH4I_LEAF_MASK = BVH4i::leaf_mask; // needed due to compiler efficiency bug

    template<typename LeafIntersector>
    void BVH4mbIntersector16Chunk<LeafIntersector>::intersect(int16* valid_i, BVH4mb* bvh, Ray16& ray)
    {
      /* near and node stack */
      __aligned(64) float16   stack_dist[3*BVH4i::maxDepth+1];
      __aligned(64) NodeRef stack_node[3*BVH4i::maxDepth+1];

      /* load ray */
      const bool16 valid0   = *(int16*)valid_i != int16(0);
      const Vec3f16 rdir     = rcp_safe(ray.dir);
 
      const Vec3f16 org_rdir = ray.org * rdir;
      float16 ray_tnear      = select(valid0,ray.tnear,pos_inf);
      float16 ray_tfar       = select(valid0,ray.tfar ,neg_inf);
      const float16 inf      = float16(pos_inf);

      /* allocate stack and push root node */
      stack_node[0] = BVH4i::invalidNode;
      stack_dist[0] = inf;
      stack_node[1] = bvh->root;
      stack_dist[1] = ray_tnear; 
      NodeRef* __restrict__ sptr_node = stack_node + 2;
      float16*   __restrict__ sptr_dist = stack_dist + 2;
      
      const Node               * __restrict__ nodes = (Node     *)bvh->nodePtr();
      const BVH4mb::Triangle01 * __restrict__ accel = (BVH4mb::Triangle01 *)bvh->triPtr();

      while (1)
      {
        /* pop next node from stack */
        NodeRef curNode = *(sptr_node-1);
        float16 curDist   = *(sptr_dist-1);
        sptr_node--;
        sptr_dist--;
	const bool16 m_stackDist = ray_tfar > curDist;

	/* stack emppty ? */
        if (unlikely(curNode == BVH4i::invalidNode))  break;
        
        /* cull node if behind closest hit point */
        if (unlikely(none(m_stackDist))) {continue;}
	        
	const unsigned int leaf_mask = BVH4I_LEAF_MASK; 

	traverse_chunk_intersect(curNode,
				 curDist,
				 rdir,
				 org_rdir,
				 ray_tnear,
				 ray_tfar,
				 ray.time,
				 sptr_node,
				 sptr_dist,
				 nodes,
				 leaf_mask);
        
        /* return if stack is empty */
        if (unlikely(curNode == BVH4i::invalidNode)) break;
        
        /* intersect leaf */
        const bool16 m_valid_leaf = ray_tfar > curDist;
        STAT3(normal.trav_leaves,1,popcnt(m_valid_leaf),16);
 
	LeafIntersector::intersect16(curNode,
				     m_valid_leaf,
				     ray.dir,
				     ray.org,
				     ray,
				     accel,
				     (Scene*)bvh->geometry);

        ray_tfar = select(m_valid_leaf,ray.tfar,ray_tfar);
      }
    }

    template<typename LeafIntersector>
    void BVH4mbIntersector16Chunk<LeafIntersector>::occluded(int16* valid_i, BVH4mb* bvh, Ray16& ray)
    {
      /* allocate stack */
      __aligned(64) float16    stack_dist[3*BVH4i::maxDepth+1];
      __aligned(64) NodeRef stack_node[3*BVH4i::maxDepth+1];

      /* load ray */
      const bool16 valid = *(int16*)valid_i != int16(0);
      bool16 m_terminated = !valid;
      const Vec3f16 rdir = rcp_safe(ray.dir);
      const Vec3f16 org_rdir = ray.org * rdir;
      float16 ray_tnear = select(valid,ray.tnear,pos_inf);
      float16 ray_tfar  = select(valid,ray.tfar ,neg_inf);
      const float16 inf = float16(pos_inf);
      
      /* push root node */
      stack_node[0] = BVH4i::invalidNode;
      stack_dist[0] = inf;
      stack_node[1] = bvh->root;
      stack_dist[1] = ray_tnear; 
      NodeRef* __restrict__ sptr_node = stack_node + 2;
      float16*   __restrict__ sptr_dist = stack_dist + 2;
      
      const Node               * __restrict__ nodes = (Node     *)bvh->nodePtr();
      const BVH4mb::Triangle01 * __restrict__ accel = (BVH4mb::Triangle01 *)bvh->triPtr();

      while (1)
      {
	const bool16 m_active = !m_terminated;

        /* pop next node from stack */
        NodeRef curNode = *(sptr_node-1);
        float16 curDist   = *(sptr_dist-1);
        sptr_node--;
        sptr_dist--;
	const bool16 m_stackDist = gt(m_active,ray_tfar,curDist);

	/* stack emppty ? */
        if (unlikely(curNode == BVH4i::invalidNode))  break;
        
        /* cull node if behind closest hit point */

        if (unlikely(none(m_stackDist))) { continue; }
	
	const unsigned int leaf_mask = BVH4I_LEAF_MASK; 

	traverse_chunk_occluded(curNode,
				 curDist,
				 rdir,
				 org_rdir,
				 ray_tnear,
				 ray_tfar,
				 m_active,
				 ray.time,
				 sptr_node,
				 sptr_dist,
				 nodes,
				 leaf_mask);
        
        /* return if stack is empty */
        if (unlikely(curNode == BVH4i::invalidNode)) break;
        
        /* intersect leaf */
        bool16 m_valid_leaf = gt(m_active,ray_tfar,curDist);
        STAT3(shadow.trav_leaves,1,popcnt(m_valid_leaf),16);

	LeafIntersector::occluded16(curNode,
				    m_valid_leaf,
				    ray.dir,
				    ray.org,
				    ray,
				    m_terminated,
				    accel,
				    (Scene*)bvh->geometry);


        if (unlikely(all(m_terminated))) break;
        ray_tfar = select(m_terminated,neg_inf,ray_tfar);
      }
      store16i(valid & m_terminated,&ray.geomID,0);
    }

    DEFINE_INTERSECTOR16    (BVH4mbTriangle1Intersector16ChunkMoeller, BVH4mbIntersector16Chunk<Triangle1mbLeafIntersector>);
  }
}
