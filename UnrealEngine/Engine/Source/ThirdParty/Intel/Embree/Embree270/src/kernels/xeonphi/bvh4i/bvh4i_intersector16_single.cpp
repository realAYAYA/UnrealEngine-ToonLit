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

#include "bvh4i_intersector16_single.h"
#include "bvh4i_leaf_intersector.h"



namespace embree
{
  namespace isa
  {
    
    static unsigned int BVH4I_LEAF_MASK = BVH4i::leaf_mask; // needed due to compiler efficiency bug
    static unsigned int M_LANE_7777 = 0x7777;               // needed due to compiler efficiency bug

    // ============================================================================================
    // ============================================================================================
    // ============================================================================================

    template<typename LeafIntersector, bool ENABLE_COMPRESSED_BVH4I_NODES, bool ROBUST>
    void BVH4iIntersector16Single<LeafIntersector,ENABLE_COMPRESSED_BVH4I_NODES,ROBUST>::intersect(int16* valid_i, BVH4i* bvh, Ray16& ray16)
    {
      /* near and node stack */
      __aligned(64) float   stack_dist[3*BVH4i::maxDepth+1];
      __aligned(64) NodeRef stack_node[3*BVH4i::maxDepth+1];

      /* setup */
      const bool16 m_valid    = *(int16*)valid_i != int16(0);
      const Vec3f16 rdir16     = rcp_safe(ray16.dir);
      const float16 inf        = float16(pos_inf);
      const float16 zero       = float16::zero();

      store16f(stack_dist,inf);

      const Node      * __restrict__ nodes = (Node     *)bvh->nodePtr();
      const Triangle1 * __restrict__ accel = (Triangle1*)bvh->triPtr();

      stack_node[0] = BVH4i::invalidNode;
      long rayIndex = -1;
      while((rayIndex = bitscan64(rayIndex,toInt(m_valid))) != BITSCAN_NO_BIT_SET_64)	    
        {
	  stack_node[1] = bvh->root;
	  size_t sindex = 2;
	  const float16 dir_xyz      = loadAOS4to16f(rayIndex,ray16.dir.x,ray16.dir.y,ray16.dir.z);
	  const float16 org_xyz      = loadAOS4to16f(rayIndex,ray16.org.x,ray16.org.y,ray16.org.z);
	  const float16 rdir_xyz     = loadAOS4to16f(rayIndex,rdir16.x,rdir16.y,rdir16.z);
	  //const float16 org_rdir_xyz = org_xyz * rdir_xyz;
	  const float16 min_dist_xyz = broadcast1to16f(&ray16.tnear[rayIndex]);
	  float16       max_dist_xyz = broadcast1to16f(&ray16.tfar[rayIndex]);

	  const unsigned int leaf_mask = BVH4I_LEAF_MASK;
	  
	  const Precalculations precalculations(org_xyz,rdir_xyz);

	  while (1)
	    {

	      NodeRef curNode = stack_node[sindex-1];
	      sindex--;

	      traverse_single_intersect<ENABLE_COMPRESSED_BVH4I_NODES,ROBUST>(curNode,
									      sindex,
									      precalculations,
									      min_dist_xyz,
									      max_dist_xyz,
									      stack_node,
									      stack_dist,
									      nodes,
									      leaf_mask);
		   


	      /* return if stack is empty */
	      if (unlikely(curNode == BVH4i::invalidNode)) break;

	      STAT3(normal.trav_leaves,1,1,1);
	      STAT3(normal.trav_prims,4,4,4);

	      /* intersect one ray against four triangles */

	      //////////////////////////////////////////////////////////////////////////////////////////////////
	      // PING;
	      const bool hit = LeafIntersector::intersect(curNode,
							  rayIndex,
							  dir_xyz,
							  org_xyz,
							  min_dist_xyz,
							  max_dist_xyz,
							  ray16,
							  precalculations,
							  accel,
							  (Scene*)bvh->geometry);
									   
	      if (hit)
		compactStack(stack_node,stack_dist,sindex,max_dist_xyz);

	      // ------------------------
	    }	  
	}
    }

