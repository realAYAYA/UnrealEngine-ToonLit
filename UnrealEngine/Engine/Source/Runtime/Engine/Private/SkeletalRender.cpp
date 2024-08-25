// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SkeletalRender.cpp: Skeletal mesh skinning/rendering code.
=============================================================================*/

#include "SkeletalRender.h"
#include "Engine/SkinnedAsset.h"
#include "Engine/SkinnedAssetCommon.h"
#include "SkeletalRenderPublic.h"
#include "SceneManagement.h"
#include "SceneView.h"
#include "GPUSkinCache.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Engine/RendererSettings.h"


/*-----------------------------------------------------------------------------
Globals
-----------------------------------------------------------------------------*/

// smallest blend weight for vertex anims
const float MinMorphTargetBlendWeight = UE_SMALL_NUMBER;
// largest blend weight for vertex anims
const float MaxMorphTargetBlendWeight = 5.0f;

static float GMorphTargetMaxBlendWeight = 5.f;
static FAutoConsoleVariableRef CVarMorphTargetMinBlendWeight(
	TEXT("r.MorphTarget.MaxBlendWeight"),
	GMorphTargetMaxBlendWeight,
	TEXT("Maximum value accepted as a morph target blend weight..\n")
	TEXT("Blend target weights will be checked against this value for validation.Values smaller than this number will be clamped.\n"),
	ECVF_Default
);

namespace UE::SkeletalRender::Settings
{
	float GetMorphTargetMaxBlendWeight()
	{
		static const IConsoleVariable* MorphTargetMaxBlendWeightCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MorphTarget.MaxBlendWeight"));

		return (MorphTargetMaxBlendWeightCVar != nullptr) ? MorphTargetMaxBlendWeightCVar->GetFloat() : GetDefault<URendererSettings>()->MorphTargetMaxBlendWeight;
	}
}

#if RHI_RAYTRACING
static bool IsSkeletalMeshRayTracingSupported()
{
	static const auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing.Geometry.SupportSkeletalMeshes"));
	static const bool SupportSkeletalMeshes = CVar->GetInt() != 0;
	return SupportSkeletalMeshes;
}
#endif // RHI_RAYTRACING

static TAutoConsoleVariable<bool> CVarSkeletalMeshClothBlendEnabled(TEXT("r.SkeletalMeshClothBlend.Enabled"), true, TEXT("Enable the use of the cloth blend weight value set by the skeletal mesh component. When disabled all cloth blend weight will become 0."));

/*-----------------------------------------------------------------------------
FSkeletalMeshObject
-----------------------------------------------------------------------------*/

