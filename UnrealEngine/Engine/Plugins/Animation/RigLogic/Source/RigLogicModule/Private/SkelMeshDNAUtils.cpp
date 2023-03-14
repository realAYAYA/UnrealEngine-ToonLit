// Copyright Epic Games, Inc. All Rights Reserved.
#include "SkelMeshDNAUtils.h"
#include "DNAToSkelMeshMap.h"

#include "RenderResource.h"
#include "RHICommandList.h"
#include "Async/ParallelFor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Misc/Paths.h"
#include "ProfilingDebugging/ScopedTimers.h"
#include "Misc/FileHelper.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/SkeletalMesh.h"
#include "ComponentReregisterContext.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#if WITH_EDITORONLY_DATA
#include "MeshUtilities.h"
#endif // WITH_EDITORONLY_DATA
#include "Modules/ModuleManager.h"

#include "AnimationRuntime.h"

#include "riglogic/RigLogic.h"

DEFINE_LOG_CATEGORY(LogDNAUtils);
/** compare based on base mesh source vertex indices */
struct FCompareMorphTargetDeltas
{
	FORCEINLINE bool operator()(const FMorphTargetDelta& A, const FMorphTargetDelta& B) const
	{
		return ((int32)A.SourceIdx - (int32)B.SourceIdx) < 0;
	}
};