    template<typename LeafIntersector,bool ENABLE_COMPRESSED_BVH4I_NODES, bool ROBUST>    
    void BVH4iIntersector16Single<LeafIntersector,ENABLE_COMPRESSED_BVH4I_NODES,ROBUST>::occluded(int16* valid_i, BVH4i* bvh, Ray16& ray16)
    {
      /* near and node stack */
      __aligned(64) NodeRef stack_node[3*BVH4i::maxDepth+1];

      /* setup */
      const bool16 m_valid = *(int16*)valid_i != int16(0);
      const Vec3f16 rdir16  = rcp_safe(ray16.dir);
      bool16 terminated    = !m_valid;
      const float16 inf     = float16(pos_inf);
      const float16 zero    = float16::zero();

      const Node      * __restrict__ nodes = (Node     *)bvh->nodePtr();
      const Triangle1 * __restrict__ accel = (Triangle1*)bvh->triPtr();

      stack_node[0] = BVH4i::invalidNode;

      long rayIndex = -1;
      while((rayIndex = bitscan64(rayIndex,toInt(m_valid))) != BITSCAN_NO_BIT_SET_64)	    
        {
	  stack_node[1] = bvh->root;
	  size_t sindex = 2;

	  const float16 org_xyz      = loadAOS4to16f(rayIndex,ray16.org.x,ray16.org.y,ray16.org.z);
	  const float16 dir_xyz      = loadAOS4to16f(rayIndex,ray16.dir.x,ray16.dir.y,ray16.dir.z);
	  const float16 rdir_xyz     = loadAOS4to16f(rayIndex,rdir16.x,rdir16.y,rdir16.z);
	  //const float16 org_rdir_xyz = org_xyz * rdir_xyz;
	  const float16 min_dist_xyz = broadcast1to16f(&ray16.tnear[rayIndex]);
	  const float16 max_dist_xyz = broadcast1to16f(&ray16.tfar[rayIndex]);
	  const int16 v_invalidNode(BVH4i::invalidNode);
	  const unsigned int leaf_mask = BVH4I_LEAF_MASK;

	  const Precalculations precalculations(org_xyz,rdir_xyz);

	  while (1)
	    {
	      NodeRef curNode = stack_node[sindex-1];
	      sindex--;

	      traverse_single_occluded< ENABLE_COMPRESSED_BVH4I_NODES,ROBUST >(curNode,
									       sindex,
									       precalculations,
									       min_dist_xyz,
									       max_dist_xyz,
									       stack_node,
									       nodes,
									       leaf_mask);

	      /* return if stack is empty */
	      if (unlikely(curNode == BVH4i::invalidNode)) break;

	      STAT3(shadow.trav_leaves,1,1,1);
	      STAT3(shadow.trav_prims,4,4,4);

	      /* intersect one ray against four triangles */

	      //////////////////////////////////////////////////////////////////////////////////////////////////

	      const bool hit = LeafIntersector::occluded(curNode,
							 rayIndex,
							 dir_xyz,
							 org_xyz,
							 min_dist_xyz,
							 max_dist_xyz,
							 ray16,
							 precalculations,
							 terminated,
							 accel,
							 (Scene*)bvh->geometry);

	      if (unlikely(hit)) break;
	      //////////////////////////////////////////////////////////////////////////////////////////////////

	    }


	  if (unlikely(all(toMask(terminated)))) break;
	}


      store16i(m_valid & toMask(terminated),&ray16.geomID,0);

    }

    typedef BVH4iIntersector16Single< Triangle1LeafIntersector  < true >, false, false  > Triangle1Intersector16SingleMoellerFilter;
    typedef BVH4iIntersector16Single< Triangle1LeafIntersector  < false >, false, false > Triangle1Intersector16SingleMoellerNoFilter;
    typedef BVH4iIntersector16Single< Triangle1mcLeafIntersector< true >, true, false  > Triangle1mcIntersector16SingleMoellerFilter;
    typedef BVH4iIntersector16Single< Triangle1mcLeafIntersector< false >, true, false > Triangle1mcIntersector16SingleMoellerNoFilter;

    typedef BVH4iIntersector16Single< Triangle1LeafIntersectorRobust  < true >, false, true  > Triangle1Intersector16SingleMoellerFilterRobust;
    typedef BVH4iIntersector16Single< Triangle1LeafIntersectorRobust  < false >, false, true > Triangle1Intersector16SingleMoellerNoFilterRobust;

    DEFINE_INTERSECTOR16    (BVH4iTriangle1Intersector16SingleMoeller          , Triangle1Intersector16SingleMoellerFilter);
    DEFINE_INTERSECTOR16    (BVH4iTriangle1Intersector16SingleMoellerNoFilter  , Triangle1Intersector16SingleMoellerNoFilter);
    DEFINE_INTERSECTOR16    (BVH4iTriangle1mcIntersector16SingleMoeller        , Triangle1mcIntersector16SingleMoellerFilter);
    DEFINE_INTERSECTOR16    (BVH4iTriangle1mcIntersector16SingleMoellerNoFilter, Triangle1mcIntersector16SingleMoellerNoFilter);

    DEFINE_INTERSECTOR16    (BVH4iTriangle1Intersector16SingleMoellerRobust          , Triangle1Intersector16SingleMoellerFilterRobust);
    DEFINE_INTERSECTOR16    (BVH4iTriangle1Intersector16SingleMoellerNoFilterRobust  , Triangle1Intersector16SingleMoellerNoFilterRobust);

    // ----------------------------------------------------------------------------------------------------------------
    // ----------------------------------------------------------------------------------------------------------------
    // ----------------------------------------------------------------------------------------------------------------



    // typedef BVH4iIntersector16Single< SubdivLeafIntersector    < true  >, false > SubdivIntersector16SingleMoellerFilter;
    // typedef BVH4iIntersector16Single< SubdivLeafIntersector    < false >, false > SubdivIntersector16SingleMoellerNoFilter;

    // DEFINE_INTERSECTOR16   (BVH4iSubdivMeshIntersector16        , SubdivIntersector16SingleMoellerFilter);
    // DEFINE_INTERSECTOR16   (BVH4iSubdivMeshIntersector16NoFilter, SubdivIntersector16SingleMoellerNoFilter);

  }
}