FSkeletalMeshObject::FSkeletalMeshObject(USkinnedMeshComponent* InMeshComponent, FSkeletalMeshRenderData* InSkelMeshRenderData, ERHIFeatureLevel::Type InFeatureLevel)
:	MinDesiredLODLevel(InMeshComponent->GetPredictedLODLevel())
,	MaxDistanceFactor(0.f)
,	WorkingMinDesiredLODLevel(MinDesiredLODLevel)
,	WorkingMaxDistanceFactor(0.f)
,   bHasBeenUpdatedAtLeastOnce(false)
#if RHI_RAYTRACING
, bSupportRayTracing(IsSkeletalMeshRayTracingSupported() && InMeshComponent->GetSkinnedAsset()->GetSupportRayTracing())
, bHiddenMaterialVisibilityDirtyForRayTracing(false)
, RayTracingMinLOD(InMeshComponent->GetSkinnedAsset()->GetRayTracingMinLOD())
#endif
#if !UE_BUILD_SHIPPING
, DebugName(InMeshComponent->GetSkinnedAsset()->GetFName())
#endif // !UE_BUILD_SHIPPING
#if WITH_EDITORONLY_DATA
,   SectionIndexPreview(InMeshComponent->GetSectionPreview())
,   MaterialIndexPreview(InMeshComponent->GetMaterialPreview())
,	SelectedEditorSection(InMeshComponent->GetSelectedEditorSection())
,	SelectedEditorMaterial(InMeshComponent->GetSelectedEditorMaterial())
#endif	
,	SkeletalMeshRenderData(InSkelMeshRenderData)
,	SkeletalMeshLODInfo(InMeshComponent->GetSkinnedAsset()->GetLODInfoArray())
,	SkinCacheEntry(nullptr)
,	SkinCacheEntryForRayTracing(nullptr)
,	LastFrameNumber(0)
,	bUsePerBoneMotionBlur(InMeshComponent->bPerBoneMotionBlur)
,	StatId(InMeshComponent->GetSkinnedAsset()->GetStatID(true))
,	FeatureLevel(InFeatureLevel)
,	ComponentId(InMeshComponent->GetPrimitiveSceneId().PrimIDValue)
,	WorldScale(InMeshComponent->GetComponentScale())
#if RHI_ENABLE_RESOURCE_INFO
,	AssetPathName(InMeshComponent->GetSkinnedAsset()->GetPathName())
#endif
{
	check(SkeletalMeshRenderData);

#if WITH_EDITORONLY_DATA
	if ( !GIsEditor )
	{
		SectionIndexPreview = -1;
		MaterialIndexPreview = -1;
	}
#endif // #if WITH_EDITORONLY_DATA

	// We want to restore the most recent value of the MaxDistanceFactor the SkeletalMeshComponent
	// cached, which will be 0.0 when first created, and a valid, updated value when recreating
	// this mesh object (e.g. during a component reregister), avoiding issues with a transient
	// assignment of 0.0 and then back to an updated value the next frame.
	MaxDistanceFactor = InMeshComponent->MaxDistanceFactor;
	WorkingMaxDistanceFactor = MaxDistanceFactor;

	InitLODInfos(InMeshComponent);
}

FSkeletalMeshObject::~FSkeletalMeshObject()
{
}

void FSkeletalMeshObject::UpdateMinDesiredLODLevel(const FSceneView* View, const FBoxSphereBounds& Bounds, int32 FrameNumber)
{
	// Thumbnail rendering doesn't contribute to MinDesiredLODLevel calculation
	if (View->Family && (View->Family->bThumbnailRendering || !View->Family->GetIsInFocus()))
	{
		return;
	}

	static const auto* SkeletalMeshLODRadiusScale = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.SkeletalMeshLODRadiusScale"));
	float LODScale = FMath::Clamp(SkeletalMeshLODRadiusScale->GetValueOnRenderThread(), 0.25f, 1.0f);

	const float ScreenRadiusSquared = ComputeBoundsScreenRadiusSquared(Bounds.Origin, Bounds.SphereRadius, *View) * LODScale * LODScale;

	checkf( SkeletalMeshLODInfo.Num() == SkeletalMeshRenderData->LODRenderData.Num(), TEXT("Mismatched LOD arrays. SkeletalMeshLODInfo.Num() = %d, SkeletalMeshRenderData->LODRenderData.Num() = %d"), SkeletalMeshLODInfo.Num(), SkeletalMeshRenderData->LODRenderData.Num());

	// Need the current LOD
	const int32 CurrentLODLevel = GetLOD();
	const float HysteresisOffset = 0.f;

	int32 NewLODLevel = 0;

	// Look for a lower LOD if the EngineShowFlags is enabled
	if( View->Family && 1==View->Family->EngineShowFlags.LOD )
	{
		// Iterate from worst to best LOD
		for(int32 LODLevel = SkeletalMeshRenderData->LODRenderData.Num()-1; LODLevel > 0; LODLevel--)
		{
			// Get ScreenSize for this LOD
			float ScreenSize = SkeletalMeshLODInfo[LODLevel].ScreenSize.GetValue();

			// If we are considering shifting to a better (lower) LOD, bias with hysteresis.
			if(LODLevel  <= CurrentLODLevel)
			{
				ScreenSize += SkeletalMeshLODInfo[LODLevel].LODHysteresis;
			}

			// If have passed this boundary, use this LOD
			if(FMath::Square(ScreenSize * 0.5f) > ScreenRadiusSquared)
			{
				NewLODLevel = LODLevel;
				break;
			}
		}
	}

	if (!LastFrameNumber)
	{
		// We don't have last frame value on the first call to FSkeletalMeshObject::UpdateMinDesiredLODLevel so
		// just reuse current frame value. Otherwise, MinDesiredLODLevel may get stale value in the update code below
		WorkingMinDesiredLODLevel = NewLODLevel;
	}

	// Different path for first-time vs subsequent-times in this function (ie splitscreen)
	if(FrameNumber != LastFrameNumber)
	{
		// Copy last frames value to the version that will be read by game thread
		MaxDistanceFactor = WorkingMaxDistanceFactor;
		MinDesiredLODLevel = WorkingMinDesiredLODLevel;
		LastFrameNumber = FrameNumber;

		WorkingMaxDistanceFactor = ScreenRadiusSquared;
		WorkingMinDesiredLODLevel = NewLODLevel;
	}
	else
	{
		WorkingMaxDistanceFactor = FMath::Max(WorkingMaxDistanceFactor, ScreenRadiusSquared);
		WorkingMinDesiredLODLevel = FMath::Min(WorkingMinDesiredLODLevel, NewLODLevel);
	}
}

