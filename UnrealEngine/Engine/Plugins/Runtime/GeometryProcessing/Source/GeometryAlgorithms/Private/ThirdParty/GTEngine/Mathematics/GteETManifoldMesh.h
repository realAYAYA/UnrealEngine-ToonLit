// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.1 (2017/01/02)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteEdgeKey.h>
#include <ThirdParty/GTEngine/Mathematics/GteTriangleKey.h>
#include <map>
#include <memory>
#include <vector>

namespace gte
{

class GTE_IMPEXP ETManifoldMesh
{
public:
    // Edge data types.
    class Edge;
    typedef std::shared_ptr<Edge> (*ECreator)(int, int);
    typedef std::map<EdgeKey<false>, std::shared_ptr<Edge>> EMap;

    // Triangle data types.
    class Triangle;
    typedef std::shared_ptr<Triangle> (*TCreator)(int, int, int);
    typedef std::map<TriangleKey<true>, std::shared_ptr<Triangle>> TMap;

    // Edge object.
    class GTE_IMPEXP Edge
    {
    public:
        virtual ~Edge();
        Edge(int v0, int v1);

        // Vertices of the edge.
        int V[2];

        // Triangles sharing the edge.
        std::weak_ptr<Triangle> T[2];
    };

    // Triangle object.
    class GTE_IMPEXP Triangle
    {
    public:
        virtual ~Triangle();
        Triangle(int v0, int v1, int v2);

        // Vertices, listed in counterclockwise order (V[0],V[1],V[2]).
        int V[3];

        // Adjacent edges.  E[i] points to edge (V[i],V[(i+1)%3]).
        std::weak_ptr<Edge> E[3];

        // Adjacent triangles.  T[i] points to the adjacent triangle
        // sharing edge E[i].
        std::weak_ptr<Triangle> T[3];
    };


    // Construction and destruction.
    virtual ~ETManifoldMesh();
    ETManifoldMesh(ECreator eCreator = nullptr, TCreator tCreator = nullptr);

    // Support for a deep copy of the mesh.  The mEMap and mTMap objects have
    // dynamically allocated memory for edges and triangles.  A shallow copy
    // of the pointers to this memory is problematic.  Allowing sharing, say,
    // via std::shared_ptr, is an option but not really the intent of copying
    // the mesh graph.
    ETManifoldMesh(ETManifoldMesh const& mesh);
    ETManifoldMesh& operator=(ETManifoldMesh const& mesh);

    // Member access.
    EMap const& GetEdges() const;
    TMap const& GetTriangles() const;

    // If the insertion of a triangle fails because the mesh would become
    // nonmanifold, the default behavior is to trigger a LogError message.
    // You can disable this behavior in situations where you want the Logger
    // system on but you want to continue gracefully without an assertion.
    // The return value is the previous value of the internal state
    // mAssertOnNonmanifoldInsertion.
    bool AssertOnNonmanifoldInsertion(bool doAssert);

    // If <v0,v1,v2> is not in the mesh, a Triangle object is created and
    // returned; otherwise, <v0,v1,v2> is in the mesh and nullptr is returned.
    // If the insertion leads to a nonmanifold mesh, the call fails with a
    // nullptr returned.
    virtual std::shared_ptr<Triangle> Insert(int v0, int v1, int v2);

    // If <v0,v1,v2> is in the mesh, it is removed and 'true' is returned;
    // otherwise, <v0,v1,v2> is not in the mesh and 'false' is returned.
    virtual bool Remove(int v0, int v1, int v2);

    // Destroy the edges and triangles to obtain an empty mesh.
    virtual void Clear();

    // A manifold mesh is closed if each edge is shared twice.  A closed
    // mesh is not necessarily oriented.  For example, you could have a
    // mesh with spherical topology.  The upper hemisphere has outer-facing
    // normals and the lower hemisphere has inner-facing normals.  The
    // discontinuity in orientation occurs on the circle shared by the
    // hemispheres.
    bool IsClosed() const;

    // Test whether all triangles in the mesh are oriented consistently and
    // that no two triangles are coincident.  The latter means that you
    // cannot have both triangles <v0,v1,v2> and <v0,v2,v1> in the mesh to
    // be considered oriented.
    bool IsOriented() const;

    // Compute the connected components of the edge-triangle graph that the
    // mesh represents.  The first function returns pointers into 'this'
    // object's containers, so you must consume the components before
    // clearing or destroying 'this'.  The second function returns triangle
    // keys, which requires three times as much storage as the pointers but
    // allows you to clear or destroy 'this' before consuming the components.
    void GetComponents(std::vector<std::vector<std::shared_ptr<Triangle>>>& components) const;
    void GetComponents(std::vector<std::vector<TriangleKey<true>>>& components) const;

protected:
    // The edge data and default edge creation.
    static std::shared_ptr<Edge> CreateEdge(int v0, int v1);
    ECreator mECreator;
    EMap mEMap;

    // The triangle data and default triangle creation.
    static std::shared_ptr<Triangle> CreateTriangle(int v0, int v1, int v2);
    TCreator mTCreator;
    TMap mTMap;
    bool mAssertOnNonmanifoldInsertion;  // default: true

    // Support for computing connected components.  This is a straightforward
    // depth-first search of the graph but uses a preallocated stack rather
    // than a recursive function that could possibly overflow the call stack.
    void DepthFirstSearch(std::shared_ptr<Triangle> const& tInitial,
        std::map<std::shared_ptr<Triangle>, int>& visited,
        std::vector<std::shared_ptr<Triangle>>& component) const;
};

}
