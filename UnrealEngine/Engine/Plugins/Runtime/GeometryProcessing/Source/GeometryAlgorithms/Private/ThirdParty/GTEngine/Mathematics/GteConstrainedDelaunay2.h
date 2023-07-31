// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteDelaunay2.h>
#include <list>

// Various parts of the code have this trap for error conditions.  With a
// correct algorithm and exact arithmetic, we do not expect to trigger the
// error.  However, with floating-point arithmetic, it is possible that the
// triangulation becomes malformed.  The constrained Delaunay triangulation
// implementation is designed to return gracefully with such a failure.  The
// following macros are used to make the code more readable.  Do NOT disable
// them, because they have necessary side effects.
#define GTE_CDT_REQUIRE(c) { if (!(c)) { Trap(); return false; } }
#define GTE_CDT_FAILURE { Trap(); return false; }
#define GTE_CDT_REQUIRE_RET(c, r)  { if (!(c)) { Trap(); return r; } }
#define GTE_CDT_FAILURE_RET(r) { Trap(); return r; }

namespace gte
{

template <typename InputType, typename ComputeType>
class ConstrainedDelaunay2 : public Delaunay2<InputType, ComputeType>
{
public:
    // The class is a functor to support computing the constrained Delaunay
    // triangulation of multiple data sets using the same class object.
    virtual ~ConstrainedDelaunay2();
    ConstrainedDelaunay2();

    // This operator computes the Delaunay triangulation only.  Read the
    // Delaunay2 constructor comments about 'vertices' and 'epsilon'.  The
    // 'edges' array has indices into the 'vertices' array.  No two edges
    // should intersect except at endpoints.
    bool operator()(int numVertices, Vector2<InputType> const* vertices,
        InputType epsilon);

    // Insert required edges into the triangulation.  For correctness of the
    // algorithm, if two edges passed to this function intersect, they must
    // do so only at vertices passed to operator().  If you have two edges
    // that intersect at a point not in the vertices, compute that point of
    // intersection and subdivide the edges at that intersection (to form
    // more edges), and add the point to the vertices before calling
    // operator().  This function has an output array that contains the
    // input edge when the only vertices on the edge are its endpoints.  If
    // the input edge passes through more vertices, the edge is subdivided
    // in this function.  The output edge is that subdivision with first
    // vertex edge[0] and last vertex edge[1], and the other vertices are
    // correctly ordered along the edge.
    bool Insert(std::array<int, 2> const& edge, std::vector<int>& outEdge);

private:
    // The top-level entry point for inserting an edge in the triangulation.
    bool Insert(std::array<int, 2> const& edge, int v0Triangle,
        std::vector<int>& outEdge);

    // Process the coincident edge.
    bool ProcessCoincident(int tri, int v0, int v1, int vOther,
        std::vector<int>& outEdge);

    // Process the triangle strip originating at the first endpoint of the
    // edge.
    bool ProcessInterior(int tri, int v0, int v1, int vNext, int vPrev,
        std::vector<int>& outEdge);

    // Remove the triangles in the triangle strip and retriangulate the
    // left and right polygons using the empty circumcircle condition.
    bool Retriangulate(std::vector<int>& polygon,
        std::vector<std::array<int, 2>> const& lBoundary,
        std::vector<std::array<int, 2>> const& rBoundary);

    int RetriangulateLRecurse(
        std::vector<std::array<int, 2>> const& lBoundary,
        int i0, int i1, int a0, std::vector<int>& polygon);

    int RetriangulateRRecurse(
        std::vector<std::array<int, 2>> const& rBoundary,
        int i0, int i1, int a0, std::vector<int>& polygon);

    int SelectSplit(std::vector<std::array<int, 2>> const& boundary, int i0,
        int i1) const;

    // Compute a pseudosquared distance from the vertex at v2 to the edge
    // <v0,v1>.
    ComputeType ComputePSD(int v0, int v1, int v2) const;

    // Search the triangulation for a triangle that contains the specified
    // vertex.
    int GetLinkTriangle(int v) const;

    // Determine the index in {0,1,2} for the triangle 'tri' that contains
    // the vertex 'v'.
    int GetIndexOfVertex(int tri, int v) const;