/**
 * List of chunks to be rendered based on instance weight usage. Full swap of weights will render with its own chunks.
 * @return Chunks to iterate over for rendering
 */
const TArray<FSkelMeshRenderSection>& FSkeletalMeshObject::GetRenderSections(int32 InLODIndex) const
{
	const FSkeletalMeshLODRenderData& LOD = SkeletalMeshRenderData->LODRenderData[InLODIndex];
	return LOD.RenderSections;
}

FColor FSkeletalMeshObject::GetSkinCacheVisualizationDebugColor(const FName& GPUSkinCacheVisualizationMode, uint32 SectionIndex) const
{
	FGPUSkinCacheEntry* RTEntry = nullptr;
#if RHI_RAYTRACING
	RTEntry = GetSkinCacheEntryForRayTracing();
#endif
	return FGPUSkinCache::GetVisualizationDebugColor(GPUSkinCacheVisualizationMode, SkinCacheEntry, RTEntry, SectionIndex);

}

/**
 * Update the hidden material section flags for an LOD entry
 *
 * @param InLODIndex - LOD entry to update hidden material flags for
 * @param HiddenMaterials - array of hidden material sections
 */
void FSkeletalMeshObject::SetHiddenMaterials(int32 InLODIndex,const TArray<bool>& HiddenMaterials)
{
	check(LODInfo.IsValidIndex(InLODIndex));
#if RHI_RAYTRACING
	bHiddenMaterialVisibilityDirtyForRayTracing = true;
#endif
	LODInfo[InLODIndex].HiddenMaterials = HiddenMaterials;
}

/**
 * Determine if the material section entry for an LOD is hidden or not
 *
 * @param InLODIndex - LOD entry to get hidden material flags for
 * @param MaterialIdx - index of the material section to check
 */
bool FSkeletalMeshObject::IsMaterialHidden(int32 InLODIndex,int32 MaterialIdx) const
{
	check(LODInfo.IsValidIndex(InLODIndex));
	return LODInfo[InLODIndex].HiddenMaterials.IsValidIndex(MaterialIdx) && LODInfo[InLODIndex].HiddenMaterials[MaterialIdx];
}
/**
 * Initialize the array of LODInfo based on the settings of the current skel mesh component
 */
void FSkeletalMeshObject::InitLODInfos(const USkinnedMeshComponent* SkelComponent)
{
	LODInfo.Empty(SkeletalMeshLODInfo.Num());
	for (int32 Idx=0; Idx < SkeletalMeshLODInfo.Num(); Idx++)
	{
		FSkelMeshObjectLODInfo& MeshLODInfo = *new(LODInfo) FSkelMeshObjectLODInfo();
		if (SkelComponent->LODInfo.IsValidIndex(Idx))
		{
			const FSkelMeshComponentLODInfo &Info = SkelComponent->LODInfo[Idx];

			MeshLODInfo.HiddenMaterials = Info.HiddenMaterials;
		}		
	}
}

float FSkeletalMeshObject::GetScreenSize(int32 LODIndex) const
{
	if (SkeletalMeshLODInfo.IsValidIndex(LODIndex))
	{
		return SkeletalMeshLODInfo[LODIndex].ScreenSize.GetValue();
	}
	return 0.f;
}

