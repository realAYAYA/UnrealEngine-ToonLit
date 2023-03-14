// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#include <ThirdParty/GTEngine/Mathematics/GteVETManifoldMesh.h>
#include <ThirdParty/GTEngine/LowLevel/GteLogger.h>
#include <ThirdParty/GTEngine/GTEnginePCH.h>
using namespace gte;

VETManifoldMesh::~VETManifoldMesh()
{
}

VETManifoldMesh::VETManifoldMesh(VCreator vCreator, ECreator eCreator, TCreator tCreator)
    :
    ETManifoldMesh(eCreator, tCreator),
    mVCreator(vCreator ? vCreator : CreateVertex)
{
}

VETManifoldMesh::VETManifoldMesh(VETManifoldMesh const& mesh)
{
    *this = mesh;
}

VETManifoldMesh& VETManifoldMesh::operator=(VETManifoldMesh const& mesh)
{
    Clear();
    mVCreator = mesh.mVCreator;
    ETManifoldMesh::operator=(mesh);
    return *this;
}

VETManifoldMesh::VMap const& VETManifoldMesh::GetVertices() const
{
    return mVMap;
}

std::shared_ptr<VETManifoldMesh::Triangle> VETManifoldMesh::Insert(int v0, int v1, int v2)
{
    std::shared_ptr<Triangle> tri = ETManifoldMesh::Insert(v0, v1, v2);
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
            if (edge)
            {
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
            else
            {
                LogError("Malformed mesh: Triangle edges must not be null.");
                return nullptr;
            }
        }
    }

    return tri;
}

bool VETManifoldMesh::Remove(int v0, int v1, int v2)
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
        if (vItem != mVMap.end())
        {
            std::shared_ptr<Vertex> vertex = vItem->second;
            for (int j = 0; j < 3; ++j)
            {
                auto edge = tri->E[j].lock();
                if (edge)
                {
                    if (edge->T[0].lock() && !edge->T[1].lock())
                    {
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
                else
                {
                    LogError("Malformed mesh: Triangle edges must not be null.");
                    return false;
                }
            }

            vertex->TAdjacent.erase(tri);

            if (vertex->TAdjacent.size() == 0)
            {
                LogAssert(vertex->VAdjacent.size() == 0 && vertex->EAdjacent.size() == 0,
                    "Malformed mesh: Inconsistent vertex adjacency information.");

                mVMap.erase(vItem);
            }
        }
        else
        {
            LogError("Malformed mesh: Vertex must exist in the mesh.");
            return false;
        }
    }

    return ETManifoldMesh::Remove(v0, v1, v2);
}

void VETManifoldMesh::Clear()
{
    mVMap.clear();
    ETManifoldMesh::Clear();
}

std::shared_ptr<VETManifoldMesh::Vertex> VETManifoldMesh::CreateVertex(int vIndex)
{
    return std::make_shared<Vertex>(vIndex);
}

VETManifoldMesh::Vertex::~Vertex()
{
}

VETManifoldMesh::Vertex::Vertex(int vIndex)
    :
    V(vIndex)
{
}
