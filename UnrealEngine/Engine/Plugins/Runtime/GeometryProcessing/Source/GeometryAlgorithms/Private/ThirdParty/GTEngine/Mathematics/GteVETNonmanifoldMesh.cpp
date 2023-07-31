// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2018
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.20.0 (2019/01/08)

#include <ThirdParty/GTEngine/Mathematics/GteVETNonmanifoldMesh.h>
#include <ThirdParty/GTEngine/LowLevel/GteLogger.h>
#include <ThirdParty/GTEngine/GTEnginePCH.h>
using namespace gte;

VETNonmanifoldMesh::~VETNonmanifoldMesh()
{
}

VETNonmanifoldMesh::VETNonmanifoldMesh(VCreator vCreator, ECreator eCreator, TCreator tCreator)
    :
    ETNonmanifoldMesh(eCreator, tCreator),
    mVCreator(vCreator ? vCreator : CreateVertex)
{
}

VETNonmanifoldMesh::VETNonmanifoldMesh(VETNonmanifoldMesh const& mesh)
{
    *this = mesh;
}

VETNonmanifoldMesh& VETNonmanifoldMesh::operator=(VETNonmanifoldMesh const& mesh)
{
    Clear();
    mVCreator = mesh.mVCreator;
    ETNonmanifoldMesh::operator=(mesh);
    return *this;
}

VETNonmanifoldMesh::VMap const& VETNonmanifoldMesh::GetVertices() const
{
    return mVMap;
}

std::shared_ptr<VETNonmanifoldMesh::Triangle> VETNonmanifoldMesh::Insert(int v0, int v1, int v2)
{
    std::shared_ptr<Triangle> tri = ETNonmanifoldMesh::Insert(v0, v1, v2);
    if (!tri)
    {
        return nullptr;
    }

    for (int i = 0; i < 3; ++i)
    {
        int vIndex = tri->V[i];
        auto vItem = mVMap.find(vIndex);
        std::shared_ptr<Vertex> vertex;
        if (vItem == mVMap.end())
        {
            vertex = mVCreator(vIndex);
            mVMap[vIndex] = vertex;
        }
        else
        {
            vertex = vItem->second;
        }

        vertex->TAdjacent.insert(tri);

        for (int j = 0; j < 3; ++j)
        {
            auto edge = tri->E[j].lock();
            if (!edge)
            {
                LogError("Unexpected condition.");
                return nullptr;
            }

            if (edge->V[0] == vIndex)
            {
                vertex->VAdjacent.insert(edge->V[1]);
                vertex->EAdjacent.insert(edge);
            }
            else if (edge->V[1] == vIndex)
            {
                vertex->VAdjacent.insert(edge->V[0]);
                vertex->EAdjacent.insert(edge);
            }
        }
    }

    return tri;
}

bool VETNonmanifoldMesh::Remove(int v0, int v1, int v2)
{
    auto tItem = mTMap.find(TriangleKey<true>(v0, v1, v2));
    if (tItem == mTMap.end())
    {
        return false;
    }

    std::shared_ptr<Triangle> tri = tItem->second;
    for (int i = 0; i < 3; ++i)
    {
        int vIndex = tri->V[i];
        auto vItem = mVMap.find(vIndex);
        if (vItem == mVMap.end())
        {
            LogError("Unexpected condition.");
            return false;
        }

        std::shared_ptr<Vertex> vertex = vItem->second;
        for (int j = 0; j < 3; ++j)
        {
            auto edge = tri->E[j].lock();
            if (!edge)
            {
                LogError("Unexpected condition.");
                return false;
            }

            // If the edge will be removed by ETNonmanifoldMesh::Remove,
            // remove the vertex references to it.
            if (edge->T.size() == 1)
            {
                for (auto const& adjw : edge->T)
                {
                    auto adj = adjw.lock();
                    if (!adj)
                    {
                        LogError("Unexpected condition.");
                        return false;
                    }

                    if (edge->V[0] == vIndex)
                    {
                        vertex->VAdjacent.erase(edge->V[1]);
                        vertex->EAdjacent.erase(edge);
                    }
                    else if (edge->V[1] == vIndex)
                    {
                        vertex->VAdjacent.erase(edge->V[0]);
                        vertex->EAdjacent.erase(edge);
                    }
                }
            }
        }

        vertex->TAdjacent.erase(tri);

        // If the vertex is no longer shared by any triangle, remove it.
        if (vertex->TAdjacent.size() == 0)
        {
            LogAssert(vertex->VAdjacent.size() == 0 && vertex->EAdjacent.size() == 0,
                "Malformed mesh: Inconsistent vertex adjacency information.");

            mVMap.erase(vItem);
        }
    }

    return ETNonmanifoldMesh::Remove(v0, v1, v2);
}

void VETNonmanifoldMesh::Clear()
{
    mVMap.clear();
    ETNonmanifoldMesh::Clear();
}

std::shared_ptr<VETNonmanifoldMesh::Vertex> VETNonmanifoldMesh::CreateVertex(int vIndex)
{
    return std::make_shared<Vertex>(vIndex);
}

VETNonmanifoldMesh::Vertex::~Vertex()
{
}

VETNonmanifoldMesh::Vertex::Vertex(int vIndex)
    :
    V(vIndex)
{
}
