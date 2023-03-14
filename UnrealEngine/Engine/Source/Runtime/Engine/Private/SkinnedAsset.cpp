// Copyright Epic Games, Inc. All Rights Reserved.

#include "Engine/SkinnedAsset.h"
#include "SkinnedAssetCompiler.h"
#include "Animation/AnimStats.h"
#include "SkeletalRenderGPUSkin.h"
#include "PSOPrecache.h"

#if INTEL_ISPC
#include "SkinnedAsset.ispc.generated.h"
static_assert(sizeof(ispc::FTransform) == sizeof(FTransform), "sizeof(ispc::FTransform) != sizeof(FTransform)");
#endif

#if !defined(ANIM_SKINNED_ASSET_ISPC_ENABLED_DEFAULT)
#define ANIM_SKINNED_ASSET_ISPC_ENABLED_DEFAULT 1
#endif

// Support run-time toggling on supported platforms in non-shipping configurations
#if !INTEL_ISPC || UE_BUILD_SHIPPING
static constexpr bool bAnim_SkinnedAsset_ISPC_Enabled = INTEL_ISPC && ANIM_SKINNED_ASSET_ISPC_ENABLED_DEFAULT;
#else
static bool bAnim_SkinnedAsset_ISPC_Enabled = ANIM_SKINNED_ASSET_ISPC_ENABLED_DEFAULT;
static FAutoConsoleVariableRef CVarAnimSkinnedAssetISPCEnabled(TEXT("a.SkinnedAsset.ISPC"), bAnim_SkinnedAsset_ISPC_Enabled, TEXT("Whether to use ISPC optimizations on skinned assets"));
#endif

DEFINE_LOG_CATEGORY_STATIC(LogSkinnedAsset, Log, All);

void USkinnedAsset::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	FSkinnedAssetAsyncBuildScope AsyncBuildScope(this);
#endif

	FSkinnedAssetPostLoadContext Context;
	BeginPostLoadInternal(Context);

#if WITH_EDITOR
	if (FSkinnedAssetCompilingManager::Get().IsAsyncCompilationAllowed(this))
	{
		PrepareForAsyncCompilation();

		FQueuedThreadPool* ThreadPool = FSkinnedAssetCompilingManager::Get().GetThreadPool();
		EQueuedWorkPriority BasePriority = FSkinnedAssetCompilingManager::Get().GetBasePriority(this);

		AsyncTask = MakeUnique<FSkinnedAssetAsyncBuildTask>(this, MoveTemp(Context));
		AsyncTask->StartBackgroundTask(ThreadPool, BasePriority, EQueuedWorkFlags::DoNotRunInsideBusyWait);
		FSkinnedAssetCompilingManager::Get().AddSkinnedAssets({ this });
	}
	else
#endif
	{
		ExecutePostLoadInternal(Context);
		FinishPostLoadInternal(Context);
	}

	if (IsResourcePSOPrecachingEnabled() &&
		GetResourceForRendering() != nullptr)
	{
		// Assume GPU skinning when precaching PSOs
		bool bCPUSkin = false;
		ERHIFeatureLevel::Type FeatureLevel = GetWorld() ? GetWorld()->FeatureLevel.GetValue() : GMaxRHIFeatureLevel;
		int32 MinLODIndex = GetMinLodIdx();
		
		TArray<FSkinnedAssetVertexFactoryTypesPerMaterialData, TInlineAllocator<4>> VFsPerMaterials = GetVertexFactoryTypesPerMaterialIndex(MinLODIndex, bCPUSkin, FeatureLevel);
		bool bAnySectionCastsShadows = GetResourceForRendering()->AnyRenderSectionCastsShadows(MinLODIndex);

		// Precache using default material & PSO precache params but mark movable
		const TArray<FSkeletalMaterial>& Materials = GetMaterials();
		FPSOPrecacheParams PrecachePSOParams;
		PrecachePSOParams.SetMobility(EComponentMobility::Movable);
		PrecachePSOParams.bCastShadow = bAnySectionCastsShadows;

		for (auto VFsPerMaterial : VFsPerMaterials)
		{
			UMaterialInterface* MaterialInterface = Materials[VFsPerMaterial.MaterialIndex].MaterialInterface;
			if (MaterialInterface)
			{
				MaterialInterface->PrecachePSOs(VFsPerMaterial.VertexFactoryTypes, PrecachePSOParams);
			}
		}
	}
}

