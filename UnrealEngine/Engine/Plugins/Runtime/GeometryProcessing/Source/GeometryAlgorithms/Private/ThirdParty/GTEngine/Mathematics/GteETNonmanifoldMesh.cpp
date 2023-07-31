// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2018
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.20.0 (2019/01/09)

#include <ThirdParty/GTEngine/Mathematics/GteETNonmanifoldMesh.h>
#include <ThirdParty/GTEngine/LowLevel/GteLogger.h>
#include <ThirdParty/GTEngine/GTEnginePCH.h>

using namespace gte;

ETNonmanifoldMesh::~ETNonmanifoldMesh()
{
}

ETNonmanifoldMesh::ETNonmanifoldMesh(ECreator eCreator, TCreator tCreator)
    :
    mECreator(eCreator ? eCreator : CreateEdge),
    mTCreator(tCreator ? tCreator : CreateTriangle)
{
}

ETNonmanifoldMesh::ETNonmanifoldMesh(ETNonmanifoldMesh const& mesh)
{
    *this = mesh;
}

ETNonmanifoldMesh& ETNonmanifoldMesh::operator=(ETNonmanifoldMesh const& mesh)
{
    Clear();

    mECreator = mesh.mECreator;
    mTCreator = mesh.mTCreator;
    for (auto const& element : mesh.mTMap)
    {
        Insert(element.first.V[0], element.first.V[1], element.first.V[2]);
    }

    return *this;
}

ETNonmanifoldMesh::EMap const& ETNonmanifoldMesh::GetEdges() const
{
    return mEMap;
}

ETNonmanifoldMesh::TMap const& ETNonmanifoldMesh::GetTriangles() const
{
    return mTMap;
}

std::shared_ptr<ETNonmanifoldMesh::Triangle> ETNonmanifoldMesh::Insert(int v0, int v1, int v2)
{
    TriangleKey<true> tkey(v0, v1, v2);
    if (mTMap.find(tkey) != mTMap.end())
    {
        // The triangle already exists.  Return a null pointer as a signal to
        // the caller that the insertion failed.
        return nullptr;
    }

    // Create the new triangle.  It will be added to mTMap at the end of the
    // function so that if an assertion is triggered and the function returns
    // early, the (bad) triangle will not be part of the mesh.
    std::shared_ptr<Triangle> tri = mTCreator(v0, v1, v2);

    // Add the edges to the mesh if they do not already exist.
    for (int i0 = 2, i1 = 0; i1 < 3; i0 = i1++)
    {
        EdgeKey<false> ekey(tri->V[i0], tri->V[i1]);
        std::shared_ptr<Edge> edge;
        auto eiter = mEMap.find(ekey);
        if (eiter == mEMap.end())
        {
            // This is the first time the edge is encountered.
            edge = mECreator(tri->V[i0], tri->V[i1]);
            mEMap[ekey] = edge;
        }
        else
        {
            // The edge was previously encountered and created.
            edge = eiter->second;
            if (!edge)
            {
                LogError("Unexpected condition.");
                return nullptr;
            }
        }

        // Associate the edge with the triangle.
        tri->E[i0] = edge;

        // Update the adjacent set of triangles for the edge.
        edge->T.insert(tri);
    }

    mTMap[tkey] = tri;
    return tri;
}

bool ETNonmanifoldMesh::Remove(int v0, int v1, int v2)
{
    TriangleKey<true> tkey(v0, v1, v2);
    auto titer = mTMap.find(tkey);
    if (titer == mTMap.end())
    {
        // The triangle does not exist.
        return false;
    }

    // Get the triangle.
    std::shared_ptr<Triangle> tri = titer->second;

    // Remove the edges and update adjacent triangles if necessary.
    for (int i = 0; i < 3; ++i)
    {
        // Inform the edges the triangle is being deleted.
        auto edge = tri->E[i].lock();
        if (!edge)
        {
            LogError("Unexpected condition.");
            return false;
        }

        // Remove the triangle from the edge's set of adjacent triangles.
        size_t numRemoved = edge->T.erase(tri);
        if (numRemoved == 0)
        {
            LogError("Unexpected condition.");
            return false;
        }

        // Remove the edge if you have the last reference to it.
        if (edge->T.size() == 0)
        {
            EdgeKey<false> ekey(edge->V[0], edge->V[1]);
            mEMap.erase(ekey);
        }
    }

    // Remove the triangle from the graph.
    mTMap.erase(tkey);
    return true;
}

