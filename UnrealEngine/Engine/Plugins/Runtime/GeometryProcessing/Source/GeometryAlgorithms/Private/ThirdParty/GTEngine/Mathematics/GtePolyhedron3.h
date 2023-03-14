// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.1 (2018/10/04)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteVector3.h>
#include <algorithm>
#include <memory>
#include <set>
#include <vector>

// The Polyhedron3 object represents a simple polyhedron.  The 'vertexPool'
// array can contain more points than needed to define the polyhedron, which
// allows the vertex pool to have multiple polyhedra associated with it.
// Thus, the programmer must ensure that the vertex pool persists as long as
// any Polyhedron3 objects exist that depend on the pool.  The number of
// polyhedron indices is 'numIndices' and must be 6 or larger  The 'indices'
// array refers to the points in 'vertexPool' that form the triangle faces,
// so 'numIndices' must be a multiple of 3.  The number of vertices is
// the number of unique elements in 'indices' and is determined during
// construction.  The programmer should ensure the polyhedron is simple.  The
// geometric queries are valid regardless of whether the polyhedron triangles
// are oriented clockwise or counterclockwise.
//
// NOTE:  Comparison operators are not provided.  The semantics of equal
// polyhedra is complicated and (at the moment) not useful.  The vertex pools
// can be different and indices do not match, but the vertices they reference
// can match.  Even with a shared vertex pool, the indices can be permuted,
// leading to the same polyhedron abstractly but the data structures do not
// match.

namespace gte
{

template <typename Real>
class Polyhedron3
{
public:
    // Construction.  The constructor succeeds when 'numIndices >= 12' (at
    // least 4 triangles), and 'vertexPool' and 'indices' are not null; we
    // cannot test whether you have a valid number of elements in the input
    // arrays.  A copy is made of 'indices', but the 'vertexPool' is not
    // copied.  If the constructor fails, the internal vertex pointer is set
    // to null, the number of vertices is set to zero, the index array has no
    // elements, and the triangle face orientation is set to clockwise.
    Polyhedron3(std::shared_ptr<std::vector<Vector3<Real>>> const& vertexPool,
        int numIndices, int const* indices, bool counterClockwise);

    // To validate construction, create an object as shown:
    //     Polyhedron3<Real> polyhedron(parameters);
    //     if (!polyhedron) { <constructor failed, handle accordingly>; }
    inline operator bool() const;

    // Member access.
    inline std::shared_ptr<std::vector<Vector3<Real>>> const& GetVertexPool() const;
    inline std::vector<Vector3<Real>> const& GetVertices() const;
    inline std::set<int> const& GetUniqueIndices() const;
    inline std::vector<int> const& GetIndices() const;
    inline bool CounterClockwise() const;

    // Geometric queries.
    Vector3<Real> ComputeVertexAverage() const;
    Real ComputeSurfaceArea() const;
    Real ComputeVolume() const;

private:
    std::shared_ptr<std::vector<Vector3<Real>>> mVertexPool;
    std::set<int> mUniqueIndices;
    std::vector<int> mIndices;
    bool mCounterClockwise;
};


template <typename Real>
Polyhedron3<Real>::Polyhedron3(std::shared_ptr<std::vector<Vector3<Real>>> const& vertexPool,
    int numIndices, int const* indices, bool counterClockwise)
    :
    mVertexPool(vertexPool),
    mCounterClockwise(counterClockwise)
{
    if (vertexPool && indices && numIndices >= 12 && (numIndices % 3) == 0)
    {
        for (int i = 0; i < numIndices; ++i)
        {
            mUniqueIndices.insert(indices[i]);
        }

        mIndices.resize(numIndices);
        std::copy(indices, indices + numIndices, mIndices.begin());
    }
    else
    {
        // Encountered an invalid input.
        mVertexPool = nullptr;
        mCounterClockwise = false;
    }
}

template <typename Real> inline
Polyhedron3<Real>::operator bool() const
{
    return mVertexPool != nullptr;
}

template <typename Real> inline
std::shared_ptr<std::vector<Vector3<Real>>> const& Polyhedron3<Real>::GetVertexPool() const
{
    return mVertexPool;
}

template <typename Real> inline
std::vector<Vector3<Real>> const& Polyhedron3<Real>::GetVertices() const
{
    return *mVertexPool.get();
}

template <typename Real> inline
std::set<int> const& Polyhedron3<Real>::GetUniqueIndices() const
{
    return mUniqueIndices;
}

template <typename Real> inline
std::vector<int> const& Polyhedron3<Real>::GetIndices() const
{
    return mIndices;
}

template <typename Real> inline
bool Polyhedron3<Real>::CounterClockwise() const
{
    return mCounterClockwise;
}

template <typename Real>
Vector3<Real> Polyhedron3<Real>::ComputeVertexAverage() const
{
    Vector3<Real> average = Vector3<Real>::Zero();
    if (mVertexPool)
    {
        auto vertexPool = GetVertices();
        for (int index : mUniqueIndices)
        {
            average += vertexPool[index];
        }
        average /= static_cast<Real>(mUniqueIndices.size());
    }
    return average;
}

template <typename Real>
Real Polyhedron3<Real>::ComputeSurfaceArea() const
{
    Real surfaceArea = (Real)0;
    if (mVertexPool)
    {
        auto vertexPool = GetVertices();
        int const numTriangles = static_cast<int>(mIndices.size()) / 3;
        int const* indices = mIndices.data();
        for (int t = 0; t < numTriangles; ++t)
        {
            int v0 = *indices++;
            int v1 = *indices++;
            int v2 = *indices++;
            Vector3<Real> edge0 = vertexPool[v1] - vertexPool[v0];
            Vector3<Real> edge1 = vertexPool[v2] - vertexPool[v0];
            Vector3<Real> cross = Cross(edge0, edge1);
            surfaceArea += Length(cross);
        }
        surfaceArea *= (Real)0.5;
    }
    return surfaceArea;
}

template <typename Real>
Real Polyhedron3<Real>::ComputeVolume() const
{
    Real volume = (Real)0;
    if (mVertexPool)
    {
        auto vertexPool = GetVertices();
        int const numTriangles = static_cast<int>(mIndices.size()) / 3;
        int const* indices = mIndices.data();
        for (int t = 0; t < numTriangles; ++t)
        {
            int v0 = *indices++;
            int v1 = *indices++;
            int v2 = *indices++;
            volume += DotCross(vertexPool[v0], vertexPool[v1], vertexPool[v2]);
        }
        volume /= (Real)6;
    }
    return std::abs(volume);
}

}