TArray<FSkinnedAssetVertexFactoryTypesPerMaterialData, TInlineAllocator<4>> USkinnedAsset::GetVertexFactoryTypesPerMaterialIndex(
	int32 MinLODIndex, bool bCPUSkin, ERHIFeatureLevel::Type FeatureLevel)
{
	TArray<FSkinnedAssetVertexFactoryTypesPerMaterialData, TInlineAllocator<4>> VFTypesPerMaterialIndex;

	FSkeletalMeshRenderData* SkelMeshRenderData = GetResourceForRendering();
	for (int32 LODIndex = MinLODIndex; LODIndex < SkelMeshRenderData->LODRenderData.Num(); LODIndex++)
	{
		FSkeletalMeshLODRenderData& LODRenderData = SkelMeshRenderData->LODRenderData[LODIndex];
		const FSkeletalMeshLODInfo& Info = *(GetLODInfo(LODIndex));

		// Check all render sections for the used material indices
		for (int32 SectionIndex = 0; SectionIndex < LODRenderData.RenderSections.Num(); SectionIndex++)
		{
			FSkelMeshRenderSection& RenderSection = LODRenderData.RenderSections[SectionIndex];

			// By default use the material index of the render section
			uint16 MaterialIndex = RenderSection.MaterialIndex;

			// Can be remapped by the LODInfo
			if (SectionIndex < Info.LODMaterialMap.Num() && IsValidMaterialIndex(Info.LODMaterialMap[SectionIndex]))
			{
				MaterialIndex = Info.LODMaterialMap[SectionIndex];
				MaterialIndex = FMath::Clamp(MaterialIndex, 0, GetNumMaterials());
			}

			FSkinnedAssetVertexFactoryTypesPerMaterialData* VFsPerMaterial = VFTypesPerMaterialIndex.FindByPredicate(
				[MaterialIndex](const FSkinnedAssetVertexFactoryTypesPerMaterialData& Other) { return Other.MaterialIndex == MaterialIndex; });
			if (VFsPerMaterial == nullptr)
			{
				VFsPerMaterial = &VFTypesPerMaterialIndex.AddDefaulted_GetRef();
				VFsPerMaterial->MaterialIndex = MaterialIndex;
			}

			// Find all the possible used vertex factory types needed to render this render section
			if (bCPUSkin)
			{
				// Force static from GPU point of view
				VFsPerMaterial->VertexFactoryTypes.AddUnique(&FLocalVertexFactory::StaticType);
			}
			else if (!SkelMeshRenderData->RequiresCPUSkinning(FeatureLevel, LODIndex))
			{
				// Add all the vertex factories which can be used for gpu skinning
				FSkeletalMeshObjectGPUSkin::GetUsedVertexFactories(LODRenderData, RenderSection, FeatureLevel, VFsPerMaterial->VertexFactoryTypes);
			}
		}
	}

	return VFTypesPerMaterialIndex;
}

#if WITH_EDITOR
bool USkinnedAsset::IsCompiling() const
{
	return AsyncTask != nullptr || AccessedProperties.load(std::memory_order_relaxed) != 0;
}
#endif // WITH_EDITOR

void USkinnedAsset::AcquireAsyncProperty(uint64 AsyncProperties, ESkinnedAssetAsyncPropertyLockType LockType)
{
#if WITH_EDITOR
	if ((LockType & ESkinnedAssetAsyncPropertyLockType::ReadOnly) == ESkinnedAssetAsyncPropertyLockType::ReadOnly)
	{
		AccessedProperties |= AsyncProperties;
	}

	if ((LockType & ESkinnedAssetAsyncPropertyLockType::WriteOnly) == ESkinnedAssetAsyncPropertyLockType::WriteOnly)
	{
		ModifiedProperties |= AsyncProperties;
	}
#endif
}