void ETNonmanifoldMesh::Clear()
{
    mEMap.clear();
    mTMap.clear();
}

bool ETNonmanifoldMesh::IsManifold() const
{
    for (auto const& element : mEMap)
    {
        if (element.second->T.size() > 2)
        {
            return false;
        }
    }
    return true;
}

bool ETNonmanifoldMesh::IsClosed() const
{
    for (auto const& element : mEMap)
    {
        if (element.second->T.size() != 2)
        {
            return false;
        }
    }
    return true;
}

void ETNonmanifoldMesh::GetComponents(std::vector<std::vector<std::shared_ptr<Triangle>>>& components) const
{
    // visited: 0 (unvisited), 1 (discovered), 2 (finished)
    std::map<std::shared_ptr<Triangle>, int> visited;
    for (auto const& element : mTMap)
    {
        visited.insert(std::make_pair(element.second, 0));
    }

    for (auto& element : mTMap)
    {
        auto tri = element.second;
        if (visited[tri] == 0)
        {
            std::vector<std::shared_ptr<Triangle>> component;
            DepthFirstSearch(tri, visited, component);
            components.push_back(component);
        }
    }
}

void ETNonmanifoldMesh::GetComponents(std::vector<std::vector<TriangleKey<true>>>& components) const
{
    // visited: 0 (unvisited), 1 (discovered), 2 (finished)
    std::map<std::shared_ptr<Triangle>, int> visited;
    for (auto const& element : mTMap)
    {
        visited.insert(std::make_pair(element.second, 0));
    }

    for (auto& element : mTMap)
    {
        std::shared_ptr<Triangle> tri = element.second;
        if (visited[tri] == 0)
        {
            std::vector<std::shared_ptr<Triangle>> component;
            DepthFirstSearch(tri, visited, component);

            std::vector<TriangleKey<true>> keyComponent;
            keyComponent.reserve(component.size());
            for (auto const& t : component)
            {
                keyComponent.push_back(TriangleKey<true>(t->V[0], t->V[1], t->V[2]));
            }
            components.push_back(keyComponent);
        }
    }
}

void ETNonmanifoldMesh::DepthFirstSearch(std::shared_ptr<Triangle> const& tInitial,
    std::map<std::shared_ptr<Triangle>, int>& visited, std::vector<std::shared_ptr<Triangle>>& component) const
{
    // Allocate the maximum-size stack that can occur in the depth-first
    // search.  The stack is empty when the index top is -1.
    std::vector<std::shared_ptr<Triangle>> tStack(mTMap.size());
    int top = -1;
    tStack[++top] = tInitial;
    while (top >= 0)
    {
        std::shared_ptr<Triangle> tri = tStack[top];
        visited[tri] = 1;
        int i;
        for (i = 0; i < 3; ++i)
        {
            auto edge = tri->E[i].lock();
            if (!edge)
            {
                LogError("Unexpected condition.");
                return;
            }

            bool foundUnvisited = false;
            for (auto const& adjw : edge->T)
            {
                auto adj = adjw.lock();
                if (!adj)
                {
                    LogError("Unexpected condition.");
                    return;
                }

                if (visited[adj] == 0)
                {
                    tStack[++top] = adj;
                    foundUnvisited = true;
                    break;
                }
            }

            if (foundUnvisited)
            {
                break;
            }
        }
        if (i == 3)
        {
            visited[tri] = 2;
            component.push_back(tri);
            --top;
        }
    }
}

std::shared_ptr<ETNonmanifoldMesh::Edge> ETNonmanifoldMesh::CreateEdge(int v0, int v1)
{
    return std::make_shared<Edge>(v0, v1);
}

std::shared_ptr<ETNonmanifoldMesh::Triangle> ETNonmanifoldMesh::CreateTriangle(int v0, int v1, int v2)
{
    return std::make_shared<Triangle>(v0, v1, v2);
}

ETNonmanifoldMesh::Edge::~Edge()
{
}

ETNonmanifoldMesh::Edge::Edge(int v0, int v1)
{
    V[0] = v0;
    V[1] = v1;
}

ETNonmanifoldMesh::Triangle::~Triangle()
{
}

ETNonmanifoldMesh::Triangle::Triangle(int v0, int v1, int v2)
{
    V[0] = v0;
    V[1] = v1;
    V[2] = v2;
}
