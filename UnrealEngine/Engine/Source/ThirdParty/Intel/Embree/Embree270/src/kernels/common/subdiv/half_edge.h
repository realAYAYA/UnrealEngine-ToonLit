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

#include "catmullclark_coefficients.h"

namespace embree
{
  class __aligned(32) HalfEdge
  {
    friend class SubdivMesh;
    public:

    enum PatchType : char { 
      REGULAR_QUAD_PATCH       = 0, //!< a regular quad patch can be represented as a B-Spline
      IRREGULAR_QUAD_PATCH     = 1, //!< an irregular quad patch can be represented as a Gregory patch
      IRREGULAR_TRIANGLE_PATCH = 2, //!< an irregular triangle patch can be represented as a Gregory patch
      COMPLEX_PATCH            = 3  //!< these patches need subdivision and cannot be processed by the above fast code paths
    };
    
    enum VertexType : char { 
      REGULAR_VERTEX           = 0, //!< regular vertex
      NON_MANIFOLD_EDGE_VERTEX = 1, //!< vertex of a non-manifold edge
    };
    
    __forceinline friend PatchType max( const PatchType& ty0, const PatchType& ty1) {
      return (PatchType) max((int)ty0,(int)ty1);
    }
    
    HalfEdge () 
      : vtx_index(-1), next_half_edge_ofs(0), prev_half_edge_ofs(0), opposite_half_edge_ofs(0), edge_crease_weight(0), 
      vertex_crease_weight(0), edge_level(0), patch_type(COMPLEX_PATCH), vertex_type(REGULAR_VERTEX)
    {
      static_assert(sizeof(HalfEdge) == 32, "invalid half edge size");
    }
    
    __forceinline bool hasOpposite() const { return opposite_half_edge_ofs != 0; }
    __forceinline void setOpposite(HalfEdge* opposite) { opposite_half_edge_ofs = opposite-this; }
    
    __forceinline       HalfEdge* next()       { assert( next_half_edge_ofs != 0 ); return &this[next_half_edge_ofs]; }
    __forceinline const HalfEdge* next() const { assert( next_half_edge_ofs != 0 ); return &this[next_half_edge_ofs]; }
    
    __forceinline       HalfEdge* prev()       { assert( prev_half_edge_ofs != 0 ); return &this[prev_half_edge_ofs]; }
    __forceinline const HalfEdge* prev() const { assert( prev_half_edge_ofs != 0 ); return &this[prev_half_edge_ofs]; }
    
    __forceinline       HalfEdge* opposite()       { assert( opposite_half_edge_ofs != 0 ); return &this[opposite_half_edge_ofs]; }
    __forceinline const HalfEdge* opposite() const { assert( opposite_half_edge_ofs != 0 ); return &this[opposite_half_edge_ofs]; }
    
    __forceinline       HalfEdge* rotate()       { return opposite()->next(); }
    __forceinline const HalfEdge* rotate() const { return opposite()->next(); }
    
    __forceinline unsigned int getStartVertexIndex() const { return vtx_index; }
    __forceinline unsigned int getEndVertexIndex  () const { return next()->vtx_index; }
    
