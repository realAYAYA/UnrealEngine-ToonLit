// Copyright Epic Games, Inc. All Rights Reserved.

#include "NearestNeighborGeomCacheSampler.h"
#include "NearestNeighborModel.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "GeometryCacheComponent.h"
#include "GeometryCache.h"
#include "GeometryCacheMeshData.h"
#include "GeometryCacheTrack.h"

using namespace UE::MLDeformer;
namespace UE::NearestNeighborModel
{
	void FNearestNeighborGeomCacheSampler::SamplePart(int32 InAnimFrameIndex, const TArray<uint32>& VertexMap)
	{
		FMLDeformerSampler::Sample(InAnimFrameIndex);
		USkeletalMesh* SkeletalMesh = SkeletalMeshComponent.Get() ? SkeletalMeshComponent->GetSkeletalMeshAsset() : nullptr;
		UGeometryCache* GeometryCache = GeometryCacheComponent.Get() ? GeometryCacheComponent->GetGeometryCache() : nullptr;
		if (SkeletalMeshComponent && SkeletalMesh && GeometryCacheComponent && GeometryCache)
		{
			const FTransform& AlignmentTransform = Model->GetAlignmentTransform();
			FSkeletalMeshModel* ImportedModel = SkeletalMesh->GetImportedModel();
			const int32 LODIndex = 0;
			const FSkeletalMeshLODModel& LODModel = ImportedModel->LODModels[LODIndex];
			const TArray<FSkelMeshImportedMeshInfo>& SkelMeshInfos = LODModel.ImportedMeshInfos;

			check(MeshMappings.Num() >= 1);
			const UE::MLDeformer::FMLDeformerGeomCacheMeshMapping& MeshMapping = MeshMappings[0]; 

			check(SkelMeshInfos.Num() > MeshMapping.MeshIndex);
			const FSkelMeshImportedMeshInfo& MeshInfo = SkelMeshInfos[MeshMapping.MeshIndex]; 
			check(MeshInfo.StartImportedVertex == 0);

			check(GeometryCache->Tracks.Num() > MeshMapping.TrackIndex);
			UGeometryCacheTrack* Track = GeometryCache->Tracks[MeshMapping.TrackIndex];
			GeomCacheMeshDatas.Reset(1);
			GeomCacheMeshDatas.AddDefaulted(1);
			FGeometryCacheMeshData& GeomCacheMeshData = GeomCacheMeshDatas[0];

			if (!Track->GetMeshDataAtTime(SampleTime, GeomCacheMeshData))
			{
				UE_LOG(LogNearestNeighborModel, Error, TEXT("FNearestNeighborGeomCacheSampler::SamplePart: Track cannot get mesh delta at frame %d"), InAnimFrameIndex);
			}

			// Calculate the vertex deltas.
			const FSkeletalMeshLODRenderData& SkelMeshLODData = SkeletalMesh->GetResourceForRendering()->LODRenderData[LODIndex];
			const FSkinWeightVertexBuffer& SkinWeightBuffer = *SkeletalMeshComponent->GetSkinWeightBuffer(LODIndex);

			const int32 NumPartVerts = VertexMap.Num();
			PartVertexDeltas.Reset();
			PartVertexDeltas.SetNum(NumPartVerts * 3);

			for(int32 PartVertexIndex = 0; PartVertexIndex < NumPartVerts; PartVertexIndex++)
			{
				const int32 VertexIndex = VertexMap[PartVertexIndex];
				const int32 GeomCacheVertexIndex = MeshMapping.SkelMeshToTrackVertexMap[PartVertexIndex];

				if (GeomCacheVertexIndex != INDEX_NONE && GeomCacheMeshData.Positions.IsValidIndex(GeomCacheVertexIndex))
				{
					FVector3f Delta = FVector3f::ZeroVector;

					const int32 ArrayIndex = 3 * PartVertexIndex;
					// Calculate the inverse skinning transform for this vertex.
					const int32 RenderVertexIndex = MeshMapping.ImportedVertexToRenderVertexMap[PartVertexIndex];
					if (RenderVertexIndex != INDEX_NONE)
					{
						const FMatrix44f InvSkinningTransform = CalcInverseSkinningTransform(RenderVertexIndex, SkelMeshLODData, SkinWeightBuffer);

						// Calculate the pre-skinning data.
						const FVector3f UnskinnedPosition = SkelMeshLODData.StaticVertexBuffers.PositionVertexBuffer.VertexPosition(RenderVertexIndex);
						const FVector3f GeomCacheVertexPos = (FVector3f)AlignmentTransform.TransformPosition((FVector)GeomCacheMeshData.Positions[GeomCacheVertexIndex]);
						const FVector3f PreSkinningTargetPos = InvSkinningTransform.TransformPosition(GeomCacheVertexPos);
						Delta = PreSkinningTargetPos - UnskinnedPosition;
					}

					PartVertexDeltas[ArrayIndex] = Delta.X;
					PartVertexDeltas[ArrayIndex + 1] = Delta.Y;
					PartVertexDeltas[ArrayIndex + 2] = Delta.Z;
				}
			}
		}
	}

	bool IsPotentialMatch(const FString& TrackName, const FString& MeshName)
	{
		return (TrackName.Find(MeshName) == 0);
	}

