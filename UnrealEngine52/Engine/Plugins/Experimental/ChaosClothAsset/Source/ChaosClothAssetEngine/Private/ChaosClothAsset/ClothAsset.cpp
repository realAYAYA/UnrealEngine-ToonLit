// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/ClothAsset.h"
#include "ChaosClothAsset/ClothAssetBuilder.h"
#include "ChaosClothAsset/ClothAdapter.h"
#include "ChaosClothAsset/ClothComponent.h"
#include "ChaosClothAsset/ClothGeometryTools.h"
#include "ChaosClothAsset/ClothAssetPrivate.h"
#include "ChaosClothAsset/ClothSimulationModel.h"
#include "Animation/Skeleton.h"
#include "Engine/RendererSettings.h"
#include "Engine/SkinnedAssetAsyncCompileUtils.h"
#include "Features/IModularFeatures.h"
#include "Rendering/SkeletalMeshModel.h"
#include "EngineUtils.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "UObject/Package.h"
#if WITH_EDITOR
#include "IMeshBuilderModule.h"
#include "DerivedDataCacheInterface.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(ClothAsset)

// If Chaos cloth asset derived data needs to be rebuilt (new format, serialization differences, etc.) replace the version GUID below with a new one. 
// In case of merge conflicts with DDC versions, you MUST generate a new GUID and set this new GUID as the version.
#define CHAOS_CLOTH_ASSET_DERIVED_DATA_VERSION TEXT("19823D996CA54F279B9A9FA8ED7A8EB6")

UChaosClothAsset::UChaosClothAsset(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, DisableBelowMinLodStripping(FPerPlatformBool(false))
	, ClothCollection(MakeShared<UE::Chaos::ClothAsset::FClothCollection>())
#if WITH_EDITORONLY_DATA
	, MeshModel(MakeShareable(new FSkeletalMeshModel()))
#endif
{
}

UChaosClothAsset::UChaosClothAsset(FVTableHelper& Helper)
	: Super(Helper)
{
}

UChaosClothAsset::~UChaosClothAsset() = default;

FSkeletalMeshLODInfo* UChaosClothAsset::GetLODInfo(int32 Index)
{
	return LODInfo.IsValidIndex(Index) ? &LODInfo[Index] : nullptr;
}

const FSkeletalMeshLODInfo* UChaosClothAsset::GetLODInfo(int32 Index) const
{
	return LODInfo.IsValidIndex(Index) ? &LODInfo[Index] : nullptr;
}

FMatrix UChaosClothAsset::GetComposedRefPoseMatrix(FName InBoneName) const
{
	FMatrix LocalPose(FMatrix::Identity);

	if (InBoneName != NAME_None)
	{
		const int32 BoneIndex = GetRefSkeleton().FindBoneIndex(InBoneName);
		if (BoneIndex != INDEX_NONE)
		{
			return GetComposedRefPoseMatrix(BoneIndex);
		}
		// TODO: Might need to add sockets like on the SkeletalMesh
	}

	return LocalPose;
}

void UChaosClothAsset::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	bool bCooked = Ar.IsCooking();
	Ar << bCooked;

	ClothCollection->Serialize(Ar);

	Ar << GetRefSkeleton();
	if (Ar.IsLoading())
	{
		constexpr bool bRebuildNameMap = false;
		GetRefSkeleton().RebuildRefSkeleton(GetSkeleton(), bRebuildNameMap);
	}

	if (bCooked && !IsTemplate() && !Ar.IsCountingMemory())
	{
		if (Ar.IsLoading())
		{
			SetResourceForRendering(MakeUnique<FSkeletalMeshRenderData>());
		}
		GetResourceForRendering()->Serialize(Ar, this);
	}
}

void UChaosClothAsset::PostLoad()
{
	Super::PostLoad();
}

