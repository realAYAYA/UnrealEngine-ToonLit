// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDHeightfieldMeshGenerator.h"

#include "Chaos/HeightField.h"

void FChaosVDHeightFieldMeshGenerator::AppendTriangle(int32& OutTriIndex, int& OutCurrentNormalIndex, int32 PolygonIndex, const Chaos::TVec2<Chaos::FReal>& InCellCoordinates, const UE::Geometry::FIndex3i& InTriangle, const Chaos::FHeightField& InHeightField)
{
	int32 NormalIndices[3];
    for (int32 LocalVertexIndex = 0; LocalVertexIndex < 3; ++LocalVertexIndex)
    {
        Normals[OutCurrentNormalIndex] = UE::Math::TVector<float>(InHeightField.GetNormalAt(InCellCoordinates));
        NormalIndices[LocalVertexIndex] = OutCurrentNormalIndex;
        OutCurrentNormalIndex++;
    }

	SetTriangle(OutTriIndex, InTriangle);
	SetTrianglePolygon(OutTriIndex, PolygonIndex);
	SetTriangleNormals(OutTriIndex, NormalIndices[0], NormalIndices[1], NormalIndices[2]);
	OutTriIndex++;
}

void FChaosVDHeightFieldMeshGenerator::GenerateFromHeightField(const Chaos::FHeightField& InHeightField)
{
	using namespace UE::Geometry;

	const int32 NumRows = InHeightField.GetNumRows();
	const int32 NumCols = InHeightField.GetNumCols();
	const int32 VertexCount = NumRows * NumCols;

	const int32 NumQuads = (NumRows - 1) * (NumCols - 1);
	const int32 NumTris = NumQuads * 2;
	const int32 NumNormals = NumTris * 3;
	constexpr int32 NumUVs = 0;
	
	SetBufferSizes(VertexCount, NumTris, NumUVs, NumNormals);

	//Fill the vertex buffer with the height data
	for (int32 Y = 0; Y < NumRows; Y++)
	{
		for (int32 X = 0; X < NumCols; X++)
		{
			const int32 SampleIndex = Y * NumCols + X;
			Vertices[SampleIndex] = InHeightField.GetPointScaled(SampleIndex);
		}
	}
  
	int32 CurrentNormalIndex = 0;
	int32 CurrentTriangleIndex = 0;
	int32 CurrentPolygonIndex = 0;
	for (int32 Y = 0; Y < NumRows - 1; Y++)
	{
		for (int32 X = 0; X < NumCols - 1; X++)
		{
			if (InHeightField.IsHole(X, Y))
			{
				continue;
			}

			Chaos::TVec2<Chaos::FReal> CellCoordinates(X,Y);

			// Vertices of each corner of the current cell
			const int32 Vertex0 = Y * NumCols + X;
			const int32 Vertex1 = Vertex0 + 1;
			const int32 Vertex2 = Vertex0 + NumCols;
			const int32 Vertex3 = Vertex2 + 1;

			// Define the two triangles that form the current cell and add it to the generator
			FIndex3i Triangle(Vertex0,Vertex3,Vertex1);
			FIndex3i Triangle2(Vertex0,Vertex2,Vertex3);

			AppendTriangle(CurrentTriangleIndex,CurrentNormalIndex, CurrentPolygonIndex, CellCoordinates, Triangle, InHeightField);
			AppendTriangle(CurrentTriangleIndex,CurrentNormalIndex, CurrentPolygonIndex, CellCoordinates, Triangle2, InHeightField);

			CurrentPolygonIndex++;
		}
	}

	bIsGenerated = true;
}


UE::Geometry::FMeshShapeGenerator& FChaosVDHeightFieldMeshGenerator::Generate()
{
	ensureAlwaysMsgf(bIsGenerated, TEXT("You need to call FChaosVDHeightFieldMeshGenerator::GenerateFromHeightField before calling Generate"));
	return *this;
}