void USkinnedAsset::ReleaseAsyncProperty(uint64 AsyncProperties, ESkinnedAssetAsyncPropertyLockType LockType)
{
#if WITH_EDITOR
	if ((LockType & ESkinnedAssetAsyncPropertyLockType::ReadOnly) == ESkinnedAssetAsyncPropertyLockType::ReadOnly)
	{
		AccessedProperties &= ~AsyncProperties;
	}

	if ((LockType & ESkinnedAssetAsyncPropertyLockType::WriteOnly) == ESkinnedAssetAsyncPropertyLockType::WriteOnly)
	{
		ModifiedProperties &= ~AsyncProperties;
	}
#endif
}

void USkinnedAsset::WaitUntilAsyncPropertyReleasedInternal(uint64 AsyncProperties, ESkinnedAssetAsyncPropertyLockType LockType) const
{
#if WITH_EDITOR
	// We need to protect internal skinned asset data from race conditions during async build
	if (IsCompiling())
	{
		if (FSkinnedAssetAsyncBuildScope::ShouldWaitOnLockedProperties(this))
		{
			bool bIsLocked = true;
			// We can remove the lock if we're accessing in read-only and there is no write-lock
			if ((LockType & ESkinnedAssetAsyncPropertyLockType::ReadOnly) == ESkinnedAssetAsyncPropertyLockType::ReadOnly)
			{
				// Maintain the lock if the write-lock bit is non-zero
				bIsLocked &= (ModifiedProperties & AsyncProperties) != 0;
			}

			if (bIsLocked)
			{
				FString PropertyName = GetAsyncPropertyName(AsyncProperties);
				TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(TEXT("SkinnedAssetCompilationStall %s"), *PropertyName));

				if (IsInGameThread())
				{
					UE_LOG(
						LogSkinnedAsset,
						Verbose,
						TEXT("Accessing property %s of the SkinnedAsset while it is still being built asynchronously will force it to be compiled before continuing. "
							"For better performance, consider making the caller async aware so it can wait until the static mesh is ready to access this property."
							"To better understand where those calls are coming from, you can use Editor.AsyncAssetDumpStallStacks on the console."),
						*PropertyName
					);

					FSkinnedAssetCompilingManager::Get().FinishCompilation({ const_cast<USkinnedAsset*>(this) });
				}
				else
				{
					// Trying to access a property from another thread that cannot force finish the compilation is invalid
					ensureMsgf(
						false,
						TEXT("Accessing property %s of the SkinnedAsset while it is still being built asynchronously is only supported on the game-thread. "
							"To avoid any race-condition, consider finishing the compilation before pushing tasks to other threads or making higher-level game-thread code async aware so it "
							"schedules the task only when the static mesh's compilation is finished. If this is a blocker, you can disable async static mesh from the editor experimental settings."),
						*PropertyName
					);
				}
			}
		}
		// If we're accessing this property from the async build thread, make sure the property is still protected from access from other threads.
		else
		{
			bool bIsLocked = true;
			if ((LockType & ESkinnedAssetAsyncPropertyLockType::ReadOnly) == ESkinnedAssetAsyncPropertyLockType::ReadOnly)
			{
				bIsLocked &= (AccessedProperties & AsyncProperties) != 0;
			}

			if ((LockType & ESkinnedAssetAsyncPropertyLockType::WriteOnly) == ESkinnedAssetAsyncPropertyLockType::WriteOnly)
			{
				bIsLocked &= (ModifiedProperties & AsyncProperties) != 0;
			}
			ensureMsgf(bIsLocked, TEXT("Property %s has not been locked properly for use by async build"), *GetAsyncPropertyName(AsyncProperties));
		}
	}
#endif
}