FName FSkeletalMeshObject::GetAssetPathName(int32 LODIndex) const
{
#if RHI_ENABLE_RESOURCE_INFO
	if (LODIndex > -1)
	{
		return FName(FString::Printf(TEXT("%s [LOD%d]"), *AssetPathName.ToString(), LODIndex));
	}
	else
	{
		return AssetPathName;
	}
#else
	return NAME_None;
#endif
}

FSkinWeightVertexBuffer* FSkeletalMeshObject::GetSkinWeightVertexBuffer(FSkeletalMeshLODRenderData& LODData, FSkelMeshComponentLODInfo* CompLODInfo)
{
	// If we have a skin weight override buffer (and it's the right size) use it
	if (CompLODInfo && CompLODInfo->OverrideSkinWeights &&
		CompLODInfo->OverrideSkinWeights->GetNumVertices() == LODData.StaticVertexBuffers.PositionVertexBuffer.GetNumVertices())
	{
		check(LODData.SkinWeightVertexBuffer.GetMaxBoneInfluences() == CompLODInfo->OverrideSkinWeights->GetMaxBoneInfluences());
		return CompLODInfo->OverrideSkinWeights;
	}
	else if (CompLODInfo && CompLODInfo->OverrideProfileSkinWeights &&
		CompLODInfo->OverrideProfileSkinWeights->GetNumVertices() == LODData.StaticVertexBuffers.PositionVertexBuffer.GetNumVertices())
	{
		check(LODData.SkinWeightVertexBuffer.GetMaxBoneInfluences() == CompLODInfo->OverrideProfileSkinWeights->GetMaxBoneInfluences());
		return CompLODInfo->OverrideProfileSkinWeights;
	}
	else
	{
		return LODData.GetSkinWeightVertexBuffer();
	}
}

FColorVertexBuffer* FSkeletalMeshObject::GetColorVertexBuffer(FSkeletalMeshLODRenderData& LODData, FSkelMeshComponentLODInfo* CompLODInfo)
{
	// If we have a vertex color override buffer (and it's the right size) use it
	if (CompLODInfo && CompLODInfo->OverrideVertexColors &&
		CompLODInfo->OverrideVertexColors->GetNumVertices() == LODData.StaticVertexBuffers.PositionVertexBuffer.GetNumVertices())
	{
		return CompLODInfo->OverrideVertexColors;
	}
	else
	{
		return &LODData.StaticVertexBuffers.ColorVertexBuffer;
	}
}

/*-----------------------------------------------------------------------------
Global functions
-----------------------------------------------------------------------------*/

