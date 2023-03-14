// Copyright Epic Games, Inc. All Rights Reserved.

#include "MLDeformerGeomCacheSampler.h"
#include "MLDeformerModel.h"
#include "MLDeformerEditorModel.h"
#include "MLDeformerInputInfo.h"
#include "MLDeformerEditorModule.h"
#include "MLDeformerEditorToolkit.h"
#include "MLDeformerGeomCacheHelpers.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "GeometryCacheComponent.h"
#include "GeometryCache.h"
#include "GeometryCacheMeshData.h"
#include "GeometryCacheTrack.h"

namespace UE::MLDeformer
{
	void FMLDeformerGeomCacheSampler::RegisterTargetComponents()
	{
		// Create the geometry cache component.
		if (GeometryCacheComponent.Get() == nullptr)
		{
			GeometryCacheComponent = NewObject<UGeometryCacheComponent>(TargetMeshActor);
			GeometryCacheComponent->RegisterComponent();
			TargetMeshActor->SetRootComponent(GeometryCacheComponent);
		}

		UGeometryCache* GeomCache = OnGetGeometryCache().IsBound() ? OnGetGeometryCache().Execute() : nullptr;
		GeometryCacheComponent->SetGeometryCache(GeomCache);
		GeometryCacheComponent->SetManualTick(true);
		GeometryCacheComponent->SetVisibility(false);

		// Generate mappings between the meshes in the SkeletalMesh and the geometry cache tracks.
		FailedImportedMeshNames.Reset();
		MeshMappings.Reset();
		GenerateGeomCacheMeshMappings(
			Model->GetSkeletalMesh(), 
			GeomCache, 
			MeshMappings, 
			FailedImportedMeshNames,
			VertexCountMisMatchNames);

		GeomCacheMeshDatas.Reset(MeshMappings.Num());
		GeomCacheMeshDatas.AddDefaulted(MeshMappings.Num());
	}

	void FMLDeformerGeomCacheSampler::Sample(int32 InAnimFrameIndex)
	{
		// Call this first to update bone and curve values.
		// This will also calculate the skinned positions if the delta space is set to PostSkinning.
		FMLDeformerSampler::Sample(InAnimFrameIndex);

		USkeletalMesh* SkeletalMesh = SkeletalMeshComponent.Get() ? SkeletalMeshComponent->GetSkeletalMeshAsset() : nullptr;
		UGeometryCache* GeometryCache = GeometryCacheComponent->GetGeometryCache();
		if (SkeletalMeshComponent && SkeletalMesh && GeometryCacheComponent && GeometryCache)
		{
			const float DeltaCutoffLength = Model->GetDeltaCutoffLength();
			const FTransform& AlignmentTransform = Model->GetAlignmentTransform();
			FSkeletalMeshModel* ImportedModel = SkeletalMesh->GetImportedModel();

			// For all mesh mappings we found.
			const int32 LODIndex = 0;
			const FSkeletalMeshLODModel& LODModel = ImportedModel->LODModels[LODIndex];
			const TArray<FSkelMeshImportedMeshInfo>& SkelMeshInfos = LODModel.ImportedMeshInfos;
			for (int32 MeshMappingIndex = 0; MeshMappingIndex < MeshMappings.Num(); ++MeshMappingIndex)
			{
				const UE::MLDeformer::FMLDeformerGeomCacheMeshMapping& MeshMapping = MeshMappings[MeshMappingIndex];
				const FSkelMeshImportedMeshInfo& MeshInfo = SkelMeshInfos[MeshMapping.MeshIndex];
				UGeometryCacheTrack* Track = GeometryCache->Tracks[MeshMapping.TrackIndex];

				// Sample the mesh data of the geom cache.
				FGeometryCacheMeshData& GeomCacheMeshData = GeomCacheMeshDatas[MeshMappingIndex];
				if (!Track->GetMeshDataAtTime(SampleTime, GeomCacheMeshData))
				{
					continue;
				}

				// Calculate the vertex deltas.
				const FSkeletalMeshLODRenderData& SkelMeshLODData = SkeletalMesh->GetResourceForRendering()->LODRenderData[LODIndex];
				const FSkinWeightVertexBuffer& SkinWeightBuffer = *SkeletalMeshComponent->GetSkinWeightBuffer(LODIndex);
				for (int32 VertexIndex = 0; VertexIndex < MeshInfo.NumVertices; ++VertexIndex)
				{
					const int32 SkinnedVertexIndex = MeshInfo.StartImportedVertex + VertexIndex;
					const int32 GeomCacheVertexIndex = MeshMapping.SkelMeshToTrackVertexMap[VertexIndex];
					if (GeomCacheVertexIndex != INDEX_NONE && GeomCacheMeshData.Positions.IsValidIndex(GeomCacheVertexIndex))
					{
						FVector3f Delta = FVector3f::ZeroVector;

						const int32 ArrayIndex = 3 * SkinnedVertexIndex;
						if (VertexDeltaSpace == EVertexDeltaSpace::PreSkinning)
						{
							// Calculate the inverse skinning transform for this vertex.
							const int32 RenderVertexIndex = MeshMapping.ImportedVertexToRenderVertexMap[VertexIndex];
							if (RenderVertexIndex != INDEX_NONE)
							{
								const FMatrix44f InvSkinningTransform = CalcInverseSkinningTransform(RenderVertexIndex, SkelMeshLODData, SkinWeightBuffer);

								// Calculate the pre-skinning data.
								const FSkeletalMeshLODRenderData& LODData = SkeletalMesh->GetResourceForRendering()->LODRenderData[0];
								const FVector3f UnskinnedPosition = LODData.StaticVertexBuffers.PositionVertexBuffer.VertexPosition(RenderVertexIndex);
								const FVector3f GeomCacheVertexPos = (FVector3f)AlignmentTransform.TransformPosition((FVector)GeomCacheMeshData.Positions[GeomCacheVertexIndex]);
								const FVector3f PreSkinningTargetPos = InvSkinningTransform.TransformPosition(GeomCacheVertexPos);
								Delta = PreSkinningTargetPos - UnskinnedPosition;
							}
						}
						else // We're post skinning.
						{
							check(VertexDeltaSpace == EVertexDeltaSpace::PostSkinning);
							const FVector3f SkinnedVertexPos = SkinnedVertexPositions[SkinnedVertexIndex];
							const FVector3f GeomCacheVertexPos = (FVector3f)AlignmentTransform.TransformPosition((FVector)GeomCacheMeshData.Positions[GeomCacheVertexIndex]);
							Delta = GeomCacheVertexPos - SkinnedVertexPos;
						}

						// Set the delta.
						if (Delta.Length() < DeltaCutoffLength)
						{
							VertexDeltas[ArrayIndex] = Delta.X;
							VertexDeltas[ArrayIndex + 1] = Delta.Y;
							VertexDeltas[ArrayIndex + 2] = Delta.Z;
						}
					}
				}
			}
		}
		else
		{
			VertexDeltas.Reset(0);
		}
	}

	float FMLDeformerGeomCacheSampler::GetTimeAtFrame(int32 InAnimFrameIndex) const
	{
		return GeometryCacheComponent.Get() ? GeometryCacheComponent->GetTimeAtFrame(InAnimFrameIndex) : 0.0f;
	}
}	// namespace UE::MLDeformer
