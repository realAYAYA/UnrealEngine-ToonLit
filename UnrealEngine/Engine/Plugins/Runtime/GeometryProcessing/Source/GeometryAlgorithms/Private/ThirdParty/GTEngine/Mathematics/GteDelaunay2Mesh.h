// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteDelaunay2.h>

namespace gte
{

template <typename InputType, typename ComputeType, typename RationalType>
class Delaunay2Mesh
{
public:
    // Construction.
    Delaunay2Mesh(Delaunay2<InputType, ComputeType> const& delaunay);

    // Mesh information.
    inline int GetNumVertices() const;
    inline int GetNumTriangles() const;
    inline Vector2<InputType> const* GetVertices() const;
    inline int const* GetIndices() const;
    inline int const* GetAdjacencies() const;

    // Containment queries.
    int GetContainingTriangle(Vector2<InputType> const& P) const;
    bool GetVertices(int t, std::array<Vector2<InputType>, 3>& vertices)
        const;
    bool GetIndices(int t, std::array<int, 3>& indices) const;
    bool GetAdjacencies(int t, std::array<int, 3>& adjacencies) const;
    bool GetBarycentrics(int t, Vector2<InputType> const& P,
        std::array<InputType, 3>& bary) const;

private:
    Delaunay2<InputType, ComputeType> const* mDelaunay;
};


template <typename InputType, typename ComputeType, typename RationalType>
Delaunay2Mesh<InputType, ComputeType, RationalType>::Delaunay2Mesh(
    Delaunay2<InputType, ComputeType> const& delaunay)
    :
    mDelaunay(&delaunay)
{
}

template <typename InputType, typename ComputeType, typename RationalType>
inline int Delaunay2Mesh<InputType, ComputeType, RationalType>::
GetNumVertices() const
{
    return mDelaunay->GetNumVertices();
}

template <typename InputType, typename ComputeType, typename RationalType>
inline int Delaunay2Mesh<InputType, ComputeType, RationalType>::
GetNumTriangles() const
{
    return mDelaunay->GetNumTriangles();
}

template <typename InputType, typename ComputeType, typename RationalType>
inline Vector2<InputType> const*
Delaunay2Mesh<InputType, ComputeType, RationalType>::
GetVertices() const
{
    return mDelaunay->GetVertices();
}

template <typename InputType, typename ComputeType, typename RationalType>
inline int const* Delaunay2Mesh<InputType, ComputeType, RationalType>::
GetIndices() const
{
    return &mDelaunay->GetIndices()[0];
}

template <typename InputType, typename ComputeType, typename RationalType>
inline int const* Delaunay2Mesh<InputType, ComputeType, RationalType>::
GetAdjacencies() const
{
    return &mDelaunay->GetAdjacencies()[0];
}

template <typename InputType, typename ComputeType, typename RationalType>
int Delaunay2Mesh<InputType, ComputeType, RationalType>::
GetContainingTriangle(Vector2<InputType> const& P) const
{
    typename Delaunay2<InputType, ComputeType>::SearchInfo info;
    return mDelaunay->GetContainingTriangle(P, info);
}

template <typename InputType, typename ComputeType, typename RationalType>
bool Delaunay2Mesh<InputType, ComputeType, RationalType>::
GetVertices(int t, std::array<Vector2<InputType>, 3>& vertices) const
{
    if (mDelaunay->GetDimension() == 2)
    {
        std::array<int, 3> indices;
        if (mDelaunay->GetIndices(t, indices))
        {
            PrimalQuery2<ComputeType> const& query = mDelaunay->GetQuery();
            Vector2<ComputeType> const* ctVertices = query.GetVertices();
            for (int i = 0; i < 3; ++i)
            {
                Vector2<ComputeType> const& V = ctVertices[indices[i]];
                for (int j = 0; j < 2; ++j)
                {
                    vertices[i][j] = (InputType)V[j];
                }
            }
            return true;
        }
    }
    return false;
}

template <typename InputType, typename ComputeType, typename RationalType>
bool Delaunay2Mesh<InputType, ComputeType, RationalType>::
GetIndices(int t, std::array<int, 3>& indices) const
{
    return mDelaunay->GetIndices(t, indices);
}

template <typename InputType, typename ComputeType, typename RationalType>
bool Delaunay2Mesh<InputType, ComputeType, RationalType>::
GetAdjacencies(int t, std::array<int, 3>& indices) const
{
    return mDelaunay->GetAdjacencies(t, indices);
}

template <typename InputType, typename ComputeType, typename RationalType>
bool Delaunay2Mesh<InputType, ComputeType, RationalType>::
GetBarycentrics(int t, Vector2<InputType> const& P,
std::array<InputType, 3>& bary) const
{
    std::array<int, 3> indices;
    if (mDelaunay->GetIndices(t, indices))
    {
        PrimalQuery2<ComputeType> const& query = mDelaunay->GetQuery();
        Vector2<ComputeType> const* vertices = query.GetVertices();
        Vector2<RationalType> rtP{ P[0], P[1] };
        std::array<Vector2<RationalType>, 3> rtV;
        for (int i = 0; i < 3; ++i)
        {
            Vector2<ComputeType> const& V = vertices[indices[i]];
            for (int j = 0; j < 2; ++j)
            {
                rtV[i][j] = (RationalType)V[j];
            }
        };

        RationalType rtBary[3];
        if (ComputeBarycentrics(rtP, rtV[0], rtV[1], rtV[2], rtBary))
        {
            for (int i = 0; i < 3; ++i)
            {
                bary[i] = (InputType)rtBary[i];
            }
            return true;
        }
    }
    return false;
}


}