void UpdateRefToLocalMatricesInner(TArray<FMatrix44f>& ReferenceToLocal, const TArray<FTransform>& ComponentTransform, const TArray<uint8>& BoneVisibilityStates, const TArray<int32>* LeaderBoneMap,
	const TArray<FMatrix44f>* RefBasesInvMatrix, const FReferenceSkeleton& RefSkeleton, const FSkeletalMeshRenderData* InSkeletalMeshRenderData, int32 LODIndex, const TArray<FBoneIndexType>* ExtraRequiredBoneIndices)
{
	const FSkeletalMeshLODRenderData& LOD = InSkeletalMeshRenderData->LODRenderData[LODIndex];

	const TArray<FBoneIndexType>* RequiredBoneSets[3] = { &LOD.ActiveBoneIndices, ExtraRequiredBoneIndices, NULL };

	const bool bBoneVisibilityStatesValid = BoneVisibilityStates.Num() == ComponentTransform.Num();
	const bool bIsLeaderCompValid = LeaderBoneMap != nullptr;
	
	for (int32 RequiredBoneSetIndex = 0; RequiredBoneSets[RequiredBoneSetIndex] != NULL; RequiredBoneSetIndex++)
	{
		const TArray<FBoneIndexType>& RequiredBoneIndices = *RequiredBoneSets[RequiredBoneSetIndex];

		// Get the index of the bone in this skeleton, and loop up in table to find index in parent component mesh.
		for (int32 BoneIndex = 0; BoneIndex < RequiredBoneIndices.Num(); BoneIndex++)
		{
			const int32 ThisBoneIndex = RequiredBoneIndices[BoneIndex];

			if ( RefBasesInvMatrix->IsValidIndex(ThisBoneIndex) )
			{
				// On the off chance the parent matrix isn't valid, revert to identity.
				ReferenceToLocal[ThisBoneIndex] = FMatrix44f::Identity;

				//if we have Leader pose component, we use LeaderBoneMap to figure out the mapping
				if( bIsLeaderCompValid )
				{
					// If valid, use matrix from parent component.
					const int32 LeaderBoneIndex = (*LeaderBoneMap)[ThisBoneIndex];
					if (ComponentTransform.IsValidIndex(LeaderBoneIndex))
					{
						const int32 ParentIndex = RefSkeleton.GetParentIndex(ThisBoneIndex);
						bool bNeedToHideBone = BoneVisibilityStates[LeaderBoneIndex] != BVS_Visible;
						if (bNeedToHideBone && ParentIndex != INDEX_NONE)
						{
							ReferenceToLocal[ThisBoneIndex] = ReferenceToLocal[ParentIndex].ApplyScale(0.f);
						}
						else
						{
							checkSlow(ComponentTransform[LeaderBoneIndex].IsRotationNormalized());
							ReferenceToLocal[ThisBoneIndex] = (FMatrix44f)ComponentTransform[LeaderBoneIndex].ToMatrixWithScale();
						}
					}
					else
					{
						const int32 ParentIndex = RefSkeleton.GetParentIndex(ThisBoneIndex);
						const FMatrix44f RefLocalPose = (FMatrix44f)RefSkeleton.GetRefBonePose()[ThisBoneIndex].ToMatrixWithScale();
						if (ParentIndex != INDEX_NONE)
						{
							ReferenceToLocal[ThisBoneIndex] = RefLocalPose * ReferenceToLocal[ParentIndex];
						}
						else
						{
							ReferenceToLocal[ThisBoneIndex] = RefLocalPose;
						}
					}
				}
				else
				{
					if (ComponentTransform.IsValidIndex(ThisBoneIndex))
					{
						if (bBoneVisibilityStatesValid)
						{
							// If we can't find this bone in the parent, we just use the reference pose.
							const int32 ParentIndex = RefSkeleton.GetParentIndex(ThisBoneIndex);
							bool bNeedToHideBone = BoneVisibilityStates[ThisBoneIndex] != BVS_Visible;
							if (bNeedToHideBone && ParentIndex != INDEX_NONE)
							{
								ReferenceToLocal[ThisBoneIndex] = ReferenceToLocal[ParentIndex].ApplyScale(0.f);
							}
							else
							{
								checkSlow(ComponentTransform[ThisBoneIndex].IsRotationNormalized());
								ReferenceToLocal[ThisBoneIndex] = (FMatrix44f)ComponentTransform[ThisBoneIndex].ToMatrixWithScale();
							}
						}
						else
						{
							checkSlow(ComponentTransform[ThisBoneIndex].IsRotationNormalized());
							ReferenceToLocal[ThisBoneIndex] = (FMatrix44f)ComponentTransform[ThisBoneIndex].ToMatrixWithScale();
						}
					}
				}
			}
			// removed else statement to set ReferenceToLocal[ThisBoneIndex] = FTransform::Identity;
			// since it failed in ( ThisMesh->RefBasesInvMatrix.IsValidIndex(ThisBoneIndex) ), ReferenceToLocal is not valid either
			// because of the initialization code line above to match both array count
			// if(ReferenceToLocal.Num() != ThisMesh->RefBasesInvMatrix.Num())					
		}
	}

	for (int32 ThisBoneIndex = 0; ThisBoneIndex < ReferenceToLocal.Num(); ++ThisBoneIndex)
	{
		ReferenceToLocal[ThisBoneIndex] = (*RefBasesInvMatrix)[ThisBoneIndex] * ReferenceToLocal[ThisBoneIndex];
	}
}
/**
 * Utility function that fills in the array of ref-pose to local-space matrices using 
 * the mesh component's updated space bases
 * @param	ReferenceToLocal - matrices to update
 * @param	SkeletalMeshComponent - mesh primitive with updated bone matrices
 * @param	LODIndex - each LOD has its own mapping of bones to update
 * @param	ExtraRequiredBoneIndices - any extra bones apart from those active in the LOD that we'd like to update
 */