    /*! tests if the start vertex of the edge is regular */
    __forceinline PatchType vertexType() const
    {
      const HalfEdge* p = this;
      size_t face_valence = 0;
      bool hasBorder = false;
      
      do
      {
        /* we need subdivision to handle edge creases */
        if (p->hasOpposite() && p->edge_crease_weight > 0.0f) 
          return COMPLEX_PATCH;
        
        face_valence++;
        
        /* test for quad */
        const HalfEdge* pp = p;
        pp = pp->next(); if (pp == p) return COMPLEX_PATCH;
        pp = pp->next(); if (pp == p) return COMPLEX_PATCH;
        pp = pp->next(); if (pp == p) return COMPLEX_PATCH;
        pp = pp->next(); if (pp != p) return COMPLEX_PATCH;
        
        /* continue with next face */
        p = p->prev();
        if (likely(p->hasOpposite())) 
          p = p->opposite();
        
        /* if there is no opposite go the long way to the other side of the border */
        else
        {
          face_valence++;
          hasBorder = true;
          p = this;
          while (p->hasOpposite()) 
            p = p->rotate();
        }
      } while (p != this); 
      
      /* calculate vertex type */
      if (face_valence == 2 && hasBorder) {
        if      (vertex_crease_weight == 0.0f      ) return REGULAR_QUAD_PATCH;
        else if (vertex_crease_weight == float(inf)) return REGULAR_QUAD_PATCH;
        else                                         return COMPLEX_PATCH;
      }
      else if (vertex_crease_weight != 0.0f)         return COMPLEX_PATCH;
      else if (face_valence == 3 &&  hasBorder)      return REGULAR_QUAD_PATCH;
      else if (face_valence == 4 && !hasBorder)      return REGULAR_QUAD_PATCH;
      else                                           return IRREGULAR_QUAD_PATCH;
    }
    
    /*! calculates the type of the patch */
    __forceinline PatchType patchType() const 
    {
      const HalfEdge* p = this;
      PatchType ret = REGULAR_QUAD_PATCH;
      
      ret = max(ret,p->vertexType());
      if ((p = p->next()) == this) return COMPLEX_PATCH;
      
      ret = max(ret,p->vertexType());
      if ((p = p->next()) == this) return COMPLEX_PATCH;
      
      ret = max(ret,p->vertexType());
      if ((p = p->next()) == this) 
      {
        /* if (ret == REGULAR_QUAD_PATCH || ret == IRREGULAR_QUAD_PATCH) */
        /*   { */
        /* 	return IRREGULAR_TRIANGLE_PATCH; */
        /*   } */
        return COMPLEX_PATCH;
      }
      
      ret = max(ret,p->vertexType());
      if ((p = p->next()) != this) return COMPLEX_PATCH;
      
      return ret;
    }
    
    /*! tests if the face is a regular b-spline face */
    __forceinline bool isRegularFace() const {
      return patch_type == REGULAR_QUAD_PATCH;
    }
    
    /*! tests if the face can be diced (using bspline or gregory patch) */
    __forceinline bool isGregoryFace() const {
      return patch_type == IRREGULAR_QUAD_PATCH || patch_type == REGULAR_QUAD_PATCH;
    }
    
    /*! tests if the base vertex of this half edge is a corner vertex */
    __forceinline bool isCorner() const {
      return !hasOpposite() && !prev()->hasOpposite();
    }
    
    /*! tests if the vertex is attached to any border */
    __forceinline bool vertexHasBorder() const 
    {
      const HalfEdge* p = this;
      do {
        if (!p->hasOpposite()) return true;
        p = p->rotate();
      } while (p != this);
      return false;
    }
    
    /*! tests if the face this half edge belongs to has some border */
    __forceinline bool faceHasBorder() const 
    {
      const HalfEdge* p = this;
      do {
        if (p->vertexHasBorder()) return true;
        p = p->next();
      } while (p != this);
      return false;
    }
    
    /*! calculates conservative bounds of a catmull clark subdivision face */
    __forceinline BBox3fa bounds(const BufferT<Vec3fa>& vertices) const
    {
      BBox3fa bounds = this->get1RingBounds(vertices);
      for (const HalfEdge* p=this->next(); p!=this; p=p->next())
        bounds.extend(p->get1RingBounds(vertices));
      return bounds;
    }
    
    /*! tests if this is a valid patch */
    __forceinline bool valid(const BufferT<Vec3fa>& vertices) const
    {
      size_t N = 1;
      if (!this->validRing(vertices)) return false;
      for (const HalfEdge* p=this->next(); p!=this; p=p->next(), N++) {
        if (!p->validRing(vertices)) return false;
      }
      return N >= 3 && N <= MAX_PATCH_VALENCE;
    }
    
    /*! counts number of polygon edges  */
    __forceinline size_t numEdges() const
    {
      size_t N = 1;
      for (const HalfEdge* p=this->next(); p!=this; p=p->next(), N++);
      return N;
    }
    