USkelMeshDNAUtils::USkelMeshDNAUtils(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FDNAToSkelMeshMap* USkelMeshDNAUtils::CreateMapForUpdatingNeutralMesh(IDNAReader* InDNAReader, USkeletalMesh* InSkelMesh)
{
#if WITH_EDITORONLY_DATA
	FDNAToSkelMeshMap* DNAToSkelMeshMap = new FDNAToSkelMeshMap();

	//only vertex map is initialized in this pass so we can mix neutral meshes fast (e.g. on slider move);
	//playing animation on a such a mesh requires updating joints and skin weights
	//getting full quality animation requires mixing morph targets too
	DNAToSkelMeshMap->InitBaseMesh(InDNAReader, InSkelMesh);
	return DNAToSkelMeshMap;
#else
	return nullptr;
#endif // WITH_EDITORONLY_DATA
}

FDNAToSkelMeshMap* USkelMeshDNAUtils::CreateMapForUpdatingNeutralMesh(USkeletalMesh* InSkelMesh)
{
#if WITH_EDITORONLY_DATA
	FDNAToSkelMeshMap* DNAToSkelMeshMap = new FDNAToSkelMeshMap();

	DNAToSkelMeshMap->InitFromDNAAsset(InSkelMesh);
	return DNAToSkelMeshMap;
#else
	return nullptr;
#endif // WITH_EDITORONLY_DATA
}

#if WITH_EDITORONLY_DATA
/** Updates bind pose using joint positions from DNA. */
void USkelMeshDNAUtils::UpdateJoints(USkeletalMesh* InSkelMesh, IDNAReader* InDNAReader, FDNAToSkelMeshMap* InDNAToSkelMeshMap)
{
	{	// Scoping of RefSkelModifier
		FReferenceSkeletonModifier RefSkelModifier(InSkelMesh->GetRefSkeleton(), InSkelMesh->GetSkeleton());

		// copy here
		TArray<FTransform> RawBonePose = InSkelMesh->GetRefSkeleton().GetRawRefBonePose();

		// When we are mounting the head to different bodies than female average, we need to use
		// component space, as the joint to which the head root is snapped to will be on a different
		// position than in the head rig!

		// calculate component space ahead of current transform
		TArray<FTransform> ComponentTransforms;
		FAnimationRuntime::FillUpComponentSpaceTransforms(InSkelMesh->GetRefSkeleton(), RawBonePose, ComponentTransforms);

		const TArray<FMeshBoneInfo>& RawBoneInfo = InSkelMesh->GetRefSkeleton().GetRawRefBoneInfo();

		// Skipping root joint (index 0) to avoid blinking of the mesh due to bounding box issue
		for (uint16 JointIndex = 0; JointIndex < InDNAReader->GetJointCount(); JointIndex++)
		{
			int32 BoneIndex = InDNAToSkelMeshMap->GetUEBoneIndex(JointIndex);

			FTransform DNATransform = FTransform::Identity;

			// Updating bind pose affects just translations.
			FVector Translate = InDNAReader->GetNeutralJointTranslation(JointIndex);
			DNATransform.SetTranslation(Translate);
			FVector RotationVector = InDNAReader->GetNeutralJointRotation(JointIndex);
			FRotator Rotation(RotationVector.X, RotationVector.Y, RotationVector.Z);

			// Joint 0 (spine_04) is a root of GeneSplicer joint hierarchy, and is a special case
			// 1) it is parent to itself
			// 2) it is in DNA space, so we need to rotate it 90 degs on x axis to UE4 space
			// 3) the head joints below it in the skeletal mesh are not spliced, as they are not in DNA,
			//    so they will retain female average transforms

			if (InDNAReader->GetJointParentIndex(JointIndex) == JointIndex)  //parent to itself
			{
				Rotation.Pitch += 90;
				DNATransform.SetRotation(Rotation.Quaternion());

				DNATransform.SetTranslation(FVector(Translate.X, Translate.Z, -Translate.Y));

				ComponentTransforms[BoneIndex] = DNATransform;

			}
			else
			{
				DNATransform.SetRotation(Rotation.Quaternion());

				if (ensure(RawBoneInfo[BoneIndex].ParentIndex != INDEX_NONE))
				{
					ComponentTransforms[BoneIndex] = DNATransform * ComponentTransforms[RawBoneInfo[BoneIndex].ParentIndex];
				}
			}

			ComponentTransforms[BoneIndex].NormalizeRotation();
		}

		for (uint16 BoneIndex = 0; BoneIndex < RawBoneInfo.Num(); BoneIndex++)
		{
			FTransform LocalTransform;

			if (BoneIndex == 0)
			{
				LocalTransform = ComponentTransforms[BoneIndex];
			}
			else
			{
				LocalTransform = ComponentTransforms[BoneIndex].GetRelativeTransform(ComponentTransforms[RawBoneInfo[BoneIndex].ParentIndex]);
			}

			LocalTransform.NormalizeRotation();

			RefSkelModifier.UpdateRefPoseTransform(BoneIndex, LocalTransform);
		}
	}

	InSkelMesh->GetRefBasesInvMatrix().Reset();
	InSkelMesh->CalculateInvRefMatrices(); // Needs to be called after RefSkelModifier is destroyed
}

/** Updates base mesh vertices using data from DNA. */
void USkelMeshDNAUtils::UpdateBaseMesh(USkeletalMesh* InSkelMesh, IDNAReader* InDNAReader, FDNAToSkelMeshMap* InDNAToSkelMeshMap, ELodUpdateOption InUpdateOption)
{

	FSkeletalMeshModel* ImportedModel = InSkelMesh->GetImportedModel();
	// Expects vertex map to be initialized beforehand	
	int32 LODStart;
	int32 LODRangeSize;
	GetLODRange(InUpdateOption, ImportedModel->LODModels.Num(), LODStart, LODRangeSize);
	for (int32 LODIndex = LODStart; LODIndex < LODRangeSize; LODIndex++)
	{
		FSkeletalMeshLODModel& LODModel = ImportedModel->LODModels[LODIndex];
		int32 SectionIndex = 0;
		for (FSkelMeshSection& Section : LODModel.Sections)
		{
			int32& DNAMeshIndex = InDNAToSkelMeshMap->ImportVtxToDNAMeshIndex[LODIndex][Section.GetVertexBufferIndex()];

			const int32 NumSoftVertices = Section.GetNumVertices();
			auto& OverlappingMap = InDNAToSkelMeshMap->OverlappingVertices[LODIndex][SectionIndex];
			int32 VertexBufferIndex = Section.GetVertexBufferIndex();
			for (int32 VertexIndex = 0; VertexIndex < NumSoftVertices; VertexIndex++)
			{
				int32& DNAVertexIndex = InDNAToSkelMeshMap->ImportVtxToDNAVtxIndex[LODIndex][VertexBufferIndex];

				if (DNAVertexIndex >= 0)
				{
					const FVector Position = InDNAReader->GetVertexPosition(DNAMeshIndex, DNAVertexIndex);
					FSoftSkinVertex& Vertex = Section.SoftVertices[VertexIndex];
					Vertex.Position = FVector3f{ Position };

					// Check if the current vertex has overlapping vertices, and then update them as well.
					TArray<int32>& OverlappedIndices = OverlappingMap[VertexIndex];
					int32 OverlappingCount = OverlappedIndices.Num();
					for (int32 OverlappingIndex = 0; OverlappingIndex < OverlappingCount; ++OverlappingIndex)
					{
						int32 OverlappingVertexIndex = OverlappedIndices[OverlappingIndex];
						FSoftSkinVertex& OverlappingVertex = Section.SoftVertices[OverlappingVertexIndex];
						OverlappingVertex.Position = FVector3f{ Position };
					}
				}
				VertexBufferIndex++;
			}
			SectionIndex++;
		}
	}
}

/** Updates Morph Targets using Blend Shapes from DNA.  */
void USkelMeshDNAUtils::UpdateMorphTargets(USkeletalMesh* InSkelMesh, IDNAReader* InDNAReader, FDNAToSkelMeshMap* InDNAToSkelMeshMap, ELodUpdateOption InUpdateOption)
{
	TArray<FDNABlendShapeTarget>& MeshBlendShapeTargets = InDNAToSkelMeshMap->GetMeshBlendShapeTargets();
	if (MeshBlendShapeTargets.Num() == 0)
	{
		UE_LOG(LogDNAUtils, Warning, TEXT("No morph targets updated!"));
		return;
	}
	FSkeletalMeshModel* ImportedModel = InSkelMesh->GetImportedModel();

	uint16 ChannelCount = InDNAReader->GetBlendShapeChannelCount();
	ParallelFor(InSkelMesh->GetMorphTargets().Num(), [&](int32 MorphIndex)
		{
			UMorphTarget* MorphTarget = InSkelMesh->GetMorphTargets()[MorphIndex];
			const FDNABlendShapeTarget& MeshTarget = MeshBlendShapeTargets[MorphIndex];
			// First get all DNA deltas for current Morph Target.
			TArrayView<const uint32> BlendShapeVertexIndices = InDNAReader->GetBlendShapeTargetVertexIndices(MeshTarget.MeshIndex, MeshTarget.TargetIndex);
			const int32 NumOfDeltas = BlendShapeVertexIndices.Num();
			if (NumOfDeltas > 0)
			{
				int32 LODStart;
				int32 LODRangeSize;
				GetLODRange(InUpdateOption, MorphTarget->GetMorphLODModels().Num(), LODStart, LODRangeSize);
				for (int32 LODIndex = LODStart; LODIndex < LODRangeSize; LODIndex++)
				{
					// MorphTarget vertex indices refer to full vertex index buffer of imported mesh.
					FMorphTargetLODModel& MorphLODModel = MorphTarget->GetMorphLODModels()[LODIndex];
					MorphLODModel.NumBaseMeshVerts = NumOfDeltas;
					MorphLODModel.bGeneratedByEngine = false;
					MorphLODModel.SectionIndices.Empty();
					MorphLODModel.Vertices.Reset(NumOfDeltas);
					TArrayView<const float> DeltaXs = InDNAReader->GetBlendShapeTargetDeltaXs(MeshTarget.MeshIndex, MeshTarget.TargetIndex);
					TArrayView<const float> DeltaYs = InDNAReader->GetBlendShapeTargetDeltaYs(MeshTarget.MeshIndex, MeshTarget.TargetIndex);
					TArrayView<const float> DeltaZs = InDNAReader->GetBlendShapeTargetDeltaZs(MeshTarget.MeshIndex, MeshTarget.TargetIndex);

					for (int32 DeltaIndex = 0; DeltaIndex < NumOfDeltas; DeltaIndex++)
					{
						FVector PositionDelta = FVector(DeltaXs[DeltaIndex], DeltaYs[DeltaIndex], DeltaZs[DeltaIndex]);
						FMorphTargetDelta MorphDelta;
						int32 DNAVertexIndex = BlendShapeVertexIndices[DeltaIndex];
						int32 UEVertexIndex = InDNAToSkelMeshMap->ImportDNAVtxToUEVtxIndex[LODIndex][MeshTarget.MeshIndex][DNAVertexIndex];
						if (UEVertexIndex > INDEX_NONE)
						{
							MorphDelta.SourceIdx = (uint32)UEVertexIndex;
							MorphDelta.PositionDelta = FVector3f{ PositionDelta };
							MorphDelta.TangentZDelta = FVector3f::ZeroVector;
							MorphLODModel.Vertices.Add(MorphDelta);
							// Find section indices that are involved in these morph deltas.
							int32 SectionIdx = InDNAToSkelMeshMap->UEVertexToSectionIndices[LODIndex][UEVertexIndex];
							if (!MorphLODModel.SectionIndices.Contains(SectionIdx) && SectionIdx > INDEX_NONE)
							{
								MorphLODModel.SectionIndices.Add(SectionIdx);
							}
						}
					}
					MorphLODModel.Vertices.Sort(FCompareMorphTargetDeltas());
				}
			}
			else
			{
				int32 LODStart;
				int32 LODRangeSize;
				GetLODRange(InUpdateOption, MorphTarget->GetMorphLODModels().Num(), LODStart, LODRangeSize);
				for (int32 LODIndex = LODStart; LODIndex < LODRangeSize; LODIndex++)
				{
					FMorphTargetLODModel& MorphLODModel = MorphTarget->GetMorphLODModels()[LODIndex];
					MorphLODModel.Reset();
				}
#ifdef DEBUG
				UE_LOG(LogDNAUtils, Warning, TEXT(" 0 deltas found for mesh %d and blend shape target %d"), MeshTarget.MeshIndex, MeshTarget.TargetIndex);
#endif 
			}
		});
}

/* Updates Bone influences using Skin Weights from DNA. */
void USkelMeshDNAUtils::UpdateSkinWeights(USkeletalMesh* InSkelMesh, IDNAReader* InDNAReader, FDNAToSkelMeshMap* InDNAToSkelMeshMap, ELodUpdateOption InUpdateOption)
{
	FSkeletalMeshModel* ImportedModel = InSkelMesh->GetImportedModel();

	bool InfluenceMismatch = false;
	//Set a threshold a bit smaller then 1/255
	constexpr float MINWEIGHT = 0.9999f / 255.0f;
	int32 LODStart;
	int32 LODRangeSize;
	GetLODRange(InUpdateOption, ImportedModel->LODModels.Num(), LODStart, LODRangeSize);
	for (int32 LODIndex = LODStart; LODIndex < LODRangeSize; ++LODIndex)
	{
		FSkeletalMeshLODModel& LODModel = ImportedModel->LODModels[LODIndex];
		for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); SectionIndex++)
		{
			FSkelMeshSection& Section = LODModel.Sections[SectionIndex];
			int32 DNAMeshIndex = InDNAToSkelMeshMap->ImportVtxToDNAMeshIndex[LODIndex][Section.GetVertexBufferIndex()];

			const int32 NumEngineVertices = Section.GetNumVertices();
			for (int32 VertexIndex = 0; VertexIndex < NumEngineVertices; VertexIndex++)
			{
				const int32 VertexBufferIndex = VertexIndex + Section.GetVertexBufferIndex();
				int32 DNAVertexIndex = InDNAToSkelMeshMap->ImportVtxToDNAVtxIndex[LODIndex][VertexBufferIndex];

				if (DNAVertexIndex < 0) continue; // Skip vertex not in DNA.

				TArrayView<const float> DNASkinWeights = InDNAReader->GetSkinWeightsValues(DNAMeshIndex, DNAVertexIndex);
				TArrayView<const uint16> DNASkinJoints = InDNAReader->GetSkinWeightsJointIndices(DNAMeshIndex, DNAVertexIndex);
				uint16 SkinJointNum = DNASkinJoints.Num();

				uint32 TotalWeight = 0;  // store all influences to vertex to ensure they add up to 255 (fix rounding errors)
				uint16 MaxInfluenceIndex = 0;
				uint8 MaxInfluenceWeight = 0;

				FSoftSkinVertex& Vertex = Section.SoftVertices[VertexIndex];

				// Update skin weights only around eyes where blue channel is not 0.
				if (Vertex.Color.B == 0) {
					//UE_LOG(LogDNAUtils, Log, TEXT("Skipping vertex UE: %d, DNA: %d"), VertexIndex, DNAVertexIndex );
					continue;
				}
				TArray<float> NormalizedSkinWeights;
				NormalizedSkinWeights.Reserve(SkinJointNum);
				TArray<uint16> NormalizedJoints;
				NormalizedJoints.Reserve(SkinJointNum);
				float TotalInfluence = 0.0;
				// Second step is to filter out all the influences below the MIN WEIGHT or above total influence limit.
				for (uint16 i = 0; i < SkinJointNum; i++)
				{
					if (DNASkinWeights[i] > MINWEIGHT && i < MAX_TOTAL_INFLUENCES)
					{
						NormalizedSkinWeights.Add(DNASkinWeights[i]);
						NormalizedJoints.Add(DNASkinJoints[i]);
						TotalInfluence += DNASkinWeights[i];
					}
				}
				SkinJointNum = NormalizedJoints.Num();
				if (SkinJointNum > 0 && (TotalInfluence != 1.0f))
				{
					// Missing fractions of influence weights have to be assigned equally along the existing influences.
					float OneOverTotalWeight = 1.f / TotalInfluence;
					for (uint16 r = 0; r < SkinJointNum; r++)
					{
						NormalizedSkinWeights[r] *= OneOverTotalWeight;
					}
				}
				// Reset all influences that are not covered by DNA data.
				for (uint16 i = SkinJointNum; i < MAX_TOTAL_INFLUENCES; i++)
				{
					Vertex.InfluenceBones[i] = 0;
					Vertex.InfluenceWeights[i] = 0;
				}
				for (uint16 InfluenceIndex = 0; InfluenceIndex < SkinJointNum; ++InfluenceIndex)
				{
					uint8 EngineWeight = 0;
					// Find Engine bone for corresponding DNAJoint for the same influence.
					int32 UpdatedBoneId = InDNAToSkelMeshMap->GetUEBoneIndex(NormalizedJoints[InfluenceIndex]);
					// BoneMap holds subset of bones belonging to current section.
					int32 BoneMapIndex = Section.BoneMap.Find(UpdatedBoneId);

					// Update which bone in the subset influences this vertex.
					Vertex.InfluenceBones[InfluenceIndex] = BoneMapIndex;
					if (BoneMapIndex != INDEX_NONE)
					{
						// Update influence weight.
						EngineWeight = (uint8)(NormalizedSkinWeights[InfluenceIndex] * 255.0f);
					}

					Vertex.InfluenceWeights[InfluenceIndex] = EngineWeight;
					TotalWeight += Vertex.InfluenceWeights[InfluenceIndex];

					if (EngineWeight > MaxInfluenceWeight)
					{
						MaxInfluenceIndex = InfluenceIndex;
						MaxInfluenceWeight = EngineWeight;
					}
				}
				// Add missing fraction to fill up to 255.
				Vertex.InfluenceWeights[MaxInfluenceIndex] += 255 - TotalWeight;
			}
		}
	}
}