extern bool bAnim_SkeletalMesh_ISPC_Enabled;
static bool IsISPCEnabled()
{
	// By default when ANIM_SKELETAL_MESH_ISPC_ENABLED_DEFAULT and CVarAnimSkeletalMeshISPCEnabled are not set, bAnim_SkeletalMesh_ISPC_Enabled should be true,
	// so when it is false it means one of the two settings are changed. To be backwards compatible, its overridden value takes priority.
	if (!bAnim_SkeletalMesh_ISPC_Enabled)
	{
#if INTEL_ISPC
		static bool bWarningLogged = false;
		if (!bWarningLogged)
		{
			UE_LOG(
				LogSkinnedAsset,
				Warning,
				TEXT("ANIM_SKELETAL_MESH_ISPC_ENABLED_DEFAULT and CVar 'a.SkeletalMesh.ISPC' are deprecated and will be removed in future releases."
					"Please switch to ANIM_SKINNED_ASSET_ISPC_ENABLED_DEFAULT and 'a.SkinnedAsset.ISPC'")
			);
			bWarningLogged = true;
		}
#endif
		return false;
	}

	// No old values detected, use the new setting
	return bAnim_SkinnedAsset_ISPC_Enabled;
}

void USkinnedAsset::FillComponentSpaceTransforms(const TArray<FTransform>& InBoneSpaceTransforms,
												 const TArray<FBoneIndexType>& InFillComponentSpaceTransformsRequiredBones, 
												 TArray<FTransform>& OutComponentSpaceTransforms) const
{
	ANIM_MT_SCOPE_CYCLE_COUNTER(FillComponentSpaceTransforms, !IsInGameThread());

	// right now all this does is populate DestSpaceBases
	check(GetRefSkeleton().GetNum() == InBoneSpaceTransforms.Num());
	check(GetRefSkeleton().GetNum() == OutComponentSpaceTransforms.Num());

	const int32 NumBones = InBoneSpaceTransforms.Num();

#if DO_GUARD_SLOW
	/** Keep track of which bones have been processed for fast look up */
	TArray<uint8, TInlineAllocator<256>> BoneProcessed;
	BoneProcessed.AddZeroed(NumBones);
#endif

	const FTransform* LocalTransformsData = InBoneSpaceTransforms.GetData();
	FTransform* ComponentSpaceData = OutComponentSpaceTransforms.GetData();

	// First bone (if we have one) is always root bone, and it doesn't have a parent.
	{
		check(InFillComponentSpaceTransformsRequiredBones.Num() == 0 || InFillComponentSpaceTransformsRequiredBones[0] == 0);
		OutComponentSpaceTransforms[0] = InBoneSpaceTransforms[0];

#if DO_GUARD_SLOW
		// Mark bone as processed
		BoneProcessed[0] = 1;
#endif
	}

	if (IsISPCEnabled())
	{
#if INTEL_ISPC
		ispc::FillComponentSpaceTransforms(
			(ispc::FTransform*)&ComponentSpaceData[0],
			(ispc::FTransform*)&LocalTransformsData[0],
			InFillComponentSpaceTransformsRequiredBones.GetData(),
			(const uint8*)GetRefSkeleton().GetRefBoneInfo().GetData(),
			sizeof(FMeshBoneInfo),
			offsetof(FMeshBoneInfo, ParentIndex),
			InFillComponentSpaceTransformsRequiredBones.Num());
#endif
	}
	else
	{
		for (int32 i = 1; i < InFillComponentSpaceTransformsRequiredBones.Num(); i++)
		{
			const int32 BoneIndex = InFillComponentSpaceTransformsRequiredBones[i];
			FTransform* SpaceBase = ComponentSpaceData + BoneIndex;

			FPlatformMisc::Prefetch(SpaceBase);

#if DO_GUARD_SLOW
			// Mark bone as processed
			BoneProcessed[BoneIndex] = 1;
#endif
			// For all bones below the root, final component-space transform is relative transform * component-space transform of parent.
			const int32 ParentIndex = GetRefSkeleton().GetParentIndex(BoneIndex);
			FTransform* ParentSpaceBase = ComponentSpaceData + ParentIndex;
			FPlatformMisc::Prefetch(ParentSpaceBase);

#if DO_GUARD_SLOW
			// Check the precondition that Parents occur before Children in the RequiredBones array.
			checkSlow(BoneProcessed[ParentIndex] == 1);
#endif
			FTransform::Multiply(SpaceBase, LocalTransformsData + BoneIndex, ParentSpaceBase);

			SpaceBase->NormalizeRotation();

			checkSlow(SpaceBase->IsRotationNormalized());
			checkSlow(!SpaceBase->ContainsNaN());
		}
	}
}