    /*! stream output */
    friend __forceinline std::ostream &operator<<(std::ostream &o, const HalfEdge &h)
    {
      return o << "{ " << 
        "vertex = " << h.vtx_index << ", " << //" -> " << h.next()->vtx_index << ", " << 
        "prev = " << h.prev_half_edge_ofs << ", " << 
        "next = " << h.next_half_edge_ofs << ", " << 
        "opposite = " << h.opposite_half_edge_ofs << ", " << 
        //"edge_crease = " << h.edge_crease_weight << ", " << 
        //"vertex_crease = " << h.vertex_crease_weight << ", " << 
        //"edge_level = " << h.edge_level << 
        " }";
    } 
    
  private:
    
    /*! calculates the bounds of the face associated with the half-edge */
    __forceinline BBox3fa getFaceBounds(const BufferT<Vec3fa>& vertices) const 
    {
      BBox3fa b = vertices[getStartVertexIndex()];
      for (const HalfEdge* p = next(); p!=this; p=p->next()) {
        b.extend(vertices[p->getStartVertexIndex()]);
      }
      return b;
    }
    
    /*! calculates the bounds of the 1-ring associated with the vertex of the half-edge */
    __forceinline BBox3fa get1RingBounds(const BufferT<Vec3fa>& vertices) const 
    {
      BBox3fa bounds = empty;
      const HalfEdge* p = this;
      do 
      {
        /* calculate bounds of current face */
        bounds.extend(p->getFaceBounds(vertices));
        p = p->prev();
        
        /* continue with next face */
        if (likely(p->hasOpposite())) 
          p = p->opposite();
        
        /* if there is no opposite go the long way to the other side of the border */
        else {
          p = this;
          while (p->hasOpposite()) 
            p = p->opposite()->next();
        }
        
      } while (p != this); 
      
      return bounds;
    }
    
    /*! tests if this is a valid face */
    __forceinline bool validFace(const BufferT<Vec3fa>& vertices, size_t& N) const 
    {
      const Vec3fa v = vertices[getStartVertexIndex()];
      if (!isvalid(v)) return false;
      size_t n = 1;
      for (const HalfEdge* p = next(); p!=this; p=p->next(), n++) {
        const Vec3fa v = vertices[p->getStartVertexIndex()];
        if (!isvalid(v)) return false;
      }
      N += n-2;
      return n >= 3 && n <= MAX_PATCH_VALENCE;
    }
    
    /*! tests if this is a valid ring */
    __forceinline bool validRing(const BufferT<Vec3fa>& vertices) const 
    {
      size_t faceValence = 0;
      size_t edgeValence = 0;
      
      BBox3fa bounds = empty;
      const HalfEdge* p = this;
      do 
      {
        /* calculate bounds of current face */
        if (!p->validFace(vertices,edgeValence)) 
          return false;
        
        faceValence++;
        p = p->prev();
        
        /* continue with next face */
        if (likely(p->hasOpposite())) 
          p = p->opposite();
        
        /* if there is no opposite go the long way to the other side of the border */
        else {
          faceValence++;
          edgeValence++;
          p = this;
          while (p->hasOpposite()) 
            p = p->opposite()->next();
        }
        
      } while (p != this); 
      
      return faceValence <= MAX_RING_FACE_VALENCE && edgeValence <= MAX_RING_EDGE_VALENCE;
    }
    
  private:
    unsigned int vtx_index;         //!< index of edge start vertex
    int next_half_edge_ofs;         //!< relative offset to next half edge of face
    int prev_half_edge_ofs;         //!< relative offset to previous half edge of face
    int opposite_half_edge_ofs;     //!< relative offset to opposite half edge
    
  public:
    float edge_crease_weight;       //!< crease weight attached to edge
    float vertex_crease_weight;     //!< crease weight attached to start vertex
    float edge_level;               //!< subdivision factor for edge
    PatchType patch_type;           //!< stores type of subdiv patch
    VertexType vertex_type;         //!< stores type of the start vertex
    char align[2];
  };
}
