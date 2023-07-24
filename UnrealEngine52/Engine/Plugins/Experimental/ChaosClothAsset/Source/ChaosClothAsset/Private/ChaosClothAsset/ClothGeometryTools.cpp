// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/ClothGeometryTools.h"
#include "ChaosClothAsset/ClothCollection.h"
#include "ChaosClothAsset/ClothAdapter.h"

namespace UE::Chaos::ClothAsset
{
	void FClothGeometryTools::CopySimMeshToRenderMesh(const TSharedPtr<FClothCollection>& ClothCollection, int32 MaterialIndex)
	{
		FClothAdapter ClothAdapter(ClothCollection);

		for (int32 LodIndex = 0; LodIndex < ClothAdapter.GetNumLods(); ++LodIndex)
		{
			FClothLodAdapter ClothLodAdapter = ClothAdapter.GetLod(LodIndex);

			const TConstArrayView<FVector2f> LodSimPosition = ClothLodAdapter.GetPatternsSimPosition();
			const TConstArrayView<FVector3f> LodSimRestPosition = ClothLodAdapter.GetPatternsSimRestPosition();

			for (int32 PatternIndex = 0; PatternIndex < ClothLodAdapter.GetNumPatterns(); ++PatternIndex)
			{
				FClothPatternAdapter ClothPatternAdapter = ClothLodAdapter.GetPattern(PatternIndex);

				const int32 NumVertices = ClothPatternAdapter.GetNumSimVertices();
				const int32 NumFaces = ClothPatternAdapter.GetNumSimFaces();

				if (!NumVertices || !NumFaces)
				{
					ClothPatternAdapter.SetNumRenderVertices(0);
					ClothPatternAdapter.SetNumRenderFaces(0);
					continue;
				}

				ClothPatternAdapter.SetNumRenderVertices(NumVertices);
				ClothPatternAdapter.SetNumRenderFaces(NumFaces);

				const TConstArrayView<FVector2f> SimPosition = ClothPatternAdapter.GetSimPosition();
				const TConstArrayView<FVector3f> SimRestPosition = ClothPatternAdapter.GetSimRestPosition();
				const TConstArrayView<FVector3f> SimRestNormal = ClothPatternAdapter.GetSimRestNormal();
				const TArrayView<FVector3f> RenderTangentU = ClothPatternAdapter.GetRenderTangentU();
				const TArrayView<FVector3f> RenderTangentV = ClothPatternAdapter.GetRenderTangentV();

				FVector2f MinPosition(TNumericLimits<float>::Max());
				FVector2f MaxPosition(TNumericLimits<float>::Lowest());

				// Calculate UVs scale and zero out tangents
				for (int32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
				{
					MinPosition = FVector2f::Min(MinPosition, SimPosition[VertexIndex]);
					MaxPosition = FVector2f::Max(MaxPosition, SimPosition[VertexIndex]);

					RenderTangentU[VertexIndex] = FVector3f::ZeroVector;
					RenderTangentV[VertexIndex] = FVector3f::ZeroVector;
				}
				const FVector2f UVScale = MaxPosition - MinPosition;
				const FVector2f UVInvScale(
					UVScale.X < SMALL_NUMBER ? 0.f : 1.f / UVScale.X,
					UVScale.Y < SMALL_NUMBER ? 0.f : 1.f / UVScale.Y);

				// Face group
				const TConstArrayView<FIntVector3> SimIndices = ClothPatternAdapter.GetSimIndices();
				const TArrayView<FIntVector3> RenderIndices = ClothPatternAdapter.GetRenderIndices();
				const TArrayView<int32> RenderMaterialIndex = ClothPatternAdapter.GetRenderMaterialIndex();
				const TArrayView<FVector3f> LodRenderTangentU = ClothLodAdapter.GetPatternsRenderTangentU();
				const TArrayView<FVector3f> LodRenderTangentV = ClothLodAdapter.GetPatternsRenderTangentV();
				for (int32 FaceIndex = 0; FaceIndex < NumFaces; ++FaceIndex)
				{
					const FIntVector3& Face = SimIndices[FaceIndex];

					RenderIndices[FaceIndex] = Face;
					RenderMaterialIndex[FaceIndex] = MaterialIndex;

					// Edges of the triangle : position delta
					const FVector3f DeltaPos1 = LodSimRestPosition[Face[1]] - LodSimRestPosition[Face[0]];
					const FVector3f DeltaPos2 = LodSimRestPosition[Face[2]] - LodSimRestPosition[Face[0]];

					// UV delta
					const FVector2f DeltaUV1 = LodSimPosition[Face[1]] - LodSimPosition[Face[0]];
					const FVector2f DeltaUV2 = LodSimPosition[Face[2]] - LodSimPosition[Face[0]];
					
					// We can now use our formula to compute the tangent and the bitangent :
					const float Denom = DeltaUV1.X * DeltaUV2.Y - DeltaUV1.Y * DeltaUV2.X;
					const float R = (Denom < SMALL_NUMBER) ? 0.f : 1.f / Denom;
					const FVector3f TangentU = (DeltaPos1 * DeltaUV2.Y - DeltaPos2 * DeltaUV1.Y) * R;
					const FVector3f TangentV = (DeltaPos2 * DeltaUV1.X - DeltaPos1 * DeltaUV2.X) * R;

					for (int32 PointIndex = 0; PointIndex < 3; ++PointIndex)
					{
						LodRenderTangentU[Face[PointIndex]] += TangentU;
						LodRenderTangentV[Face[PointIndex]] += TangentV;
					}
				}

				// Vertex group
				const TArrayView<FVector3f> RenderPosition = ClothPatternAdapter.GetRenderPosition();
				const TArrayView<FVector3f> RenderNormal = ClothPatternAdapter.GetRenderNormal();
				const TArrayView<TArray<FVector2f>> RenderUVs = ClothPatternAdapter.GetRenderUVs();
				const TArrayView<FLinearColor> RenderColor = ClothPatternAdapter.GetRenderColor();

				for (int32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
				{
					RenderPosition[VertexIndex] = SimRestPosition[VertexIndex];
					RenderNormal[VertexIndex] = SimRestNormal[VertexIndex];
					RenderUVs[VertexIndex] = { SimPosition[VertexIndex] * UVInvScale };
					RenderColor[VertexIndex] = FLinearColor::White;
					RenderTangentU[VertexIndex].Normalize();
					RenderTangentV[VertexIndex].Normalize();
				}
			}
		}
	}
}  // End namespace UE::Chaos::ClothAsset