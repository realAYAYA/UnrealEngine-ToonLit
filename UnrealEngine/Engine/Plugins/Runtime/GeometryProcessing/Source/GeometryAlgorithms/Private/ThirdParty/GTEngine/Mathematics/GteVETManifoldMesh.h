// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteETManifoldMesh.h>
#include <set>

// The VETManifoldMesh class represents an edge-triangle manifold mesh
// but additionally stores vertex adjacency information.

namespace gte
{

class GTE_IMPEXP VETManifoldMesh : public ETManifoldMesh
{
public:
    // Vertex data types.
    class Vertex;
    typedef std::shared_ptr<Vertex> (*VCreator)(int);
    typedef std::map<int, std::shared_ptr<Vertex>> VMap;

    // Vertex object.
    class GTE_IMPEXP Vertex
    {
    public:
        virtual ~Vertex();
        Vertex(int vIndex);

        // The index into the vertex pool of the mesh.
        int V;

        // Adjacent objects.
        std::set<int> VAdjacent;
        std::set<std::shared_ptr<Edge>> EAdjacent;
        std::set<std::shared_ptr<Triangle>> TAdjacent;
    };


    // Construction and destruction.
    virtual ~VETManifoldMesh();
    VETManifoldMesh(VCreator vCreator = nullptr, ECreator eCreator = nullptr, TCreator tCreator = nullptr);

    // Support for a deep copy of the mesh.  The mVMap, mEMap, and mTMap
    // objects have dynamically allocated memory for vertices, edges, and
    // triangles.  A shallow copy of the pointers to this memory is
    // problematic.  Allowing sharing, say, via std::shared_ptr, is an
    // option but not really the intent of copying the mesh graph.
    VETManifoldMesh(VETManifoldMesh const& mesh);
    VETManifoldMesh& operator=(VETManifoldMesh const& mesh);

    // Member access.
    VMap const& GetVertices() const;

    // If <v0,v1,v2> is not in the mesh, a Triangle object is created and
    // returned; otherwise, <v0,v1,v2> is in the mesh and nullptr is returned.
    // If the insertion leads to a nonmanifold mesh, the call fails with a
    // nullptr returned.
    virtual std::shared_ptr<Triangle> Insert(int v0, int v1, int v2) override;

    // If <v0,v1,v2> is in the mesh, it is removed and 'true' is returned;
    // otherwise, <v0,v1,v2> is not in the mesh and 'false' is returned.
    virtual bool Remove(int v0, int v1, int v2) override;

    // Destroy the vertices, edges, and triangles to obtain an empty mesh.
    virtual void Clear() override;

protected:
    // The vertex data and default vertex creation.
    static std::shared_ptr<Vertex> CreateVertex(int vIndex);
    VCreator mVCreator;
    VMap mVMap;
};

}