/** Rebuilds render data from LODModel and inits resources. */
void USkelMeshDNAUtils::RebuildRenderData(USkeletalMesh* InSkelMesh)
{
	FPlatformTime::InitTiming();

	double StartTime = FPlatformTime::Seconds();
	{
		InSkelMesh->FlushRenderState();
	}
	double TimeToFlush = FPlatformTime::Seconds();
	{
		FSkeletalMeshRenderData* RenderData = InSkelMesh->GetResourceForRendering();
		int32 LODIndex = 0;

		for (FSkeletalMeshLODRenderData& LODRenderData : RenderData->LODRenderData)
		{
			FSkeletalMeshLODModel& LODModelRef = InSkelMesh->GetImportedModel()->LODModels[LODIndex];
			for (int32 i = 0; i < LODModelRef.Sections.Num(); i++)
			{
				FSkelMeshSection& ModelSection = LODModelRef.Sections[i];
				ModelSection.CalcMaxBoneInfluences();
				ModelSection.CalcUse16BitBoneIndex();
			}

			const FSkeletalMeshLODModel* LODModelPtr = &LODModelRef;
			LODRenderData.BuildFromLODModel(LODModelPtr, 0);
			LODIndex++;
		}
	}
	double TimeToRebuildModel = FPlatformTime::Seconds();
	{
		if (FApp::CanEverRender())
		{
			// Reinitialize the static mesh's resources.
			InSkelMesh->InitResources();
		}
	}
	double TimeToInitResources = FPlatformTime::Seconds();
	{
		// Re-register scope
		TArray<UActorComponent*> ComponentsToReregister;
		for (TObjectIterator<USkeletalMeshComponent> It; It; ++It)
		{
			USkeletalMeshComponent* MeshComponent = *It;
			if (MeshComponent && !MeshComponent->IsTemplate() && MeshComponent->GetSkeletalMeshAsset() == InSkelMesh)
			{
				ComponentsToReregister.Add(*It);
			}
		}
		FMultiComponentReregisterContext ReregisterContext(ComponentsToReregister);
	}
}