void UpdateRefToLocalMatrices( TArray<FMatrix44f>& ReferenceToLocal, const USkinnedMeshComponent* InMeshComponent, const FSkeletalMeshRenderData* InSkeletalMeshRenderData, int32 LODIndex, const TArray<FBoneIndexType>* ExtraRequiredBoneIndices )
{
	const USkinnedAsset* const SkinnedAsset = InMeshComponent->GetSkinnedAsset();
	const USkinnedMeshComponent* const LeaderComp = InMeshComponent->LeaderPoseComponent.Get();
	const FSkeletalMeshLODRenderData& LOD = InSkeletalMeshRenderData->LODRenderData[LODIndex];

	const FReferenceSkeleton& RefSkeleton = SkinnedAsset->GetRefSkeleton();
	const TArray<int32>& LeaderBoneMap = InMeshComponent->GetLeaderBoneMap();
	const bool bIsLeaderCompValid = LeaderComp && LeaderBoneMap.Num() == SkinnedAsset->GetRefSkeleton().GetNum();
	const TArray<FTransform>& ComponentTransform = (bIsLeaderCompValid)? LeaderComp->GetComponentSpaceTransforms() : InMeshComponent->GetComponentSpaceTransforms();
	const TArray<uint8>& BoneVisibilityStates = (bIsLeaderCompValid) ? LeaderComp->GetBoneVisibilityStates() : InMeshComponent->GetBoneVisibilityStates();
	// Get inv ref pose matrices
	const TArray<FMatrix44f>* RefBasesInvMatrix = &SkinnedAsset->GetRefBasesInvMatrix();

	// Check if there is an override (and it's the right size)
	if( InMeshComponent->GetRefPoseOverride() && 
		InMeshComponent->GetRefPoseOverride()->RefBasesInvMatrix.Num() == RefBasesInvMatrix->Num() )
	{
		RefBasesInvMatrix = &InMeshComponent->GetRefPoseOverride()->RefBasesInvMatrix;
	}

	check( RefBasesInvMatrix->Num() != 0 );

	if(ReferenceToLocal.Num() != RefBasesInvMatrix->Num())
	{
		ReferenceToLocal.Empty(RefBasesInvMatrix->Num());
		ReferenceToLocal.AddUninitialized(RefBasesInvMatrix->Num());

		for (int32 Index = 0; Index < ReferenceToLocal.Num(); ++Index)
		{
			ReferenceToLocal[Index] = FMatrix44f::Identity;
		}
	}

	if (!InSkeletalMeshRenderData->LODRenderData.IsValidIndex(LODIndex))
	{
		UE_LOG(LogSkeletalMesh, Error,
			TEXT("Mesh %s : Invalid LODIndex [count %d, index %d], streaming[Ready(%d), F(%d), P(%d)], \
			ExtraRequiredBoneIndices is (%d), and total number is (%d)"), *GetNameSafe(SkinnedAsset),
			InSkeletalMeshRenderData->LODRenderData.Num(), LODIndex, InSkeletalMeshRenderData->bReadyForStreaming,
			InSkeletalMeshRenderData->CurrentFirstLODIdx, InSkeletalMeshRenderData->PendingFirstLODIdx,
			(ExtraRequiredBoneIndices) ? 1 : 0, (ExtraRequiredBoneIndices) ? ExtraRequiredBoneIndices->Num() : 0);

		for (int32 Index = 0; Index < ReferenceToLocal.Num(); ++Index)
		{
			ReferenceToLocal[Index] = FMatrix44f::Identity;
		}

		return;
	}

	UpdateRefToLocalMatricesInner(ReferenceToLocal, ComponentTransform, BoneVisibilityStates, (bIsLeaderCompValid)? &LeaderBoneMap : nullptr, RefBasesInvMatrix, RefSkeleton, InSkeletalMeshRenderData, LODIndex, ExtraRequiredBoneIndices);
}