#if WITH_EDITOR
void UChaosClothAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UChaosClothAsset, PhysicsAsset))
	{
		ReregisterComponents();
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif // #if WITH_EDITOR

void UChaosClothAsset::BeginPostLoadInternal(FSkinnedAssetPostLoadContext& Context)
{
#if WITH_EDITOR
	TRACE_CPUPROFILER_EVENT_SCOPE(UChaosClothAsset::BeginPostLoadInternal);

	checkf(IsInGameThread(), TEXT("Cannot execute function UChaosClothAsset::BeginPostLoadInternal asynchronously. Asset: %s"), *this->GetFullName());
	SetInternalFlags(EInternalObjectFlags::Async);

	// Lock all properties that should not be modified/accessed during async post-load
	AcquireAsyncProperty();

	// This scope allows us to use any locked properties without causing stalls
	FSkinnedAssetAsyncBuildScope AsyncBuildScope(this);

	BuildClothSimulationModel();  // TODO: Cache ClothSimulationModel?

	BuildMeshModel();
#endif // #if WITH_EDITOR
}

void UChaosClothAsset::ExecutePostLoadInternal(FSkinnedAssetPostLoadContext& Context)
{
#if WITH_EDITOR
	TRACE_CPUPROFILER_EVENT_SCOPE(UChaosClothAsset::ExecutePostLoadInternal);

	// This scope allows us to use any locked properties without causing stalls
	FSkinnedAssetAsyncBuildScope AsyncBuildScope(this);

	if (!GetOutermost()->bIsCookedForEditor)
	{
		if (GetResourceForRendering() == nullptr)
		{
			CacheDerivedData(&Context);
			Context.bHasCachedDerivedData = true;
		}
	}
#endif // WITH_EDITOR
}

void UChaosClothAsset::FinishPostLoadInternal(FSkinnedAssetPostLoadContext& Context)
{
#if WITH_EDITOR
	TRACE_CPUPROFILER_EVENT_SCOPE(UChaosClothAsset::FinishPostLoadInternal);

	checkf(IsInGameThread(), TEXT("Cannot execute function UChaosClothAsset::FinishPostLoadInternal asynchronously. Asset: %s"), *this->GetFullName());
	ClearInternalFlags(EInternalObjectFlags::Async);

	// This scope allows us to use any locked properties without causing stalls
	FSkinnedAssetAsyncBuildScope AsyncBuildScope(this);
#endif

	if (FApp::CanEverRender())
	{
		InitResources();
	}

	CalculateInvRefMatrices();
	CalculateBounds();

#if WITH_EDITOR
	ReleaseAsyncProperty();
#endif
}

void UChaosClothAsset::BeginDestroy()
{
	check(IsInGameThread());

	Super::BeginDestroy();

	// Release the mesh's render resources now
	ReleaseResources();
}

bool UChaosClothAsset::IsReadyForFinishDestroy()
{
	if (!Super::IsReadyForFinishDestroy())
	{
		return false;
	}

	ReleaseResources();

	// see if we have hit the resource flush fence
	return ReleaseResourcesFence.IsFenceComplete();
}

void UChaosClothAsset::InitResources()
{
	LLM_SCOPE_BYNAME(TEXT("ClothAsset/InitResources"));

	if (SkeletalMeshRenderData.IsValid())
	{
		TArray<UMorphTarget*> DummyMorphTargets;  // Not even used by InitResources() at the moment
		SkeletalMeshRenderData->InitResources(false, DummyMorphTargets, this);
	}
}

void UChaosClothAsset::ReleaseResources()
{
	if (SkeletalMeshRenderData && SkeletalMeshRenderData->IsInitialized())
	{
		if (GIsEditor && !GIsPlayInEditorWorld)
		{
			// Flush the rendering command to be sure there is no command left that can create/modify a rendering resource
			FlushRenderingCommands();
		}

		SkeletalMeshRenderData->ReleaseResources();

		// Insert a fence to signal when these commands completed
		ReleaseResourcesFence.BeginFence();
	}
}

void UChaosClothAsset::CalculateInvRefMatrices()
{
	auto GetRefPoseMatrix = [this](int32 BoneIndex)->FMatrix
	{
		check(BoneIndex >= 0 && BoneIndex < RefSkeleton.GetRawBoneNum());
		FTransform BoneTransform = RefSkeleton.GetRawRefBonePose()[BoneIndex];
		BoneTransform.NormalizeRotation();  // Make sure quaternion is normalized!
		return BoneTransform.ToMatrixWithScale();
	};

	const int32 NumRealBones = RefSkeleton.GetRawBoneNum();

	RefBasesInvMatrix.Empty(NumRealBones);
	RefBasesInvMatrix.AddUninitialized(NumRealBones);

	// Reset cached mesh-space ref pose
	TArray<FMatrix> ComposedRefPoseMatrices;
	ComposedRefPoseMatrices.SetNumUninitialized(NumRealBones);

	// Precompute the Mesh.RefBasesInverse
	for (int32 BoneIndex = 0; BoneIndex < NumRealBones; ++BoneIndex)
	{
		// Render the default pose
		ComposedRefPoseMatrices[BoneIndex] = GetRefPoseMatrix(BoneIndex);

		// Construct mesh-space skeletal hierarchy
		if (BoneIndex > 0)
		{
			int32 Parent = RefSkeleton.GetRawParentIndex(BoneIndex);
			ComposedRefPoseMatrices[BoneIndex] = ComposedRefPoseMatrices[BoneIndex] * ComposedRefPoseMatrices[Parent];
		}

		FVector XAxis, YAxis, ZAxis;
		ComposedRefPoseMatrices[BoneIndex].GetScaledAxes(XAxis, YAxis, ZAxis);
		if (XAxis.IsNearlyZero(UE_SMALL_NUMBER) &&
			YAxis.IsNearlyZero(UE_SMALL_NUMBER) &&
			ZAxis.IsNearlyZero(UE_SMALL_NUMBER))
		{
			// This is not allowed, warn them
			UE_LOG(
				LogChaosClothAsset,
				Warning,
				TEXT("Reference Pose for asset %s for joint (%s) includes NIL matrix. Zero scale isn't allowed on ref pose."),
				*GetPathName(),
				*RefSkeleton.GetBoneName(BoneIndex).ToString());
		}

		// Precompute inverse so we can use from-refpose-skin vertices
		RefBasesInvMatrix[BoneIndex] = FMatrix44f(ComposedRefPoseMatrices[BoneIndex].Inverse());
	}
}

void UChaosClothAsset::CalculateBounds()
{
	using namespace UE::Chaos::ClothAsset;

	FBox BoundingBox(ForceInit);

	const FClothConstAdapter Cloth(ClothCollection);

	for (int32 LodIndex = 0; LodIndex < Cloth.GetNumLods(); ++LodIndex)
	{
		const FClothLodConstAdapter ClothLod = Cloth.GetLod(LodIndex);

		for (int32 ClothPatternIndex = 0; ClothPatternIndex < ClothLod.GetNumPatterns(); ++ClothPatternIndex)
		{
			const FClothPatternConstAdapter ClothPattern = ClothLod.GetPattern(ClothPatternIndex);
			const int32 PatternElementIndex = ClothPattern.GetElementIndex();

			const int32 RenderVerticesStart = ClothCollection->RenderVerticesStart[PatternElementIndex];
			const int32 RenderVerticesEnd = ClothCollection->RenderVerticesEnd[PatternElementIndex];

			for (int32 RenderVertexIndex = RenderVerticesStart; RenderVertexIndex <= RenderVerticesEnd; ++RenderVertexIndex)
			{
				BoundingBox += (FVector)ClothCollection->RenderPosition[RenderVertexIndex];
			}
		}
	}

	Bounds = FBoxSphereBounds(BoundingBox);
}

void UChaosClothAsset::UpdateSkeleton(bool bRebuildClothSimulationModel)
{
	CalculateInvRefMatrices();

	if (bRebuildClothSimulationModel)
	{
		// Rebuild simulation model  // TODO: How does this work with skinning
		BuildClothSimulationModel();
	}
}

void UChaosClothAsset::Build()
{
	using namespace UE::Chaos::ClothAsset;

	// Release render resources
	ReleaseResources();

	// Set a new Guid to invalidate the DDC
	AssetGuid = FGuid::NewGuid();

	// Rebuild Skeleton
	constexpr bool bRebuildClothSimulationModel = false;
	UpdateSkeleton(bRebuildClothSimulationModel);

	// Update bounds
	CalculateBounds();

	// Add LODs to the render data
	const FClothConstAdapter Cloth(ClothCollection);
	const int32 NumLods = Cloth.GetNumLods();

	// Rebuild LOD Infos
	LODInfo.Reset(NumLods);
	LODInfo.AddDefaulted(NumLods);  // TODO: Expose some properties to fill up the LOD infos

	// Build simulation model
	BuildClothSimulationModel();

	// Rebuild LOD Model
#if WITH_EDITORONLY_DATA
	BuildMeshModel();
#endif

	// Load/save render data from/to DDC
#if WITH_EDITOR
	CacheDerivedData(nullptr);
#endif

	if (FApp::CanEverRender())
	{
		InitResources();
	}

	// Re-register any components using this asset to restart the simulation with the updated asset
	ReregisterComponents();
}

#if WITH_EDITORONLY_DATA
void UChaosClothAsset::BuildMeshModel()
{
	const TArray<IClothAssetBuilderClassProvider*> ClassProviders = IModularFeatures::Get().GetModularFeatureImplementations<IClothAssetBuilderClassProvider>(IClothAssetBuilderClassProvider::FeatureName);
	if (const TSubclassOf<UClothAssetBuilder> ClothAssetBuilderClass = ClassProviders.Num() ? ClassProviders[0]->GetClothAssetBuilderClass() : nullptr)
	{
		if (const UClothAssetBuilder* const ClothAssetBuilder = ClothAssetBuilderClass->GetDefaultObject<UClothAssetBuilder>())
		{
			using namespace UE::Chaos::ClothAsset;

			const FClothConstAdapter Cloth(GetClothCollection());
			const int32 NumLods = Cloth.GetNumLods();

			check(MeshModel);  // MeshModel should always be created in the Cloth Asset constructor WITH_EDITORONLY_DATA
			MeshModel->LODModels.Empty();

			for (int32 LodIndex = 0; LodIndex < NumLods; ++LodIndex)
			{
				FSkeletalMeshLODModel* LODModel = new FSkeletalMeshLODModel();
				ClothAssetBuilder->BuildLod(*LODModel, *this, LodIndex);
				MeshModel->LODModels.Add(LODModel);
			}
		}
	}
}
#endif  // #if WITH_EDITORONLY_DATA

void UChaosClothAsset::BuildClothSimulationModel()
{
	ClothSimulationModel = MakeShared<FChaosClothSimulationModel>(GetClothCollection(), GetRefSkeleton());
}

const FMeshUVChannelInfo* UChaosClothAsset::GetUVChannelData(int32 MaterialIndex) const
{
	if (GetMaterials().IsValidIndex(MaterialIndex))
	{
		// TODO: enable ensure when UVChannelData is setup
		//ensure(GetMaterials()[MaterialIndex].UVChannelData.bInitialized);
		return &GetMaterials()[MaterialIndex].UVChannelData;
	}

	return nullptr;
}

FSkeletalMeshRenderData* UChaosClothAsset::GetResourceForRendering() const
{
	WaitUntilAsyncPropertyReleased(EClothAssetAsyncProperties::RenderData);
	return SkeletalMeshRenderData.Get();
}

void UChaosClothAsset::SetResourceForRendering(TUniquePtr<FSkeletalMeshRenderData>&& InSkeletalMeshRenderData)
{
	WaitUntilAsyncPropertyReleased(EClothAssetAsyncProperties::RenderData);
	SkeletalMeshRenderData = MoveTemp(InSkeletalMeshRenderData);
}

void UChaosClothAsset::WaitUntilAsyncPropertyReleased(EClothAssetAsyncProperties AsyncProperties, ESkinnedAssetAsyncPropertyLockType LockType) const
{
	// Cast strongly typed enum to uint64
	WaitUntilAsyncPropertyReleasedInternal((uint64)AsyncProperties, LockType);
}

FString UChaosClothAsset::GetAsyncPropertyName(uint64 Property) const
{
	return StaticEnum<EClothAssetAsyncProperties>()->GetNameByValue(Property).ToString();
}

#if WITH_EDITOR
void UChaosClothAsset::CacheDerivedData(FSkinnedAssetPostLoadContext* Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UChaosClothAsset::CacheDerivedData);

	if (!GetOutermost()->bIsCookedForEditor)
	{
		if (GetResourceForRendering() == nullptr)
		{
			// Cache derived data for the running platform.
			ITargetPlatform* RunningPlatform = GetTargetPlatformManagerRef().GetRunningTargetPlatform();
			check(RunningPlatform);

			// Create the render data
			SetResourceForRendering(MakeUnique<FSkeletalMeshRenderData>());
			// Load render data from DDC, or generate it and save to DDC
			GetResourceForRendering()->Cache(RunningPlatform, this, Context);
		}
	}
}