void USkelMeshDNAUtils::RebuildRenderData_VertexPosition(USkeletalMesh* InSkelMesh)
{
	if (!FApp::CanEverRender())
	{
		return;
	}

	{
		FSkeletalMeshModel* MeshModel = InSkelMesh->GetImportedModel();
		FSkeletalMeshRenderData* RenderData = InSkelMesh->GetResourceForRendering();

		for (int32 LODIdx = 0; LODIdx < RenderData->LODRenderData.Num(); ++LODIdx)
		{
			FSkeletalMeshLODModel& LODModel = MeshModel->LODModels[LODIdx];
			FSkeletalMeshLODRenderData& LODRenderData = RenderData->LODRenderData[LODIdx];

			ENQUEUE_RENDER_COMMAND(FSkelMeshDNAUpdatePositions)
				([&LODModel, &LODRenderData](FRHICommandListImmediate& RHICmdList)
					{
						LLM_SCOPE(ELLMTag::SkeletalMesh);

						TArray<FSoftSkinVertex> Vertices;
						LODModel.GetVertices(Vertices);
						check(Vertices.Num() == LODRenderData.StaticVertexBuffers.PositionVertexBuffer.GetNumVertices());
						LODRenderData.StaticVertexBuffers.PositionVertexBuffer.Init(Vertices.Num());
						for (int32 i = 0; i < Vertices.Num(); i++)
						{
							LODRenderData.StaticVertexBuffers.PositionVertexBuffer.VertexPosition(i) = Vertices[i].Position;
						}

						auto& VertexBuffer = LODRenderData.StaticVertexBuffers.PositionVertexBuffer;
						void* VertexBufferData = RHILockBuffer(VertexBuffer.VertexBufferRHI, 0, VertexBuffer.GetNumVertices() * VertexBuffer.GetStride(), RLM_WriteOnly);
						FMemory::Memcpy(VertexBufferData, VertexBuffer.GetVertexData(), VertexBuffer.GetNumVertices() * VertexBuffer.GetStride());
						RHIUnlockBuffer(VertexBuffer.VertexBufferRHI);
					});
		}
	}
}

