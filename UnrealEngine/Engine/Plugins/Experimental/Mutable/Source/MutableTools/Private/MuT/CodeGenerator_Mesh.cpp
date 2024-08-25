// Copyright Epic Games, Inc. All Rights Reserved.

#include "Containers/Array.h"
#include "Math/IntPoint.h"
#include "Math/UnrealMathSSE.h"
#include "Misc/AssertionMacros.h"
#include "MuR/Layout.h"
#include "MuR/Mesh.h"
#include "MuR/MeshPrivate.h"
#include "MuR/MeshBufferSet.h"
#include "MuR/MutableMath.h"
#include "MuR/Operations.h"
#include "MuR/ParametersPrivate.h"
#include "MuR/Platform.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuR/System.h"
#include "MuT/AST.h"
#include "MuT/ASTOpConditional.h"
#include "MuT/ASTOpConstantResource.h"
#include "MuT/ASTOpMeshApplyPose.h"
#include "MuT/ASTOpMeshApplyShape.h"
#include "MuT/ASTOpMeshBindShape.h"
#include "MuT/ASTOpMeshClipDeform.h"
#include "MuT/ASTOpMeshClipMorphPlane.h"
#include "MuT/ASTOpMeshExtractLayoutBlocks.h"
#include "MuT/ASTOpMeshFormat.h"
#include "MuT/ASTOpMeshGeometryOperation.h"
#include "MuT/ASTOpMeshMorphReshape.h"
#include "MuT/ASTOpMeshTransform.h"
#include "MuT/ASTOpMeshDifference.h"
#include "MuT/ASTOpMeshMorph.h"
#include "MuT/ASTOpMeshAddTags.h"
#include "MuT/ASTOpSwitch.h"
#include "MuT/CodeGenerator.h"
#include "MuT/CodeGenerator_FirstPass.h"
#include "MuT/CompilerPrivate.h"
#include "MuT/ErrorLog.h"
#include "MuT/ErrorLogPrivate.h"
#include "MuT/NodeImageProject.h"
#include "MuT/NodeLayout.h"
#include "MuT/NodeLayoutPrivate.h"
#include "MuT/NodeMesh.h"
#include "MuT/NodeMeshApplyPose.h"
#include "MuT/NodeMeshApplyPosePrivate.h"
#include "MuT/NodeMeshClipDeform.h"
#include "MuT/NodeMeshClipDeformPrivate.h"
#include "MuT/NodeMeshClipMorphPlane.h"
#include "MuT/NodeMeshClipMorphPlanePrivate.h"
#include "MuT/NodeMeshClipWithMesh.h"
#include "MuT/NodeMeshClipWithMeshPrivate.h"
#include "MuT/NodeMeshConstant.h"
#include "MuT/NodeMeshConstantPrivate.h"
#include "MuT/NodeMeshFormat.h"
#include "MuT/NodeMeshFormatPrivate.h"
#include "MuT/NodeMeshFragment.h"
#include "MuT/NodeMeshFragmentPrivate.h"
#include "MuT/NodeMeshGeometryOperation.h"
#include "MuT/NodeMeshGeometryOperationPrivate.h"
#include "MuT/NodeMeshInterpolate.h"
#include "MuT/NodeMeshInterpolatePrivate.h"
#include "MuT/NodeMeshMakeMorph.h"
#include "MuT/NodeMeshMakeMorphPrivate.h"
#include "MuT/NodeMeshMorph.h"
#include "MuT/NodeMeshMorphPrivate.h"
#include "MuT/NodeMeshReshape.h"
#include "MuT/NodeMeshReshapePrivate.h"
#include "MuT/NodeMeshSwitch.h"
#include "MuT/NodeMeshSwitchPrivate.h"
#include "MuT/NodeMeshTable.h"
#include "MuT/NodeMeshTablePrivate.h"
#include "MuT/NodeMeshTransform.h"
#include "MuT/NodeMeshTransformPrivate.h"
#include "MuT/NodeMeshVariation.h"
#include "MuT/NodeMeshVariationPrivate.h"
#include "MuT/NodeScalar.h"
#include "MuT/Table.h"
#include "MuT/TablePrivate.h"

#include "Spatial/PointHashGrid3.h"


