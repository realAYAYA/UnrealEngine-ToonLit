// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/GTEngineDEF.h>
#include <map>
#include <memory>

namespace gte
{

class GTE_IMPEXP VEManifoldMesh
{
public:
    // Vertex data types.
    class Vertex;
    typedef std::shared_ptr<Vertex> (*VCreator)(int);
    typedef std::map<int, std::shared_ptr<Vertex>> VMap;

    // Edge data types.
    class Edge;
    typedef std::shared_ptr<Edge> (*ECreator)(int, int);
    typedef std::map<std::pair<int, int>, std::shared_ptr<Edge>> EMap;

    // Vertex object.
    class GTE_IMPEXP Vertex
    {
    public:
        virtual ~Vertex();
        Vertex(int v);

        // The unique vertex index.
        int V;

        // The edges (if any) sharing the vertex.
        std::weak_ptr<Edge> E[2];
    };

    // Edge object.
    class GTE_IMPEXP Edge
    {
    public:
        virtual ~Edge();
        Edge(int v0, int v1);

        // Vertices, listed as a directed edge <V[0],V[1]>.
        int V[2];

        // Adjacent edges.  E[i] points to edge sharing V[i].
        std::weak_ptr<Edge> E[2];
    };


    // Construction and destruction.
    virtual ~VEManifoldMesh();
    VEManifoldMesh(VCreator vCreator = nullptr, ECreator eCreator = nullptr);

    // Member access.
    VMap const& GetVertices() const;
    EMap const& GetEdges() const;

    // If the insertion of an edge fails because the mesh would become
    // nonmanifold, the default behavior is to trigger a LogError message.
    // You can disable this behavior in situations where you want the Logger
    // system on but you want to continue gracefully without an assertion.
    void AssertOnNonmanifoldInsertion(bool doAssert);

    // If <v0,v1> is not in the mesh, an Edge object is created and returned;
    // otherwise, <v0,v1> is in the mesh and nullptr is returned.  If the
    // insertion leads to a nonmanifold mesh, the call fails with a nullptr
    // returned.
    std::shared_ptr<Edge> Insert(int v0, int v1);

    // If <v0,v1> is in the mesh, it is removed and 'true' is returned;
    // otherwise, <v0,v1> is not in the mesh and 'false' is returned.
    bool Remove(int v0, int v1);

    // A manifold mesh is closed if each vertex is shared twice.
    bool IsClosed() const;

protected:
    // The vertex data and default vertex creation.
    static std::shared_ptr<Vertex> CreateVertex(int v0);
    VCreator mVCreator;
    VMap mVMap;

    // The edge data and default edge creation.
    static std::shared_ptr<Edge> CreateEdge(int v0, int v1);
    ECreator mECreator;
    EMap mEMap;
    bool mAssertOnNonmanifoldInsertion;  // default: true
};

}