void UpdatePreviousRefToLocalMatrices(TArray<FMatrix44f>& ReferenceToLocal, const USkinnedMeshComponent* InMeshComponent, const FSkeletalMeshRenderData* InSkeletalMeshRenderData, int32 LODIndex, const TArray<FBoneIndexType>* ExtraRequiredBoneIndices)
{
	const USkinnedAsset* const ThisMesh = InMeshComponent->GetSkinnedAsset();
	const USkinnedMeshComponent* const LeaderComp = InMeshComponent->LeaderPoseComponent.Get();
	const FSkeletalMeshLODRenderData& LOD = InSkeletalMeshRenderData->LODRenderData[LODIndex];

	const FReferenceSkeleton& RefSkeleton = ThisMesh->GetRefSkeleton();
	const TArray<int32>& LeaderBoneMap = InMeshComponent->GetLeaderBoneMap();
	const bool bIsLeaderCompValid = LeaderComp && LeaderBoneMap.Num() == ThisMesh->GetRefSkeleton().GetNum();
	const TArray<FTransform>& ComponentTransform = (bIsLeaderCompValid) ? LeaderComp->GetPreviousComponentTransformsArray() : InMeshComponent->GetPreviousComponentTransformsArray();
	const TArray<uint8>& BoneVisibilityStates = (bIsLeaderCompValid) ? LeaderComp->GetPreviousBoneVisibilityStates() : InMeshComponent->GetPreviousBoneVisibilityStates();
	// Get inv ref pose matrices
	const TArray<FMatrix44f>* RefBasesInvMatrix = &ThisMesh->GetRefBasesInvMatrix();
	// Check if there is an override (and it's the right size)
	if (InMeshComponent->GetRefPoseOverride() &&
		InMeshComponent->GetRefPoseOverride()->RefBasesInvMatrix.Num() == RefBasesInvMatrix->Num())
	{
		RefBasesInvMatrix = &InMeshComponent->GetRefPoseOverride()->RefBasesInvMatrix;
	}

	check(RefBasesInvMatrix->Num() != 0);

	if (ReferenceToLocal.Num() != RefBasesInvMatrix->Num())
	{
		ReferenceToLocal.Empty(RefBasesInvMatrix->Num());
		ReferenceToLocal.AddUninitialized(RefBasesInvMatrix->Num());

		for (int32 Index = 0; Index < ReferenceToLocal.Num(); ++Index)
		{
			ReferenceToLocal[Index] = FMatrix44f::Identity;
		}
	}

	if (!InSkeletalMeshRenderData->LODRenderData.IsValidIndex(LODIndex))
	{
		UE_LOG(LogSkeletalMesh, Error,
			TEXT("Mesh %s : Invalid LODIndex [count %d, index %d], streaming[Ready(%d), F(%d), P(%d)], \
			ExtraRequiredBoneIndices is (%d), and total number is (%d)"), *GetNameSafe(ThisMesh),
			InSkeletalMeshRenderData->LODRenderData.Num(), LODIndex, InSkeletalMeshRenderData->bReadyForStreaming,
			InSkeletalMeshRenderData->CurrentFirstLODIdx, InSkeletalMeshRenderData->PendingFirstLODIdx,
			(ExtraRequiredBoneIndices) ? 1 : 0, (ExtraRequiredBoneIndices) ? ExtraRequiredBoneIndices->Num() : 0);

		for (int32 Index = 0; Index < ReferenceToLocal.Num(); ++Index)
		{
			ReferenceToLocal[Index] = FMatrix44f::Identity;
		}

		return;
	}
	UpdateRefToLocalMatricesInner(ReferenceToLocal, ComponentTransform, BoneVisibilityStates, (bIsLeaderCompValid) ? &LeaderBoneMap : nullptr, RefBasesInvMatrix, RefSkeleton, InSkeletalMeshRenderData, LODIndex, ExtraRequiredBoneIndices);
}

bool IsSkeletalMeshClothBlendEnabled()
{
	return CVarSkeletalMeshClothBlendEnabled.GetValueOnAnyThread();
}