	void FNearestNeighborGeomCacheSampler::GeneratePartMeshMappings(const TArray<uint32>& VertexMap, bool bUsePartOnlyMesh)
	{
		USkeletalMesh* SkeletalMesh = SkeletalMeshComponent.Get() ? SkeletalMeshComponent->GetSkeletalMeshAsset() : nullptr;
		UGeometryCache* GeometryCache = GeometryCacheComponent.Get() ? GeometryCacheComponent->GetGeometryCache() : nullptr;
		// TODO: make this more general
		if (SkeletalMeshComponent && SkeletalMesh && GeometryCacheComponent && GeometryCache)
		{
			if (!bUsePartOnlyMesh)
			{
				TArray<FString> FailedNames;
				TArray<FString> VertexMisMatchNames;
				GenerateGeomCacheMeshMappings(SkeletalMesh, GeometryCache, MeshMappings, FailedNames, VertexMisMatchNames);
				return;
			}
			FSkeletalMeshModel* ImportedModel = SkeletalMesh->GetImportedModel();
			check(ImportedModel);
			check(ImportedModel->LODModels[0].ImportedMeshInfos.Num() >= 1);

			const TArray<FSkelMeshImportedMeshInfo>& SkelMeshInfos = ImportedModel->LODModels[0].ImportedMeshInfos;
			if (SkelMeshInfos.IsEmpty())
			{
				return;
			}

			MeshMappings.Reset();

			FString SkelMeshName;
			const bool bIsSoloMesh = (GeometryCache->Tracks.Num() == 1 && SkelMeshInfos.Num() == 1);	// Do we just have one mesh and one track?

			for (int32 TrackIndex = 0; TrackIndex < GeometryCache->Tracks.Num(); ++TrackIndex)
			{
				// Check if this is a candidate based on the mesh and track name.
				UGeometryCacheTrack* Track = GeometryCache->Tracks[TrackIndex];

				bool bFoundMatch = false;
				for (int32 SkelMeshIndex = 0; SkelMeshIndex < SkelMeshInfos.Num(); ++SkelMeshIndex)
				{
					const FSkelMeshImportedMeshInfo& MeshInfo = SkelMeshInfos[SkelMeshIndex];

					// Hack for now
					if (MeshInfo.StartImportedVertex != 0)
					{
						continue;
					}

					SkelMeshName = MeshInfo.Name.ToString();
					if (Track &&
						(IsPotentialMatch(Track->GetName(), SkelMeshName) || bIsSoloMesh))
					{
						// Extract the geom cache mesh data.
						FGeometryCacheMeshData GeomCacheMeshData;
						if (!Track->GetMeshDataAtTime(SampleTime, GeomCacheMeshData))
						{
							continue;
						}

						// Verify that we have imported vertex numbers.
						if (GeomCacheMeshData.ImportedVertexNumbers.IsEmpty())
						{
							continue;
						}

						// Create a new mesh mapping entry.
						MeshMappings.AddDefaulted();
						UE::MLDeformer::FMLDeformerGeomCacheMeshMapping& Mapping = MeshMappings.Last();
						Mapping.MeshIndex = SkelMeshIndex;
						Mapping.TrackIndex = TrackIndex;

						const int32 NumPartVerts = VertexMap.Num();
						Mapping.SkelMeshToTrackVertexMap.AddUninitialized(NumPartVerts);
						Mapping.ImportedVertexToRenderVertexMap.AddUninitialized(NumPartVerts);


						for(int32 PartVertexIndex = 0; PartVertexIndex < NumPartVerts; PartVertexIndex++)
						{
							Mapping.SkelMeshToTrackVertexMap[PartVertexIndex] = GeomCacheMeshData.ImportedVertexNumbers.Find(PartVertexIndex);
							const int32 VertexIndex = VertexMap[PartVertexIndex];
							const int32 RenderVertexIndex = ImportedModel->LODModels[0].MeshToImportVertexMap.Find(VertexIndex);
							Mapping.ImportedVertexToRenderVertexMap[PartVertexIndex] = RenderVertexIndex;
						}

						// We found a match, no need to iterate over more Tracks.
						bFoundMatch = true;
						break;
					} // If the track name matches the skeletal meshes internal mesh name.
				} // For all meshes in the Skeletal Mesh.

				if (Track && !bFoundMatch)
				{
					UE_LOG(LogMLDeformer, Warning, TEXT("Geometry cache '%s' cannot be matched with a mesh inside the Skeletal Mesh."), *Track->GetName());
				}
			} // For all tracks.
		}
	}

	void FNearestNeighborGeomCacheSampler::SampleKMeansAnim(const int32 SkeletonId)
	{
		UNearestNeighborModel* NearestNeighborModel = static_cast<UNearestNeighborModel*>(Model);
		if (NearestNeighborModel && SkeletonId < NearestNeighborModel->SourceSkeletons.Num() && SkeletalMeshComponent)
		{
			const TObjectPtr<UAnimSequence> AnimSequence = NearestNeighborModel->SourceSkeletons[SkeletonId];
			SkeletalMeshComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			SkeletalMeshComponent->SetAnimation(AnimSequence);
			SkeletalMeshComponent->SetPosition(0.0f);
			SkeletalMeshComponent->SetPlayRate(1.0f);
			SkeletalMeshComponent->Play(false);
			SkeletalMeshComponent->RefreshBoneTransforms();
		}
	}

	void FNearestNeighborGeomCacheSampler::SampleKMeansFrame(const int32 Frame)
	{
		AnimFrameIndex = Frame;
		SampleTime = GetTimeAtFrame(Frame);

		const USkeletalMesh* SkeletalMesh = SkeletalMeshComponent->GetSkeletalMeshAsset();
		if (SkeletalMeshComponent && SkeletalMesh)
		{
			UpdateSkeletalMeshComponent();
			UpdateBoneRotations();
			UpdateCurveValues();
		}
	}
};
