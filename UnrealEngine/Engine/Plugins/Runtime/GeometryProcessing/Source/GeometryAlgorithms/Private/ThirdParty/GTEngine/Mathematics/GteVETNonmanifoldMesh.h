// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2018
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.20.0 (2019/01/09)

#pragma once

#include <ThirdParty/GTEngine/LowLevel/GteSharedPtrCompare.h>
#include <ThirdParty/GTEngine/Mathematics/GteETNonmanifoldMesh.h>

// The VETNonmanifoldMesh class represents an edge-triangle nonmanifold mesh
// but additionally stores vertex adjacency information.

namespace gte
{
    class GTE_IMPEXP VETNonmanifoldMesh : public ETNonmanifoldMesh
    {
    public:
        // Vertex data types.
        class Vertex;
        typedef std::shared_ptr<Vertex>(*VCreator)(int);
        typedef std::map<int, std::shared_ptr<Vertex>> VMap;

        // Vertex object.
        class GTE_IMPEXP Vertex
        {
        public:
            virtual ~Vertex();
            Vertex(int vIndex);

            // The index into the vertex pool of the mesh.
            int V;

            bool operator<(Vertex const& other) const
            {
                return V < other.V;
            }

            // Adjacent objects.
            std::set<int> VAdjacent;
            std::set<std::shared_ptr<Edge>, SharedPtrLT<Edge>> EAdjacent;
            std::set<std::shared_ptr<Triangle>, SharedPtrLT<Triangle>> TAdjacent;
        };


        // Construction and destruction.
        virtual ~VETNonmanifoldMesh();
        VETNonmanifoldMesh(VCreator vCreator = nullptr, ECreator eCreator = nullptr, TCreator tCreator = nullptr);

        // Support for a deep copy of the mesh.  The mVMap, mEMap, and mTMap
        // objects have dynamically allocated memory for vertices, edges, and
        // triangles.  A shallow copy of the pointers to this memory is
        // problematic.  Allowing sharing, say, via std::shared_ptr, is an
        // option but not really the intent of copying the mesh graph.
        VETNonmanifoldMesh(VETNonmanifoldMesh const& mesh);
        VETNonmanifoldMesh& operator=(VETNonmanifoldMesh const& mesh);

        // Member access.
        VMap const& GetVertices() const;

        // If <v0,v1,v2> is not in the mesh, a Triangle object is created and
        // returned; otherwise, <v0,v1,v2> is in the mesh and nullptr is returned.
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
