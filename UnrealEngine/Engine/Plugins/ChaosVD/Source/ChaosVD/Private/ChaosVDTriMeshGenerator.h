// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Chaos/TriangleMeshImplicitObject.h"
#include "Generators/MeshShapeGenerator.h"

namespace Chaos
{
	class FTriangleMeshImplicitObject;
}

/** Generates a Dynamic mesh based on a Triangle Mesh Implicit object*/
class FChaosVDTriMeshGenerator : public UE::Geometry::FMeshShapeGenerator
{

public:
	/** Triangle Mesh Implicit object used as data source to generate the dynamic mesh */
	void GenerateFromTriMesh(const Chaos::FTriangleMeshImplicitObject& InTriMesh);

	virtual FMeshShapeGenerator& Generate() override;

private:

	template<typename BufferIndexType>
	void ProcessTriangles(const TArray<BufferIndexType>& InTriangles, const int32 NumTriangles, const Chaos::FTriangleMeshImplicitObject& InTriMesh);

	bool bIsGenerated = false;;
};

template <typename BufferIndexType>
void FChaosVDTriMeshGenerator::ProcessTriangles(const TArray<BufferIndexType>& InTriangles, const int32 NumTriangles, const Chaos::FTriangleMeshImplicitObject& InTriMesh)
{
	int CurrentNormalIndex = 0;
	for (int32 TriangleIndex = 0; TriangleIndex < NumTriangles; ++TriangleIndex)
	{
		UE::Geometry::FIndex3i Triangle(InTriangles[TriangleIndex][0],InTriangles[TriangleIndex][1],InTriangles[TriangleIndex][2]);

		// Create a Normal entry per triangle vertex.
		int NormalIndices[3];
		for (int32 LocalVertexIndex = 0; LocalVertexIndex < 3; ++LocalVertexIndex)
		{
			// Use the normal of the face for all it's vertices
			Normals[CurrentNormalIndex] = UE::Math::TVector<float>(InTriMesh.GetFaceNormal(TriangleIndex));
			NormalIndices[LocalVertexIndex] = CurrentNormalIndex;
			CurrentNormalIndex++;
		}

		SetTriangle(TriangleIndex, MoveTemp(Triangle));
		SetTrianglePolygon(TriangleIndex, TriangleIndex);
		SetTriangleNormals(TriangleIndex, NormalIndices[0], NormalIndices[1], NormalIndices[2]);
	}
}