namespace mu
{
	class Node;

	
	//---------------------------------------------------------------------------------------------
    //! Create a map from vertices into vertices, collapsing vertices that have the same position. 
	//! This version uses UE Containers to return.	
    //---------------------------------------------------------------------------------------------
	void MeshCreateCollapsedVertexMap(const mu::Mesh* Mesh,
		TArray<int32>& CollapsedVertices)
	{
		MUTABLE_CPUPROFILER_SCOPE(LayoutUV_CreateCollapsedVertexMap);
	
		const int32 NumVertices = Mesh->GetVertexCount();
		CollapsedVertices.Reserve(NumVertices);
	
		UE::Geometry::TPointHashGrid3f<int32> VertHash(0.01f, INDEX_NONE);
		VertHash.Reserve(NumVertices);
	
		TArray<FVector3f> Vertices;
		Vertices.SetNumUninitialized(NumVertices);
	
		mu::UntypedMeshBufferIteratorConst ItPosition = mu::UntypedMeshBufferIteratorConst(Mesh->GetVertexBuffers(), mu::MBS_POSITION);
	
		FVector3f* VertexData = Vertices.GetData();
	
		for (int32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
		{
			*VertexData = ItPosition.GetAsVec3f();
			VertHash.InsertPointUnsafe(VertexIndex, *VertexData);
	
			++ItPosition;
			++VertexData;
		}
	
		// Find unique vertices
		CollapsedVertices.Init(INDEX_NONE, NumVertices);
	
		TArray<int32> NearbyVertices;
		for (int32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
		{
			if (CollapsedVertices[VertexIndex] != INDEX_NONE)
			{
				continue;
			}
	
			const FVector3f& Vertex = Vertices[VertexIndex];
	
			NearbyVertices.Reset();
			VertHash.FindPointsInBall(Vertex, 0.00001,
				[&Vertex, &Vertices](const int32& Other) -> float {return FVector3f::DistSquared(Vertices[Other], Vertex); },
				NearbyVertices);
	
			// Find equals
			for (int32 NearbyVertexIndex : NearbyVertices)
			{
				CollapsedVertices[NearbyVertexIndex] = VertexIndex;
			}
		}
	}

	// Helper used to connect triangles of a UV island
	struct FTriangle
	{
		uint32 Indices[3];
		uint32 CollapsedIndices[3];

		uint16 BlockIndices[3];

		bool bUVsFixed = false;
	};

	//---------------------------------------------------------------------------------------------
	//! Fill an array with the indices of all triangles belonging to the same UV island as InFirstTriangle. 
	//---------------------------------------------------------------------------------------------
	void GetUVIsland(TArray<FTriangle>& InTriangles,
		const uint32 InFirstTriangle,
		TArray<uint32>& OutTriangleIndices,
		const TArray<FVector2f>& InUVs,
		const TMultiMap<int32, uint32>& InVertexToTriangleMap)
	{
		MUTABLE_CPUPROFILER_SCOPE(LayoutUV_GetUVIsland);

		const uint32 NumTriangles = (uint32)InTriangles.Num();
		
		OutTriangleIndices.Reserve(NumTriangles);
		OutTriangleIndices.Add(InFirstTriangle);

		TArray<bool> SkipTrinalges;
		SkipTrinalges.Init(false, NumTriangles);

		TArray<uint32> PendingTriangles;
		PendingTriangles.Reserve(NumTriangles / 64);
		PendingTriangles.Add(InFirstTriangle);

		while (!PendingTriangles.IsEmpty())
		{
			const uint32 TriangleIndex = PendingTriangles.Pop();

			// Triangle about to be proccessed, mark as skip;
			SkipTrinalges[TriangleIndex] = true;

			bool ConnectedEdges[3] = { false, false, false };

			const FTriangle& Triangle = InTriangles[TriangleIndex];

			// Find Triangles connected to edges 0 and 2
			int32 CollapsedVertex1 = Triangle.CollapsedIndices[1];
			int32 CollapsedVertex2 = Triangle.CollapsedIndices[2];

			TArray<uint32> FoundTriangleIndices;
			InVertexToTriangleMap.MultiFind(Triangle.CollapsedIndices[0], FoundTriangleIndices);

			for (uint32 OtherTriangleIndex : FoundTriangleIndices)
			{
				const FTriangle& OtherTriangle = InTriangles[OtherTriangleIndex];

				for (int32 OtherIndex = 0; OtherIndex < 3; ++OtherIndex)
				{
					const int32 OtherCollapsedIndex = OtherTriangle.CollapsedIndices[OtherIndex];
					if (OtherCollapsedIndex == CollapsedVertex1)
					{
						// Check if the vertex is in the same UV Island 
						if (!SkipTrinalges[OtherTriangleIndex]
							&& InUVs[Triangle.Indices[1]].Equals(InUVs[OtherTriangle.Indices[OtherIndex]], 0.00001f))
						{
							OutTriangleIndices.Add(OtherTriangleIndex);
							PendingTriangles.Add(OtherTriangleIndex);
							SkipTrinalges[OtherTriangleIndex] = true;
						}

						// Connected but already processed or in another island
						break;
					}

					if (OtherCollapsedIndex  == CollapsedVertex2)
					{
						// Check if the vertex is in the same UV Island 
						if (!SkipTrinalges[OtherTriangleIndex]
							&& InUVs[Triangle.Indices[2]].Equals(InUVs[OtherTriangle.Indices[OtherIndex]], 0.00001f))
						{
							OutTriangleIndices.Add(OtherTriangleIndex);
							PendingTriangles.Add(OtherTriangleIndex);
							SkipTrinalges[OtherTriangleIndex] = true;
						}

						// Connected but already processed or in another UV Island
						break;
					}
				}

			}

			// Find the triangle connected to edge 1
			FoundTriangleIndices.Reset();
			InVertexToTriangleMap.MultiFind(CollapsedVertex1, FoundTriangleIndices);

			for (uint32 OtherTriangleIndex : FoundTriangleIndices)
			{
				const FTriangle& OtherTriangle = InTriangles[OtherTriangleIndex];

				for (int32 OtherIndex = 0; OtherIndex < 3; ++OtherIndex)
				{
					const int32 OtherCollapsedIndex = OtherTriangle.CollapsedIndices[OtherIndex];
					if (OtherCollapsedIndex == CollapsedVertex2)
					{
						// Check if the vertex belong to the same UV island
						if (!SkipTrinalges[OtherTriangleIndex]
							&& InUVs[Triangle.Indices[2]].Equals(InUVs[OtherTriangle.Indices[OtherIndex]], 0.00001f))
						{
							OutTriangleIndices.Add(OtherTriangleIndex);
							PendingTriangles.Add(OtherTriangleIndex);
							SkipTrinalges[OtherTriangleIndex] = true;
						}

						// Connected but already processed or in another island
						break;
					}
				}
			}
		}
	}

    //---------------------------------------------------------------------------------------------
	void CodeGenerator::PrepareForLayout(Ptr<const Layout> GeneratedLayout,
		Ptr<Mesh> currentLayoutMesh,
		int32 currentLayoutChannel,
		const void* errorContext,
		const FMeshGenerationOptions& MeshOptions
		)
	{
		MUTABLE_CPUPROFILER_SCOPE(LayoutUV_PrepareForLayout);

		if (currentLayoutMesh->GetVertexCount() == 0)
		{
			return;
		}

		// The layout must have block ids.
		check(GeneratedLayout->m_blocks.IsEmpty() || GeneratedLayout->m_blocks[0].m_id != -1);


		// 
		Ptr<const Layout> Layout = GeneratedLayout;
		currentLayoutMesh->AddLayout(Layout);

		// Create the layout block vertex buffer
		uint16* LayoutData = nullptr;
		{
			const int32 LayoutBufferIndex = currentLayoutMesh->GetVertexBuffers().GetBufferCount();
			currentLayoutMesh->GetVertexBuffers().SetBufferCount(LayoutBufferIndex + 1);

			// TODO
			check(Layout->GetBlockCount() < MAX_uint16);
			const MESH_BUFFER_SEMANTIC semantic = MBS_LAYOUTBLOCK;
			const int32 semanticIndex = int32(currentLayoutChannel);
			const MESH_BUFFER_FORMAT format = MBF_UINT16;
			const int32 components = 1;
			const int32 offset = 0;
			currentLayoutMesh->GetVertexBuffers().SetBuffer
			(
				LayoutBufferIndex,
				sizeof(uint16),
				1,
				&semantic, &semanticIndex,
				&format, &components,
				&offset
			);
			LayoutData = (uint16*)currentLayoutMesh->GetVertexBuffers().GetBufferData(LayoutBufferIndex);
		}

		const int32 NumVertices = currentLayoutMesh->GetVertexCount();
		const int32 NumBlocks = Layout->GetBlockCount();

		// Clear block data
		const uint16 NullBlockId = MAX_uint16 - 1;
		for (int32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
		{
			*(LayoutData + VertexIndex) = NullBlockId;
		}

		// Find block ids for each block in the grid
		const FIntPoint Grid = Layout->GetGridSize();
		TArray<int32> GridBlockBlockId;
		GridBlockBlockId.Init(MAX_uint16, Grid.X * Grid.Y);

		// 
		TArray<uint16> BlockIds;
		TArray<box<FVector2f>> BlockRects;

		BlockIds.SetNumUninitialized(NumBlocks);
		BlockRects.SetNumUninitialized(NumBlocks);

		// Create an array of block index per cell
		TArray<int32> OverlappingBlocks;
		for (int32 BlockIndex = 0; BlockIndex < NumBlocks; ++BlockIndex)
		{
			// Get the block id
			BlockIds[BlockIndex] = Layout->m_blocks[BlockIndex].m_id;

			// Get the block rect
			uint16 MinX, MinY, SizeX, SizeY;
			Layout->GetBlock(BlockIndex, &MinX, &MinY, &SizeX, &SizeY);

			box<FVector2f>& BlockRect = BlockRects[BlockIndex];
			BlockRect.min[0] = ((float)MinX) / (float)Grid.X;
			BlockRect.min[1] = ((float)MinY) / (float)Grid.Y;
			BlockRect.size[0] = ((float)SizeX) / (float)Grid.X;
			BlockRect.size[1] = ((float)SizeY) / (float)Grid.Y;

			// Create block index per cell array
			for (uint16 Y = MinY; Y < MinY + SizeY; ++Y)
			{
				const uint16 PositionY = Y * Grid.X;

				for (uint16 X = MinX; X < MinX + SizeX; ++X)
				{
					if (GridBlockBlockId[PositionY + X] == MAX_uint16)
					{
						GridBlockBlockId[PositionY + X] = BlockIndex;
					}
					else
					{
						OverlappingBlocks.AddUnique(BlockIndex);
					}
				}
			}
		}


		// Notify Overlapping layout blocks
		if (!OverlappingBlocks.IsEmpty())
		{
			FString Msg = FString::Printf(TEXT("Source mesh has %d layout block overlapping in LOD %d"),
				OverlappingBlocks.Num() + 1, m_currentParents.Last().m_lod
			);
			m_pErrorLog->GetPrivate()->Add(Msg, ELMT_WARNING, errorContext);
		}

		// Get the information about the texture coordinates channel
		int buffer = -1;
		int channel = -1;
		currentLayoutMesh->GetVertexBuffers().FindChannel(MBS_TEXCOORDS,
			(int)currentLayoutChannel,
			&buffer,
			&channel);
		check(buffer >= 0);
		check(channel >= 0);

		MESH_BUFFER_SEMANTIC semantic;
		int semanticIndex;
		MESH_BUFFER_FORMAT format;
		int components;
		int offset;
		currentLayoutMesh->GetVertexBuffers().GetChannel
		(buffer, channel, &semantic, &semanticIndex, &format, &components, &offset);
		check(semantic == MBS_TEXCOORDS);

		uint8* pData = currentLayoutMesh->GetVertexBuffers().GetBufferData(buffer);
		int elemSize = currentLayoutMesh->GetVertexBuffers().GetElementSize(buffer);
		int channelOffset = currentLayoutMesh->GetVertexBuffers().GetChannelOffset(buffer, channel);
		pData += channelOffset;


		// Temp copy of the UVs
		TArray<FVector2f> TempUVs;
		TempUVs.SetNumUninitialized(NumVertices);

		// Get a copy of the UVs as FVector2f to work with them. 
		{
			bool bNonNormalizedUVs = false;
			const bool bIsOverlayLayout = GeneratedLayout->GetLayoutPackingStrategy() == mu::EPackStrategy::OVERLAY_LAYOUT;

			const uint8* pVertices = pData;
			for (int32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
			{
				FVector2f& UV = TempUVs[VertexIndex];
				if (format == MBF_FLOAT32)
				{
					UV = *((FVector2f*)pVertices);
				}
				else if (format == MBF_FLOAT16)
				{
					const FFloat16* pUV = reinterpret_cast<const FFloat16*>(pVertices);
					UV = FVector2f(float(pUV[0]), float(pUV[1]));
				}

				// Check that UVs are normalized. If not, clamp the values and throw a warning.
				if (MeshOptions.bNormalizeUVs && !bIsOverlayLayout
					&& (UV[0] < 0.f || UV[0] > 1.f || UV[1] < 0.f || UV[1] > 1.f))
				{
					UV[0] = FMath::Clamp(UV[0], 0.f, 1.f);
					UV[1] = FMath::Clamp(UV[1], 0.f, 1.f);
					bNonNormalizedUVs = true;
				}

				pVertices += elemSize;
			}

			// Mutable does not support non-normalized UVs
			if (bNonNormalizedUVs && !bIsOverlayLayout)
			{
				FString Msg = FString::Printf(TEXT("Source mesh has non-normalized UVs in LOD %d"), m_currentParents.Last().m_lod );
				m_pErrorLog->GetPrivate()->Add(Msg, ELMT_WARNING, errorContext);
			}
		}


		const int32 NumTriangles = currentLayoutMesh->GetIndexCount() / 3;
		TArray<FTriangle> Triangles;

		// Vertices mapped to unique vertex index
		TArray<int32> CollapsedVertices;

		// Vertex to face map used to speed up connectivity building
		TMultiMap<int32, uint32> VertexToFaceMap;

		// Find Unique Vertices
		if (MeshOptions.bClampUVIslands)
		{
			VertexToFaceMap.Reserve(NumVertices);
			Triangles.SetNumUninitialized(NumTriangles);

			MeshCreateCollapsedVertexMap(currentLayoutMesh.get(), CollapsedVertices);
		}

		UntypedMeshBufferIteratorConst ItIndices(currentLayoutMesh->GetIndexBuffers(), MBS_VERTEXINDEX);
		TArray<int32> ConflictiveTriangles;

		const uint32 MaxGridX = MeshOptions.bNormalizeUVs ? MAX_uint32 : Grid.X - 1;
		const uint32 MaxGridY = MeshOptions.bNormalizeUVs ? MAX_uint32 : Grid.Y - 1;

		// TODO: We could skip this if there's only one block and it fits the entire grid
		for (int32 TriangleIndex = 0; TriangleIndex < NumTriangles; ++TriangleIndex)
		{
			uint32 Index0 = int(ItIndices.GetAsUINT32());
			++ItIndices;
			uint32 Index1 = int(ItIndices.GetAsUINT32());
			++ItIndices;
			uint32 Index2 = int(ItIndices.GetAsUINT32());
			++ItIndices;

			uint16 X, Y;

			uint16& BlockIndexV0 = *(LayoutData + Index0);
			if (BlockIndexV0 == NullBlockId)
			{
				X = (uint16)FMath::Min<uint32>(MaxGridX, FMath::Max<uint32>(0, (uint32)(Grid.X * TempUVs[Index0][0])));
				Y = (uint16)FMath::Min<uint32>(MaxGridY, FMath::Max<uint32>(0, (uint32)(Grid.Y * TempUVs[Index0][1])));
				BlockIndexV0 = (X < Grid.X && Y < Grid.Y) ? GridBlockBlockId[Y * Grid.X + X] : 0;
			}

			uint16& BlockIndexV1 = *(LayoutData + Index1);
			if (BlockIndexV1 == NullBlockId)
			{
				X = (uint16)FMath::Min<uint32>(MaxGridX, FMath::Max<uint32>(0, (uint32)(Grid.X * TempUVs[Index1][0])));
				Y = (uint16)FMath::Min<uint32>(MaxGridY, FMath::Max<uint32>(0, (uint32)(Grid.Y * TempUVs[Index1][1])));
				BlockIndexV1 = (X < Grid.X && Y < Grid.Y) ? GridBlockBlockId[Y * Grid.X + X] : 0;
			}

			uint16& BlockIndexV2 = *(LayoutData + Index2);
			if (BlockIndexV2 == NullBlockId)
			{
				X = (uint16)FMath::Min<uint32>(MaxGridX, FMath::Max<uint32>(0, (uint32)(Grid.X * TempUVs[Index2][0])));
				Y = (uint16)FMath::Min<uint32>(MaxGridY, FMath::Max<uint32>(0, (uint32)(Grid.Y * TempUVs[Index2][1])));
				BlockIndexV2 = (X < Grid.X && Y < Grid.Y) ? GridBlockBlockId[Y * Grid.X + X] : 0;
			}

			if (MeshOptions.bClampUVIslands)
			{
				if (BlockIndexV0 != BlockIndexV1 || BlockIndexV0 != BlockIndexV2)
				{
					ConflictiveTriangles.Add(TriangleIndex);
				}

				FTriangle& Triangle = Triangles[TriangleIndex];

				Triangle.Indices[0] = Index0;
				Triangle.Indices[1] = Index1;
				Triangle.Indices[2] = Index2;
				Triangle.CollapsedIndices[0] = CollapsedVertices[Index0];
				Triangle.CollapsedIndices[1] = CollapsedVertices[Index1];
				Triangle.CollapsedIndices[2] = CollapsedVertices[Index2];

				Triangle.BlockIndices[0] = BlockIndexV0;
				Triangle.BlockIndices[1] = BlockIndexV1;
				Triangle.BlockIndices[2] = BlockIndexV2;
				Triangle.bUVsFixed = false;

				VertexToFaceMap.Add(Triangle.CollapsedIndices[0], TriangleIndex);
				VertexToFaceMap.Add(Triangle.CollapsedIndices[1], TriangleIndex);
				VertexToFaceMap.Add(Triangle.CollapsedIndices[2], TriangleIndex);
			}
		}

		// Clamp UV islands to the predominant block of each island. Will only happen if bClampUVIslands is true.
		for (int32 ConflictiveTriangleIndex : ConflictiveTriangles)
		{
			FTriangle& Triangle = Triangles[ConflictiveTriangleIndex];

			// Skip the ones that have been fixed already
			if (Triangle.bUVsFixed)
			{
				continue;
			}

			// Find triangles from the same UV Island
			TArray<uint32> TriangleIndices;
			GetUVIsland(Triangles, ConflictiveTriangleIndex, TriangleIndices, TempUVs, VertexToFaceMap);

			// Get predominant BlockId != MAX_uint16
			TArray<uint32> NumVerticesPerBlock;
			NumVerticesPerBlock.SetNumZeroed(NumBlocks);

			for (int32 TriangleIndex : TriangleIndices)
			{
				FTriangle& OtherTriangle = Triangles[TriangleIndex];
				for (int32 VertexIndex = 0; VertexIndex < 3; ++VertexIndex)
				{
					const uint16& BlockIndex = OtherTriangle.BlockIndices[VertexIndex];
					if (BlockIndex != MAX_uint16)
					{
						NumVerticesPerBlock[BlockIndex]++;
					}
				}
			}

			uint16 BlockIndex = 0;
			uint32 CurrentMaxVertices = 0;
			for (int32 Index = 0; Index < NumBlocks; ++Index)
			{
				if (NumVerticesPerBlock[Index] > CurrentMaxVertices)
				{
					BlockIndex = Index;
					CurrentMaxVertices = NumVerticesPerBlock[Index];
				}
			}

			// Get the limits of the predominant block rect
			const Layout::FBlock& LayoutBlock = Layout->m_blocks[BlockIndex];

			const float SmallNumber = 0.000001;
			const float MinX = ((float)LayoutBlock.m_min.X) / (float)Grid.X + SmallNumber;
			const float MinY = ((float)LayoutBlock.m_min.Y) / (float)Grid.Y + SmallNumber;
			const float MaxX = (((float)LayoutBlock.m_size.X + LayoutBlock.m_min.X) / (float)Grid.X) - 2 * SmallNumber;
			const float MaxY = (((float)LayoutBlock.m_size.Y + LayoutBlock.m_min.Y) / (float)Grid.Y) - 2 * SmallNumber;

			// Iterate triangles and clamp the UVs
			FVector2f* TempUVsData = TempUVs.GetData();
			for (int32 TriangleIndex : TriangleIndices)
			{
				FTriangle& OtherTriangle = Triangles[TriangleIndex];

				for (int8 VertexIndex = 0; VertexIndex < 3; ++VertexIndex)
				{
					if (OtherTriangle.BlockIndices[VertexIndex] == BlockIndex)
					{
						continue;
					}

					OtherTriangle.BlockIndices[VertexIndex] = BlockIndex;

					// Clamp UVs
					const int32 UVIndex = OtherTriangle.Indices[VertexIndex];
					FVector2f& UV = TempUVs[UVIndex];
					UV[0] = FMath::Clamp(UV[0], MinX, MaxX);
					UV[1] = FMath::Clamp(UV[1], MinY, MaxY);
					*(LayoutData + UVIndex) = BlockIndex;
				}

				OtherTriangle.bUVsFixed = true;
			}
		}

		// Warn about vertices without a block id
		if (Layout->FirstLODToIgnoreWarnings == -1 || m_currentParents.Last().m_lod < Layout->FirstLODToIgnoreWarnings)
		{
			TArray<float> UnassignedUVs;
			UnassignedUVs.Reserve(NumVertices / 100);

			const FVector2f* UVs = TempUVs.GetData();
			for (int32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
			{
				if (LayoutData[VertexIndex] == MAX_uint16)
				{
					UnassignedUVs.Add((*(UVs + VertexIndex))[0]);
					UnassignedUVs.Add((*(UVs + VertexIndex))[1]);
				}
			}

			if (!UnassignedUVs.IsEmpty())
			{
				FString Msg = FString::Printf(TEXT("Source mesh has %d vertices not assigned to any layout block in LOD %d"), UnassignedUVs.Num(), m_currentParents.Last().m_lod);

				ErrorLogMessageAttachedDataView attachedDataView;
				attachedDataView.m_unassignedUVs = UnassignedUVs.GetData();
				attachedDataView.m_unassignedUVsSize = (size_t)UnassignedUVs.Num();

				m_pErrorLog->GetPrivate()->Add(Msg, attachedDataView, ELMT_WARNING, errorContext);
			}
		}

		// Format and copy UVs
		{
			uint8* pVertices = pData;
			FVector2f* UVs = TempUVs.GetData();

			for (int32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
			{
				FVector2f* UV = &TempUVs[VertexIndex];

				uint16& LayoutBlockIndex = LayoutData[VertexIndex];
				if (BlockIds.IsValidIndex(LayoutBlockIndex))
				{
					// Homogenize UVs
					*UV = BlockRects[LayoutBlockIndex].Homogenize(*UV);

					// Replace block index by the actual id of the block
					LayoutBlockIndex = BlockIds[LayoutBlockIndex];
				}
				else
				{
					// Map vertices without block to the first block.
					LayoutBlockIndex = 0;
				}

				// Copy UVs
				if (format == MBF_FLOAT32)
				{
					FVector2f* pUV = reinterpret_cast<FVector2f*>(pVertices);
					*pUV = *UV;
				}
				else if (format == MBF_FLOAT16)
				{
					FFloat16* pUV = reinterpret_cast<FFloat16*>(pVertices);
					pUV[0] = FFloat16((*UV)[0]);
					pUV[1] = FFloat16((*UV)[1]);
				}

				pVertices += elemSize;
			}
		}
	}
	

    //---------------------------------------------------------------------------------------------
    void CodeGenerator::GenerateMesh( const FMeshGenerationOptions& InOptions, FMeshGenerationResult& OutResult, const NodeMeshPtrConst& InUntypedNode)
    {
        if (!InUntypedNode)
        {
            OutResult = FMeshGenerationResult();
            return;
        }

        // See if it was already generated
		FGeneratedMeshCacheKey Key;
		Key.Node = InUntypedNode;
		Key.Options = InOptions;
        GeneratedMeshMap::ValueType* it = m_generatedMeshes.Find(Key);
        if ( it )
        {
			OutResult = *it;
            return;
        }

		const NodeMesh* Node = InUntypedNode.get();

        // Generate for each different type of node
		switch (Node->GetMeshNodeType())
		{
		case NodeMesh::EType::Constant: GenerateMesh_Constant(InOptions, OutResult, static_cast<const NodeMeshConstant*>(Node)); break;
		case NodeMesh::EType::Format: GenerateMesh_Format(InOptions, OutResult, static_cast<const NodeMeshFormat*>(Node)); break;
		case NodeMesh::EType::Morph: GenerateMesh_Morph(InOptions, OutResult, static_cast<const NodeMeshMorph*>(Node)); break;
		case NodeMesh::EType::MakeMorph: GenerateMesh_MakeMorph(InOptions, OutResult, static_cast<const NodeMeshMakeMorph*>(Node)); break;
		case NodeMesh::EType::Fragment: GenerateMesh_Fragment(InOptions, OutResult, static_cast<const NodeMeshFragment*>(Node)); break;
		case NodeMesh::EType::Interpolate: GenerateMesh_Interpolate(InOptions, OutResult, static_cast<const NodeMeshInterpolate*>(Node)); break;
		case NodeMesh::EType::Switch: GenerateMesh_Switch(InOptions, OutResult, static_cast<const NodeMeshSwitch*>(Node)); break;
		case NodeMesh::EType::Transform: GenerateMesh_Transform(InOptions, OutResult, static_cast<const NodeMeshTransform*>(Node)); break;
		case NodeMesh::EType::ClipMorphPlane: GenerateMesh_ClipMorphPlane(InOptions, OutResult, static_cast<const NodeMeshClipMorphPlane*>(Node)); break;
		case NodeMesh::EType::ClipWithMesh: GenerateMesh_ClipWithMesh(InOptions, OutResult, static_cast<const NodeMeshClipWithMesh*>(Node)); break;
		case NodeMesh::EType::ApplyPose: GenerateMesh_ApplyPose(InOptions, OutResult, static_cast<const NodeMeshApplyPose*>(Node)); break;
		case NodeMesh::EType::Variation: GenerateMesh_Variation(InOptions, OutResult, static_cast<const NodeMeshVariation*>(Node)); break;
		case NodeMesh::EType::Table: GenerateMesh_Table(InOptions, OutResult, static_cast<const NodeMeshTable*>(Node)); break;
		case NodeMesh::EType::GeometryOperation: GenerateMesh_GeometryOperation(InOptions, OutResult, static_cast<const NodeMeshGeometryOperation*>(Node)); break;
		case NodeMesh::EType::Reshape: GenerateMesh_Reshape(InOptions, OutResult, static_cast<const NodeMeshReshape*>(Node)); break;
		case NodeMesh::EType::ClipDeform: GenerateMesh_ClipDeform(InOptions, OutResult, static_cast<const NodeMeshClipDeform*>(Node)); break;
		case NodeMesh::EType::None: check(false);
		}

        // Cache the result
        m_generatedMeshes.Add( Key, OutResult);
    }


    //---------------------------------------------------------------------------------------------
    void CodeGenerator::GenerateMesh_Morph(
		const FMeshGenerationOptions& InOptions, 
		FMeshGenerationResult& OutResult, 
		const NodeMeshMorph* InMorphNode 
	)
    {
        NodeMeshMorph::Private& node = *InMorphNode->GetPrivate();

        Ptr<ASTOpMeshMorph> OpMorph = new ASTOpMeshMorph();

        // Factor
        if ( node.Factor )
        {
            OpMorph->Factor = Generate( node.Factor.get(), InOptions );
        }
        else
        {
            // This argument is required
            OpMorph->Factor = GenerateMissingScalarCode(TEXT("Morph factor"), 0.5f, node.m_errorContext );
        }

        // Base
        FMeshGenerationResult BaseResult;
        if ( node.Base )
        {
			FMeshGenerationOptions BaseOptions = InOptions;
			BaseOptions.bUniqueVertexIDs = true;
            GenerateMesh(BaseOptions,BaseResult, node.Base );
            OpMorph->Base = BaseResult.meshOp;
        }
        else
        {
            // This argument is required
            m_pErrorLog->GetPrivate()->Add( "Mesh morph base node is not set.",
                                            ELMT_ERROR, node.m_errorContext );
        }        

		if (node.Morph)
        {
            FMeshGenerationResult TargetResult;
			FMeshGenerationOptions TargetOptions = InOptions;
			TargetOptions.bUniqueVertexIDs = false;
			TargetOptions.bLayouts = false;
			// We need to override the layouts with the layouts that were generated for the base to make
			// sure that we get the correct mesh when generating the target
			TargetOptions.OverrideLayouts = BaseResult.GeneratedLayouts;
			TargetOptions.ActiveTags.Empty();
            GenerateMesh(TargetOptions, TargetResult, node.Morph);

            // TODO: Make sure that the target is a mesh with the morph format
            Ptr<ASTOp> target = TargetResult.meshOp;

            OpMorph->Target = target;
        }
 
        const bool bReshapeEnabled = node.bReshapeSkeleton || node.bReshapePhysicsVolumes;
        
        Ptr<ASTOpMeshMorphReshape> OpMorphReshape;
        if ( bReshapeEnabled )
        {
		    Ptr<ASTOpMeshBindShape> OpBind = new ASTOpMeshBindShape();
		    Ptr<ASTOpMeshApplyShape> OpApply = new ASTOpMeshApplyShape();

			// Setting bReshapeVertices to false the bind op will remove all mesh members except 
			// PhysicsBodies and the Skeleton.
            OpBind->bReshapeVertices = false;
            OpBind->bApplyLaplacian = false;
		    OpBind->bReshapeSkeleton = node.bReshapeSkeleton;
		    OpBind->BonesToDeform = node.BonesToDeform;
    	    OpBind->bReshapePhysicsVolumes = node.bReshapePhysicsVolumes; 
			OpBind->PhysicsToDeform = node.PhysicsToDeform;
			OpBind->BindingMethod = static_cast<uint32>(EShapeBindingMethod::ReshapeClosestProject);
            
			OpBind->Mesh = BaseResult.meshOp;
            OpBind->Shape = BaseResult.meshOp;
           
			OpApply->bReshapeVertices = OpBind->bReshapeVertices;
		    OpApply->bReshapeSkeleton = OpBind->bReshapeSkeleton;
		    OpApply->bReshapePhysicsVolumes = OpBind->bReshapePhysicsVolumes;

			OpApply->Mesh = OpBind;
            OpApply->Shape = OpMorph;

            OpMorphReshape = new ASTOpMeshMorphReshape();
            OpMorphReshape->Morph = OpMorph;
            OpMorphReshape->Reshape = OpApply;
        }

 		if (OpMorphReshape)
		{
			OutResult.meshOp = OpMorphReshape;
		}
		else
		{
			OutResult.meshOp = OpMorph;
		}

        OutResult.baseMeshOp = BaseResult.baseMeshOp;
		OutResult.GeneratedLayouts = BaseResult.GeneratedLayouts;
}


    //---------------------------------------------------------------------------------------------
    void CodeGenerator::GenerateMesh_MakeMorph(const FMeshGenerationOptions& InOptions, FMeshGenerationResult& OutResult, const NodeMeshMakeMorph* InMakeMorphNode )
    {
        NodeMeshMakeMorph::Private& node = *InMakeMorphNode->GetPrivate();

        Ptr<ASTOpMeshDifference> op = new ASTOpMeshDifference();

        // \todo Texcoords are broken?
        op->bIgnoreTextureCoords = true;
	
		// UE only has position and normal morph data, optimize for this case if indicated. 
		if (node.bOnlyPositionAndNormal)
		{
			op->Channels = { {static_cast<uint8>(MBS_POSITION), 0}, {static_cast<uint8>(MBS_NORMAL), 0} };
		}

        // Base
        FMeshGenerationResult BaseResult;
        if ( node.m_pBase )
        {
			FMeshGenerationOptions BaseOptions = InOptions;
			BaseOptions.bUniqueVertexIDs = true;
			BaseOptions.bLayouts = false;
			GenerateMesh(BaseOptions, BaseResult, node.m_pBase );

            op->Base = BaseResult.meshOp;
        }
        else
        {
            // This argument is required
            m_pErrorLog->GetPrivate()->Add( "Mesh make morph base node is not set.",
                                            ELMT_ERROR, node.m_errorContext );
        }

        // Target
		if ( node.m_pTarget )
        {
			FMeshGenerationOptions TargetOptions = InOptions;
			TargetOptions.bUniqueVertexIDs = false;
			TargetOptions.bLayouts = false;
			TargetOptions.OverrideLayouts.Empty();
			TargetOptions.ActiveTags.Empty();
			FMeshGenerationResult TargetResult;
            GenerateMesh( TargetOptions, TargetResult, node.m_pTarget );

            op->Target = TargetResult.meshOp;
        }
        else
        {
            // This argument is required
            m_pErrorLog->GetPrivate()->Add( "Mesh make morph target node is not set.",
                                            ELMT_ERROR, node.m_errorContext );
        }

        OutResult.meshOp = op;
        OutResult.baseMeshOp = BaseResult.baseMeshOp;
		OutResult.GeneratedLayouts = BaseResult.GeneratedLayouts;
	}

    //---------------------------------------------------------------------------------------------
    void CodeGenerator::GenerateMesh_Fragment(const FMeshGenerationOptions& InOptions, FMeshGenerationResult& OutResult,
		const NodeMeshFragment* fragment )
    {
        NodeMeshFragment::Private& node = *fragment->GetPrivate();

        FMeshGenerationResult BaseResult;
        if ( node.m_pMesh )
        {
			FMeshGenerationOptions BaseOptions = InOptions;
			if (node.m_fragmentType == NodeMeshFragment::FT_LAYOUT_BLOCKS)
			{
				BaseOptions.bLayouts = true;
			}
            GenerateMesh( BaseOptions, BaseResult, node.m_pMesh );

            if ( node.m_fragmentType==NodeMeshFragment::FT_LAYOUT_BLOCKS )
            {
                Ptr<ASTOpMeshExtractLayoutBlocks> op = new ASTOpMeshExtractLayoutBlocks();
                OutResult.meshOp = op;

                op->Source = BaseResult.meshOp;

                if (BaseResult.GeneratedLayouts.Num()>node.m_layoutOrGroup )
                {
                    const Layout* pLayout = BaseResult.GeneratedLayouts[node.m_layoutOrGroup].get();
                    op->Layout = (uint16)node.m_layoutOrGroup;

                    for ( int32 i=0; i<node.m_blocks.Num(); ++i )
                    {
                        if (node.m_blocks[i]>=0 && node.m_blocks[i]<pLayout->m_blocks.Num() )
                        {
                            int bid = pLayout->m_blocks[ node.m_blocks[i] ].m_id;
                            op->Blocks.Add(bid);
                        }
                        else
                        {
                            m_pErrorLog->GetPrivate()->Add( "Internal layout block index error.",
                                                            ELMT_ERROR, node.m_errorContext );
                        }
                    }
                }
                else
                {
                    // This argument is required
                    m_pErrorLog->GetPrivate()->Add( "Missing layout in mesh fragment source.",
                                                    ELMT_ERROR, node.m_errorContext );
                }
            }

            else
            {
				check(false);
            }

        }
        else
        {
            // This argument is required
            m_pErrorLog->GetPrivate()->Add( "Mesh fragment source is not set.",
                                            ELMT_ERROR, node.m_errorContext );
        }

        OutResult.baseMeshOp = BaseResult.baseMeshOp;
		OutResult.GeneratedLayouts = BaseResult.GeneratedLayouts;
	}


    //---------------------------------------------------------------------------------------------
    void CodeGenerator::GenerateMesh_Interpolate(const FMeshGenerationOptions& InOptions, FMeshGenerationResult& OutResult,
		const NodeMeshInterpolate* interpolate )
    {
        NodeMeshInterpolate::Private& node = *interpolate->GetPrivate();

        // Generate the code
        Ptr<ASTOpFixed> op = new ASTOpFixed();
        op->op.type = OP_TYPE::ME_INTERPOLATE;
        OutResult.meshOp = op;

        // Factor
        if ( Node* pFactor = node.m_pFactor.get() )
        {
            op->SetChild( op->op.args.MeshInterpolate.factor, Generate( pFactor, InOptions ) );
        }
        else
        {
            // This argument is required
            op->SetChild( op->op.args.MeshInterpolate.factor,
                          GenerateMissingScalarCode(TEXT("Interpolation factor"), 0.5f, node.m_errorContext ) );
        }

        //
        Ptr<ASTOp> base = 0;
        int count = 0;
        for ( int32 t=0
            ; t<node.m_targets.Num() && t<MUTABLE_OP_MAX_INTERPOLATE_COUNT-1
            ; ++t )
        {
            if ( NodeMesh* pA = node.m_targets[t].get() )
            {
				FMeshGenerationOptions TargetOptions = InOptions;
				TargetOptions.bUniqueVertexIDs = true;
				TargetOptions.OverrideLayouts.Empty();

                FMeshGenerationResult TargetResult;
                GenerateMesh( TargetOptions, TargetResult, pA );


                // The first target is the base
                if (count==0)
                {
                    base = TargetResult.meshOp;
                    op->SetChild( op->op.args.MeshInterpolate.base, TargetResult.meshOp );

                    OutResult.baseMeshOp = TargetResult.baseMeshOp;
					OutResult.GeneratedLayouts = TargetResult.GeneratedLayouts;
				}
                else
                {
                    Ptr<ASTOpMeshDifference> dop = new ASTOpMeshDifference();
                    dop->Base = base;
                    dop->Target = TargetResult.meshOp;

                    // \todo Texcoords are broken?
                    dop->bIgnoreTextureCoords = true;

                    for ( size_t c=0; c<node.m_channels.Num(); ++c)
                    {
                        check( node.m_channels[c].semantic < 256 );
						check(node.m_channels[c].semanticIndex < 256);
						
						ASTOpMeshDifference::FChannel Channel;
						Channel.Semantic = uint8(node.m_channels[c].semantic);
						Channel.SemanticIndex = uint8(node.m_channels[c].semanticIndex);
						dop->Channels.Add(Channel);
                    }

                    op->SetChild( op->op.args.MeshInterpolate.targets[count-1], dop );
                }
                count++;
            }
        }

        // At least one mesh is required
        if (!count)
        {
            // TODO
            //op.args.MeshInterpolate.target[0] = GenerateMissingImageCode( "First mesh", IF_RGB_UBYTE );
            m_pErrorLog->GetPrivate()->Add
                ( "Mesh interpolation: at least the first mesh is required.",
                  ELMT_ERROR, node.m_errorContext );
        }
    }


    //---------------------------------------------------------------------------------------------
    void CodeGenerator::GenerateMesh_Switch(const FMeshGenerationOptions& InOptions, FMeshGenerationResult& OutResult, const NodeMeshSwitch* sw )
    {
        NodeMeshSwitch::Private& node = *sw->GetPrivate();

        if (node.m_options.Num() == 0)
        {
            // No options in the switch!
            // TODO
            OutResult = FMeshGenerationResult();
			return;
        }

        Ptr<ASTOpSwitch> op = new ASTOpSwitch();
        op->type = OP_TYPE::ME_SWITCH;

        // Factor
        if ( node.m_pParameter )
        {
            op->variable = Generate( node.m_pParameter.get(), InOptions);
        }
        else
        {
            // This argument is required
            op->variable = GenerateMissingScalarCode(TEXT("Switch variable"), 0.0f, node.m_errorContext );
        }

        // Options
		bool bFirstValidConnectionFound = false;
        for ( int32 t=0; t< node.m_options.Num(); ++t )
        {
			FMeshGenerationOptions TargetOptions = InOptions;

            if ( node.m_options[t] )
            {
				// Take the layouts from the first non-null connection.
				// \TODO: Take them from the first connection that actually returns layouts?
				if (bFirstValidConnectionFound)
				{
					TargetOptions.OverrideLayouts = OutResult.GeneratedLayouts;
				}
				
				FMeshGenerationResult BranchResults;
                GenerateMesh(TargetOptions, BranchResults, node.m_options[t] );

                Ptr<ASTOp> branch = BranchResults.meshOp;
                op->cases.Emplace((int16)t,op,branch);

				if (!bFirstValidConnectionFound)
				{
					bFirstValidConnectionFound = true;
                    OutResult = BranchResults;
                }
            }
        }

        OutResult.meshOp = op;
    }


	//---------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateMesh_Table(const FMeshGenerationOptions& InOptions, FMeshGenerationResult& OutResult, const NodeMeshTable* TableNode)
	{
		//
		FMeshGenerationResult NewResult = OutResult;
		int t = 0;
		bool bFirstRowGenerated = false;

		Ptr<ASTOp> Op = GenerateTableSwitch<NodeMeshTable::Private, ETableColumnType::Mesh, OP_TYPE::ME_SWITCH>(*TableNode->GetPrivate(),
			[this, &NewResult, &bFirstRowGenerated, &InOptions] (const NodeMeshTable::Private& node, int colIndex, int row, ErrorLog* pErrorLog)
			{
				mu::Ptr<mu::Mesh> pMesh = node.Table->GetPrivate()->Rows[row].Values[colIndex].Mesh;
				FMeshGenerationResult BranchResults;

				if (pMesh)
				{
					NodeMeshConstantPtr pCell = new NodeMeshConstant();
					pCell->SetValue(pMesh);

					// TODO Take into account layout strategy
					int numLayouts = node.Layouts.Num();
					pCell->SetLayoutCount(numLayouts);
					for (int i = 0; i < numLayouts; ++i)
					{
						pCell->SetLayout(i, node.Layouts[i]);
					}

					FMeshGenerationOptions TargetOptions = InOptions;

					if (bFirstRowGenerated)
					{
						TargetOptions.OverrideLayouts = NewResult.GeneratedLayouts;
					}

					TargetOptions.OverrideContext = node.Table->GetPrivate()->Rows[row].Values[colIndex].ErrorContext;

					GenerateMesh(TargetOptions, BranchResults, pCell);

					if (!bFirstRowGenerated)
					{
						NewResult = BranchResults;
						bFirstRowGenerated = true;
					}
				}

				return BranchResults.meshOp;
			});

		NewResult.meshOp = Op;

		OutResult = NewResult;
	}


    //---------------------------------------------------------------------------------------------
    void CodeGenerator::GenerateMesh_Variation(const FMeshGenerationOptions& InOptions, FMeshGenerationResult& OutResult,
		const NodeMeshVariation* va )
    {
        NodeMeshVariation::Private& node = *va->GetPrivate();

        FMeshGenerationResult currentResult;
        Ptr<ASTOp> currentMeshOp;

        bool firstOptionProcessed = false;

        // Default case
        if ( node.m_defaultMesh )
        {
            FMeshGenerationResult BranchResults;
			FMeshGenerationOptions DefaultOptions = InOptions;

			GenerateMesh(DefaultOptions, BranchResults, node.m_defaultMesh );
            currentMeshOp = BranchResults.meshOp;
            currentResult = BranchResults;
            firstOptionProcessed = true;
        }

        // Process variations in reverse order, since conditionals are built bottom-up.
        for ( int32 t = node.m_variations.Num()-1; t >= 0; --t )
        {
            int tagIndex = -1;
            const FString& tag = node.m_variations[t].m_tag;
            for ( int i = 0; i < m_firstPass.m_tags.Num(); ++i )
            {
                if ( m_firstPass.m_tags[i].tag==tag)
                {
                    tagIndex = i;
                }
            }

            if ( tagIndex < 0 )
            {
                m_pErrorLog->GetPrivate()->Add( 
					FString::Printf(TEXT("Unknown tag found in mesh variation [%s]."), *tag),
					ELMT_WARNING,
					node.m_errorContext,
					ELMSB_UNKNOWN_TAG
				);
                continue;
            }

            Ptr<ASTOp> variationMeshOp;
            if ( node.m_variations[t].m_mesh )
            {
				FMeshGenerationOptions VariationOptions = InOptions;

                if (firstOptionProcessed)
                {
					VariationOptions.OverrideLayouts = currentResult.GeneratedLayouts;
                }
         
                FMeshGenerationResult BranchResults;
				GenerateMesh(VariationOptions, BranchResults, node.m_variations[t].m_mesh );

                variationMeshOp = BranchResults.meshOp;

                if ( !firstOptionProcessed )
                {
                    firstOptionProcessed = true;                   
                    currentResult = BranchResults;
                }
            }

            Ptr<ASTOpConditional> conditional = new ASTOpConditional;
            conditional->type = OP_TYPE::ME_CONDITIONAL;
            conditional->no = currentMeshOp;
            conditional->yes = variationMeshOp;            
            conditional->condition = m_firstPass.m_tags[tagIndex].genericCondition;

            currentMeshOp = conditional;
        }

        OutResult = currentResult;
        OutResult.meshOp = currentMeshOp;
    }


    //---------------------------------------------------------------------------------------------
    void CodeGenerator::GenerateMesh_Constant(const FMeshGenerationOptions& InOptions, FMeshGenerationResult& OutResult, const NodeMeshConstant* constant )
    {
        NodeMeshConstant::Private& node = *constant->GetPrivate();

        Ptr<ASTOpConstantResource> op = new ASTOpConstantResource();
        op->Type = OP_TYPE::ME_CONSTANT;
		OutResult.baseMeshOp = op;
		OutResult.meshOp = op;
		OutResult.GeneratedLayouts.Empty();

		bool bIsOverridingLayouts = !InOptions.OverrideLayouts.IsEmpty();

        MeshPtr pMesh = node.m_pValue.get();
		if (!pMesh)
		{
			// This data is required
			MeshPtr pTempMesh = new Mesh();
			op->SetValue(pTempMesh, m_compilerOptions->OptimisationOptions.DiskCacheContext);
			m_constantMeshes.Add(pTempMesh);

			// Log an error message
			m_pErrorLog->GetPrivate()->Add("Constant mesh not set.", ELMT_WARNING, node.m_errorContext);

			return;
		}

		// Separate the tags from the mesh
		TArray<FString> Tags = pMesh->m_tags;
		if (Tags.Num())
		{
			Ptr<Mesh> TaglessMesh = CloneOrTakeOver(pMesh.get());
			TaglessMesh->m_tags.SetNum(0, EAllowShrinking::No);
			pMesh = TaglessMesh;
		}

		// Find out if we can (or have to) reuse a mesh that we have already generated.
		MeshPtrConst DuplicateOf;
		for (int32 i = 0; i < m_constantMeshes.Num(); ++i)
		{
			MeshPtrConst Candidate = m_constantMeshes[i];
			
			bool bCompareLayouts = InOptions.bLayouts && !bIsOverridingLayouts;

			if (Candidate->IsSimilar(*pMesh, bCompareLayouts))
			{
				// If it is similar, and we need unique vertex IDs, check that it also has them. This was skipped in the IsSimilar.
				if (InOptions.bUniqueVertexIDs)
				{
					int32 FoundBuffer = -1;
					int32 FoundChannel = -1;
					Candidate->GetVertexBuffers().FindChannel(MBS_VERTEXINDEX, 0, &FoundBuffer, &FoundChannel);
					bool bHasUniqueVertexIDs = FoundBuffer >= 0 && FoundChannel >= 0;
					if (!bHasUniqueVertexIDs)
					{
						continue;
					}
				}

				// If it is similar and we are overriding the layouts, we must compare the layouts of the candidate with the ones
				// we are using to override.
				if (bIsOverridingLayouts)
				{
					if (Candidate->GetLayoutCount() != InOptions.OverrideLayouts.Num())
					{
						continue;
					}

					bool bLayoutsAreEqual = true;
					for (int32 l = 0; l < Candidate->GetLayoutCount(); ++l)
					{
						bLayoutsAreEqual = (*Candidate->GetLayout(l) == *InOptions.OverrideLayouts[l]);
						if ( !bLayoutsAreEqual )
						{
							break;
						}
					}

					if (!bLayoutsAreEqual)
					{
						continue;
					}
				}

				DuplicateOf = Candidate;
				break;
			}
		}

		Ptr<const Mesh> FinalMesh;
		if (DuplicateOf)
		{
			// Make sure the source layouts of the mesh are mapped to the layouts of the duplicated mesh.
			if (InOptions.bLayouts)
			{
				if (bIsOverridingLayouts)
				{
					for (int32 l = 0; l < DuplicateOf->GetLayoutCount(); ++l)
					{
						const Layout* OverridingLayout = InOptions.OverrideLayouts[l].get();
						OutResult.GeneratedLayouts.Add(OverridingLayout);
					}
				}
				else
				{
					for (int32 l = 0; l < DuplicateOf->GetLayoutCount(); ++l)
					{
						const Layout* DuplicatedLayout = DuplicateOf->GetLayout(l);
						OutResult.GeneratedLayouts.Add(DuplicatedLayout);
					}
				}
			}

			FinalMesh = DuplicateOf;
		}
		else
		{
			// We need to clone the mesh in the node because we will modify it.
			Ptr<Mesh> pCloned = pMesh->Clone();
			pCloned->EnsureSurfaceData();

			if (InOptions.bLayouts)
			{
				if (!bIsOverridingLayouts)
				{
					// Apply whatever transform is necessary for every layout
					for (int32 LayoutIndex = 0; LayoutIndex < node.m_layouts.Num(); ++LayoutIndex)
					{
						Ptr<NodeLayout> pLayoutNode = node.m_layouts[LayoutIndex];
						if (!pLayoutNode)
						{
							continue;
						}

						// TODO: In a cleanup of the design of the layouts, we should remove this cast.
						check(pLayoutNode->GetType()==NodeLayoutBlocks::GetStaticType() );
						const NodeLayoutBlocks* TypedNode = static_cast<const NodeLayoutBlocks*>(pLayoutNode.get());

						Ptr<const Layout> SourceLayout = TypedNode->GetPrivate()->m_pLayout;
						Ptr<const Layout> GeneratedLayout = AddLayout( SourceLayout );
						const void* Context = InOptions.OverrideContext.Get(node.m_errorContext);
						PrepareForLayout(GeneratedLayout, pCloned, LayoutIndex, Context, InOptions);

						OutResult.GeneratedLayouts.Add(GeneratedLayout);
					}
				}
				else
				{
					// We need to apply the transform of the layouts used to override
					for (int32 LayoutIndex = 0; LayoutIndex < InOptions.OverrideLayouts.Num(); ++LayoutIndex)
					{
						Ptr<const Layout> GeneratedLayout = InOptions.OverrideLayouts[LayoutIndex];
						const void* Context = InOptions.OverrideContext.Get(node.m_errorContext);
						PrepareForLayout(GeneratedLayout, pCloned, LayoutIndex, Context, InOptions);

						OutResult.GeneratedLayouts.Add(GeneratedLayout);
					}
				}
			}

			if (InOptions.bUniqueVertexIDs)
			{
				// Enumerate the vertices uniquely unless they already have indices
				int buf = -1;
				int chan = -1;
				pCloned->GetVertexBuffers().FindChannel(MBS_VERTEXINDEX, 0, &buf, &chan);
				bool hasVertexIndices = (buf >= 0 && chan >= 0);
				if (!hasVertexIndices)
				{
					int newBuffer = pCloned->GetVertexBuffers().GetBufferCount();
					pCloned->GetVertexBuffers().SetBufferCount(newBuffer + 1);
					MESH_BUFFER_SEMANTIC semantic = MBS_VERTEXINDEX;
					int semanticIndex = 0;
					MESH_BUFFER_FORMAT format = MBF_UINT32;
					int components = 1;
					int offset = 0;
					pCloned->GetVertexBuffers().SetBuffer
					(
						newBuffer,
						sizeof(uint32),
						1,
						&semantic, &semanticIndex,
						&format, &components,
						&offset
					);
					uint32* pIdData = (uint32*)pCloned->GetVertexBuffers().GetBufferData(newBuffer);
					for (int i = 0; i < pMesh->GetVertexCount(); ++i)
					{
						check(m_freeVertexIndex < TNumericLimits<uint32>::Max());

						(*pIdData++) = m_freeVertexIndex++;
						check(m_freeVertexIndex < TNumericLimits<uint32>::Max());
					}
				}
			}

			// Add the constant data
			m_constantMeshes.Add(pCloned);
			FinalMesh = pCloned;
		}

		op->SetValue(FinalMesh, m_compilerOptions->OptimisationOptions.DiskCacheContext);

		Ptr<ASTOp> LastMeshOp = op;

		// Add the tags operation
		if (Tags.Num())
		{
			Ptr<ASTOpMeshAddTags> AddTagsOp = new ASTOpMeshAddTags;
			AddTagsOp->Source = LastMeshOp;
			AddTagsOp->Tags = Tags;
			LastMeshOp = AddTagsOp;
		}

		// Apply the modifier for the pre-normal operations stage.
		bool bModifiersForBeforeOperations = true;
		OutResult.meshOp = ApplyMeshModifiers(InOptions, LastMeshOp, bModifiersForBeforeOperations, node.m_errorContext);
    }


    //---------------------------------------------------------------------------------------------
    void CodeGenerator::GenerateMesh_Format(const FMeshGenerationOptions& InOptions, FMeshGenerationResult& OutResult,
		const NodeMeshFormat* format )
    {
        NodeMeshFormat::Private& node = *format->GetPrivate();

        if ( node.m_pSource )
        {
			FMeshGenerationOptions Options = InOptions;

			FMeshGenerationResult baseResult;
			GenerateMesh(Options,baseResult, node.m_pSource);
            Ptr<ASTOpMeshFormat> op = new ASTOpMeshFormat();
            op->Source = baseResult.meshOp;
            op->Buffers = 0;

            MeshPtr pFormatMesh = new Mesh();

            if (node.m_VertexBuffers.GetBufferCount())
            {
                op->Buffers |= OP::MeshFormatArgs::BT_VERTEX;
                pFormatMesh->m_VertexBuffers = node.m_VertexBuffers;
            }

            if (node.m_IndexBuffers.GetBufferCount())
            {
				op->Buffers |= OP::MeshFormatArgs::BT_INDEX;
                pFormatMesh->m_IndexBuffers = node.m_IndexBuffers;
            }

            if (node.m_FaceBuffers.GetBufferCount())
            {
                op->Buffers |= OP::MeshFormatArgs::BT_FACE;
                pFormatMesh->m_FaceBuffers = node.m_FaceBuffers;
            }

            Ptr<ASTOpConstantResource> cop = new ASTOpConstantResource();
            cop->Type = OP_TYPE::ME_CONSTANT;
            cop->SetValue( pFormatMesh, m_compilerOptions->OptimisationOptions.DiskCacheContext );
            op->Format = cop;

            m_constantMeshes.Add(pFormatMesh);

            OutResult.meshOp = op;
            OutResult.baseMeshOp = baseResult.baseMeshOp;
			OutResult.GeneratedLayouts = baseResult.GeneratedLayouts;
		}
        else
        {
            // Put something there
            GenerateMesh(InOptions, OutResult, new NodeMeshConstant() );
        }
    }


    //---------------------------------------------------------------------------------------------
    void CodeGenerator::GenerateMesh_Transform(const FMeshGenerationOptions& InOptions, FMeshGenerationResult& OutResult,
                                                const NodeMeshTransform* trans )
    {
        const auto& node = *trans->GetPrivate();

        Ptr<ASTOpMeshTransform> op = new ASTOpMeshTransform();

        // Base
        if (node.Source)
        {
            GenerateMesh(InOptions, OutResult, node.Source);
            op->source = OutResult.meshOp;
        }
        else
        {
            // This argument is required
            m_pErrorLog->GetPrivate()->Add("Mesh transform base node is not set.",
                ELMT_ERROR, node.m_errorContext);
        }

        op->matrix = node.Transform;

        OutResult.meshOp = op;
    }


    //---------------------------------------------------------------------------------------------
    void CodeGenerator::GenerateMesh_ClipMorphPlane(const FMeshGenerationOptions& InOptions, FMeshGenerationResult& OutResult,
                                                     const NodeMeshClipMorphPlane* clip )
    {
        const auto& node = *clip->GetPrivate();

        Ptr<ASTOpMeshClipMorphPlane> op = new ASTOpMeshClipMorphPlane();

        // Base
        if (node.m_pSource)
        {
			FMeshGenerationOptions BaseOptions = InOptions;
			BaseOptions.bUniqueVertexIDs = true;
            GenerateMesh(BaseOptions, OutResult, node.m_pSource);
            op->source = OutResult.meshOp;
        }
        else
        {
            // This argument is required
            m_pErrorLog->GetPrivate()->Add("Mesh clip-morph-plane source node is not set.",
                ELMT_ERROR, node.m_errorContext);
        }

        // Morph to an ellipse
        {
            op->morphShape.type = (uint8_t)FShape::Type::Ellipse;
            op->morphShape.position = node.m_origin;
            op->morphShape.up = node.m_normal;
            op->morphShape.size = FVector3f(node.m_radius1, node.m_radius2, node.m_rotation); // TODO: Move rotation to ellipse rotation reference base instead of passing it directly

                                                                                      // Generate a "side" vector.
                                                                                      // \todo: make generic and move to the vector class
            {
                // Generate vector perpendicular to normal for ellipse rotation reference base
				FVector3f aux_base(0.f, 1.f, 0.f);

                if (fabs(FVector3f::DotProduct(node.m_normal, aux_base)) > 0.95f)
                {
                    aux_base = FVector3f(0.f, 0.f, 1.f);
                }

                op->morphShape.side = FVector3f::CrossProduct(node.m_normal, aux_base);
            }
        }

        // Selection by shape
        if (node.m_vertexSelectionType== NodeMeshClipMorphPlane::Private::VS_SHAPE)
        {
            op->vertexSelectionType = OP::MeshClipMorphPlaneArgs::VS_SHAPE;
            op->selectionShape.type = (uint8_t)FShape::Type::AABox;
            op->selectionShape.position = node.m_selectionBoxOrigin;
            op->selectionShape.size = node.m_selectionBoxRadius;
        }
        else if (node.m_vertexSelectionType == NodeMeshClipMorphPlane::Private::VS_BONE_HIERARCHY)
        {
            // Selection by bone hierarchy?
            op->vertexSelectionType = OP::MeshClipMorphPlaneArgs::VS_BONE_HIERARCHY;
            op->vertexSelectionBone = node.m_vertexSelectionBone;
			op->vertexSelectionBoneMaxRadius = node.m_maxEffectRadius;
        }
        else
        {
            op->vertexSelectionType = OP::MeshClipMorphPlaneArgs::VS_NONE;
        }

        // Parameters
        op->dist = node.m_dist;
        op->factor = node.m_factor;

        OutResult.meshOp = op;
    }


    //---------------------------------------------------------------------------------------------
    void CodeGenerator::GenerateMesh_ClipWithMesh(const FMeshGenerationOptions& InOptions, FMeshGenerationResult& OutResult,
                                                   const NodeMeshClipWithMesh* clip )
    {
        const auto& node = *clip->GetPrivate();

        Ptr<ASTOpFixed> op = new ASTOpFixed();
        op->op.type = OP_TYPE::ME_CLIPWITHMESH;

        // Base
        if (node.m_pSource)
        {
			FMeshGenerationOptions BaseOptions = InOptions;
			BaseOptions.bUniqueVertexIDs = true;

            GenerateMesh(BaseOptions, OutResult, node.m_pSource );
            op->SetChild( op->op.args.MeshClipWithMesh.source, OutResult.meshOp );
        }
        else
        {
            // This argument is required
            m_pErrorLog->GetPrivate()->Add("Mesh clip-with-mesh source node is not set.",
                ELMT_ERROR, node.m_errorContext);
        }

        // Clipping mesh
        if (node.m_pClipMesh)
        {
			FMeshGenerationOptions ClipOptions = InOptions;
			ClipOptions.bUniqueVertexIDs = false;
			ClipOptions.bLayouts = false;
			ClipOptions.OverrideLayouts.Empty();
			ClipOptions.ActiveTags.Empty();

            FMeshGenerationResult clipResult;
            GenerateMesh(ClipOptions, clipResult, node.m_pClipMesh);
            op->SetChild( op->op.args.MeshClipWithMesh.clipMesh, clipResult.meshOp );
		}
        else
        {
            // This argument is required
            m_pErrorLog->GetPrivate()->Add("Mesh clip-with-mesh clipping mesh node is not set.",
                ELMT_ERROR, node.m_errorContext);
        }

        OutResult.meshOp = op;
    }

	//---------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateMesh_ClipDeform(const FMeshGenerationOptions& InOptions, FMeshGenerationResult& Result, const NodeMeshClipDeform* ClipDeform)
	{
		const auto& Node = *ClipDeform->GetPrivate();

		const Ptr<ASTOpMeshBindShape> OpBind = new ASTOpMeshBindShape();
		const Ptr<ASTOpMeshClipDeform> OpClipDeform = new ASTOpMeshClipDeform();

		// Base Mesh
		if (Node.m_pBaseMesh)
		{
			FMeshGenerationOptions BaseOptions = InOptions;
			BaseOptions.bUniqueVertexIDs = true;

			GenerateMesh(BaseOptions, Result, Node.m_pBaseMesh);
			OpBind->Mesh = Result.meshOp;
		}
		else
		{
			// This argument is required
			m_pErrorLog->GetPrivate()->Add("Mesh Clip Deform base mesh node is not set.",
				ELMT_ERROR, Node.m_errorContext);
		}

		// Base Shape
		if (Node.m_pClipShape)
		{
			FMeshGenerationOptions ClipOptions = InOptions;
			ClipOptions.bUniqueVertexIDs = false;
			ClipOptions.bLayouts = false;
			ClipOptions.OverrideLayouts.Empty();
			ClipOptions.ActiveTags.Empty();

			FMeshGenerationResult baseResult;
			GenerateMesh(ClipOptions, baseResult, Node.m_pClipShape);
			OpBind->Shape = baseResult.meshOp;
			OpClipDeform->ClipShape = baseResult.meshOp;
		}

		OpClipDeform->Mesh = OpBind;

		Result.meshOp = OpClipDeform;
	}

    //---------------------------------------------------------------------------------------------
    void CodeGenerator::GenerateMesh_ApplyPose(const FMeshGenerationOptions& InOptions, FMeshGenerationResult& OutResult,
                                                const NodeMeshApplyPose* pose )
    {
        const auto& node = *pose->GetPrivate();

        Ptr<ASTOpMeshApplyPose> op = new ASTOpMeshApplyPose();

        // Base
        if (node.m_pBase)
        {
            GenerateMesh(InOptions, OutResult, node.m_pBase );
            op->base = OutResult.meshOp;
        }
        else
        {
            // This argument is required
            m_pErrorLog->GetPrivate()->Add("Mesh apply-pose base node is not set.",
                ELMT_ERROR, node.m_errorContext);
        }

        // Pose mesh
        if (node.m_pPose)
        {
			FMeshGenerationOptions PoseOptions = InOptions;
			PoseOptions.bUniqueVertexIDs = false;
			PoseOptions.bLayouts = false;
			PoseOptions.OverrideLayouts.Empty();
			PoseOptions.ActiveTags.Empty();

            FMeshGenerationResult poseResult;
            GenerateMesh(PoseOptions, poseResult, node.m_pPose );
            op->pose = poseResult.meshOp;
		}
        else
        {
            // This argument is required
            m_pErrorLog->GetPrivate()->Add("Mesh apply-pose pose node is not set.",
                ELMT_ERROR, node.m_errorContext);
        }

        OutResult.meshOp = op;
    }


	//---------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateMesh_GeometryOperation(const FMeshGenerationOptions& InOptions, FMeshGenerationResult& OutResult, const NodeMeshGeometryOperation* geom)
	{
		const auto& node = *geom->GetPrivate();

		Ptr<ASTOpMeshGeometryOperation> op = new ASTOpMeshGeometryOperation();

		// Mesh A
		if (node.m_pMeshA)
		{
			GenerateMesh(InOptions, OutResult, node.m_pMeshA);
			op->meshA = OutResult.meshOp;
		}
		else
		{
			// This argument is required
			m_pErrorLog->GetPrivate()->Add("Mesh geometric op mesh-a node is not set.",
				ELMT_ERROR, node.m_errorContext);
		}

		// Mesh B
		if (node.m_pMeshB)
		{
			FMeshGenerationOptions OtherOptions = InOptions;
			OtherOptions.bUniqueVertexIDs = false;
			OtherOptions.bLayouts = false;
			OtherOptions.OverrideLayouts.Empty();
			OtherOptions.ActiveTags.Empty();

			FMeshGenerationResult bResult;
			GenerateMesh(OtherOptions, bResult, node.m_pMeshB);
			op->meshB = bResult.meshOp;
		}

		op->scalarA = Generate(node.m_pScalarA, InOptions);
		op->scalarB = Generate(node.m_pScalarB, InOptions);

		OutResult.meshOp = op;
	}


	//---------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateMesh_Reshape(const FMeshGenerationOptions& InOptions, FMeshGenerationResult& OutResult, const NodeMeshReshape* Reshape)
	{
		const NodeMeshReshape::Private& Node = *Reshape->GetPrivate();

		Ptr<ASTOpMeshBindShape> OpBind = new ASTOpMeshBindShape();
		Ptr<ASTOpMeshApplyShape> OpApply = new ASTOpMeshApplyShape();

		OpBind->bReshapeSkeleton = Node.bReshapeSkeleton;	
		OpBind->BonesToDeform = Node.BonesToDeform;
    	OpBind->bReshapePhysicsVolumes = Node.bReshapePhysicsVolumes;
		OpBind->PhysicsToDeform = Node.PhysicsToDeform;
		OpBind->bReshapeVertices = Node.bReshapeVertices;
		OpBind->bApplyLaplacian = Node.bApplyLaplacian;
		OpBind->BindingMethod = static_cast<uint32>(EShapeBindingMethod::ReshapeClosestProject);

		OpBind->RChannelUsage = Node.ColorRChannelUsage;
		OpBind->GChannelUsage = Node.ColorGChannelUsage;
		OpBind->BChannelUsage = Node.ColorBChannelUsage;
		OpBind->AChannelUsage = Node.ColorAChannelUsage;

		OpApply->bReshapeVertices = OpBind->bReshapeVertices;
		OpApply->bReshapeSkeleton = OpBind->bReshapeSkeleton;
		OpApply->bApplyLaplacian = OpBind->bApplyLaplacian;
		OpApply->bReshapePhysicsVolumes = OpBind->bReshapePhysicsVolumes;

		// Base Mesh
		if (Node.BaseMesh)
		{
			FMeshGenerationOptions BaseOptions = InOptions;
			BaseOptions.bUniqueVertexIDs = true;

			GenerateMesh(BaseOptions, OutResult, Node.BaseMesh);
			OpBind->Mesh = OutResult.meshOp;
		}
		else
		{
			// This argument is required
			m_pErrorLog->GetPrivate()->Add("Mesh reshape base node is not set.", ELMT_ERROR, Node.m_errorContext);
		}

		// Base and target shapes shouldn't have layouts or modifiers.
		FMeshGenerationOptions ShapeOptions = InOptions;
		ShapeOptions.bUniqueVertexIDs = false;
		ShapeOptions.bLayouts = false;
		ShapeOptions.OverrideLayouts.Empty();
		ShapeOptions.ActiveTags.Empty();

		// Base Shape
		if (Node.BaseShape)
		{
			FMeshGenerationResult baseResult;
			GenerateMesh(ShapeOptions, baseResult, Node.BaseShape);
			OpBind->Shape = baseResult.meshOp;
		}

		OpApply->Mesh = OpBind;

		// Target Shape
		if (Node.TargetShape)
		{
			FMeshGenerationResult targetResult;
			GenerateMesh(ShapeOptions, targetResult, Node.TargetShape);
			OpApply->Shape = targetResult.meshOp;
		}

		OutResult.meshOp = OpApply;
	}

}

