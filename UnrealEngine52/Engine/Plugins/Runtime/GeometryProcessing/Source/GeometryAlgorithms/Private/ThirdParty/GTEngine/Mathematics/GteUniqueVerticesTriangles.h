// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/GTEngineDEF.h>
#include <algorithm>
#include <map>
#include <vector>

// The VertexType must have an operator< member because it is used as the
// key in a std::map<VertexType,int>.  For example, if VertexType is
// Vector3<float>, the comparison operator already exist.  Another example is
// that you have a vertex type used for vertex coloring, say,
//   struct VertexType
//   {
//       Vector3<float> position;
//       Vector4<float> color;
//       bool operator< (VertexType const& v) const
//       {
//           return position < v.position;
//       }
//   }
// The comparision will guarantee unique vertex positions, although if you
// have two VertexType objects with the same position but different colors,
// there is no guarantee which color will occur in the final result.

namespace gte
{

template <typename VertexType>
class UniqueVerticesTriangles
{
public:
    // Triangle soup.  The input vertex array consists of triples of vertices,
    // each triple representing a triangle.  The array 'inVertices' must have
    // a multiple of 3 elements.  An array 'outVertices' of unique vertices
    // and an array 'outIndices' of 'inVertices.size()/3' unique index triples
    // are computed.  The indices are relative to the array of unique vertices
    // and each index triple represents a triangle.
    UniqueVerticesTriangles(
        std::vector<VertexType> const& inVertices,
        std::vector<VertexType>& outVertices,
        std::vector<int>& outIndices);

    // Indexed triangles.  The input vertex array consists of all vertices
    // referenced by the input index array.  The array 'inIndices' must have a
    // multiple of 3 elements.  An array 'outVertices' of unique vertices and
    // an array 'outIndices' of 'indices.size()/3' unique index triples are
    // computed.  The indices are relative to the array of unique vertices and
    // each index triple represents a triangle.
    UniqueVerticesTriangles(
        std::vector<VertexType> const& inVertices,
        std::vector<int> const& inIndices,
        std::vector<VertexType>& outVertices,
        std::vector<int>& outIndices);

    // The input vertices have indices 0 <= i < VInNum.  The output vertices
    // have indices 0 <= j < VOutNum.  The construction leads to a mapping of
    // input indices i to output indices j.  Duplicate vertices have different
    // input indices but the same output index.  The following function gives
    // you access to the mapping.  If the input index is invalid (i < 0 or
    // i >= VINum), the return value is -1.
    inline int GetOutputIndexFor(int index) const;

private:
    void ConstructUniqueVertices(std::vector<VertexType> const& inVertices,
        std::vector<VertexType>& outVertices);

    int mNumInVertices, mNumOutVertices;
    std::vector<int> mInToOutMapping;
};


template <typename VertexType>
UniqueVerticesTriangles<VertexType>::UniqueVerticesTriangles(
    std::vector<VertexType> const& inVertices,
    std::vector<VertexType>& outVertices, std::vector<int>& outIndices)
{
    ConstructUniqueVertices(inVertices, outVertices);

    // The input index array is implicitly {<0,1,2>,<3,4,5>,...,<n-3,n-2,n-1>}
    // where n is the number of vertices.  The output index array is the same
    // as the mapping array.
    outIndices.resize(inVertices.size());
    std::copy(mInToOutMapping.begin(), mInToOutMapping.end(),
        outIndices.begin());
}

template <typename VertexType>
UniqueVerticesTriangles<VertexType>::UniqueVerticesTriangles(
    std::vector<VertexType> const& inVertices,
    std::vector<int> const& inIndices, std::vector<VertexType>& outVertices,
    std::vector<int>& outIndices)
{
    ConstructUniqueVertices(inVertices, outVertices);

    // The input index array needs it indices mapped to the unique vertex
    // indices.
    outIndices.resize(inIndices.size());
    for (size_t i = 0; i < inIndices.size(); ++i)
    {
        outIndices[i] = mInToOutMapping[inIndices[i]];
    }
}

template <typename VertexType> inline
int UniqueVerticesTriangles<VertexType>::GetOutputIndexFor(int index) const
{
    return mInToOutMapping[index];
}

template <typename VertexType>
void UniqueVerticesTriangles<VertexType>::ConstructUniqueVertices(
    std::vector<VertexType> const& inVertices,
    std::vector<VertexType>& outVertices)
{
    // Construct the unique vertices.
    mNumInVertices = (int)inVertices.size();
    mInToOutMapping.resize(mNumInVertices);
    std::map<VertexType, int> table;
    mNumOutVertices = 0;
    for (int i = 0; i < mNumInVertices; ++i)
    {
        auto const iter = table.find(inVertices[i]);
        if (iter != table.end())
        {
            // Vertex i is a duplicate of one inserted earlier into the
            // table.  Map vertex i to the first-found copy.
            mInToOutMapping[i] = iter->second;
        }
        else
        {
            // Vertex i is the first occurrence of such a point.
            table.insert(std::make_pair(inVertices[i], mNumOutVertices));
            mInToOutMapping[i] = mNumOutVertices;
            ++mNumOutVertices;
        }
    }

    // Pack the unique vertices into an array in the correct order.
    outVertices.resize(mNumOutVertices);
    for (auto const& element : table)
    {
        outVertices[element.second] = element.first;
    }
}


}