    // Given a triangle 'tri' with CCW-edge <v0,v1>, return <adj,v2> where
    // 'adj' is the index of the triangle adjacent to 'tri' that shares the
    // edge and 'v2' is the vertex of the adjacent triangle opposite the
    // edge.  This function supports traversing a triangle strip that contains
    // a constraint edge, so it is called only when an adjacent triangle
    // actually exists.
    std::array<int, 2> GetAdjInterior(int tri, int v0, int v1) const;

    // Given a triangle 'tri' of the triangle strip, the boundary edge must
    // contain the vertex with index 'needBndVertex'.  The input
    // 'needAdjVIndex' specifies where to look for the index of the triangle
    // outside the strip but adjacent to the boundary edge.  The return
    // value is <needBndVertex, adj> and is used to connect 'tri' and 'adj'
    // across a triangle strip boundary.
    std::array<int, 2> GetAdjBoundary(int tri, int needBndVertex,
        int needAdjVIndex) const;

    // Set the indices and adjacencies arrays so that 'tri' and 'adj' share
    // the common edge; 'tri' has CCW-edge <v0,v1> and 'adj' has CCW-edge
    // <v1,v0>.
    bool Connect(int tri, int adj, int v0, int v1);

    // Create an ordered list of triangles forming the link of a vertex.  The
    // pair of the list is <triangle,GetIndexOfV(triangle)>.  This allows us
    // to cache the index of v relative to each triangle in the link.  The
    // vertex v might be a boundary vertex, in which case the neighborhood is
    // open; otherwise, v is an interior vertex and the neighborhood is
    // closed.  The 'isOpen' parameter specifies the case.
    bool BuildLink(int v, int vTriangle, std::list<std::pair<int,int>>& link,
        bool& isOpen) const;