FString UChaosClothAsset::BuildDerivedDataKey(const ITargetPlatform* TargetPlatform)
{
	FString KeySuffix(TEXT(""));
	KeySuffix += AssetGuid.ToString();

	FString TmpPartialKeySuffix;
	//Synchronize the user data that are part of the key
	GetImportedModel()->SyncronizeLODUserSectionsData();

	// Model GUID is not generated so exclude GetImportedModel()->GetIdString() from DDC key.

	// Add the hashed string generated from the model data 
	TmpPartialKeySuffix = GetImportedModel()->GetLODModelIdString();
	KeySuffix += TmpPartialKeySuffix;

	//Add the max gpu bone per section
	const int32 MaxGPUSkinBones = FGPUBaseSkinVertexFactory::GetMaxGPUSkinBones(TargetPlatform);
	KeySuffix += FString::FromInt(MaxGPUSkinBones);
	// Add unlimited bone influences mode
	IMeshBuilderModule::GetForPlatform(TargetPlatform).AppendToDDCKey(KeySuffix, true);
	const bool bUnlimitedBoneInfluences = FGPUBaseSkinVertexFactory::GetUnlimitedBoneInfluences(TargetPlatform);
	KeySuffix += bUnlimitedBoneInfluences ? "1" : "0";

	// Include the global default bone influences limit in case any LODs don't set an explicit limit (highly likely)
	KeySuffix += FString::FromInt(GetDefault<URendererSettings>()->DefaultBoneInfluenceLimit.GetValueForPlatform(*TargetPlatform->IniPlatformName()));

	// Add LODInfoArray
	TmpPartialKeySuffix = TEXT("");
	TArray<FSkeletalMeshLODInfo>& LODInfos = GetLODInfoArray();
	for (int32 LODIndex = 0; LODIndex < GetLODNum(); ++LODIndex)
	{
		check(LODInfos.IsValidIndex(LODIndex));
		FSkeletalMeshLODInfo& LOD = LODInfos[LODIndex];
		LOD.BuildGUID = LOD.ComputeDeriveDataCacheKey(nullptr);	// TODO: FSkeletalMeshLODGroupSettings
		TmpPartialKeySuffix += LOD.BuildGUID.ToString(EGuidFormats::Digits);
	}
	KeySuffix += TmpPartialKeySuffix;

	//KeySuffix += GetHasVertexColors() ? "1" : "0";
	//KeySuffix += GetVertexColorGuid().ToString(EGuidFormats::Digits);

	//if (GetEnableLODStreaming(TargetPlatform))
	//{
	//	const int32 MaxNumStreamedLODs = GetMaxNumStreamedLODs(TargetPlatform);
	//	const int32 MaxNumOptionalLODs = GetMaxNumOptionalLODs(TargetPlatform);
	//	KeySuffix += *FString::Printf(TEXT("1%08x%08x"), MaxNumStreamedLODs, MaxNumOptionalLODs);
	//}
	//else
	//{
	//	KeySuffix += TEXT("0zzzzzzzzzzzzzzzz");
	//}

	//if (TargetPlatform->GetPlatformInfo().PlatformGroupName == TEXT("Desktop")
	//	&& GStripSkeletalMeshLodsDuringCooking != 0
	//	&& GSkeletalMeshKeepMobileMinLODSettingOnDesktop != 0)
	//{
	//	KeySuffix += TEXT("_MinMLOD");
	//}

	return FDerivedDataCacheInterface::BuildCacheKey(
		TEXT("CHAOSCLOTH"),
		CHAOS_CLOTH_ASSET_DERIVED_DATA_VERSION,
		*KeySuffix
	);
}

bool UChaosClothAsset::IsInitialBuildDone() const
{
	//We are consider built if we have a valid lod model
	return GetImportedModel() != nullptr &&
		GetImportedModel()->LODModels.Num() > 0 &&
		GetImportedModel()->LODModels[0].Sections.Num() > 0;
}
#endif // WITH_EDITOR

void UChaosClothAsset::CopySimMeshToRenderMesh(int32 MaterialIndex)
{
	using namespace UE::Chaos::ClothAsset;

	check(ClothCollection.IsValid());
	FClothGeometryTools::CopySimMeshToRenderMesh(ClothCollection, MaterialIndex);
}

void UChaosClothAsset::ReregisterComponents()
{
	// Recreate the simulation proxies with the updated physics asset
	for (TObjectIterator<UChaosClothComponent> ObjectIterator; ObjectIterator; ++ObjectIterator)
	{
		if (UChaosClothComponent* const Component = *ObjectIterator)
		{
			if (Component->GetClothAsset() == this)
			{
				const FComponentReregisterContext Context(Component);  // Context goes out of scope, causing the Component to be re-registered
			}
		}
	}
}
