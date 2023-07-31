// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#include <ThirdParty/GTEngine/Mathematics/GteTSManifoldMesh.h>
#include <ThirdParty/GTEngine/LowLevel/GteLogger.h>
#include <ThirdParty/GTEngine/GTEnginePCH.h>

using namespace gte;

TSManifoldMesh::~TSManifoldMesh()
{
}

TSManifoldMesh::TSManifoldMesh(TCreator tCreator, SCreator sCreator)
    :
    mTCreator(tCreator ? tCreator : CreateTriangle),
    mSCreator(sCreator ? sCreator : CreateTetrahedron),
    mAssertOnNonmanifoldInsertion(true)
{
}

TSManifoldMesh::TSManifoldMesh(TSManifoldMesh const& mesh)
{
    *this = mesh;
}

TSManifoldMesh& TSManifoldMesh::operator=(TSManifoldMesh const& mesh)
{
    Clear();

    mTCreator = mesh.mTCreator;
    mSCreator = mesh.mSCreator;
    mAssertOnNonmanifoldInsertion = mesh.mAssertOnNonmanifoldInsertion;
    for (auto const& element : mesh.mSMap)
    {
        Insert(element.first.V[0], element.first.V[1], element.first.V[2], element.first.V[3]);
    }

    return *this;
}

TSManifoldMesh::TMap const& TSManifoldMesh::GetTriangles() const
{
    return mTMap;
}

TSManifoldMesh::SMap const& TSManifoldMesh::GetTetrahedra() const
{
    return mSMap;
}

std::shared_ptr<TSManifoldMesh::Triangle> TSManifoldMesh::CreateTriangle(int v0, int v1, int v2)
{
    return std::make_shared<Triangle>(v0, v1, v2);
}

std::shared_ptr<TSManifoldMesh::Tetrahedron> TSManifoldMesh::CreateTetrahedron(int v0, int v1, int v2, int v3)
{
    return std::make_shared<Tetrahedron>(v0, v1, v2, v3);
}

void TSManifoldMesh::AssertOnNonmanifoldInsertion(bool doAssert)
{
    mAssertOnNonmanifoldInsertion = doAssert;
}

std::shared_ptr<TSManifoldMesh::Tetrahedron> TSManifoldMesh::Insert(int v0, int v1, int v2, int v3)
{
    TetrahedronKey<true> skey(v0, v1, v2, v3);
    if (mSMap.find(skey) != mSMap.end())
    {
        // The tetrahedron already exists.  Return a null pointer as a signal
        // to the caller that the insertion failed.
        return nullptr;
    }

    // Add the new tetrahedron.
    std::shared_ptr<Tetrahedron> tetra = mSCreator(v0, v1, v2, v3);
    mSMap[skey] = tetra;

    // Add the faces to the mesh if they do not already exist.
    for (int i = 0; i < 4; ++i)
    {
        auto opposite = TetrahedronKey<true>::oppositeFace[i];
        TriangleKey<false> tkey(tetra->V[opposite[0]], tetra->V[opposite[1]], tetra->V[opposite[2]]);
        std::shared_ptr<Triangle> face;
        auto titer = mTMap.find(tkey);
        if (titer == mTMap.end())
        {
            // This is the first time the face is encountered.
            face = mTCreator(tetra->V[opposite[0]], tetra->V[opposite[1]], tetra->V[opposite[2]]);
            mTMap[tkey] = face;

            // Update the face and tetrahedron.
            face->T[0] = tetra;
            tetra->T[i] = face;
        }
        else
        {
            // This is the second time the face is encountered.
            face = titer->second;
            if (!face)
            {
                LogError("Unexpected condition.");
                return nullptr;
            }

            // Update the face.
            if (face->T[1].lock())
            {
                if (mAssertOnNonmanifoldInsertion)
                {
                    LogInformation("The mesh must be manifold.");
                }
                return nullptr;
            }
            face->T[1] = tetra;

            // Update the adjacent tetrahedra.
            auto adjacent = face->T[0].lock();
            if (!adjacent)
            {
                LogError("Unexpected condition.");
                return nullptr;
            }
            for (int j = 0; j < 4; ++j)
            {
                if (adjacent->T[j].lock() == face)
                {
                    adjacent->S[j] = tetra;
                    break;
                }
            }

            // Update the tetrahedron.
            tetra->T[i] = face;
            tetra->S[i] = adjacent;
        }
    }

    return tetra;
}

bool TSManifoldMesh::Remove(int v0, int v1, int v2, int v3)
{
    TetrahedronKey<true> skey(v0, v1, v2, v3);
    auto siter = mSMap.find(skey);
    if (siter == mSMap.end())
    {
        // The tetrahedron does not exist.
        return false;
    }

    // Get the tetrahedron.
    std::shared_ptr<Tetrahedron> tetra = siter->second;

    // Remove the faces and update adjacent tetrahedra if necessary.
    for (int i = 0; i < 4; ++i)
    {
        // Inform the faces the tetrahedron is being deleted.
        auto face = tetra->T[i].lock();
        if (!face)
        {
            // The triangle edge should be nonnull.
            LogError("Unexpected condition.");
            return false;
        }

        if (face->T[0].lock() == tetra)
        {
            // One-tetrahedron faces always have pointer at index zero.
            face->T[0] = face->T[1];
            face->T[1].reset();
        }
        else if (face->T[1].lock() == tetra)
        {
            face->T[1].reset();
        }
        else
        {
            LogError("Unexpected condition.");
            return false;
        }

        // Remove the face if you have the last reference to it.
        if (!face->T[0].lock() && !face->T[1].lock())
        {
            TriangleKey<false> tkey(face->V[0], face->V[1], face->V[2]);
            mTMap.erase(tkey);
        }

        // Inform adjacent tetrahedra the tetrahedron is being deleted.
        auto adjacent = tetra->S[i].lock();
        if (adjacent)
        {
            for (int j = 0; j < 4; ++j)
            {
                if (adjacent->S[j].lock() == tetra)
                {
                    adjacent->S[j].reset();
                    break;
                }
            }
        }
    }

    mSMap.erase(skey);
    return true;
}

void TSManifoldMesh::Clear()
{
    mTMap.clear();
    mSMap.clear();
}

bool TSManifoldMesh::IsClosed() const
{
    for (auto const& element : mSMap)
    {
        auto tri = element.second;
        if (!tri->S[0].lock() || !tri->S[1].lock())
        {
            return false;
        }
    }
    return true;
}

TSManifoldMesh::Triangle::~Triangle()
{
}

TSManifoldMesh::Triangle::Triangle(int v0, int v1, int v2)
{
    V[0] = v0;
    V[1] = v1;
    V[2] = v2;
}

TSManifoldMesh::Tetrahedron::~Tetrahedron()
{
}

TSManifoldMesh::Tetrahedron::Tetrahedron(int v0, int v1, int v2, int v3)
{
    V[0] = v0;
    V[1] = v1;
    V[2] = v2;
    V[3] = v3;
}