    // Support for return-false-on-error.  This allows us to investigate the
    // error by setting a break point, trigger an assert when the logger is
    // active, and get a call stack.
    void Trap() const;
};


template <typename InputType, typename ComputeType>
ConstrainedDelaunay2<InputType, ComputeType>::~ConstrainedDelaunay2()
{
}

template <typename InputType, typename ComputeType>
ConstrainedDelaunay2<InputType, ComputeType>::ConstrainedDelaunay2()
    :
    Delaunay2<InputType, ComputeType>()
{
}

template <typename InputType, typename ComputeType>
bool ConstrainedDelaunay2<InputType, ComputeType>::operator()(
    int numVertices, Vector2<InputType> const* vertices, InputType epsilon)
{
    return Delaunay2<InputType, ComputeType>::operator()(numVertices,
        vertices, epsilon);
}

template <typename InputType, typename ComputeType>
bool ConstrainedDelaunay2<InputType, ComputeType>::Insert(
    std::array<int, 2> const& edge, std::vector<int>& outEdge)
{
    int v0 = edge[0], v1 = edge[1];
    if (0 <= v0 && v0 < this->mNumVertices
        && 0 <= v1 && v1 < this->mNumVertices)
    {
        int v0Triangle = GetLinkTriangle(v0);
        if (v0Triangle >= 0)
        {
            // Once an edge is inserted, the base-class mGraph no longer
            // represents the triangulation.  Clear it in case the user tries
            // to access it.
            this->mGraph.Clear();

            outEdge.clear();
            return Insert(edge, v0Triangle, outEdge);
        }
    }
    return false;
}

template <typename InputType, typename ComputeType>
bool ConstrainedDelaunay2<InputType, ComputeType>::Insert(
    std::array<int, 2> const& edge, int v0Triangle, std::vector<int>& outEdge)
{
	// TODO: this check helps avoid cases where the algorithm gets stuck in endless recursion walking the mesh (resulting in a stack overflow) ... overflow could still happen for a large mesh, so more work to prevent such an endless loop would be valuable!
	// TODO: (and/or: rewrite the insert function to not be recursive!)
	GTE_CDT_REQUIRE(outEdge.size() <= this->mComputeVertices.size());

    // Create the neighborhood of triangles that share the vertex v0.  On
    // entry we already know one such triangle (v0Triangle).
    int v0 = edge[0], v1 = edge[1];
    std::list<std::pair<int, int>> link;
    bool isOpen = true;
    GTE_CDT_REQUIRE(BuildLink(v0, v0Triangle, link, isOpen));

    // Determine which triangle contains the edge.  Process the edge according
    // to whether it is strictly between two triangle edges or is coincident
    // with a triangle edge.
    auto item = link.begin();
    std::array<int, 3> indices;
    GTE_CDT_REQUIRE(this->GetIndices(item->first, indices));
    int vNext = indices[(item->second + 1) % 3];
    int qr0 = this->mQuery.ToLine(v1, v0, vNext);
    while (item != link.end())
    {
        if (qr0 == 0)
        {
            // We have to be careful about parallel edges that point in
            // the opposite direction of <v0,v1>.
            Vector2<ComputeType> const& ctv0 = this->mComputeVertices[v0];
            Vector2<ComputeType> const& ctv1 = this->mComputeVertices[v1];
            Vector2<ComputeType> const& ctvnext =
                this->mComputeVertices[vNext];
            if (Dot(ctv1 - ctv0, ctvnext - ctv0) > (ComputeType)0)
            {
                // <v0,v1> is coincident to triangle edge0.
                return ProcessCoincident(item->first, v0, v1, vNext,
                    outEdge);
            }

            // Make sure we enter the next "if" statement to continue
            // traversing the link.
            qr0 = 1;
        }

        if (qr0 > 0)
        {
            // <v0,v1> is not in triangle.  Visit the next triangle.
            if (++item == link.end())
            {
                return false;
            }
            GTE_CDT_REQUIRE(this->GetIndices(item->first, indices));
            vNext = indices[(item->second + 1) % 3];
            qr0 = this->mQuery.ToLine(v1, v0, vNext);
            continue;
        }

        int vPrev = indices[(item->second + 2) % 3];
        int qr1 = this->mQuery.ToLine(v1, v0, vPrev);
        while (item != link.end())
        {
            if (qr1 == 0)
            {
                // We have to be careful about parallel edges that point in
                // the opposite direction of <v0,v1>.
                Vector2<ComputeType> const& ctv0 = this->mComputeVertices[v0];
                Vector2<ComputeType> const& ctv1 = this->mComputeVertices[v1];
                Vector2<ComputeType> const& ctvprev =
                    this->mComputeVertices[vPrev];
                if (Dot(ctv1 - ctv0, ctvprev - ctv0) > (ComputeType)0)
                {
                    // <v0,v1> is coincident to triangle edge1.
                    return ProcessCoincident(item->first, v0, v1, vPrev,
                        outEdge);
                }

                // Make sure we enter the next "if" statement to continue
                // traversing the link.
                qr1 = -1;
            }

            if (qr1 < 0)
            {
                // <v0,v1> is not in triangle.  Visit the next triangle.
                if (++item == link.end())
                {
                    return false;
                }
                this->GetIndices(item->first, indices);
                vNext = vPrev;
                vPrev = indices[(item->second + 2) % 3];
                qr1 = this->mQuery.ToLine(v1, v0, vPrev);
                continue;
            }

            // <v0,v1> is interior to triangle <v0,vNext,vPrev>.
            return ProcessInterior(item->first, v0, v1, vNext, vPrev,
                outEdge);
        }
        break;
    }

    // The edge must be contained in some link triangle.
    GTE_CDT_FAILURE;
}

template <typename InputType, typename ComputeType>
bool ConstrainedDelaunay2<InputType, ComputeType>::ProcessCoincident(int tri,
    int v0, int v1, int vOther, std::vector<int>& outEdge)
{
    outEdge.push_back(v0);
    if (v1 != vOther)
    {
        // Decompose the edge and process the right-most subedge.
        return Insert({{ vOther, v1 }}, tri, outEdge);
    }
    else
    {
        // <v0,v1> is already in the triangulation.
        outEdge.push_back(v1);
        return true;
    }
}

template <typename InputType, typename ComputeType>
bool ConstrainedDelaunay2<InputType, ComputeType>::ProcessInterior(int tri,
    int v0, int v1, int vNext, int vPrev, std::vector<int>& outEdge)
{
    // The triangles of the strip are stored in 'polygon'.  The
    // retriangulation leads to the same number of triangles, so we can reuse
    // the mIndices[] and mAdjacencies[] locations implied by the 'polygons'
    // indices.
    std::vector<int> polygon;

    // The sBoundary[i] (s in {l,r}) array element is <v0,adj>; see the
    // header comments for GetAdjBoundary about what these mean.  The
    // boundary vertex is 'v0', the adjacent triangle 'adj' is outside the
    // strip and shares the edge <sBoundary[i-1][0], sBoundary[i][0]> with a
    // triangle in 'polygon'.  This information allows us to connect the
    // adjacent triangles outside the strip to new triangles inserted by
    // the retriangulation.  The value sBoundary[0][1,2] values are set to
    // -1 but they are not used in the construction.
    std::vector<std::array<int, 2>> lBoundary, rBoundary;
    std::array<int, 2> binfo;

    polygon.push_back(tri);

    lBoundary.push_back({{ v0, -1 }});
    binfo = GetAdjBoundary(tri, vPrev, vPrev);
    GTE_CDT_REQUIRE(binfo[0] != -2);
    lBoundary.push_back(binfo);

    rBoundary.push_back({{ v0, -1 }});
    binfo = GetAdjBoundary(tri, vNext, v0);
    GTE_CDT_REQUIRE(binfo[0] != -2);
    rBoundary.push_back(binfo);

    // Visit the triangles in the strip.  Guard against an infinite loop.
    for (int i = 0; i < this->mNumTriangles; ++i)
    {
        // Find the vertex of the adjacent triangle that is opposite the
        // edge <vNext,vPrev> shared with the current triangle.
        auto iinfo = GetAdjInterior(tri, vNext, vPrev);
        int adj = iinfo[0], vOpposite = iinfo[1];
        GTE_CDT_REQUIRE(vOpposite >= 0);

        // Visit the adjacent triangle and insert it into the polygon.
        tri = adj;
        polygon.push_back(tri);

        int qr = this->mQuery.ToLine(vOpposite, v0, v1);
        if (qr == 0)
        {
            // We have encountered a vertex that terminates the triangle
            // strip.  Retriangulate the polygon.  If the edge continues
            // through vOpposite, decompose the edge and insert the
            // right-most subedge.
            binfo = GetAdjBoundary(tri, vOpposite, vOpposite);
            GTE_CDT_REQUIRE(binfo[0] != -2);
            lBoundary.push_back(binfo);

            binfo = GetAdjBoundary(tri, vOpposite, vNext);
            GTE_CDT_REQUIRE(binfo[0] != -2);
            rBoundary.push_back(binfo);

            Retriangulate(polygon, lBoundary, rBoundary);
            if (vOpposite != v1)
            {
                outEdge.push_back(v0);
                return Insert({{ vOpposite, v1 }}, tri, outEdge);
            }
            else
            {
				outEdge.push_back(v0);
				outEdge.push_back(v1);
                return true;
            }
        }

        if (qr < 0)
        {
            binfo = GetAdjBoundary(tri, vOpposite, vOpposite);
            GTE_CDT_REQUIRE(binfo[0] != -2);
            lBoundary.push_back(binfo);
            vPrev = vOpposite;
        }
        else  // qr > 0
        {
            binfo = GetAdjBoundary(tri, vOpposite, vNext);
            GTE_CDT_REQUIRE(binfo[0] != -2);
            rBoundary.push_back(binfo);
            vNext = vOpposite;
        }
    }

    // The triangle strip should have been located in the loop.
    GTE_CDT_FAILURE;
}

template <typename InputType, typename ComputeType>
bool ConstrainedDelaunay2<InputType, ComputeType>::Retriangulate(
    std::vector<int>& polygon,
    std::vector<std::array<int, 2>> const& lBoundary,
    std::vector<std::array<int, 2>> const& rBoundary)
{
    int t0 = RetriangulateLRecurse(lBoundary, 0,
        static_cast<int>(lBoundary.size()) - 1, -1, polygon);

    int t1 = RetriangulateRRecurse(rBoundary, 0,
        static_cast<int>(rBoundary.size()) - 1, -1, polygon);

    int v0 = lBoundary.front()[0];
    int v1 = lBoundary.back()[0];
    GTE_CDT_REQUIRE(Connect(t0, t1, v0, v1));
    return true;
}

template <typename InputType, typename ComputeType>
int ConstrainedDelaunay2<InputType, ComputeType>::RetriangulateLRecurse(
    std::vector<std::array<int, 2>> const& lBoundary, int i0, int i1, int a0,
    std::vector<int>& polygon)
{
    // Create triangles when recursing down, connect adjacent triangles when
    // returning.

    int v0 = lBoundary[i0][0];
    int v1 = lBoundary[i1][0];

    if (i1 - i0 == 1)
    {
        GTE_CDT_REQUIRE_RET(Connect(a0, lBoundary[i1][1], v1, v0), -2);
        return -1;  // No triangle created.
    }
    else
    {
        // Select i2 in [i0+1,i1-1] for minimum distance to edge <i0,i1>.
        int i2 = SelectSplit(lBoundary, i0, i1);
        int v2 = lBoundary[i2][0];

        // Reuse a triangle and fill in its new vertices.
        int tri = polygon.back();
        polygon.pop_back();
        this->mIndices[3 * tri + 0] = v0;
        this->mIndices[3 * tri + 1] = v1;
        this->mIndices[3 * tri + 2] = v2;

        // Recurse downward and create triangles.
        int ret0 = RetriangulateLRecurse(lBoundary, i0, i2, tri, polygon);
        GTE_CDT_REQUIRE_RET(ret0 >= -1, -2);
        int ret1 = RetriangulateLRecurse(lBoundary, i2, i1, tri, polygon);
        GTE_CDT_REQUIRE_RET(ret1 >= -1, -2);

        // Return and connect triangles.
        GTE_CDT_REQUIRE_RET(Connect(a0, tri, v1, v0), -2);
        return tri;
    }
}

template <typename InputType, typename ComputeType>
int ConstrainedDelaunay2<InputType, ComputeType>::RetriangulateRRecurse(
    std::vector<std::array<int, 2>> const& rBoundary, int i0, int i1, int a0,
    std::vector<int>& polygon)
{
    // Create triangles when recursing down, connect adjacent triangles when
    // returning.

    int v0 = rBoundary[i0][0];
    int v1 = rBoundary[i1][0];

    if (i1 - i0 == 1)
    {
        GTE_CDT_REQUIRE_RET(Connect(a0, rBoundary[i1][1], v0, v1), -2);
        return -1;  // No triangle created.
    }
    else
    {
        // Select i2 in [i0+1,i1-1] for minimum distance to edge <i0,i1>.
        int i2 = SelectSplit(rBoundary, i0, i1);
        int v2 = rBoundary[i2][0];

        // Reuse a triangle and fill in its new vertices.
        int tri = polygon.back();
        polygon.pop_back();
        this->mIndices[3 * tri + 0] = v1;
        this->mIndices[3 * tri + 1] = v0;
        this->mIndices[3 * tri + 2] = v2;

        // Recurse downward and create triangles.
        int ret0 = RetriangulateRRecurse(rBoundary, i0, i2, tri, polygon);
        GTE_CDT_REQUIRE_RET(ret0 >= -1, -2);
        int ret1 = RetriangulateRRecurse(rBoundary, i2, i1, tri, polygon);
        GTE_CDT_REQUIRE_RET(ret1 >= -1, -2);

        // Return and connect triangles.
        GTE_CDT_REQUIRE_RET(Connect(a0, tri, v0, v1), -2);
        return tri;
    }
}

template <typename InputType, typename ComputeType>
int ConstrainedDelaunay2<InputType, ComputeType>::SelectSplit(
    std::vector<std::array<int, 2>> const& boundary, int i0, int i1) const
{
    int i2;
    if (i1 - i0 == 2)
    {
        // This is the only candidate.
        i2 = i0 + 1;
    }
    else  // i1 - i0 > 2
    {
        // Select the index i2 in [i0+1,i1-1] for which the distance from the
        // vertex v2 at i2 to the edge <v0,v1> is minimized.  To allow exact
        // arithmetic, use a pseudosquared distance that avoids divisions and
        // square roots.
        i2 = i0 + 1;
        int v0 = boundary[i0][0];
        int v1 = boundary[i1][0];
        int v2 = boundary[i2][0];
        ComputeType minpsd = ComputePSD(v0, v1, v2);
        for (int i = i2 + 1; i < i1; ++i)
        {
            v2 = boundary[i][0];
            ComputeType psd = ComputePSD(v0, v1, v2);
            if (psd < minpsd)
            {
                minpsd = psd;
                i2 = i;
            }
        }
    }
    return i2;
}

template <typename InputType, typename ComputeType>
ComputeType ConstrainedDelaunay2<InputType, ComputeType>::ComputePSD(int v0,
    int v1, int v2) const
{
    ComputeType psd;

    Vector2<ComputeType> const& ctv0 = this->mComputeVertices[v0];
    Vector2<ComputeType> const& ctv1 = this->mComputeVertices[v1];
    Vector2<ComputeType> const& ctv2 = this->mComputeVertices[v2];

    Vector2<ComputeType> V1mV0 = ctv1 - ctv0;
    Vector2<ComputeType> V2mV0 = ctv2 - ctv0;
    ComputeType sqrlen10 = Dot(V1mV0, V1mV0);
    ComputeType dot = Dot(V1mV0, V2mV0);
    ComputeType zero(0);

    if (dot <= zero)
    {
        ComputeType sqrlen20 = Dot(V2mV0, V2mV0);
        psd = sqrlen10 * sqrlen20;
    }
    else
    {
        Vector2<ComputeType> V2mV1 = ctv2 - ctv1;
        dot = Dot(V1mV0, V2mV1);
        if (dot >= zero)
        {
            ComputeType sqrlen21 = Dot(V2mV1, V2mV1);
            psd = sqrlen10 * sqrlen21;
        }
        else
        {
            dot = DotPerp(V2mV0, V1mV0);
            psd = dot * dot;
        }
    }

    return psd;
}

template <typename InputType, typename ComputeType>
int ConstrainedDelaunay2<InputType, ComputeType>::GetLinkTriangle(int v) const
{
    // Remap in case an edge vertex was specified that is a duplicate.
    v = this->mDuplicates[v];

	// TODO: this algorithm does not work for point location in a CDT; it's only guaranteed for a (non-C)DT.
	//  See https://hal.inria.fr/inria-00072509/document for a report on different approaches to walking a triangulation,
	//  including an example of where this approach (visibility walk) fails.
	//  For now, I've added a brute-force fallback below.  It would be better to choose a correct algorithm to put here though!
    int tri = 0;
    for (int i = 0; i < this->mNumTriangles; ++i)
    {
        // Test whether v is a vertex of the triangle.
        std::array<int, 3> indices;
        GTE_CDT_REQUIRE(this->GetIndices(tri, indices));
        for (int j = 0; j < 3; ++j)
        {
            if (v == indices[j])
            {
                return tri;
            }
        }

        // v must be outside the triangle.
        for (int j0 = 2, j1 = 0; j1 < 3; j0 = j1++)
        {
            if (this->mQuery.ToLine(v, indices[j0], indices[j1]) > 0)
            {
                // Vertex v sees the edge from outside, so traverse to the
                // triangle sharing the edge.
                std::array<int, 3> adjacencies;
                GTE_CDT_REQUIRE(this->GetAdjacencies(tri, adjacencies));
                int adj = adjacencies[j0];
                GTE_CDT_REQUIRE_RET(adj >= 0, -1);
                tri = adj;
                break;
            }
        }
    }

	// fallback to brute force search
	// TODO: rm this after fixing the above point location to something that works for all triangulations
	for (tri = 0; tri < this->mNumTriangles; ++tri)
	{
		// Test whether v is a vertex of the triangle.
		std::array<int, 3> indices;
		GTE_CDT_REQUIRE(this->GetIndices(tri, indices));
		for (int j = 0; j < 3; ++j)
		{
			if (v == indices[j])
			{
				return tri;
			}
		}
	}

    // The vertex must be in the triangulation.
    GTE_CDT_FAILURE_RET(-1);
}

template <typename InputType, typename ComputeType>
int ConstrainedDelaunay2<InputType, ComputeType>::GetIndexOfVertex(int tri,
    int v) const
{
    std::array<int, 3> indices;
    GTE_CDT_REQUIRE_RET(this->GetIndices(tri, indices), -1);
    int indexOfV;
    for (indexOfV = 0; indexOfV < 3; ++indexOfV)
    {
        if (v == indices[indexOfV])
        {
            return indexOfV;
        }
    }

    // Unexpected failure.
    GTE_CDT_FAILURE_RET(-1);
}

template <typename InputType, typename ComputeType>
std::array<int, 2> ConstrainedDelaunay2<InputType, ComputeType>::
GetAdjInterior(int tri, int v0, int v1) const
{
    std::array<int, 2> error = {{ -2, -2 }};
    int vIndex = GetIndexOfVertex(tri, v0);
    GTE_CDT_REQUIRE_RET(vIndex >= 0, error);
    int adj = this->mAdjacencies[3 * tri + vIndex];
    if (adj >= 0)
    {
        for (int v2Index = 0; v2Index < 3; ++v2Index)
        {
            int v2 = this->mIndices[3 * adj + v2Index];
            if (v2 != v0 && v2 != v1)
            {
                return {{ adj, v2 }};
            }
        }
    }
    else
    {
        return {{ -1, -1 }};
    }

    GTE_CDT_FAILURE_RET(error);
}

template <typename InputType, typename ComputeType>
std::array<int, 2> ConstrainedDelaunay2<InputType, ComputeType>::
GetAdjBoundary(int tri, int needBndVertex, int needAdjVIndex) const
{
    std::array<int, 2> error = {{ -2, -2 }};
    int vIndex = GetIndexOfVertex(tri, needAdjVIndex);
    GTE_CDT_REQUIRE_RET(vIndex >= 0, error);
    int adj = this->mAdjacencies[3 * tri + vIndex];
    return {{ needBndVertex, adj }};
}

template <typename InputType, typename ComputeType>
bool ConstrainedDelaunay2<InputType, ComputeType>::Connect(int tri, int adj,
    int v0, int v1)
{
    if (tri >= 0)
    {
        int v0Index = GetIndexOfVertex(tri, v0);
        GTE_CDT_REQUIRE(v0Index >= 0);
        if (adj >= 0)
        {
            int v1Index = GetIndexOfVertex(adj, v1);
            GTE_CDT_REQUIRE(v1Index >= 0);
            this->mAdjacencies[3 * adj + v1Index] = tri;
        }

        this->mAdjacencies[3 * tri + v0Index] = adj;
    }
    // else: tri = -1, which occurs in the top-level call to retriangulate
    return true;
}

template <typename InputType, typename ComputeType>
bool ConstrainedDelaunay2<InputType, ComputeType>::BuildLink(int v,
    int vTriangle, std::list<std::pair<int, int>>& link, bool& isOpen) const
{
    // The list starts with a known triangle in the link of v.
    int vStartIndex = GetIndexOfVertex(vTriangle, v);
    GTE_CDT_REQUIRE(vStartIndex >= 0);
    link.push_front(std::make_pair(vTriangle, vStartIndex));

    // Traverse adjacent triangles to the "left" of v.  Guard against an
    // infinite loop.
    int tri = vTriangle, vIndex = vStartIndex;
    std::array<int, 3> adjacencies;
    for (int i = 0; i < this->mNumTriangles; ++i)
    {
        GTE_CDT_REQUIRE(this->GetAdjacencies(tri, adjacencies));
        int adjPrev = adjacencies[(vIndex + 2) % 3];
        if (adjPrev >= 0)
        {
            if (adjPrev != vTriangle)
            {
                tri = adjPrev;
                vIndex = GetIndexOfVertex(tri, v);
                GTE_CDT_REQUIRE(vIndex >= 0);
                link.push_back(std::make_pair(tri, vIndex));
            }
            else
            {
                // We have reached the starting triangle, so v is an interior
                // vertex.
                isOpen = false;
                return true;
            }
        }
        else
        {
            // We have reached a triangle with boundary edge, so v is a
            // boundary vertex.  We mush find more triangles by searching
            // to the "right" of v.  Guard against an infinite loop.
            isOpen = true;
            tri = vTriangle;
            vIndex = vStartIndex;
            for (int j = 0; j < this->mNumTriangles; ++j)
            {
                this->GetAdjacencies(tri, adjacencies);
                int adjNext = adjacencies[vIndex];
                if (adjNext >= 0)
                {
                    tri = adjNext;
                    vIndex = GetIndexOfVertex(tri, v);
                    GTE_CDT_REQUIRE(vIndex >= 0);
                    link.push_front(std::make_pair(tri, vIndex));
                }
                else
                {
                    // We have reached the other boundary edge that shares v.
                    return true;
                }
            }
            break;
        }
    }

    // Unexpected failure.
    GTE_CDT_FAILURE;
}

template <typename InputType, typename ComputeType>
void ConstrainedDelaunay2<InputType, ComputeType>::Trap() const
{
    LogError("CDT failure.");
}


}
