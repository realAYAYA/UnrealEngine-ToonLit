// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#include <ThirdParty/GTEngine/Mathematics/GteVEManifoldMesh.h>
#include <ThirdParty/GTEngine/LowLevel/GteLogger.h>
#include <ThirdParty/GTEngine/GTEnginePCH.h>
using namespace gte;

VEManifoldMesh::~VEManifoldMesh()
{
}

VEManifoldMesh::VEManifoldMesh(VCreator vCreator, ECreator eCreator)
    :
    mVCreator(vCreator ? vCreator : CreateVertex),
    mECreator(eCreator ? eCreator : CreateEdge),
    mAssertOnNonmanifoldInsertion(true)
{
}

VEManifoldMesh::VMap const& VEManifoldMesh::GetVertices() const
{
    return mVMap;
}

VEManifoldMesh::EMap const& VEManifoldMesh::GetEdges() const
{
    return mEMap;
}

std::shared_ptr<VEManifoldMesh::Vertex> VEManifoldMesh::CreateVertex(int v)
{
    return std::make_shared<Vertex>(v);
}

std::shared_ptr<VEManifoldMesh::Edge> VEManifoldMesh::CreateEdge(int v0, int v1)
{
    return std::make_shared<Edge>(v0, v1);
}

void VEManifoldMesh::AssertOnNonmanifoldInsertion(bool doAssert)
{
    mAssertOnNonmanifoldInsertion = doAssert;
}

std::shared_ptr<VEManifoldMesh::Edge> VEManifoldMesh::Insert(int v0, int v1)
{
    std::pair<int,int> ekey(v0, v1);
    if (mEMap.find(ekey) != mEMap.end())
    {
        // The edge already exists.  Return a null pointer as a signal to
        // the caller that the insertion failed.
        return nullptr;
    }

    // Add the new edge.
    std::shared_ptr<Edge> edge = mECreator(v0, v1);
    mEMap[ekey] = edge;

    // Add the vertices if they do not already exist.
    for (int i = 0; i < 2; ++i)
    {
        int v = edge->V[i];
        std::shared_ptr<Vertex> vertex;
        auto viter = mVMap.find(v);
        if (viter == mVMap.end())
        {
            // This is the first time the vertex is encountered.
            vertex = mVCreator(v);
            mVMap[v] = vertex;

            // Update the vertex.
            vertex->E[0] = edge;
        }
        else
        {
            // This is the second time the vertex is encountered.
            vertex = viter->second;
            if (!vertex)
            {
                LogError("Unexpected condition.");
                return nullptr;
            }

            // Update the vertex.
            if (vertex->E[1].lock())
            {
                if (mAssertOnNonmanifoldInsertion)
                {
                    LogInformation("The mesh must be manifold.");
                }
                return nullptr;
            }
            vertex->E[1] = edge;

            // Update the adjacent edge.
            auto adjacent = vertex->E[0].lock();
            if (!adjacent)
            {
                LogError("Unexpected condition.");
                return nullptr;
            }
            for (int j = 0; j < 2; ++j)
            {
                if (adjacent->V[j] == v)
                {
                    adjacent->E[j] = edge;
                    break;
                }
            }

            // Update the edge.
            edge->E[i] = adjacent;
        }
    }

    return edge;
}

bool VEManifoldMesh::Remove(int v0, int v1)
{
    std::pair<int,int> ekey(v0, v1);
    auto eiter = mEMap.find(ekey);
    if (eiter == mEMap.end())
    {
        // The edge does not exist.
        return false;
    }

    // Get the edge.
    std::shared_ptr<Edge> edge = eiter->second;

    // Remove the vertices if necessary (when they are not shared).
    for (int i = 0; i < 2; ++i)
    {
        // Inform the vertices the edge is being deleted.
        auto viter = mVMap.find(edge->V[i]);
        if (viter == mVMap.end())
        {
            // The edge vertices should be in the map.
            LogError("Unexpected condition.");
            return false;
        }

        std::shared_ptr<Vertex> vertex = viter->second;
        if (!vertex)
        {
            // Any <vkey,vertex> in the map must have a dynamically allocated
            // Vertex object.
            LogError("Unexpected condition.");
            return false;
        }
        if (vertex->E[0].lock() == edge)
        {
            // One-edge vertices always have pointer at index zero.
            vertex->E[0] = vertex->E[1];
            vertex->E[1].reset();
        }
        else if (vertex->E[1].lock() == edge)
        {
            vertex->E[1].reset();
        }
        else
        {
            LogError("Unexpected condition.");
            return false;
        }

        // Remove the vertex if you have the last reference to it.
        if (!vertex->E[0].lock() && !vertex->E[1].lock())
        {
            mVMap.erase(vertex->V);
        }

        // Inform adjacent edges the edge is being deleted.
        auto adjacent = edge->E[i].lock();
        if (adjacent)
        {
            for (int j = 0; j < 2; ++j)
            {
                if (adjacent->E[j].lock() == edge)
                {
                    adjacent->E[j].reset();
                    break;
                }
            }
        }
    }

    mEMap.erase(ekey);
    return true;
}

bool VEManifoldMesh::IsClosed() const
{
    for (auto const& element : mVMap)
    {
        auto vertex = element.second;
        if (!vertex->E[0].lock() || !vertex->E[1].lock())
        {
            return false;
        }
    }
    return true;
}

VEManifoldMesh::Vertex::~Vertex()
{
}

VEManifoldMesh::Vertex::Vertex(int v)
{
    V = v;
}

VEManifoldMesh::Edge::~Edge()
{
}

VEManifoldMesh::Edge::Edge(int v0, int v1)
{
    V[0] = v0;
    V[1] = v1;
}