void USkelMeshDNAUtils::UpdateJointBehavior(USkeletalMeshComponent* InSkelMeshComponent)
{
	//DNAAsset->SetBehaviorReader is called before calling this method
	//it is not here to avoid having DNAAsset in the API, as in future we might want
	//to generalize SkelMeshUpdate to be dna-independent

	//the rig behavior has changed, we need to force re-initializing of RigLogic
	//this will set RigLogic RigUnit to initial state
	InSkelMeshComponent->InitAnim(true);
}

void USkelMeshDNAUtils::UpdateSourceData(USkeletalMesh* InSkelMesh)
{
	IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");
	// Source data must be updated during cooking.
	InSkelMesh->EmptyAllImportData();
	MeshUtilities.CreateImportDataFromLODModel(InSkelMesh);
}

UDNAAsset* USkelMeshDNAUtils::GetMeshDNA(USkeletalMesh* InSkelMesh)
{
	if (InSkelMesh && InSkelMesh->GetAssetUserDataArray())
	{
		for (UAssetUserData* UserData : *InSkelMesh->GetAssetUserDataArray())
		{
			if (UserData && UserData->IsA<UDNAAsset>())
			{
				UDNAAsset* DNAAsset = Cast<UDNAAsset>(UserData);
				if (DNAAsset->GetBehaviorReader() && DNAAsset->GetGeometryReader())
				{
					return DNAAsset;
				}
			}
		}
	}

	return nullptr;
}

#endif // WITH_EDITORONLY_DATA
