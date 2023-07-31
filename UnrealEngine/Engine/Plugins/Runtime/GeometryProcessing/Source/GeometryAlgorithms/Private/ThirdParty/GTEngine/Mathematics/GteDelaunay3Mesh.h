// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteDelaunay3.h>

namespace gte
{

template <typename InputType, typename ComputeType, typename RationalType>
class Delaunay3Mesh
{
public:
    // Construction.
    Delaunay3Mesh(Delaunay3<InputType, ComputeType> const& delaunay);

    // Mesh information.
    inline int GetNumVertices() const;
    inline int GetNumTetrahedra() const;
    inline Vector3<InputType> const* GetVertices() const;
    inline int const* GetIndices() const;
    inline int const* GetAdjacencies() const;

    // Containment queries.
    int GetContainingTetrahedron(Vector3<InputType> const& P) const;
    bool GetVertices(int t, std::array<Vector3<InputType>, 4>& vertices)
        const;
    bool GetIndices(int t, std::array<int, 4>& indices) const;
    bool GetAdjacencies(int t, std::array<int, 4>& adjacencies) const;
    bool GetBarycentrics(int t, Vector3<InputType> const& P,
        std::array<InputType, 4>& bary) const;

private:
    Delaunay3<InputType, ComputeType> const* mDelaunay;
};


template <typename InputType, typename ComputeType, typename RationalType>
Delaunay3Mesh<InputType, ComputeType, RationalType>::Delaunay3Mesh(
    Delaunay3<InputType, ComputeType> const& delaunay)
    :
    mDelaunay(&delaunay)
{
}

template <typename InputType, typename ComputeType, typename RationalType>
inline int Delaunay3Mesh<InputType, ComputeType, RationalType>::
GetNumVertices() const
{
    return mDelaunay->GetNumVertices();
}

template <typename InputType, typename ComputeType, typename RationalType>
inline int Delaunay3Mesh<InputType, ComputeType, RationalType>::
GetNumTetrahedra() const
{
    return mDelaunay->GetNumTetrahedra();
}

template <typename InputType, typename ComputeType, typename RationalType>
inline Vector3<InputType> const*
Delaunay3Mesh<InputType, ComputeType, RationalType>::
GetVertices() const
{
    return mDelaunay->GetVertices();
}

template <typename InputType, typename ComputeType, typename RationalType>
inline int const* Delaunay3Mesh<InputType, ComputeType, RationalType>::
GetIndices() const
{
    return &mDelaunay->GetIndices()[0];
}

template <typename InputType, typename ComputeType, typename RationalType>
inline int const* Delaunay3Mesh<InputType, ComputeType, RationalType>::
GetAdjacencies() const
{
    return &mDelaunay->GetAdjacencies()[0];
}

template <typename InputType, typename ComputeType, typename RationalType>
int Delaunay3Mesh<InputType, ComputeType, RationalType>::
GetContainingTetrahedron(Vector3<InputType> const& P) const
{
    typename Delaunay3<InputType, ComputeType>::SearchInfo info;
    return mDelaunay->GetContainingTetrahedron(P, info);
}

template <typename InputType, typename ComputeType, typename RationalType>
bool Delaunay3Mesh<InputType, ComputeType, RationalType>::
GetVertices(int t, std::array<Vector3<InputType>, 4>& vertices) const
{
    if (mDelaunay->GetDimension() == 3)
    {
        std::array<int, 4> indices;
        if (mDelaunay->GetIndices(t, indices))
        {
            PrimalQuery3<ComputeType> const& query = mDelaunay->GetQuery();
            Vector3<ComputeType> const* ctVertices = query.GetVertices();
            for (int i = 0; i < 4; ++i)
            {
                Vector3<ComputeType> const& V = ctVertices[indices[i]];
                for (int j = 0; j < 3; ++j)
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
bool Delaunay3Mesh<InputType, ComputeType, RationalType>::
GetIndices(int t, std::array<int, 4>& indices) const
{
    return mDelaunay->GetIndices(t, indices);
}

template <typename InputType, typename ComputeType, typename RationalType>
bool Delaunay3Mesh<InputType, ComputeType, RationalType>::
GetAdjacencies(int t, std::array<int, 4>& indices) const
{
    return mDelaunay->GetAdjacencies(t, indices);
}

template <typename InputType, typename ComputeType, typename RationalType>
bool Delaunay3Mesh<InputType, ComputeType, RationalType>::
GetBarycentrics(int t, Vector3<InputType> const& P,
std::array<InputType, 4>& bary) const
{
    std::array<int, 4> indices;
    if (mDelaunay->GetIndices(t, indices))
    {
        PrimalQuery3<ComputeType> const& query = mDelaunay->GetQuery();
        Vector3<ComputeType> const* vertices = query.GetVertices();
        Vector3<RationalType> rtP{ P[0], P[1], P[2] };
        std::array<Vector3<RationalType>, 4> rtV;
        for (int i = 0; i < 4; ++i)
        {
            Vector3<ComputeType> const& V = vertices[indices[i]];
            for (int j = 0; j < 3; ++j)
            {
                rtV[i][j] = (RationalType)V[j];
            }
        };

        RationalType rtBary[4];
        if (ComputeBarycentrics(rtP, rtV[0], rtV[1], rtV[2], rtV[3], rtBary))
        {
            for (int i = 0; i < 4; ++i)
            {
                bary[i] = (InputType)rtBary[i];
            }
            return true;
        }
    }
    return false;
}


}
