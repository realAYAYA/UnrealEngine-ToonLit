// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SkeletalMesh.cpp: Unreal skeletal mesh and animation implementation.
=============================================================================*/

#include "Engine/SkeletalMesh.h"

#include "Serialization/CustomVersion.h"
#include "Misc/App.h"
#include "Misc/PackageSegment.h"
#include "Modules/ModuleManager.h"
#include "EngineStats.h"
#include "EngineGlobals.h"
#include "RawIndexBuffer.h"
#include "Engine/TextureStreamingTypes.h"
#include "Engine/Brush.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Components/SkinnedMeshComponent.h"
#include "Animation/SmartName.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "ComponentReregisterContext.h"
#include "UObject/CoreObjectVersion.h"
#include "UObject/CoreRedirects.h"
#include "UObject/EditorObjectVersion.h"
#include "UObject/FrameworkObjectVersion.h"
#include "UObject/NiagaraObjectVersion.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/PackageResourceManager.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/RenderingObjectVersion.h"
#include "UObject/UE5MainStreamObjectVersion.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UE5MainStreamObjectVersion.h"
#include "EngineUtils.h"
#include "EditorSupportDelegates.h"
#include "GPUSkinVertexFactory.h"
#include "SkeletalRenderPublic.h"
#include "Logging/TokenizedMessage.h"
#include "Logging/MessageLog.h"
#include "SceneManagement.h"
#include "PhysicsPublic.h"
#include "Animation/MorphTarget.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Engine/AssetUserData.h"
#include "Engine/Engine.h"
#include "Animation/NodeMappingContainer.h"
#include "GPUSkinCache.h"
#include "Misc/ConfigCacheIni.h"
#include "SkeletalMeshTypes.h"
#include "Rendering/SkeletalMeshVertexBuffer.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Templates/UniquePtr.h"
#include "AnimationRuntime.h"
#include "Animation/AnimSequence.h"
#include "Animation/SkinWeightProfile.h"
#include "Streaming/SkeletalMeshUpdate.h"
#include "HAL/FileManager.h"

#if WITH_EDITOR
#include "Async/ParallelFor.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODImporterData.h"
#include "IMeshBuilderModule.h"
#include "IMeshReductionManagerModule.h"
#include "IMeshReductionInterfaces.h"
#include "SkinnedAssetCompiler.h"
#include "MeshUtilities.h"
#include "Engine/SkeletalMeshEditorData.h"
#include "DerivedDataCacheInterface.h"
#include "IMeshReductionManagerModule.h"
#include "SkeletalMeshReductionSettings.h"
#include "Engine/RendererSettings.h"
#include "Engine/PoseWatchRenderData.h"
#include "SkeletalDebugRendering.h"
#endif // #if WITH_EDITOR

#include "UnrealEngine.h"
#include "Misc/CoreMisc.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"

#include "EditorFramework/AssetImportData.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Components/BrushComponent.h"
#include "Streaming/UVChannelDensity.h"
#include "Misc/Paths.h"
#include "Misc/Crc.h"

#include "ClothingAssetBase.h"

#if WITH_EDITOR
#include "ClothingAssetFactoryInterface.h"
#include "ClothingSystemEditorInterfaceModule.h"
#include "ScopedTransaction.h"
#endif
#include "SkeletalDebugRendering.h"
#include "Misc/RuntimeErrors.h"
#include "PlatformInfo.h"
#include "Async/Async.h"

#if RHI_RAYTRACING
#include "RayTracingInstance.h"
#endif
#include "Animation/SkinWeightProfileManager.h"

#define LOCTEXT_NAMESPACE "SkeltalMesh"

DEFINE_LOG_CATEGORY(LogSkeletalMesh);
DECLARE_CYCLE_STAT(TEXT("GetShadowShapes"), STAT_GetShadowShapes, STATGROUP_Anim);

TAutoConsoleVariable<int32> CVarDebugDrawSimpleBones(TEXT("a.DebugDrawSimpleBones"), 0, TEXT("When drawing bones (using Show Bones), draw bones as simple lines."));
TAutoConsoleVariable<int32> CVarDebugDrawBoneAxes(TEXT("a.DebugDrawBoneAxes"), 0, TEXT("When drawing bones (using Show Bones), draw bone axes."));

const FGuid FSkeletalMeshCustomVersion::GUID(0xD78A4A00, 0xE8584697, 0xBAA819B5, 0x487D46B4);
FCustomVersionRegistration GRegisterSkeletalMeshCustomVersion(FSkeletalMeshCustomVersion::GUID, FSkeletalMeshCustomVersion::LatestVersion, TEXT("SkeletalMeshVer"));

static TAutoConsoleVariable<int32> CVarRayTracingSkeletalMeshes(
	TEXT("r.RayTracing.Geometry.SkeletalMeshes"),
	1,
	TEXT("Include skeletal meshes in ray tracing effects (default = 1 (skeletal meshes enabled in ray tracing))"));

static TAutoConsoleVariable<int32> CVarSkeletalMeshLODMaterialReference(
	TEXT("r.SkeletalMesh.LODMaterialReference"),
	1,
	TEXT("Whether a material needs to be referenced by at least one unstripped mesh LOD to be considered as used."));

static TAutoConsoleVariable<int32> CVarRayTracingSupportSkeletalMeshes(
	TEXT("r.RayTracing.Geometry.SupportSkeletalMeshes"),
	1,
	TEXT("Whether the project supports skeletal meshes in ray tracing effects. ")
	TEXT("Turning this off disables creation of all skeletal mesh ray tracing GPU resources, saving GPU memory and time. ")
	TEXT("This setting is read-only at runtime. (default: 1)"),
	ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarRayTracingSkeletalMeshLODBias(
	TEXT("r.RayTracing.Geometry.SkeletalMeshes.LODBias"),
	0,
	TEXT("Global LOD bias for skeletal meshes in ray tracing.\n")
	TEXT("When non-zero, a different LOD level other than the predicted LOD level will be used for ray tracing. Advanced features like morph targets and cloth simulation may not work properly.\n")
	TEXT("Final LOD level to use in ray tracing is the sum of this global bias and the bias set on each skeletal mesh asset."),
	ECVF_RenderThreadSafe);

const TCHAR* GSkeletalMeshMinLodQualityLevelCVarName = TEXT("r.SkeletalMesh.MinLodQualityLevel");
const TCHAR* GSkeletalMeshMinLodQualityLevelScalabilitySection = TEXT("ViewDistanceQuality");
int32 GSkeletalMeshMinLodQualityLevel = -1;
static FAutoConsoleVariableRef CVarSkeletalMeshMinLodQualityLevel(
	GSkeletalMeshMinLodQualityLevelCVarName,
	GSkeletalMeshMinLodQualityLevel,
	TEXT("The quality level for the Min stripping LOD. \n"),
	FConsoleVariableDelegate::CreateStatic(&USkeletalMesh::OnLodStrippingQualityLevelChanged),
	ECVF_Scalability);

/*-----------------------------------------------------------------------------
FGPUSkinVertexBase
-----------------------------------------------------------------------------*/

/**
* Serializer
*
* @param Ar - archive to serialize with
*/
void TGPUSkinVertexBase::Serialize(FArchive& Ar)
{
	Ar << TangentX;
	Ar << TangentZ;
}

const FGuid FRecomputeTangentCustomVersion::GUID(0x5579F886, 0x933A4C1F, 0x83BA087B, 0x6361B92F);
// Register the custom version with core
FCustomVersionRegistration GRegisterRecomputeTangentCustomVersion(FRecomputeTangentCustomVersion::GUID, FRecomputeTangentCustomVersion::LatestVersion, TEXT("RecomputeTangentCustomVer"));

const FGuid FOverlappingVerticesCustomVersion::GUID(0x612FBE52, 0xDA53400B, 0x910D4F91, 0x9FB1857C);
// Register the custom version with core
FCustomVersionRegistration GRegisterOverlappingVerticesCustomVersion(FOverlappingVerticesCustomVersion::GUID, FOverlappingVerticesCustomVersion::LatestVersion, TEXT("OverlappingVerticeDetectionVer"));


FArchive& operator<<(FArchive& Ar, FMeshToMeshVertData& V)
{
	Ar.UsingCustomVersion(FReleaseObjectVersion::GUID);

	Ar << V.PositionBaryCoordsAndDist
		<< V.NormalBaryCoordsAndDist
		<< V.TangentBaryCoordsAndDist
		<< V.SourceMeshVertIndices[0]
		<< V.SourceMeshVertIndices[1]
		<< V.SourceMeshVertIndices[2]
		<< V.SourceMeshVertIndices[3];

	if (Ar.IsLoading() && 
		Ar.CustomVer(FReleaseObjectVersion::GUID) < FReleaseObjectVersion::WeightFMeshToMeshVertData)
	{
		// Old version had "uint32 Padding[2]"
		uint32 Discard;
		Ar << Discard << V.Padding;
	}
	else
	{
		// New version has "float Weight and "uint32 Padding"
		Ar << V.Weight << V.Padding;
	}

	return Ar;
}

FArchive& operator<<(FArchive& Ar, FClothBufferIndexMapping& ClothBufferIndexMapping)
{
	return Ar
		<< ClothBufferIndexMapping.BaseVertexIndex
		<< ClothBufferIndexMapping.MappingOffset
		<< ClothBufferIndexMapping.LODBiasStride;
}

void FreeSkeletalMeshBuffersSinkCallback()
{
	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.FreeSkeletalMeshBuffers"));
	static bool bPreviousFreeSkeletalMeshBuffers = false;
	const bool bFreeSkeletalMeshBuffers = CVar->GetValueOnGameThread() == 1;

	// We previously relied on this sink occurring periodically to free helper structures, so run it if we're not using the new logic that frees on submit.
	if (!GFreeStructuresOnRHIBufferCreation || (bPreviousFreeSkeletalMeshBuffers != bFreeSkeletalMeshBuffers))
	{
		bPreviousFreeSkeletalMeshBuffers = bFreeSkeletalMeshBuffers;
		if (bFreeSkeletalMeshBuffers)
		{
			FlushRenderingCommands();
			for (TObjectIterator<USkeletalMesh> It; It; ++It)
			{
				USkeletalMesh* SkeletalMesh = *It;
				if (!SkeletalMesh->HasPendingInitOrStreaming() && !SkeletalMesh->GetResourceForRendering()->RequiresCPUSkinning(GMaxRHIFeatureLevel))
				{
					SkeletalMesh->ReleaseCPUResources();
				}
			}
		}
	}
}

/*-----------------------------------------------------------------------------
	FClothingAssetData
-----------------------------------------------------------------------------*/

FArchive& operator<<(FArchive& Ar, FClothingAssetData_Legacy& A)
{
	// Serialization to load and skip legacy clothing assets
	if( Ar.IsLoading() )
	{
		uint32 AssetSize;
		Ar << AssetSize;

		if( AssetSize > 0 )
		{
			// Load the binary blob data
			TArray<uint8> Buffer;
			Buffer.AddUninitialized( AssetSize );
			Ar.Serialize( Buffer.GetData(), AssetSize );
		}
	}
	else
	if( Ar.IsSaving() )
	{
		{
			uint32 AssetSize = 0;
			Ar << AssetSize;
		}
	}

	return Ar;
}


FSkeletalMeshClothBuildParams::FSkeletalMeshClothBuildParams()
	: TargetAsset(nullptr)
	, TargetLod(INDEX_NONE)
	, bRemapParameters(false)
	, AssetName("Clothing")
	, LodIndex(0)
	, SourceSection(0)
	, bRemoveFromMesh(false)
	, PhysicsAsset(nullptr)
{

}

#if WITH_EDITOR
FScopedSkeletalMeshPostEditChange::FScopedSkeletalMeshPostEditChange(USkeletalMesh* InSkeletalMesh, bool InbCallPostEditChange /*= true*/, bool InbReregisterComponents /*= true*/)
{
	SkeletalMesh = nullptr;
	bReregisterComponents = InbReregisterComponents;
	bCallPostEditChange = InbCallPostEditChange;
	RecreateExistingRenderStateContext = nullptr;
	ComponentReregisterContexts.Empty();
	//Validation of the data
	if (bCallPostEditChange && !bReregisterComponents)
	{
		//We never want to call PostEditChange without re register components, since PostEditChange will recreate the skeletalmesh render resources
		ensure(bReregisterComponents);
		bReregisterComponents = true;
	}
	if (InSkeletalMesh != nullptr)
	{
		//Only set a valid skeletal mesh
		SetSkeletalMesh(InSkeletalMesh);
	}
}

FScopedSkeletalMeshPostEditChange::~FScopedSkeletalMeshPostEditChange()
{
	//If decrementing the post edit change stack counter return 0 it mean we are the first scope call instance, so we have to call posteditchange and re register component
	if (SkeletalMesh != nullptr && SkeletalMesh->UnStackPostEditChange() == 0)
	{
		if (bCallPostEditChange)
		{
			SkeletalMesh->PostEditChange();
		}
	}
	//If there is some re register data it will be delete when the destructor go out of scope. This will re register
}

void FScopedSkeletalMeshPostEditChange::SetSkeletalMesh(USkeletalMesh* InSkeletalMesh)
{
	//Skip only if we are calling post edit change
	bool bSkipCompiling = InSkeletalMesh->IsCompiling() && bCallPostEditChange;
	//Some parallel task may try to call post edit change, we must prevent it
	if (!IsInGameThread() || bSkipCompiling)
	{
		return;
	}
	//We cannot set a different skeletal mesh, check that it was construct with null
	check(SkeletalMesh == nullptr);
	//We can only set a valid skeletal mesh
	check(InSkeletalMesh != nullptr);

	SkeletalMesh = InSkeletalMesh;
	//If we are the first to increment, unregister the data we need to
	if (SkeletalMesh->StackPostEditChange() == 1)
	{
		//Only allocate data if we re register
		if (bReregisterComponents)
		{
			//Make sure all components using this skeletalmesh have there render ressources free
			RecreateExistingRenderStateContext = new FSkinnedMeshComponentRecreateRenderStateContext(InSkeletalMesh, false);

			// Now iterate over all skeletal mesh components and unregister them from the world, we will reregister them in the destructor
			for (TObjectIterator<USkeletalMeshComponent> It; It; ++It)
			{
				USkeletalMeshComponent* SkelComp = *It;
				if (SkelComp->GetSkeletalMeshAsset() == SkeletalMesh)
				{
					ComponentReregisterContexts.Add(new FComponentReregisterContext(SkelComp));
				}
			}
		}

		if (bCallPostEditChange)
		{
			//Make sure the render ressource use by the skeletalMesh is free, we will reconstruct them when a PostEditChange will be call
			SkeletalMesh->FlushRenderState();
		}
	}
}

const FString& GetSkeletalMeshDerivedDataVersion()
{
	static FString CachedVersionString = FDevSystemGuids::GetSystemGuid(FDevSystemGuids::Get().SkeletalMeshDerivedDataVersion).ToString();
	return CachedVersionString;
}

namespace SkeletalMeshImpl
{
	// Condition specifically designed to detect if we're going to enter the non-thread safe part of FLODUtilities::SimplifySkeletalMeshLOD while building.
	static bool HasInlineReductions(USkeletalMesh* SkeletalMesh)
	{
		for (int32 LODIndex = 0; LODIndex < SkeletalMesh->GetLODNum(); ++LODIndex)
		{
			if (SkeletalMesh->IsReductionActive(LODIndex))
			{
				if (FSkeletalMeshLODInfo* LODInfo = SkeletalMesh->GetLODInfo(LODIndex))
				{
					// If the BaseLOD has the same index as the LOD itself, it means we're going to have inline reduction where
					// some original data will need to be saved, which is not currently thread-safe.
					if (!LODInfo->bHasBeenSimplified || LODInfo->ReductionSettings.BaseLOD == LODIndex)
					{
						return true;
					}
				}
			}
		}

		return false;
	}
}

#endif // #if WITH_EDITOR

/*-----------------------------------------------------------------------------
	USkeletalMesh
-----------------------------------------------------------------------------*/

USkeletalMesh::USkeletalMesh(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	SetSkelMirrorAxis(EAxis::X);
	SetSkelMirrorFlipAxis(EAxis::Z);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

#if WITH_EDITORONLY_DATA
	SetImportedModel(MakeShareable(new FSkeletalMeshModel()));
	SetVertexColorGuid(FGuid());
	SetSupportLODStreaming(FPerPlatformBool(false));
	SetMaxNumStreamedLODs(FPerPlatformInt(0));
	// TODO: support saving some but not all optional LODs
	SetMaxNumOptionalLODs(FPerPlatformInt(0));
#endif
	SetMinLod(FPerPlatformInt(0));
	SetQualityLevelMinLod(0);
	MinQualityLevelLOD.Init(GSkeletalMeshMinLodQualityLevelCVarName, GSkeletalMeshMinLodQualityLevelScalabilitySection);
	SetDisableBelowMinLodStripping(FPerPlatformBool(false));
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	bSupportRayTracing = true;
	RayTracingMinLOD = 0;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

USkeletalMesh::USkeletalMesh(FVTableHelper& Helper)
	: Super(Helper)
{
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
USkeletalMesh::~USkeletalMesh() = default;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

void USkeletalMesh::PostInitProperties()
{
#if WITH_EDITORONLY_DATA
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		SetAssetImportData(NewObject<UAssetImportData>(this, TEXT("AssetImportData")));
	}
#endif
	Super::PostInitProperties();
}

FBoxSphereBounds USkeletalMesh::GetBounds() const
{
	WaitUntilAsyncPropertyReleased(ESkeletalMeshAsyncProperties::ExtendedBounds, ESkinnedAssetAsyncPropertyLockType::ReadOnly);
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return ExtendedBounds;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

FBoxSphereBounds USkeletalMesh::GetImportedBounds() const
{
	WaitUntilAsyncPropertyReleased(ESkeletalMeshAsyncProperties::ImportedBounds, ESkinnedAssetAsyncPropertyLockType::ReadOnly);
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return ImportedBounds;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void USkeletalMesh::SetImportedBounds(const FBoxSphereBounds& InBounds)
{
	WaitUntilAsyncPropertyReleased(ESkeletalMeshAsyncProperties::ImportedBounds);
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	ImportedBounds = InBounds;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	CalculateExtendedBounds();
}

void USkeletalMesh::SetPositiveBoundsExtension(const FVector& InExtension)
{
	WaitUntilAsyncPropertyReleased(ESkeletalMeshAsyncProperties::PositiveBoundsExtension);
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	PositiveBoundsExtension = InExtension;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	CalculateExtendedBounds();
}

void USkeletalMesh::SetNegativeBoundsExtension(const FVector& InExtension)
{
	WaitUntilAsyncPropertyReleased(ESkeletalMeshAsyncProperties::NegativeBoundsExtension);
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	NegativeBoundsExtension = InExtension;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	CalculateExtendedBounds();
}

void USkeletalMesh::CalculateExtendedBounds()
{
	FBoxSphereBounds CalculatedBounds = GetImportedBounds();

	// Convert to Min and Max
	FVector Min = CalculatedBounds.Origin - CalculatedBounds.BoxExtent;
	FVector Max = CalculatedBounds.Origin + CalculatedBounds.BoxExtent;
	// Apply bound extensions
	Min -= GetNegativeBoundsExtension();
	Max += GetPositiveBoundsExtension();
	// Convert back to Origin, Extent and update SphereRadius
	CalculatedBounds.Origin = (Min + Max) / 2;
	CalculatedBounds.BoxExtent = (Max - Min) / 2;
	CalculatedBounds.SphereRadius = CalculatedBounds.BoxExtent.GetAbsMax();

	SetExtendedBounds(CalculatedBounds);
}

void USkeletalMesh::ValidateBoundsExtension()
{
	FVector HalfExtent = GetImportedBounds().BoxExtent;

	FVector::FReal MaxVal = MAX_flt;
	FVector Bounds = GetPositiveBoundsExtension();
	Bounds.X = FMath::Clamp(Bounds.X, -HalfExtent.X, MaxVal);
	Bounds.Y = FMath::Clamp(Bounds.Y, -HalfExtent.Y, MaxVal);
	Bounds.Z = FMath::Clamp(Bounds.Z, -HalfExtent.Z, MaxVal);
	SetPositiveBoundsExtension(Bounds);

	Bounds = GetNegativeBoundsExtension();
	Bounds.X = FMath::Clamp(Bounds.X, -HalfExtent.X, MaxVal);
	Bounds.Y = FMath::Clamp(Bounds.Y, -HalfExtent.Y, MaxVal);
	Bounds.Z = FMath::Clamp(Bounds.Z, -HalfExtent.Z, MaxVal);
	SetNegativeBoundsExtension(Bounds);
}

#if WITH_EDITOR

bool USkeletalMesh::IsInitialBuildDone() const
{
	//We are consider built if we have a valid lod model and a valid inline reduction cache
	return GetImportedModel() != nullptr &&
		GetImportedModel()->LODModels.Num() > 0 &&
		GetImportedModel()->LODModels[0].Sections.Num() > 0 &&
		GetImportedModel()->InlineReductionCacheDatas.Num() > 0;
}

/* Return true if the reduction settings are setup to reduce a LOD*/
bool USkeletalMesh::IsReductionActive(int32 LODIndex) const
{
	//Invalid LOD are not reduced
	if(!IsValidLODIndex(LODIndex))
	{
		return false;
	}

	bool bReductionActive = false;
	if (IMeshReduction* ReductionModule = FModuleManager::Get().LoadModuleChecked<IMeshReductionManagerModule>("MeshReductionInterface").GetSkeletalMeshReductionInterface())
	{
		FSkeletalMeshOptimizationSettings ReductionSettings = GetReductionSettings(LODIndex);
		uint32 LODVertexNumber = MAX_uint32;
		uint32 LODTriNumber = MAX_uint32;
		const FSkeletalMeshLODInfo* LODInfoPtr = GetLODInfo(LODIndex);
		const bool bLODHasBeenSimplified = LODInfoPtr && LODInfoPtr->bHasBeenSimplified;
		//If we are not inline reduced, we wont set the LODVertexNumber and LODTriNumber from the LODModel or from the cache.
		const bool bInlineReduction = LODInfoPtr && (LODInfoPtr->ReductionSettings.BaseLOD == LODIndex);
		if (bInlineReduction && GetImportedModel() && GetImportedModel()->LODModels.IsValidIndex(LODIndex))
		{
			if (!bLODHasBeenSimplified)
			{
				LODVertexNumber = 0;
				LODTriNumber = 0;
				const FSkeletalMeshLODModel& LODModel = GetImportedModel()->LODModels[LODIndex];
				//We can take the vertices and triangles count from the source model
				for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); ++SectionIndex)
				{
					const FSkelMeshSection& Section = LODModel.Sections[SectionIndex];

					//Make sure the count fit in a uint32
					LODVertexNumber += Section.NumVertices < 0 ? 0 : Section.NumVertices;
					LODTriNumber += Section.NumTriangles;
				}
			}
			else if (GetImportedModel()->InlineReductionCacheDatas.IsValidIndex(LODIndex))
			{
				//In this case we have to use the inline cache reduction data to know how many vertices/triangles we have before the reduction
				GetImportedModel()->InlineReductionCacheDatas[LODIndex].GetCacheGeometryInfo(LODVertexNumber, LODTriNumber);
			}
		}
		bReductionActive = ReductionModule->IsReductionActive(ReductionSettings, LODVertexNumber, LODTriNumber);
	}
	return bReductionActive;
}

/* Get a copy of the reduction settings for a specified LOD index. */
FSkeletalMeshOptimizationSettings USkeletalMesh::GetReductionSettings(int32 LODIndex) const
{
	check(IsValidLODIndex(LODIndex));
	const FSkeletalMeshLODInfo& CurrentLODInfo = *(GetLODInfo(LODIndex));
	return CurrentLODInfo.ReductionSettings;
}

#endif

void USkeletalMesh::AddClothingAsset(UClothingAssetBase* InNewAsset)
{
	check(IsInGameThread());

	// Check the outer is us
	if (InNewAsset && InNewAsset->GetOuter() == this)
	{
		// Ok this should be a correctly created asset, we can add it
		GetMeshClothingAssets().AddUnique(InNewAsset);

		// Consolidate the shared cloth configs
		InNewAsset->PostUpdateAllAssets();

#if WITH_EDITOR
		OnClothingChange.Broadcast();
#endif
	}
}

#if WITH_EDITOR
void USkeletalMesh::RemoveClothingAsset(int32 InLodIndex, int32 InSectionIndex)
{
	check(IsInGameThread());
	if (UClothingAssetBase* Asset = GetSectionClothingAsset(InLodIndex, InSectionIndex))
	{
		Asset->UnbindFromSkeletalMesh(this, InLodIndex);
		GetMeshClothingAssets().Remove(Asset);
		OnClothingChange.Broadcast();
	}
}
#endif

UClothingAssetBase* USkeletalMesh::GetSectionClothingAsset(int32 InLodIndex, int32 InSectionIndex)
{
	if (FSkeletalMeshRenderData* SkelResource = GetResourceForRendering())
	{
		if (SkelResource->LODRenderData.IsValidIndex(InLodIndex))
		{
			FSkeletalMeshLODRenderData& LodData = SkelResource->LODRenderData[InLodIndex];
			if (LodData.RenderSections.IsValidIndex(InSectionIndex))
			{
				FSkelMeshRenderSection& Section = LodData.RenderSections[InSectionIndex];

				FGuid ClothingAssetGuid = Section.ClothingData.AssetGuid;

				if (ClothingAssetGuid.IsValid())
				{
					UClothingAssetBase** FoundAsset = GetMeshClothingAssets().FindByPredicate([&](UClothingAssetBase* InAsset)
					{
						return InAsset && InAsset->GetAssetGuid() == ClothingAssetGuid;
					});

					return FoundAsset ? *FoundAsset : nullptr;
				}
			}
		}
	}

	return nullptr;
}

const UClothingAssetBase* USkeletalMesh::GetSectionClothingAsset(int32 InLodIndex, int32 InSectionIndex) const
{
	if (FSkeletalMeshRenderData* SkelResource = GetResourceForRendering())
	{
		if (SkelResource->LODRenderData.IsValidIndex(InLodIndex))
		{
			FSkeletalMeshLODRenderData& LodData = SkelResource->LODRenderData[InLodIndex];
			if (LodData.RenderSections.IsValidIndex(InSectionIndex))
			{
				FSkelMeshRenderSection& Section = LodData.RenderSections[InSectionIndex];

				FGuid ClothingAssetGuid = Section.ClothingData.AssetGuid;

				if (ClothingAssetGuid.IsValid())
				{
					UClothingAssetBase* const* FoundAsset = GetMeshClothingAssets().FindByPredicate([&](UClothingAssetBase* InAsset)
					{
						return InAsset && InAsset->GetAssetGuid() == ClothingAssetGuid;
					});

					return FoundAsset ? *FoundAsset : nullptr;
				}
			}
		}
	}

	return nullptr;
}

UClothingAssetBase* USkeletalMesh::GetClothingAsset(const FGuid& InAssetGuid) const
{
	if (!InAssetGuid.IsValid())
	{
		return nullptr;
	}

	UClothingAssetBase* const* FoundAsset = GetMeshClothingAssets().FindByPredicate([&](UClothingAssetBase* CurrAsset)
	{
		return CurrAsset && CurrAsset->GetAssetGuid() == InAssetGuid;
	});

	return FoundAsset ? *FoundAsset : nullptr;
}

int32 USkeletalMesh::GetClothingAssetIndex(UClothingAssetBase* InAsset) const
{
	return InAsset ? GetClothingAssetIndex(InAsset->GetAssetGuid()) : INDEX_NONE;
}

int32 USkeletalMesh::GetClothingAssetIndex(const FGuid& InAssetGuid) const
{
	const TArray<UClothingAssetBase*>& CachedMeshClothingAssets = GetMeshClothingAssets();
	const int32 NumAssets = CachedMeshClothingAssets.Num();
	for(int32 SearchIndex = 0; SearchIndex < NumAssets; ++SearchIndex)
	{
		if(CachedMeshClothingAssets[SearchIndex] &&
		   CachedMeshClothingAssets[SearchIndex]->GetAssetGuid() == InAssetGuid)
		{
			return SearchIndex;
		}
	}
	return INDEX_NONE;
}

bool USkeletalMesh::HasActiveClothingAssets() const
{
#if WITH_EDITOR
	return ComputeActiveClothingAssets();
#else
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return bHasActiveClothingAssets;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
}

bool USkeletalMesh::HasActiveClothingAssetsForLOD(int32 LODIndex) const
{
	if (FSkeletalMeshRenderData* Resource = GetResourceForRendering())
	{
		if (Resource->LODRenderData.IsValidIndex(LODIndex))
		{
			const FSkeletalMeshLODRenderData& LodData = Resource->LODRenderData[LODIndex];
			const int32 NumSections = LodData.RenderSections.Num();
			for (int32 SectionIdx = 0; SectionIdx < NumSections; ++SectionIdx)
			{
				const FSkelMeshRenderSection& Section = LodData.RenderSections[SectionIdx];

				if (Section.ClothingData.AssetGuid.IsValid())
				{
					return true;
				}
			}
		}
	}

	return false;
}

bool USkeletalMesh::ComputeActiveClothingAssets() const
{
	if (FSkeletalMeshRenderData* Resource = GetResourceForRendering())
	{
		for (const FSkeletalMeshLODRenderData& LodData : Resource->LODRenderData)
		{
			const int32 NumSections = LodData.RenderSections.Num();
			for (int32 SectionIdx = 0; SectionIdx < NumSections; ++SectionIdx)
			{
				const FSkelMeshRenderSection& Section = LodData.RenderSections[SectionIdx];

				if(Section.ClothingData.AssetGuid.IsValid())
				{
					return true;
				}
			}
		}
	}

	return false;
}

void USkeletalMesh::GetClothingAssetsInUse(TArray<UClothingAssetBase*>& OutClothingAssets) const
{
	OutClothingAssets.Reset();
	
	if (FSkeletalMeshRenderData* Resource = GetResourceForRendering())
	{
		for (FSkeletalMeshLODRenderData& LodData : Resource->LODRenderData)
		{
			const int32 NumSections = LodData.RenderSections.Num();
			for (int32 SectionIdx = 0; SectionIdx < NumSections; ++SectionIdx)
			{
				FSkelMeshRenderSection& Section = LodData.RenderSections[SectionIdx];
				if (Section.ClothingData.AssetGuid.IsValid())
				{
					if (UClothingAssetBase* Asset = GetClothingAsset(Section.ClothingData.AssetGuid))
					{
						OutClothingAssets.AddUnique(Asset);
					}
				}
			}
		}
	}
}

bool USkeletalMesh::NeedCPUData(int32 LODIndex) const
{
	return GetSamplingInfo().IsSamplingEnabled(this, LODIndex);
}

void USkeletalMesh::InitResources()
{
	LLM_SCOPE_BYNAME(TEXT("SkeletalMesh/InitResources")); // This is an important test case for SCOPE_BYNAME without a matching LLM_DEFINE_TAG

	UpdateUVChannelData(false);
	CachedSRRState.Clear();

	FSkeletalMeshRenderData* SkelMeshRenderData = GetResourceForRendering();
	if (SkelMeshRenderData)
	{
#if WITH_EDITOR
		//Editor sanity check, we must ensure all the data is in sync between LODModel, RenderData and UserSectionsData
		if (GetImportedModel())
		{
			for (int32 LODIndex = 0; LODIndex < GetLODNum(); ++LODIndex)
			{
				if (!GetImportedModel()->LODModels.IsValidIndex(LODIndex) || !GetSkeletalMeshRenderData()->LODRenderData.IsValidIndex(LODIndex))
				{
					continue;
				}
				const FSkeletalMeshLODModel& ImportLODModel = GetImportedModel()->LODModels[LODIndex];
				FSkeletalMeshLODRenderData& RenderLODModel = GetSkeletalMeshRenderData()->LODRenderData[LODIndex];
				check(ImportLODModel.Sections.Num() == RenderLODModel.RenderSections.Num());
				for (int32 SectionIndex = 0; SectionIndex < ImportLODModel.Sections.Num(); ++SectionIndex)
				{
					const FSkelMeshSection& ImportSection = ImportLODModel.Sections[SectionIndex];
					
					//In Editor we want to make sure the data is in sync between UserSectionsData and LODModel Sections
					const FSkelMeshSourceSectionUserData& SectionUserData = ImportLODModel.UserSectionsData.FindChecked(ImportSection.OriginalDataSectionIndex);
					bool bImportDataInSync = SectionUserData.bDisabled == ImportSection.bDisabled &&
						SectionUserData.bCastShadow == ImportSection.bCastShadow &&
						SectionUserData.bVisibleInRayTracing == ImportSection.bVisibleInRayTracing &&
						SectionUserData.bRecomputeTangent == ImportSection.bRecomputeTangent &&
						SectionUserData.RecomputeTangentsVertexMaskChannel == ImportSection.RecomputeTangentsVertexMaskChannel;
					//Check the cloth only for parent section, since chunked section should not have cloth
					if (bImportDataInSync && ImportSection.ChunkedParentSectionIndex == INDEX_NONE)
					{
						bImportDataInSync = SectionUserData.CorrespondClothAssetIndex == ImportSection.CorrespondClothAssetIndex &&
							SectionUserData.ClothingData.AssetGuid == ImportSection.ClothingData.AssetGuid &&
							SectionUserData.ClothingData.AssetLodIndex == ImportSection.ClothingData.AssetLodIndex;
					}
					
					//In Editor we want to make sure the data is in sync between UserSectionsData and RenderSections
					const FSkelMeshRenderSection& RenderSection = RenderLODModel.RenderSections[SectionIndex];
					bool bRenderDataInSync = SectionUserData.bDisabled == RenderSection.bDisabled &&
						SectionUserData.bCastShadow == RenderSection.bCastShadow &&
						SectionUserData.bVisibleInRayTracing == RenderSection.bVisibleInRayTracing &&
						SectionUserData.bRecomputeTangent == RenderSection.bRecomputeTangent &&
						SectionUserData.RecomputeTangentsVertexMaskChannel == RenderSection.RecomputeTangentsVertexMaskChannel &&
						SectionUserData.CorrespondClothAssetIndex == RenderSection.CorrespondClothAssetIndex &&
						SectionUserData.ClothingData.AssetGuid == RenderSection.ClothingData.AssetGuid &&
						SectionUserData.ClothingData.AssetLodIndex == RenderSection.ClothingData.AssetLodIndex;

					if (!bImportDataInSync || !bRenderDataInSync)
					{
						UE_ASSET_LOG(LogSkeletalMesh, Error, this, TEXT("Data out of sync in lod %d. bImportDataInSync=%d, bRenderDataInSync=%d. This happen when DDC cache has corrupted data (Key has change during the skeletalmesh build)"), LODIndex, bImportDataInSync, bRenderDataInSync);
					}
				}
			}
		}
#endif
		bool bAllLODsLookValid = true;	// TODO figure this out
		for (int32 LODIdx = 0; LODIdx < GetSkeletalMeshRenderData()->LODRenderData.Num(); ++LODIdx)
		{
			const FSkeletalMeshLODRenderData& LODRenderData = GetSkeletalMeshRenderData()->LODRenderData[LODIdx];
			if (!LODRenderData.GetNumVertices() && (!LODRenderData.bIsLODOptional || LODRenderData.BuffersSize > 0))
			{
				bAllLODsLookValid = false;
				break;
			}
		}

		{
			const int32 NumLODs = SkelMeshRenderData->LODRenderData.Num();
			const int32 MinFirstLOD = GetMinLodIdx(true);

			CachedSRRState.NumNonStreamingLODs = SkelMeshRenderData->NumInlinedLODs;
			CachedSRRState.NumNonOptionalLODs = SkelMeshRenderData->NumNonOptionalLODs;
			// Limit the number of LODs based on MinLOD value.
			CachedSRRState.MaxNumLODs = FMath::Clamp<int32>(NumLODs - MinFirstLOD, SkelMeshRenderData->NumInlinedLODs, NumLODs);
			CachedSRRState.AssetLODBias = MinFirstLOD;
			CachedSRRState.LODBiasModifier = SkelMeshRenderData->LODBiasModifier;
			// The optional LOD might be culled now.
			CachedSRRState.NumNonOptionalLODs = FMath::Min(CachedSRRState.NumNonOptionalLODs, CachedSRRState.MaxNumLODs);
			// Set LOD count to fit the current state.
			CachedSRRState.NumResidentLODs = NumLODs - SkelMeshRenderData->CurrentFirstLODIdx;
			CachedSRRState.NumRequestedLODs = CachedSRRState.NumResidentLODs;
			// Set whether the mips can be streamed.
			CachedSRRState.bSupportsStreaming = !NeverStream && bAllLODsLookValid && CachedSRRState.NumNonStreamingLODs != CachedSRRState.MaxNumLODs;
		}

		// TODO : Update RenderData->CurrentFirstLODIdx based on whether IStreamingManager::Get().IsRenderAssetStreamingEnabled(EStreamableRenderAssetType::SkeletalMesh).

		SkelMeshRenderData->InitResources(GetHasVertexColors(), GetMorphTargets(), this);
		CachedSRRState.bHasPendingInitHint = true;

		// For now in the editor force all LODs to stream to make sure tools have all LODs available
		if (GIsEditor && CachedSRRState.bSupportsStreaming)
		{
			bForceMiplevelsToBeResident = true;
		}
	}

	LinkStreaming();
}


void USkeletalMesh::ReleaseResources()
{
	FSkeletalMeshRenderData* SkelMeshRenderData = GetResourceForRendering();
	if (SkelMeshRenderData && SkelMeshRenderData->IsInitialized())
	{

		if(GIsEditor && !GIsPlayInEditorWorld)
		{
			//Flush the rendering command to be sure there is no command left that can create/modify a rendering ressource
			FlushRenderingCommands();
		}

		SkelMeshRenderData->ReleaseResources();

		// insert a fence to signal when these commands completed
		ReleaseResourcesFence.BeginFence();
	}
}

#if WITH_EDITORONLY_DATA
int32 USkeletalMesh::GetNumImportedVertices() const
{
	const FSkeletalMeshModel* SkeletalMeshModel = GetImportedModel();
	if (SkeletalMeshModel)
	{
		const int32 MaxIndex = SkeletalMeshModel->LODModels[0].MaxImportVertex;
		return (MaxIndex > 0) ? (MaxIndex + 1) : 0;
	}

	return 0;
}
#endif

#if WITH_EDITORONLY_DATA
static void AccumulateUVDensities(float* OutWeightedUVDensities, float* OutWeights, const FSkeletalMeshLODRenderData& LODData, const FSkelMeshRenderSection& Section)
{
	const int32 NumTotalTriangles = LODData.GetTotalFaces();
	const int32 NumCoordinateIndex = FMath::Min<int32>(LODData.GetNumTexCoords(), TEXSTREAM_MAX_NUM_UVCHANNELS);

	FUVDensityAccumulator UVDensityAccs[TEXSTREAM_MAX_NUM_UVCHANNELS];
	for (int32 CoordinateIndex = 0; CoordinateIndex < NumCoordinateIndex; ++CoordinateIndex)
	{
		UVDensityAccs[CoordinateIndex].Reserve(NumTotalTriangles);
	}

	TArray<uint32> Indices;
	LODData.MultiSizeIndexContainer.GetIndexBuffer( Indices );
	if (!Indices.Num()) return;

	const uint32* SrcIndices = Indices.GetData() + Section.BaseIndex;
	uint32 NumTriangles = Section.NumTriangles;

	// Figure out Unreal unit per texel ratios.
	for (uint32 TriangleIndex=0; TriangleIndex < NumTriangles; TriangleIndex++ )
	{
		//retrieve indices
		uint32 Index0 = SrcIndices[TriangleIndex*3];
		uint32 Index1 = SrcIndices[TriangleIndex*3+1];
		uint32 Index2 = SrcIndices[TriangleIndex*3+2];

		const float Aera = FUVDensityAccumulator::GetTriangleAera(
			LODData.StaticVertexBuffers.PositionVertexBuffer.VertexPosition(Index0),
			LODData.StaticVertexBuffers.PositionVertexBuffer.VertexPosition(Index1),
			LODData.StaticVertexBuffers.PositionVertexBuffer.VertexPosition(Index2));

		if (Aera > UE_SMALL_NUMBER)
		{
			for (int32 CoordinateIndex = 0; CoordinateIndex < NumCoordinateIndex; ++CoordinateIndex)
			{
				const float UVAera = FUVDensityAccumulator::GetUVChannelAera(
					FVector2D(LODData.StaticVertexBuffers.StaticMeshVertexBuffer.GetVertexUV(Index0, CoordinateIndex)),
					FVector2D(LODData.StaticVertexBuffers.StaticMeshVertexBuffer.GetVertexUV(Index1, CoordinateIndex)),
					FVector2D(LODData.StaticVertexBuffers.StaticMeshVertexBuffer.GetVertexUV(Index2, CoordinateIndex)));

				UVDensityAccs[CoordinateIndex].PushTriangle(Aera, UVAera);
			}
		}
	}

	for (int32 CoordinateIndex = 0; CoordinateIndex < NumCoordinateIndex; ++CoordinateIndex)
	{
		UVDensityAccs[CoordinateIndex].AccumulateDensity(OutWeightedUVDensities[CoordinateIndex], OutWeights[CoordinateIndex]);
	}
}
#endif

void USkeletalMesh::UpdateUVChannelData(bool bRebuildAll)
{
#if WITH_EDITORONLY_DATA
	// Once cooked, the data requires to compute the scales will not be CPU accessible.
	FSkeletalMeshRenderData* Resource = GetResourceForRendering();
	if (FPlatformProperties::HasEditorOnlyData() && Resource)
	{
		TArray<FSkeletalMaterial>& MeshMaterials = GetMaterials();
		for (int32 MaterialIndex = 0; MaterialIndex < MeshMaterials.Num(); ++MaterialIndex)
		{
			FMeshUVChannelInfo& UVChannelData = MeshMaterials[MaterialIndex].UVChannelData;

			// Skip it if we want to keep it.
			if (UVChannelData.IsInitialized() && (!bRebuildAll || UVChannelData.bOverrideDensities))
				continue;

			float WeightedUVDensities[TEXSTREAM_MAX_NUM_UVCHANNELS] = {0, 0, 0, 0};
			float Weights[TEXSTREAM_MAX_NUM_UVCHANNELS] = {0, 0, 0, 0};

			for (int32 LODIndex = 0; LODIndex < Resource->LODRenderData.Num(); ++LODIndex)
			{
				const FSkeletalMeshLODRenderData& LODData = Resource->LODRenderData[LODIndex];
				const TArray<int32>& RemappedMaterialIndices = GetLODInfoArray()[LODIndex].LODMaterialMap;

				for (int32 SectionIndex = 0; SectionIndex < LODData.RenderSections.Num(); ++SectionIndex)
				{
					const FSkelMeshRenderSection& SectionInfo = LODData.RenderSections[SectionIndex];
					const int32 UsedMaterialIndex =
						SectionIndex < RemappedMaterialIndices.Num() && MeshMaterials.IsValidIndex(RemappedMaterialIndices[SectionIndex]) ?
						RemappedMaterialIndices[SectionIndex] :
						SectionInfo.MaterialIndex;

					if (UsedMaterialIndex != MaterialIndex)
							continue;

					AccumulateUVDensities(WeightedUVDensities, Weights, LODData, SectionInfo);
				}
			}

			UVChannelData.bInitialized = true;
			UVChannelData.bOverrideDensities = false;
			for (int32 CoordinateIndex = 0; CoordinateIndex < TEXSTREAM_MAX_NUM_UVCHANNELS; ++CoordinateIndex)
			{
				UVChannelData.LocalUVDensities[CoordinateIndex] = (Weights[CoordinateIndex] > UE_KINDA_SMALL_NUMBER) ? (WeightedUVDensities[CoordinateIndex] / Weights[CoordinateIndex]) : 0;
			}
		}

		Resource->SyncUVChannelData(GetMaterials());
	}
#endif
}

const FMeshUVChannelInfo* USkeletalMesh::GetUVChannelData(int32 MaterialIndex) const
{
	if (GetMaterials().IsValidIndex(MaterialIndex))
	{
		ensure(GetMaterials()[MaterialIndex].UVChannelData.bInitialized);
		return &GetMaterials()[MaterialIndex].UVChannelData;
	}

	return nullptr;
}

void USkeletalMesh::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	Super::GetResourceSizeEx(CumulativeResourceSize);
	// Default implementation handles subobjects

	if (GetSkeletalMeshRenderData())
	{
		GetSkeletalMeshRenderData()->GetResourceSizeEx(CumulativeResourceSize);
	}

	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(GetRefBasesInvMatrix().GetAllocatedSize());
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(GetRefSkeleton().GetDataSize());
}

int32 USkeletalMesh::CalcCumulativeLODSize(int32 NumLODs) const
{
	uint32 Accum = 0;
	const int32 LODCount = GetLODNum();
	const int32 LastLODIdx = LODCount - NumLODs;
	for (int32 LODIdx = LODCount - 1; LODIdx >= LastLODIdx; --LODIdx)
	{
		Accum += GetSkeletalMeshRenderData()->LODRenderData[LODIdx].BuffersSize;
	}
	check(Accum >= 0u);
	return Accum;
}

FIoFilenameHash USkeletalMesh::GetMipIoFilenameHash(const int32 MipIndex) const
{
	if (GetSkeletalMeshRenderData() && GetSkeletalMeshRenderData()->LODRenderData.IsValidIndex(MipIndex))
	{
		return GetSkeletalMeshRenderData()->LODRenderData[MipIndex].StreamingBulkData.GetIoFilenameHash();
	}
	else
	{
		return INVALID_IO_FILENAME_HASH;
	}
}

bool USkeletalMesh::DoesMipDataExist(const int32 MipIndex) const
{
	return GetSkeletalMeshRenderData() && GetSkeletalMeshRenderData()->LODRenderData.IsValidIndex(MipIndex) && GetSkeletalMeshRenderData()->LODRenderData[MipIndex].StreamingBulkData.DoesExist();
}

bool USkeletalMesh::HasPendingRenderResourceInitialization() const
{
	return GetSkeletalMeshRenderData() && !GetSkeletalMeshRenderData()->bReadyForStreaming;
}

bool USkeletalMesh::StreamOut(int32 NewMipCount)
{
	check(IsInGameThread());
	if (!HasPendingInitOrStreaming() && CachedSRRState.StreamOut(NewMipCount))
	{
		PendingUpdate = new FSkeletalMeshStreamOut(this);
		return !PendingUpdate->IsCancelled();
	}
	return false;
}

bool USkeletalMesh::StreamIn(int32 NewMipCount, bool bHighPrio)
{
	check(IsInGameThread());
	if (!HasPendingInitOrStreaming() && CachedSRRState.StreamIn(NewMipCount))
	{
#if WITH_EDITOR
		// If editor data is available for the current platform, and the package isn't actually cooked.
		if (FPlatformProperties::HasEditorOnlyData() && !GetOutermost()->bIsCookedForEditor)
		{
			if (GRHISupportsAsyncTextureCreation)
			{
				PendingUpdate = new FSkeletalMeshStreamIn_DDC_Async(this);
			}
			else
			{
				PendingUpdate = new FSkeletalMeshStreamIn_DDC_RenderThread(this);
			}
		}
		else
#endif
		{
			if (GRHISupportsAsyncTextureCreation)
			{
				PendingUpdate = new FSkeletalMeshStreamIn_IO_Async(this, bHighPrio);
			}
			else
			{
				PendingUpdate = new FSkeletalMeshStreamIn_IO_RenderThread(this, bHighPrio);
			}
		}
		return !PendingUpdate->IsCancelled();
	}
	return false;
}

void USkeletalMesh::CancelAllPendingStreamingActions()
{
	FlushRenderingCommands();

	for (TObjectIterator<USkeletalMesh> It; It; ++It)
	{
		USkeletalMesh* StaticMesh = *It;
		StaticMesh->CancelPendingStreamingRequest();
	}

	FlushRenderingCommands();
}

/**
 * Operator for MemCount only, so it only serializes the arrays that needs to be counted.
*/
FArchive &operator<<( FArchive& Ar, FSkeletalMeshLODInfo& I )
{
	Ar << I.LODMaterialMap;

#if WITH_EDITORONLY_DATA
	if ( Ar.IsLoading() && Ar.UEVer() < VER_UE4_MOVE_SKELETALMESH_SHADOWCASTING )
	{
		Ar << I.bEnableShadowCasting_DEPRECATED;
	}
#endif

	// fortnite version
	Ar.UsingCustomVersion(FFortniteMainBranchObjectVersion::GUID);
	Ar.UsingCustomVersion(FSkeletalMeshCustomVersion::GUID);
	if (Ar.CustomVer(FSkeletalMeshCustomVersion::GUID) < FSkeletalMeshCustomVersion::RemoveTriangleSorting)
	{
		uint8 DummyTriangleSorting;
		Ar << DummyTriangleSorting;

		uint8 DummyCustomLeftRightAxis;
		Ar << DummyCustomLeftRightAxis;

		FName DummyCustomLeftRightBoneName;
		Ar << DummyCustomLeftRightBoneName;
		}


	return Ar;
}

void RefreshSkelMeshOnPhysicsAssetChange(const USkeletalMesh* InSkeletalMesh)
{
	if (InSkeletalMesh)
	{
		for (FThreadSafeObjectIterator Iter(USkeletalMeshComponent::StaticClass()); Iter; ++Iter)
		{
			USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(*Iter);
			// if PhysicsAssetOverride is NULL, it uses SkeletalMesh Physics Asset, so I'll need to update here
			if  (SkeletalMeshComponent->GetSkeletalMeshAsset() == InSkeletalMesh &&
				 SkeletalMeshComponent->PhysicsAssetOverride == NULL)
			{
				// it needs to recreate IF it already has been created
				if (SkeletalMeshComponent->IsPhysicsStateCreated())
				{
					// do not call SetPhysAsset as it will setup physics asset override
					SkeletalMeshComponent->RecreatePhysicsState();
					SkeletalMeshComponent->UpdateHasValidBodies();
				}
			}
		}
#if WITH_EDITOR
		FEditorSupportDelegates::RedrawAllViewports.Broadcast();
#endif // WITH_EDITOR
	}
}

#if WITH_EDITOR

int32 USkeletalMesh::StackPostEditChange()
{
	check(PostEditChangeStackCounter >= 0);
	//Return true if this is the first stack ID
	PostEditChangeStackCounter++;
	return PostEditChangeStackCounter;
}

int32 USkeletalMesh::UnStackPostEditChange()
{
	check(PostEditChangeStackCounter > 0);
	PostEditChangeStackCounter--;
	return PostEditChangeStackCounter;
}

void USkeletalMesh::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	check(IsInGameThread());

	if (PostEditChangeStackCounter > 0)
	{
		//Ignore those call when we have an active delay stack
		return;
	}
	//Block any re-entrant call by incrementing PostEditChangeStackCounter. It will be decrement when we will go out of scope.
	const bool bCallPostEditChange = false;
	const bool bReRegisterComponents = false;
	FScopedSkeletalMeshPostEditChange BlockRecursiveCallScope(this, bCallPostEditChange, bReRegisterComponents);

	bool bFullPrecisionUVsReallyChanged = false;

	FProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	
	if (GIsEditor &&
		PropertyThatChanged &&
		(PropertyThatChanged->GetFName() == FName(TEXT("bSupportRayTracing")) ||
		 PropertyThatChanged->GetFName() == FName(TEXT("RayTracingMinLOD")) ||
		 PropertyThatChanged->GetFName() == FName(TEXT("ClothLODBiasMode"))))
	{
		// Update the extra cloth deformer mapping LOD bias using this cloth entry
		for (UClothingAssetBase* const ClothingAsset : GetMeshClothingAssets())
		{
			if (ClothingAsset)
			{
				ClothingAsset->UpdateAllLODBiasMappings(this);
			}
		}

		// Invalidate the DDC, since the bias mappings are cached with the mesh sections, this needs to be done before the call to Build()
		InvalidateDeriveDataCacheGUID();
	}

	bool bWasBuilt = false;
	bool bHasToReregisterComponent = false;
	// Don't invalidate render data when dragging sliders, too slow
	if (PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		Build();
		bWasBuilt = true;
		bHasToReregisterComponent = true;
	}

	if( GIsEditor &&
		PropertyThatChanged &&
		PropertyThatChanged->GetFName() == FName(TEXT("PhysicsAsset")) )
	{
		RefreshSkelMeshOnPhysicsAssetChange(this);
	}

	if( GIsEditor &&
		CastField<FObjectProperty>(PropertyThatChanged) &&
		CastField<FObjectProperty>(PropertyThatChanged)->PropertyClass == UMorphTarget::StaticClass() )
	{
		// A morph target has changed, reinitialize morph target maps
		InitMorphTargets();
	}

	if ( GIsEditor &&
		 PropertyThatChanged &&
		 PropertyThatChanged->GetFName() == GetEnablePerPolyCollisionMemberName()
		)
	{
		BuildPhysicsData();
	}

	if (FProperty* MemberProperty = PropertyChangedEvent.MemberProperty)
	{
		if (MemberProperty->GetFName() == GetPositiveBoundsExtensionMemberName() ||
			MemberProperty->GetFName() == GetNegativeBoundsExtensionMemberName())
		{
			// If the bounds extensions change, recalculate extended bounds.
			ValidateBoundsExtension();
			CalculateExtendedBounds();
			bHasToReregisterComponent = true;
		}
	}
		
	if (PropertyThatChanged && PropertyThatChanged->GetFName() == USkeletalMesh::GetPostProcessAnimBlueprintMemberName())
	{
		bHasToReregisterComponent = true;
	}

	if (bHasToReregisterComponent)
	{
		TArray<UActorComponent*> ComponentsToReregister;
		for(TObjectIterator<USkeletalMeshComponent> It; It; ++It)
		{
			USkeletalMeshComponent* MeshComponent = *It;
			if(MeshComponent && !MeshComponent->IsTemplate() && MeshComponent->GetSkeletalMeshAsset() == this)
			{
				ComponentsToReregister.Add(*It);
			}
		}
		FMultiComponentReregisterContext ReregisterContext(ComponentsToReregister);
	}

	// Those are already handled by the Build method, no need to process those if Build() has been called.
	if (!bWasBuilt)
	{
		if (PropertyThatChanged && PropertyChangedEvent.MemberProperty)
		{
			if (PropertyChangedEvent.MemberProperty->GetFName() == GetSamplingInfoMemberName())
			{
				GetSamplingInfoInternal().BuildRegions(this);
			}
			else if (PropertyChangedEvent.MemberProperty->GetFName() == GetLODInfoMemberName())
			{
				GetSamplingInfoInternal().BuildWholeMesh(this);
			}
			else if (PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(FSkeletalMeshLODInfo, bSupportUniformlyDistributedSampling))
			{
				GetSamplingInfoInternal().BuildWholeMesh(this);
			}
		}
		else
		{
			//Rebuild the lot. No property could mean a reimport.
			GetSamplingInfoInternal().BuildRegions(this);
			GetSamplingInfoInternal().BuildWholeMesh(this);
		}

		UpdateUVChannelData(true);
		UpdateGenerateUpToData();
	}

	OnMeshChanged.Broadcast();

	for (UAssetUserData* Datum : AssetUserData)
	{
		if (Datum != nullptr)
		{
			Datum->PostEditChangeOwner();
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);

	//The stack counter here should be 1 since the BlockRecursiveCallScope protection has the lock and it will be decrement to 0 when we get out of the function scope
	check(PostEditChangeStackCounter == 1);
}

void USkeletalMesh::PostEditUndo()
{
	check(IsInGameThread());

	Super::PostEditUndo();
	for( TObjectIterator<USkeletalMeshComponent> It; It; ++It )
	{
		USkeletalMeshComponent* MeshComponent = *It;
		if( MeshComponent && 
			!MeshComponent->IsTemplate() &&
			MeshComponent->GetSkeletalMeshAsset() == this )
		{
			FComponentReregisterContext Context(MeshComponent);
		}
	}

	if(GetMorphTargets().Num() > GetMorphTargetIndexMap().Num())
	{
		// A morph target remove has been undone, reinitialise
		InitMorphTargets();
	}
}

void USkeletalMesh::UpdateGenerateUpToData()
{
	for (int32 LodIndex = 0; LodIndex < GetImportedModel()->LODModels.Num(); ++LodIndex)
	{
		FSkeletalMeshLODModel& LodModel = GetImportedModel()->LODModels[LodIndex];
		for (int32 SectionIndex = 0; SectionIndex < LodModel.Sections.Num(); ++SectionIndex)
		{
			int32 SpecifiedLodIndex = LodModel.Sections[SectionIndex].GenerateUpToLodIndex;
			if (SpecifiedLodIndex != -1 && SpecifiedLodIndex < LodIndex)
			{
				LodModel.Sections[SectionIndex].GenerateUpToLodIndex = LodIndex;
			}
		}
	}
}

#endif // WITH_EDITOR

void USkeletalMesh::BeginDestroy()
{
	check(IsInGameThread());

	Super::BeginDestroy();

	if (FSkinWeightProfileManager * Manager = FSkinWeightProfileManager::Get(GetWorld()))
	{
		Manager->CancelSkinWeightProfileRequest(this);
	}

#if WITH_EDITOR
	// Before trying to touch GetSkeleton which might cause a wait on the async task,
	// tell the async task we don't need it anymore so it gets cancelled if possible.
	TryCancelAsyncTasks();
#endif

	// remove the cache of link up
	if (GetSkeleton())
	{
		GetSkeleton()->RemoveLinkup(this);
	}

	// Release the mesh's render resources now if no pending streaming op.
	if (!HasPendingInitOrStreaming())
	{
		ReleaseResources();
	}
}

bool USkeletalMesh::IsReadyForFinishDestroy()
{
#if WITH_EDITOR
	// We're being garbage collected and might still have async tasks pending
	if (!TryCancelAsyncTasks())
	{
		return false;
	}
#endif

	if (!Super::IsReadyForFinishDestroy())
	{
		return false;
	}

	ReleaseResources();

	// see if we have hit the resource flush fence
	return ReleaseResourcesFence.IsFenceComplete();
}

#if WITH_EDITOR
void CachePlatform(USkeletalMesh* Mesh, const ITargetPlatform* TargetPlatform, FSkeletalMeshRenderData* PlatformRenderData, const bool bIsSerializeSaving)
{
	//Cache the platform, dcc should be valid so it will be fast
	check(TargetPlatform);
	FSkinnedAssetBuildContext Context;
	Context.bIsSerializeSaving = bIsSerializeSaving;
	PlatformRenderData->Cache(TargetPlatform, Mesh, &Context);
}

static FSkeletalMeshRenderData& GetPlatformSkeletalMeshRenderData(USkeletalMesh* Mesh, const ITargetPlatform* TargetPlatform, const bool bIsSerializeSaving)
{
	const FString PlatformDerivedDataKey = Mesh->BuildDerivedDataKey(TargetPlatform);
	FSkeletalMeshRenderData* PlatformRenderData = Mesh->GetResourceForRendering();
	if (Mesh->GetOutermost()->bIsCookedForEditor)
	{
		check(PlatformRenderData);
		return *PlatformRenderData;
	}

	while (PlatformRenderData && PlatformRenderData->DerivedDataKey != PlatformDerivedDataKey)
	{
		PlatformRenderData = PlatformRenderData->NextCachedRenderData.Get();
	}

	if (PlatformRenderData == NULL)
	{
		// Cache render data for this platform and insert it in to the linked list.
		PlatformRenderData = new FSkeletalMeshRenderData();
		CachePlatform(Mesh, TargetPlatform, PlatformRenderData, bIsSerializeSaving);
		check(PlatformRenderData->DerivedDataKey == PlatformDerivedDataKey);
		Swap(PlatformRenderData->NextCachedRenderData, Mesh->GetResourceForRendering()->NextCachedRenderData);
		Mesh->GetResourceForRendering()->NextCachedRenderData = TUniquePtr<FSkeletalMeshRenderData>(PlatformRenderData);
	}
	check(PlatformRenderData->DerivedDataKey == PlatformDerivedDataKey);
	check(PlatformRenderData);
	return *PlatformRenderData;
}
#endif

LLM_DEFINE_TAG(SkeletalMesh_Serialize); // This is an important test case for LLM_DEFINE_TAG

void USkeletalMesh::Serialize( FArchive& Ar )
{
	LLM_SCOPE_BYNAME(TEXT("SkeletalMesh/Serialize")); // This is an important test case for SCOPE_BYNAME with a matching LLM_DEFINE_TAG
	DECLARE_SCOPE_CYCLE_COUNTER( TEXT("USkeletalMesh::Serialize"), STAT_SkeletalMesh_Serialize, STATGROUP_LoadTime );
	TRACE_CPUPROFILER_EVENT_SCOPE(USkeletalMesh::Serialize);

#if WITH_EDITOR
	if (IsCompiling())
	{
		// Skip serialization during compilation if told to do so.
		if (Ar.ShouldSkipCompilingAssets())
		{
			return;
		}

		// Since UPROPERTY are accessed directly by offset during serialization instead of using accessors, 
		// the protection put in place to automatically finish compilation if a locked property is accessed will not work. 
		// We have no choice but to force finish the compilation here to avoid potential race conditions between 
		// async compilation and the serialization.
		FSkinnedAssetCompilingManager::Get().FinishCompilation({ this });
	}
#endif

	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FFrameworkObjectVersion::GUID);
	Ar.UsingCustomVersion(FEditorObjectVersion::GUID);
	Ar.UsingCustomVersion(FSkeletalMeshCustomVersion::GUID);
	Ar.UsingCustomVersion(FRenderingObjectVersion::GUID);
	Ar.UsingCustomVersion(FFortniteMainBranchObjectVersion::GUID);
	Ar.UsingCustomVersion(FNiagaraObjectVersion::GUID);
	Ar.UsingCustomVersion(FUE5MainStreamObjectVersion::GUID);

	FStripDataFlags StripFlags( Ar );

	FBoxSphereBounds LocalImportedBounds = GetImportedBounds();
	Ar << LocalImportedBounds;
	SetImportedBounds(LocalImportedBounds);

	Ar << GetMaterials();

	Ar << GetRefSkeleton();

	if (Ar.IsLoading())
	{
		const bool bRebuildNameMap = false;
		GetRefSkeleton().RebuildRefSkeleton(GetSkeleton(), bRebuildNameMap);
	}

#if WITH_EDITORONLY_DATA
	// Serialize the source model (if we want editor data)
	if (!StripFlags.IsEditorDataStripped())
	{
		GetImportedModel()->Serialize(Ar, this);
	}
#endif // WITH_EDITORONLY_DATA

	if (Ar.CustomVer(FSkeletalMeshCustomVersion::GUID) >= FSkeletalMeshCustomVersion::SplitModelAndRenderData)
	{
		bool bCooked = Ar.IsCooking();
		Ar << bCooked;

		const bool bIsDuplicating = Ar.HasAnyPortFlags(PPF_Duplicate);

		// Inline the derived data for cooked builds. Never include render data when
		// counting memory as it is included by GetResourceSize.
		if ((bIsDuplicating || bCooked) && !IsTemplate() && !Ar.IsCountingMemory())
		{
			if (Ar.IsLoading())
			{
				SetSkeletalMeshRenderData(MakeUnique<FSkeletalMeshRenderData>());
				GetSkeletalMeshRenderData()->Serialize(Ar, this);
			}
			else if (Ar.IsSaving())
			{

				FSkeletalMeshRenderData* LocalSkeletalMeshRenderData = GetSkeletalMeshRenderData();
				if (bCooked)
				{
#if WITH_EDITORONLY_DATA
					ITargetPlatform* RunningPlatform = GetTargetPlatformManagerRef().GetRunningTargetPlatform();
					const ITargetPlatform* ArchiveCookingTarget = Ar.CookingTarget();
					constexpr bool bIsSerializeSaving = true;
					if (ArchiveCookingTarget)
					{
						LocalSkeletalMeshRenderData = &GetPlatformSkeletalMeshRenderData(this, ArchiveCookingTarget, bIsSerializeSaving);
					}
					else
					{
						//Fall back in case we use an archive that the cooking target has not been set (i.e. Duplicate archive)
						check(RunningPlatform != NULL);
						LocalSkeletalMeshRenderData = &GetPlatformSkeletalMeshRenderData(this, RunningPlatform, bIsSerializeSaving);
					}
#endif
					int32 MaxBonesPerChunk = LocalSkeletalMeshRenderData->GetMaxBonesPerSection();

					TArray<FName> DesiredShaderFormats;
					Ar.CookingTarget()->GetAllTargetedShaderFormats(DesiredShaderFormats);

					for (int32 FormatIndex = 0; FormatIndex < DesiredShaderFormats.Num(); ++FormatIndex)
					{
						const EShaderPlatform LegacyShaderPlatform = ShaderFormatToLegacyShaderPlatform(DesiredShaderFormats[FormatIndex]);
						const ERHIFeatureLevel::Type FeatureLevelType = GetMaxSupportedFeatureLevel(LegacyShaderPlatform);

						int32 MaxNrBones = FGPUBaseSkinVertexFactory::GetMaxGPUSkinBones(Ar.CookingTarget());
						if (MaxBonesPerChunk > MaxNrBones)
						{
							FString FeatureLevelName;
							GetFeatureLevelName(FeatureLevelType, FeatureLevelName);
							UE_LOG(LogSkeletalMesh, Warning, TEXT("Skeletal mesh %s has a LOD section with %d bones and the maximum supported number for feature level %s is %d.\n!This mesh will not be rendered on the specified platform!"),
								*GetFullName(), MaxBonesPerChunk, *FeatureLevelName, MaxNrBones);
						}
					}
				}
				LocalSkeletalMeshRenderData->Serialize(Ar, this);
			}
		}
	}

	// make sure we're counting properly
	if (!Ar.IsLoading() && !Ar.IsSaving())
	{
		Ar << GetRefBasesInvMatrix();
	}

	if( Ar.UEVer() < VER_UE4_REFERENCE_SKELETON_REFACTOR )
	{
		TMap<FName, int32> DummyNameIndexMap;
		Ar << DummyNameIndexMap;
	}

	//@todo legacy
	TArray<UObject*> DummyObjs;
	Ar << DummyObjs;

	if (Ar.IsLoading() && Ar.CustomVer(FRenderingObjectVersion::GUID) < FRenderingObjectVersion::TextureStreamingMeshUVChannelData)
	{
		TArray<float> CachedStreamingTextureFactors;
		Ar << CachedStreamingTextureFactors;
	}

#if WITH_EDITORONLY_DATA
	if ( !StripFlags.IsEditorDataStripped() )
	{
		// Backwards compat for old SourceData member
		// Doing a <= check here as no asset from UE streams could ever have been saved at exactly 11, but a stray no-op vesion increment was added
		// in Fortnite/Main meaning some assets there were at exactly version 11. Doing a <= allows us to properly apply this version even to those assets
		if (Ar.IsLoading() && Ar.CustomVer(FSkeletalMeshCustomVersion::GUID) <= FSkeletalMeshCustomVersion::RemoveSourceData)
		{
			bool bHaveSourceData = false;
			Ar << bHaveSourceData;
			if (bHaveSourceData)
			{
				FSkeletalMeshLODModel DummyLODModel;
				DummyLODModel.Serialize(Ar, this, INDEX_NONE);
			}
		}
	}

	if ( Ar.IsLoading() && Ar.UEVer() < VER_UE4_ASSET_IMPORT_DATA_AS_JSON && !GetAssetImportData())
	{
		// AssetImportData should always be valid
		SetAssetImportData(NewObject<UAssetImportData>(this, TEXT("AssetImportData")));
	}
	
	// SourceFilePath and SourceFileTimestamp were moved into a subobject
	if ( Ar.IsLoading() && Ar.UEVer() < VER_UE4_ADDED_FBX_ASSET_IMPORT_DATA && GetAssetImportData())
	{
		// AssetImportData should always have been set up in the constructor where this is relevant
		FAssetImportInfo Info;
		Info.Insert(FAssetImportInfo::FSourceFile(SourceFilePath_DEPRECATED));
		GetAssetImportData()->SourceData = MoveTemp(Info);
		
		SourceFilePath_DEPRECATED = TEXT("");
		SourceFileTimestamp_DEPRECATED = TEXT("");
	}

	if (Ar.UEVer() >= VER_UE4_APEX_CLOTH)
	{
		if(Ar.CustomVer(FSkeletalMeshCustomVersion::GUID) < FSkeletalMeshCustomVersion::NewClothingSystemAdded)
		{
		// Serialize non-UPROPERTY ApexClothingAsset data.
			for(int32 Idx = 0; Idx < ClothingAssets_DEPRECATED.Num(); Idx++)
		{
				Ar << ClothingAssets_DEPRECATED[Idx];
			}
		}

		if (Ar.UEVer() < VER_UE4_REFERENCE_SKELETON_REFACTOR)
		{
			RebuildRefSkeletonNameToIndexMap();
		}
	}

	if ( Ar.IsLoading() && Ar.UEVer() < VER_UE4_MOVE_SKELETALMESH_SHADOWCASTING )
	{
		// Previous to this version, shadowcasting flags were stored in the LODInfo array
		// now they're in the Materials array so we need to move them over
		MoveDeprecatedShadowFlagToMaterials();
	}

	if (Ar.UEVer() < VER_UE4_SKELETON_ASSET_PROPERTY_TYPE_CHANGE)
	{
		GetPreviewAttachedAssetContainer().SaveAttachedObjectsFromDeprecatedProperties();
	}
#endif

	if (GetEnablePerPolyCollision())
	{
		const USkeletalMesh* ConstThis = this;
		UBodySetup* LocalBodySetup = ConstThis->GetBodySetup();
		Ar << LocalBodySetup;
		SetBodySetup(LocalBodySetup);
	}

#if WITH_EDITORONLY_DATA
	if (Ar.CustomVer(FEditorObjectVersion::GUID) < FEditorObjectVersion::RefactorMeshEditorMaterials)
	{
		MoveMaterialFlagsToSections();
	}
#endif

#if WITH_EDITORONLY_DATA
	SetRequiresLODScreenSizeConversion(Ar.CustomVer(FFrameworkObjectVersion::GUID) < FFrameworkObjectVersion::LODsUseResolutionIndependentScreenSize);
	SetRequiresLODHysteresisConversion(Ar.CustomVer(FFrameworkObjectVersion::GUID) < FFrameworkObjectVersion::LODHysteresisUseResolutionIndependentScreenSize);
#endif

	if (Ar.CustomVer(FFortniteMainBranchObjectVersion::GUID) < FFortniteMainBranchObjectVersion::ConvertReductionSettingOptions)
	{
		TArray<FSkeletalMeshLODInfo>& LODInfoArray = GetLODInfoArray();
		const int32 TotalLODNum = LODInfoArray.Num();
		for (int32 LodIndex = 1; LodIndex < TotalLODNum; LodIndex++)
		{
			FSkeletalMeshLODInfo& ThisLODInfo = LODInfoArray[LodIndex];
			// prior to this version, both of them were used
			ThisLODInfo.ReductionSettings.ReductionMethod = SMOT_TriangleOrDeviation;
			if (ThisLODInfo.ReductionSettings.MaxDeviationPercentage == 0.f)
			{
				// 0.f and 1.f should produce same result. However, it is bad to display 0.f in the slider
				// as 0.01 and 0.f causes extreme confusion. 
				ThisLODInfo.ReductionSettings.MaxDeviationPercentage = 1.f;
			}
		}
	}

	if (Ar.IsLoading() && Ar.CustomVer(FEditorObjectVersion::GUID) < FEditorObjectVersion::SkeletalMeshBuildRefactor)
	{
		TArray<FSkeletalMeshLODInfo>& LODInfoArray = GetLODInfoArray();
		const int32 TotalLODNum = LODInfoArray.Num();
		for (int32 LodIndex = 0; LodIndex < TotalLODNum; LodIndex++)
		{
			FSkeletalMeshLODInfo& ThisLODInfo = LODInfoArray[LodIndex];
			// Restore the deprecated settings
			ThisLODInfo.BuildSettings.bUseFullPrecisionUVs = bUseFullPrecisionUVs_DEPRECATED;
			ThisLODInfo.BuildSettings.bUseHighPrecisionTangentBasis = bUseHighPrecisionTangentBasis_DEPRECATED;
			ThisLODInfo.BuildSettings.bRemoveDegenerates = true;
			//We cannot get back the imported build option here since those option are store in the UAssetImportData which FBX has derive in the UnrealEd module
			//We are in engine module so there is no way to recover this data.
			//Anyway because the asset was not re-import yet the build settings will not be shown in the UI and the asset will not be build
			//With the new build until it get re-import (geo and skinning)
			//So we will leave the default value for the rest of the new build settings
		}
	}
	
	// Automatically detect assets saved before CL 16135278 which changed F16 to RTNE
	//	set them to bUseBackwardsCompatibleF16TruncUVs
	if (Ar.IsLoading() && Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) < FUE5MainStreamObjectVersion::DirLightsAreAtmosphereLightsByDefault)
	{
		TArray<FSkeletalMeshLODInfo>& LODInfoArray = GetLODInfoArray();
		const int32 TotalLODNum = LODInfoArray.Num();
		for (int32 LodIndex = 0; LodIndex < TotalLODNum; LodIndex++)
		{
			FSkeletalMeshLODInfo& ThisLODInfo = LODInfoArray[LodIndex];

			ThisLODInfo.BuildSettings.bUseBackwardsCompatibleF16TruncUVs = true;
		}
	}
}

#if WITH_EDITORONLY_DATA
void USkeletalMesh::DeclareCustomVersions(FArchive& Ar, const UClass* SpecificSubclass)
{
	Super::DeclareCustomVersions(Ar, SpecificSubclass);
	FSkeletalMaterial::DeclareCustomVersions(Ar);
	FSkeletalMeshLODModel::DeclareCustomVersions(Ar);
}
#endif

void USkeletalMesh::GetPreloadDependencies(TArray<UObject*>& OutDeps)
{
	Super::GetPreloadDependencies(OutDeps);
	OutDeps.Add(GetSkeleton());
}

void USkeletalMesh::FlushRenderState()
{
	//TComponentReregisterContext<USkeletalMeshComponent> ReregisterContext;

	// Release the mesh's render resources.
	ReleaseResources();

	// Flush the resource release commands to the rendering thread to ensure that the edit change doesn't occur while a resource is still
	// allocated, and potentially accessing the mesh data.
	ReleaseResourcesFence.Wait();
}

#if WITH_EDITOR

void USkeletalMesh::PrepareForAsyncCompilation()
{
	// Make sure statics are initialized before calling from multiple threads
	GetSkeletalMeshDerivedDataVersion();

	// Make sure the target platform is properly initialized before accessing it from multiple threads
	ITargetPlatformManagerModule& TargetPlatformManager = GetTargetPlatformManagerRef();
	ITargetPlatform* RunningPlatform = TargetPlatformManager.GetRunningTargetPlatform();
	check(RunningPlatform);

	// Ensure those modules are loaded on the main thread - we'll need them in async tasks
	FModuleManager::Get().LoadModuleChecked<IMeshUtilities>(TEXT("MeshUtilities"));
	FModuleManager::Get().LoadModuleChecked<IMeshReductionManagerModule>(TEXT("MeshReductionInterface"));
	IMeshBuilderModule::GetForRunningPlatform();
	for (const ITargetPlatform* TargetPlatform : TargetPlatformManager.GetActiveTargetPlatforms())
	{
		IMeshBuilderModule::GetForPlatform(TargetPlatform);
	}

	// Release any property that are not touched by async build/postload here

	// The properties are still protected so if an async step tries to 
	// use them without protection, it will assert and will mean we have
	// to either avoid touching them asynchronously or we need to remove
	// the release property here at the cost of maybe causing more stalls
	// from the game thread.

	// Not touched during async build and can cause stalls when the content browser refresh its tiles.
	ReleaseAsyncProperty((uint64)ESkeletalMeshAsyncProperties::ThumbnailInfo);
}

void USkeletalMesh::Build()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(USkeletalMesh::Build);

	if (IsCompiling())
	{
		FSkinnedAssetCompilingManager::Get().FinishCompilation({this});
	}

	FSkinnedAssetAsyncBuildScope AsyncBuildScope(this);

	FSkinnedAssetBuildContext Context;
	BeginBuildInternal(Context);
	
	// Inline reduction is not thread-safe, prevent async build until this is fixed.
	if (FSkinnedAssetCompilingManager::Get().IsAsyncCompilationAllowed(this))
	{
		PrepareForAsyncCompilation();

		FQueuedThreadPool* SkeletalMeshThreadPool = FSkinnedAssetCompilingManager::Get().GetThreadPool();
		EQueuedWorkPriority BasePriority = FSkinnedAssetCompilingManager::Get().GetBasePriority(this);
		check(AsyncTask == nullptr);
		AsyncTask = MakeUnique<FSkinnedAssetAsyncBuildTask>(this, MoveTemp(Context));
		AsyncTask->StartBackgroundTask(SkeletalMeshThreadPool, BasePriority, EQueuedWorkFlags::DoNotRunInsideBusyWait);
		FSkinnedAssetCompilingManager::Get().AddSkinnedAssets({ this });
	}
	else
	{
		ExecuteBuildInternal(Context);
		FinishBuildInternal(Context);
	}
}

void USkeletalMesh::BeginBuildInternal(FSkinnedAssetBuildContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(USkeletalMesh::BeginBuildInternal);

	SetInternalFlags(EInternalObjectFlags::Async);
	
	// Unregister all instances of this component
	Context.RecreateRenderStateContext = MakeUnique<FSkinnedMeshComponentRecreateRenderStateContext>(this, false);

	// Release the static mesh's resources.
	ReleaseResources();

	//Make sure InlineReduction structure are created
	const int32 MaxLodIndex = GetLODNum() - 1;
	if (GetImportedModel()->InlineReductionCacheDatas.IsValidIndex(MaxLodIndex))
	{
		//We should not do that in main thread, this is why there is an ensure
		GetImportedModel()->InlineReductionCacheDatas.AddDefaulted((MaxLodIndex + 1) - GetImportedModel()->InlineReductionCacheDatas.Num());
	}

	// Flush the resource release commands to the rendering thread to ensure that the build doesn't occur while a resource is still
	// allocated, and potentially accessing the USkeletalMesh.
	ReleaseResourcesFence.Wait();

	// Lock all properties that should not be modified/accessed during async post-load
	AcquireAsyncProperty();
}

void USkeletalMesh::ExecuteBuildInternal(FSkinnedAssetBuildContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(USkeletalMesh::ExecuteBuildInternal);

	// This scope allows us to use any locked properties without causing stalls
	FSkinnedAssetAsyncBuildScope AsyncBuildScope(this);

	// rebuild render data from imported model
	CacheDerivedData(&Context);

	// Do not need to fix up 16-bit UVs here, as we assume all editor platforms support them.
	ensure(GVertexElementTypeSupport.IsSupported(VET_Half2));

	GetSamplingInfoInternal().BuildRegions(this);
	GetSamplingInfoInternal().BuildWholeMesh(this);

	UpdateUVChannelData(true);
	UpdateGenerateUpToData();
}

void USkeletalMesh::ApplyFinishBuildInternalData(FSkinnedAssetCompilationContext* ContextPtr)
{
	//We cannot execute this code outside of the game thread
	checkf(IsInGameThread(), TEXT("Cannot execute function USkeletalMesh::ApplyFinishBuildInternalData asynchronously. Asset: %s"), *this->GetFullName());
	check(ContextPtr);
}

void USkeletalMesh::FinishBuildInternal(FSkinnedAssetBuildContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(USkeletalMesh::FinishBuildInternal);

	ClearInternalFlags(EInternalObjectFlags::Async);

	ReleaseAsyncProperty();

	ApplyFinishBuildInternalData(&Context);
	
	// Note: meshes can be built during automated importing.  We should not create resources in that case
	// as they will never be released when this object is deleted
	if (FApp::CanEverRender())
	{
		// Reinitialize the static mesh's resources.
		InitResources();
	}

	PostMeshCached.Broadcast(this);
}

void USkeletalMesh::LockPropertiesUntil(FEvent* Event)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(USkeletalMesh::Import);
	if (IsCompiling())
	{
		FSkinnedAssetCompilingManager::Get().FinishCompilation({ this });
	}

	auto AsyncTaskFunction = [Event]()
	{
		Event->Wait();
	};

	//Use the async task compile to lock the properties
	FSkinnedAsyncTaskContext Context(!HasAnyInternalFlags(EInternalObjectFlags::Async), AsyncTaskFunction);
	BeginAsyncTaskInternal(Context);
	PrepareForAsyncCompilation();
	FQueuedThreadPool* SkeletalMeshThreadPool = FSkinnedAssetCompilingManager::Get().GetThreadPool();
	EQueuedWorkPriority BasePriority = FSkinnedAssetCompilingManager::Get().GetBasePriority(this);
	check(AsyncTask == nullptr);
	AsyncTask = MakeUnique<FSkinnedAssetAsyncBuildTask>(this, MoveTemp(Context));
	AsyncTask->StartBackgroundTask(SkeletalMeshThreadPool, BasePriority, EQueuedWorkFlags::DoNotRunInsideBusyWait);
	FSkinnedAssetCompilingManager::Get().AddSkinnedAssets({ this });
}

void USkeletalMesh::BeginAsyncTaskInternal(FSkinnedAsyncTaskContext& Context)
{
	check(IsInGameThread());

	if (Context.bResetAsyncFlagOnFinish)
	{
		SetInternalFlags(EInternalObjectFlags::Async);
	}
	AcquireAsyncProperty();
	//Allow thumbnail data so content browser get refresh properly
	ReleaseAsyncProperty((uint64)ESkeletalMeshAsyncProperties::ThumbnailInfo);
}

void USkeletalMesh::ExecuteAsyncTaskInternal(FSkinnedAsyncTaskContext& Context)
{
	Context.AsyncTaskFunction();
}

void USkeletalMesh::FinishAsyncTaskInternal(FSkinnedAsyncTaskContext& Context)
{
	check(IsInGameThread());
	if (Context.bResetAsyncFlagOnFinish)
	{
		ClearInternalFlags(EInternalObjectFlags::Async);
	}
	ReleaseAsyncProperty();
}

#endif // #if WITH_EDITOR

void USkeletalMesh::PreSave(const class ITargetPlatform* TargetPlatform)
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS;
	Super::PreSave(TargetPlatform);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS;
}

void USkeletalMesh::PreSave(FObjectPreSaveContext ObjectSaveContext)
{
	// check the parent index of the root bone is invalid
	check((GetRefSkeleton().GetNum() == 0) || (GetRefSkeleton().GetRefBoneInfo()[0].ParentIndex == INDEX_NONE));

	Super::PreSave(ObjectSaveContext);
}

// Pre-calculate refpose-to-local transforms
void USkeletalMesh::CalculateInvRefMatrices()
{
	const int32 NumRealBones = GetRefSkeleton().GetRawBoneNum();
	
	TArray<FMatrix>& ComposedRefPoseMatrices = GetCachedComposedRefPoseMatrices();

	if(GetRefBasesInvMatrix().Num() != NumRealBones)
	{
		GetRefBasesInvMatrix().Empty(NumRealBones);
		GetRefBasesInvMatrix().AddUninitialized(NumRealBones);

		// Reset cached mesh-space ref pose
		ComposedRefPoseMatrices.Empty(NumRealBones);
		ComposedRefPoseMatrices.AddUninitialized(NumRealBones);

		// Precompute the Mesh.RefBasesInverse.
		for( int32 b=0; b<NumRealBones; b++)
		{
			// Render the default pose.
			ComposedRefPoseMatrices[b] = GetRefPoseMatrix(b);

			// Construct mesh-space skeletal hierarchy.
			if( b>0 )
			{
				int32 Parent = GetRefSkeleton().GetRawParentIndex(b);
				ComposedRefPoseMatrices[b] = ComposedRefPoseMatrices[b] * ComposedRefPoseMatrices[Parent];
			}

			FVector XAxis, YAxis, ZAxis;

			ComposedRefPoseMatrices[b].GetScaledAxes(XAxis, YAxis, ZAxis);
			if(	XAxis.IsNearlyZero(UE_SMALL_NUMBER) &&
				YAxis.IsNearlyZero(UE_SMALL_NUMBER) &&
				ZAxis.IsNearlyZero(UE_SMALL_NUMBER))
			{
				// this is not allowed, warn them 
				UE_LOG(LogSkeletalMesh, Warning, TEXT("Reference Pose for asset %s for joint (%s) includes NIL matrix. Zero scale isn't allowed on ref pose. "), *GetPathName(), *GetRefSkeleton().GetBoneName(b).ToString());
			}

			// Precompute inverse so we can use from-refpose-skin vertices.
			GetRefBasesInvMatrix()[b] = FMatrix44f(ComposedRefPoseMatrices[b].Inverse());
		}

#if WITH_EDITORONLY_DATA
		if(GetRetargetBasePose().Num() == 0)
		{
			SetRetargetBasePose(GetRefSkeleton().GetRefBonePose());
		}
#endif // WITH_EDITORONLY_DATA
	}
}

#if WITH_EDITOR
void USkeletalMesh::ReallocateRetargetBasePose()
{
	// if you're adding other things here, please note that this function is called during postLoad
	// fix up retarget base pose if VB has changed
	// if we have virtual joints, we make sure Retarget Base Pose matches
	const int32 RawNum = GetRefSkeleton().GetRawBoneNum();
	const int32 VBNum = GetRefSkeleton().GetVirtualBoneRefData().Num();
	const int32 BoneNum = GetRefSkeleton().GetNum();
	check(RawNum + VBNum == BoneNum);

	const int32 OldRetargetBasePoseNum = GetRetargetBasePose().Num();
	// we want to make sure retarget base pose contains raw numbers PREVIOUSLY
	// otherwise, we may override wrong transform
	if (OldRetargetBasePoseNum >= RawNum)
	{
		// we have to do this in case buffer size changes (shrink for example)
		GetRetargetBasePose().SetNum(BoneNum);

		// if we have VB, we should override them
		// they're not editable, so it's fine to override them from raw bones
		if (VBNum > 0)
		{
			const TArray<FTransform>& BonePose = GetRefSkeleton().GetRefBonePose();
			check(GetRetargetBasePose().GetTypeSize() == BonePose.GetTypeSize());
			const int32 ElementSize = GetRetargetBasePose().GetTypeSize();
			FMemory::Memcpy(GetRetargetBasePose().GetData() + RawNum, BonePose.GetData() + RawNum, ElementSize*VBNum);
		}
	}
	else
	{
		// else we think, something has changed, we just override retarget base pose to current pose
		GetRetargetBasePose() = GetRefSkeleton().GetRefBonePose();
	}
}

void USkeletalMesh::CalculateRequiredBones(FSkeletalMeshLODModel& LODModel, const struct FReferenceSkeleton& InRefSkeleton, const TMap<FBoneIndexType, FBoneIndexType> * BonesToRemove)
{
	// RequiredBones for base model includes all raw bones.
	int32 RequiredBoneCount = InRefSkeleton.GetRawBoneNum();
	LODModel.RequiredBones.Empty(RequiredBoneCount);
	for(int32 i=0; i<RequiredBoneCount; i++)
	{
		// Make sure it's not in BonesToRemove
		// @Todo change this to one TArray
		if (!BonesToRemove || BonesToRemove->Find(i) == NULL)
		{
			LODModel.RequiredBones.Add(i);
		}
	}

	LODModel.RequiredBones.Shrink();	
}

void USkeletalMesh::RemoveLegacyClothingSections()
{
	// Remove duplicate skeletal mesh sections previously used for clothing simulation
	if(GetLinkerCustomVersion(FSkeletalMeshCustomVersion::GUID) < FSkeletalMeshCustomVersion::RemoveDuplicatedClothingSections)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(USkeletalMesh::RemoveLegacyClothingSections);

		if(FSkeletalMeshModel* Model = GetImportedModel())
		{
			for(FSkeletalMeshLODModel& LodModel : Model->LODModels)
			{
				int32 ClothingSectionCount = 0;
				uint32 BaseVertex = MAX_uint32;
				int32 VertexCount = 0;
				uint32 BaseIndex = MAX_uint32;
				int32 IndexCount = 0;

				for(int32 SectionIndex = 0; SectionIndex < LodModel.Sections.Num(); ++SectionIndex)
				{
					FSkelMeshSection& Section = LodModel.Sections[SectionIndex];

					// If the section is disabled, it could be a clothing section
					if(Section.bLegacyClothingSection_DEPRECATED && Section.CorrespondClothSectionIndex_DEPRECATED != INDEX_NONE)
					{
						FSkelMeshSection& DuplicatedSection = LodModel.Sections[Section.CorrespondClothSectionIndex_DEPRECATED];

						// Cache the base index for the first clothing section (will be in correct order)
						if(ClothingSectionCount == 0)
						{
							PreEditChange(nullptr);
						}
						
						BaseVertex = FMath::Min(DuplicatedSection.BaseVertexIndex, BaseVertex);
						BaseIndex = FMath::Min(DuplicatedSection.BaseIndex, BaseIndex);

						VertexCount += DuplicatedSection.SoftVertices.Num();
						IndexCount += DuplicatedSection.NumTriangles * 3;

						// Mapping data for clothing could be built either on the source or the
						// duplicated section and has changed a few times, so check here for
						// where to get our data from
						constexpr int32 ClothLODBias = 0;  // There isn't any cloth LOD bias on legacy sections
						if (DuplicatedSection.ClothMappingDataLODs.Num() && DuplicatedSection.ClothMappingDataLODs[ClothLODBias].Num())
						{
							Section.ClothingData = DuplicatedSection.ClothingData;
							Section.ClothMappingDataLODs = DuplicatedSection.ClothMappingDataLODs;
						}

						Section.CorrespondClothAssetIndex = GetMeshClothingAssets().IndexOfByPredicate([&Section](const UClothingAssetBase* CurrAsset)
						{
							return CurrAsset && CurrAsset->GetAssetGuid() == Section.ClothingData.AssetGuid;
						});

						Section.BoneMap = DuplicatedSection.BoneMap;
						Section.bLegacyClothingSection_DEPRECATED = false;

						// Remove the reference index
						Section.CorrespondClothSectionIndex_DEPRECATED = INDEX_NONE;

						//Make sure the UserSectionsData is up to date
						if (FSkelMeshSourceSectionUserData* SectionUserData = LodModel.UserSectionsData.Find(Section.OriginalDataSectionIndex))
						{
							SectionUserData->CorrespondClothAssetIndex = Section.CorrespondClothAssetIndex;
							SectionUserData->ClothingData = Section.ClothingData;
						}

						ClothingSectionCount++;
					}
					else
					{
						Section.CorrespondClothAssetIndex = INDEX_NONE;
						Section.ClothingData.AssetGuid = FGuid();
						Section.ClothingData.AssetLodIndex = INDEX_NONE;
						Section.ClothMappingDataLODs.Empty();
					}
				}

				if(BaseVertex != MAX_uint32 && BaseIndex != MAX_uint32)
				{
					// Remove from section list
					LodModel.Sections.RemoveAt(LodModel.Sections.Num() - ClothingSectionCount, ClothingSectionCount);

					// Clean up actual geometry
					LodModel.IndexBuffer.RemoveAt(BaseIndex, IndexCount);
					LodModel.NumVertices -= VertexCount;

					// Clean up index entries above the base we removed.
					// Ideally this shouldn't be unnecessary as clothing was at the end of the buffer
					// but this will always be safe to run to make sure adjacency generates correctly.
					for(uint32& Index : LodModel.IndexBuffer)
					{
						if(Index >= BaseVertex)
						{
							Index -= VertexCount;
						}
					}
				}
			}
		}
	}
}

USkeletalMeshEditorData& USkeletalMesh::GetMeshEditorData() const
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (MeshEditorDataObject == nullptr)
	{
			//The asset is created in the skeletalmesh package. We keep it private so the user cannot see it in the content browser
		//RF_Transactional make sure the asset can be transactional if we want to edit it
		USkeletalMesh* NonConstSkeletalMesh = const_cast<USkeletalMesh*>(this);
		MeshEditorDataObject = NewObject<USkeletalMeshEditorData>(NonConstSkeletalMesh, NAME_None, RF_Transactional);
	}
	//Make sure we have a valid pointer
	check(MeshEditorDataObject != nullptr);
	return *MeshEditorDataObject;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void USkeletalMesh::LoadLODImportedData(const int32 LODIndex, FSkeletalMeshImportData& OutMesh) const
{
	GetMeshEditorData().GetLODImportedData(LODIndex).LoadRawMesh(OutMesh);
}

void USkeletalMesh::SaveLODImportedData(const int32 LODIndex, FSkeletalMeshImportData& InMesh)
{
	FRawSkeletalMeshBulkData& RawSkeletalMeshBulkData = GetMeshEditorData().GetLODImportedData(LODIndex);
	RawSkeletalMeshBulkData.SaveRawMesh(InMesh);
	//Update the cache
	check(GetImportedModel()->LODModels.IsValidIndex(LODIndex));
	GetImportedModel()->LODModels[LODIndex].RawSkeletalMeshBulkDataID = RawSkeletalMeshBulkData.GetIdString();
	GetImportedModel()->LODModels[LODIndex].bIsBuildDataAvailable = RawSkeletalMeshBulkData.IsBuildDataAvailable();
	GetImportedModel()->LODModels[LODIndex].bIsRawSkeletalMeshBulkDataEmpty = RawSkeletalMeshBulkData.IsEmpty();
}

bool USkeletalMesh::IsLODImportedDataBuildAvailable(const int32 LODIndex) const
{
	if (!GetImportedModel()->LODModels.IsValidIndex(LODIndex))
	{
		return false;
	}
	return GetImportedModel()->LODModels[LODIndex].bIsBuildDataAvailable;
}

bool USkeletalMesh::IsLODImportedDataEmpty(const int32 LODIndex) const
{
	if (!GetImportedModel()->LODModels.IsValidIndex(LODIndex))
	{
		return false;
	}
	return GetImportedModel()->LODModels[LODIndex].bIsRawSkeletalMeshBulkDataEmpty;
}

void USkeletalMesh::GetLODImportedDataVersions(const int32 LODIndex, ESkeletalMeshGeoImportVersions& OutGeoImportVersion, ESkeletalMeshSkinningImportVersions& OutSkinningImportVersion) const
{
	const FRawSkeletalMeshBulkData& RawSkeletalMeshBulkData = GetMeshEditorData().GetLODImportedData(LODIndex);
	OutGeoImportVersion = RawSkeletalMeshBulkData.GeoImportVersion;
	OutSkinningImportVersion = RawSkeletalMeshBulkData.SkinningImportVersion;
}

void USkeletalMesh::SetLODImportedDataVersions(const int32 LODIndex, const ESkeletalMeshGeoImportVersions& InGeoImportVersion, const ESkeletalMeshSkinningImportVersions& InSkinningImportVersion)
{
	FRawSkeletalMeshBulkData& RawSkeletalMeshBulkData = GetMeshEditorData().GetLODImportedData(LODIndex);
	RawSkeletalMeshBulkData.GeoImportVersion = InGeoImportVersion;
	RawSkeletalMeshBulkData.SkinningImportVersion = InSkinningImportVersion;
	//Update the cache
	check(GetImportedModel()->LODModels.IsValidIndex(LODIndex));
	GetImportedModel()->LODModels[LODIndex].RawSkeletalMeshBulkDataID = RawSkeletalMeshBulkData.GetIdString();
	GetImportedModel()->LODModels[LODIndex].bIsBuildDataAvailable = RawSkeletalMeshBulkData.IsBuildDataAvailable();
	GetImportedModel()->LODModels[LODIndex].bIsRawSkeletalMeshBulkDataEmpty = RawSkeletalMeshBulkData.IsEmpty();
}

void USkeletalMesh::CopyImportedData(int32 SrcLODIndex, USkeletalMesh* SrcSkeletalMesh, int32 DestLODIndex, USkeletalMesh* DestSkeletalMesh)
{
	check(DestSkeletalMesh->GetImportedModel()->LODModels.IsValidIndex(DestLODIndex));
	FRawSkeletalMeshBulkData& SrcRawMesh = SrcSkeletalMesh->GetMeshEditorData().GetLODImportedData(SrcLODIndex);
	FRawSkeletalMeshBulkData& DestRawMesh = DestSkeletalMesh->GetMeshEditorData().GetLODImportedData(DestLODIndex);
	FSkeletalMeshImportData SrcImportData;
	SrcRawMesh.LoadRawMesh(SrcImportData);
	DestRawMesh.SaveRawMesh(SrcImportData);
	DestRawMesh.GeoImportVersion = SrcRawMesh.GeoImportVersion;
	DestRawMesh.SkinningImportVersion = SrcRawMesh.SkinningImportVersion;
	
	FSkeletalMeshLODModel& DestLODModel = DestSkeletalMesh->GetImportedModel()->LODModels[DestLODIndex];
	DestLODModel.RawSkeletalMeshBulkDataID = DestRawMesh.GetIdString();
	DestLODModel.bIsBuildDataAvailable = DestRawMesh.IsBuildDataAvailable();
	DestLODModel.bIsRawSkeletalMeshBulkDataEmpty = DestRawMesh.IsEmpty();
}

void USkeletalMesh::ReserveLODImportData(int32 MaxLODIndex)
{
	//Getting the LODImportedData will allocate the data to default value.
	GetMeshEditorData().GetLODImportedData(MaxLODIndex);
}

void USkeletalMesh::ForceBulkDataResident(const int32 LODIndex)
{
	GetMeshEditorData().GetLODImportedData(LODIndex).GetBulkData().ForceBulkDataResident();
}

void USkeletalMesh::EmptyLODImportData(const int32 LODIndex)
{
	if(!GetImportedModel()->LODModels.IsValidIndex(LODIndex) || !GetMeshEditorData().IsLODImportDataValid(LODIndex))
	{
		return;
	}

	FRawSkeletalMeshBulkData& RawMesh = GetMeshEditorData().GetLODImportedData(LODIndex);
	FSkeletalMeshImportData EmptyData;
	RawMesh.SaveRawMesh(EmptyData);
	RawMesh.GeoImportVersion = ESkeletalMeshGeoImportVersions::Before_Versionning;
	RawMesh.SkinningImportVersion = ESkeletalMeshSkinningImportVersions::Before_Versionning;
	GetImportedModel()->LODModels[LODIndex].RawSkeletalMeshBulkDataID = RawMesh.GetIdString();
	GetImportedModel()->LODModels[LODIndex].bIsBuildDataAvailable = RawMesh.IsBuildDataAvailable();
	GetImportedModel()->LODModels[LODIndex].bIsRawSkeletalMeshBulkDataEmpty = RawMesh.IsEmpty();
}

void USkeletalMesh::EmptyAllImportData()
{
	const int32 LODNumber = GetLODNum();
	for(int32 LODIndex = 0; LODIndex < LODNumber; ++LODIndex)
	{
		EmptyLODImportData(LODIndex);
	}
}

void USkeletalMesh::CreateUserSectionsDataForLegacyAssets()
{
	//We want to avoid changing the ddc if we load an old asset.
	//This bool should be put to false at the end of the postload, if there is another posteditchange call after a new ddc will be created
	SetUseLegacyMeshDerivedDataKey(true);
	//Fill up the Section ChunkedParentSectionIndex and OriginalDataSectionIndex
	//We also want to create the UserSectionsData structure so the user can change the section data
	for (int32 LodIndex = 0; LodIndex < GetLODInfoArray().Num(); LodIndex++)
	{
		FSkeletalMeshLODModel& ThisLODModel = GetImportedModel()->LODModels[LodIndex];
		FSkeletalMeshLODInfo* ThisLODInfo = GetLODInfo(LodIndex);
		check(ThisLODInfo);

		//Reset the reduction setting to a non active state if the asset has active reduction but have no RawSkeletalMeshBulkData (we cannot reduce it)
		const bool bIsLODReductionActive = IsReductionActive(LodIndex);


		bool bMustUseReductionSourceData = bIsLODReductionActive
			&& ThisLODInfo->bHasBeenSimplified
			&& GetImportedModel()->OriginalReductionSourceMeshData_DEPRECATED.IsValidIndex(LodIndex)
			&& !(GetImportedModel()->OriginalReductionSourceMeshData_DEPRECATED[LodIndex]->IsEmpty());

		if (bIsLODReductionActive && !ThisLODInfo->bHasBeenSimplified && IsLODImportedDataEmpty(LodIndex))
		{
			if (LodIndex > ThisLODInfo->ReductionSettings.BaseLOD)
			{
				ThisLODInfo->bHasBeenSimplified = true;
			}
			else if (LodIndex == ThisLODInfo->ReductionSettings.BaseLOD)
			{
				if (ThisLODInfo->ReductionSettings.TerminationCriterion == SkeletalMeshTerminationCriterion::SMTC_AbsNumOfTriangles
					|| ThisLODInfo->ReductionSettings.TerminationCriterion == SkeletalMeshTerminationCriterion::SMTC_AbsNumOfVerts
					|| ThisLODInfo->ReductionSettings.TerminationCriterion == SkeletalMeshTerminationCriterion::SMTC_AbsTriangleOrVert)
				{
					//MaxNum.... cannot be inactive, switch to NumOfTriangle
					ThisLODInfo->ReductionSettings.TerminationCriterion = SMTC_NumOfTriangles;
				}

				//Now that we use triangle or vert num, set an inactive value
				if (ThisLODInfo->ReductionSettings.TerminationCriterion == SkeletalMeshTerminationCriterion::SMTC_NumOfTriangles
					|| ThisLODInfo->ReductionSettings.TerminationCriterion == SkeletalMeshTerminationCriterion::SMTC_TriangleOrVert)
				{
					ThisLODInfo->ReductionSettings.NumOfTrianglesPercentage = 1.0f;
				}
				if (ThisLODInfo->ReductionSettings.TerminationCriterion == SkeletalMeshTerminationCriterion::SMTC_NumOfVerts
					|| ThisLODInfo->ReductionSettings.TerminationCriterion == SkeletalMeshTerminationCriterion::SMTC_TriangleOrVert)
				{
					ThisLODInfo->ReductionSettings.NumOfVertPercentage = 1.0f;
				}
			}
			bMustUseReductionSourceData = false;
		}

		ThisLODModel.UpdateChunkedSectionInfo(GetName());

		if (bMustUseReductionSourceData)
		{
			//We must load the reduction source model, since reduction can remove section
			FSkeletalMeshLODModel ReductionSrcLODModel;
			TMap<FString, TArray<FMorphTargetDelta>> TmpMorphTargetData;
			GetImportedModel()->OriginalReductionSourceMeshData_DEPRECATED[LodIndex]->LoadReductionData(ReductionSrcLODModel, TmpMorphTargetData, this);
			
			//Fill the user data with the original value
			TMap<int32, FSkelMeshSourceSectionUserData> BackupUserSectionsData = ThisLODModel.UserSectionsData;
			ThisLODModel.UserSectionsData.Empty();

			ThisLODModel.UserSectionsData = ReductionSrcLODModel.UserSectionsData;

			//Now restore the reduce section user change and adjust the originalDataSectionIndex to point on the correct UserSectionData
			TBitArray<> SourceSectionMatched;
			SourceSectionMatched.Init(false, ReductionSrcLODModel.Sections.Num());
			for (int32 SectionIndex = 0; SectionIndex < ThisLODModel.Sections.Num(); ++SectionIndex)
			{
				FSkelMeshSection& Section = ThisLODModel.Sections[SectionIndex];
				FSkelMeshSourceSectionUserData& BackupUserData = FSkelMeshSourceSectionUserData::GetSourceSectionUserData(BackupUserSectionsData, Section);
				for (int32 SourceSectionIndex = 0; SourceSectionIndex < ReductionSrcLODModel.Sections.Num(); ++SourceSectionIndex)
				{
					if (SourceSectionMatched[SourceSectionIndex])
					{
						continue;
					}
					FSkelMeshSection& SourceSection = ReductionSrcLODModel.Sections[SourceSectionIndex];
					FSkelMeshSourceSectionUserData& UserData = FSkelMeshSourceSectionUserData::GetSourceSectionUserData(ThisLODModel.UserSectionsData, SourceSection);
					if (Section.MaterialIndex == SourceSection.MaterialIndex)
					{
						Section.OriginalDataSectionIndex = SourceSection.OriginalDataSectionIndex;
						UserData = BackupUserData;
						SourceSectionMatched[SourceSectionIndex] = true;
						break;
					}
				}
			}
			ThisLODModel.SyncronizeUserSectionsDataArray();
		}
	}
}

void USkeletalMesh::ValidateAllLodMaterialIndexes()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(USkeletalMesh::ValidateAllLodMaterialIndexes);

	for (int32 LodIndex = 0; LodIndex < GetLODNum(); LodIndex++)
	{
		FSkeletalMeshLODInfo* LODInfoPtr = GetLODInfo(LodIndex);
		if (!LODInfoPtr)
		{
			continue;
		}
		FSkeletalMeshLODModel& LODModel = GetImportedModel()->LODModels[LodIndex];
		const int32 SectionNum = LODModel.Sections.Num();
		const TArray<FSkeletalMaterial>& ListOfMaterials = GetMaterials();
		//See if more then one section use the same UserSectionData
		for (int32 SectionIndex = 0; SectionIndex < SectionNum; ++SectionIndex)
		{
			int32 LodMaterialMapOverride = INDEX_NONE;
			FSkelMeshSection& Section = LODModel.Sections[SectionIndex];
			//Validate and fix the LODMaterialMap override
			if (LODInfoPtr->LODMaterialMap.IsValidIndex(SectionIndex) && LODInfoPtr->LODMaterialMap[SectionIndex] != INDEX_NONE)
			{
				LodMaterialMapOverride = LODInfoPtr->LODMaterialMap[SectionIndex];
				if (!ListOfMaterials.IsValidIndex(LodMaterialMapOverride))
				{
					UE_ASSET_LOG(LogSkeletalMesh
						, Display
						, this
						, TEXT("Fix LOD %d Section %d LODMaterialMap override material index from %d to INDEX_NONE. The value is not pointing on a valid Material slot index.")
						, LodIndex
						, SectionIndex
						, LodMaterialMapOverride);
					LODInfoPtr->LODMaterialMap[SectionIndex] = INDEX_NONE;
				}
			}
			//Validate and fix the section material index
			{
				if (!ListOfMaterials.IsValidIndex(Section.MaterialIndex))
				{
					if (LodMaterialMapOverride != INDEX_NONE)
					{
						UE_ASSET_LOG(LogSkeletalMesh
							, Display
							, this
							, TEXT("Fix LOD %d Section %d Material index from %d to %d. The fallback value is from the LODMaterialMap Override. The value is not pointing on a valid Material slot index.")
							, LodIndex
							, SectionIndex
							, Section.MaterialIndex
							, LodMaterialMapOverride);
						Section.MaterialIndex = LodMaterialMapOverride;
					}
					else
					{
						//Fall back on the original section index
						if (ListOfMaterials.IsValidIndex(Section.OriginalDataSectionIndex))
						{
							UE_ASSET_LOG(LogSkeletalMesh
								, Display
								, this
								, TEXT("Fix LOD %d Section %d Material index from %d to %d. The fallback value is from the OriginalDataSectionIndex. The value is not pointing on a valid Material slot index.")
								, LodIndex
								, SectionIndex
								, Section.MaterialIndex
								, Section.OriginalDataSectionIndex);
							Section.MaterialIndex = Section.OriginalDataSectionIndex;
						}
						else
						{
							UE_ASSET_LOG(LogSkeletalMesh
								, Display
								, this
								, TEXT("Fix LOD %d Section %d Material index from %d to 0. The fallback value is 0. The value is not pointing on a valid Material slot index.")
								, LodIndex
								, SectionIndex
								, Section.MaterialIndex);
							//Fallback on material index 0
							Section.MaterialIndex = 0;
						}
					}
				}
			}
		}
	}
}

void USkeletalMesh::PostLoadValidateUserSectionData()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(USkeletalMesh::PostLoadValidateUserSectionData);

	for (int32 LodIndex = 0; LodIndex < GetLODNum(); LodIndex++)
	{
		const FSkeletalMeshLODInfo* LODInfoPtr = GetLODInfo(LodIndex);
		if (!LODInfoPtr || !LODInfoPtr->bHasBeenSimplified)
		{
			//We validate only generated LOD from a base LOD
			continue;
		}

		const int32 ReductionBaseLOD = LODInfoPtr->ReductionSettings.BaseLOD;
		if (!GetImportedModel()->LODModels.IsValidIndex(ReductionBaseLOD))
		{
			//The base LOD should always be valid for generated LOD
			UE_ASSET_LOG(LogSkeletalMesh, Display, this, TEXT("This asset generated lod %d, is base on an invalid LOD index %d."), LodIndex, ReductionBaseLOD);
			continue;
		}
		FSkeletalMeshLODModel& BaseReductionLODModel = GetImportedModel()->LODModels[ReductionBaseLOD];
		FSkeletalMeshLODModel& ThisLODModel = GetImportedModel()->LODModels[LodIndex];
		const int32 SectionNum = ThisLODModel.Sections.Num();
		const int32 UserSectionsDataNum = ThisLODModel.UserSectionsData.Num();
		const int32 BaseUserSectionsDataNum = BaseReductionLODModel.UserSectionsData.Num();
		//We must make sure the result is similar to what the reduction will give. So we will not have more user section data then the number we have for the base LOD.
		//Because reduction reset the UserSectionData to the number of parent section after the reduction.
		const bool bIsInlineReduction = LodIndex == ReductionBaseLOD;
		bool bLODHaveSectionIssue = !bIsInlineReduction && ( (UserSectionsDataNum > SectionNum) || (UserSectionsDataNum > BaseUserSectionsDataNum) );
		if (!bLODHaveSectionIssue)
		{
			//See if more then one section use the same UserSectionData
			TBitArray<> AvailableUserSectionData;
			AvailableUserSectionData.Init(true, ThisLODModel.UserSectionsData.Num());
			for (int32 SectionIndex = 0; SectionIndex < SectionNum; ++SectionIndex)
			{
				FSkelMeshSection& Section = ThisLODModel.Sections[SectionIndex];
				if (Section.ChunkedParentSectionIndex != INDEX_NONE)
				{
					continue;
				}
				if (!AvailableUserSectionData.IsValidIndex(Section.OriginalDataSectionIndex) || !AvailableUserSectionData[Section.OriginalDataSectionIndex])
				{
					bLODHaveSectionIssue = true;
					break;
				}
				AvailableUserSectionData[Section.OriginalDataSectionIndex] = false;
			}
			if (!bLODHaveSectionIssue)
			{
				//Everything is good nothing to fix
				continue;
			}
		}

		//Force the source UserSectionData, then restore the UserSectionData value each section was using
		//We use the source section user data entry in case we do not have any override
		TMap<int32, FSkelMeshSourceSectionUserData> NewUserSectionsData;

		int32 CurrentOriginalSectionIndex = -1;
		for (int32 SectionIndex = 0; SectionIndex < SectionNum; ++SectionIndex)
		{
			FSkelMeshSection& Section = ThisLODModel.Sections[SectionIndex];
			if (Section.ChunkedParentSectionIndex != INDEX_NONE)
			{
				//The section zero must never be a chunked children
				if (!ensure(CurrentOriginalSectionIndex >= 0))
				{
					CurrentOriginalSectionIndex = 0;
				}
				//We do not restore user section data for chunked section, the parent has already fix it
				Section.OriginalDataSectionIndex = CurrentOriginalSectionIndex;
				continue;
			}

			//Parent (non chunked) section must increment the index
			CurrentOriginalSectionIndex++;

			FSkelMeshSourceSectionUserData& SectionUserData = NewUserSectionsData.FindOrAdd(CurrentOriginalSectionIndex);
			if(const FSkelMeshSourceSectionUserData* BackupSectionUserData = ThisLODModel.UserSectionsData.Find(Section.OriginalDataSectionIndex))
			{
				SectionUserData = *BackupSectionUserData;
			}
			else if(const FSkelMeshSourceSectionUserData* BaseSectionUserData = BaseReductionLODModel.UserSectionsData.Find(CurrentOriginalSectionIndex))
			{
				SectionUserData = *BaseSectionUserData;
			}

			Section.OriginalDataSectionIndex = CurrentOriginalSectionIndex;
		}
		ThisLODModel.UserSectionsData = NewUserSectionsData;

		UE_ASSET_LOG(LogSkeletalMesh, Display, this, TEXT("Fix some section data of this asset for lod %d. Verify all sections of this mesh are ok and save the asset to fix this issue."), LodIndex);
	}
}

bool USkeletalMesh::IsAsyncTaskComplete() const
{
	return AsyncTask == nullptr || AsyncTask->IsWorkDone();
}

bool USkeletalMesh::TryCancelAsyncTasks()
{
	if (AsyncTask)
	{
		if (AsyncTask->IsDone() || AsyncTask->Cancel())
		{
			AsyncTask.Reset();
		}
	}

	return AsyncTask == nullptr;
}

void USkeletalMesh::PostLoadEnsureImportDataExist()
{
	//If we have a LODModel with no import data and the LOD model have at least one section using more bone then any platform max GPU bone count. We will recreate the import data to allow the asset to be build and chunk properly.
	const int32 MinimumPerPlatformMaxGPUSkinBones = FGPUBaseSkinVertexFactory::GetMinimumPerPlatformMaxGPUSkinBonesValue();
	//This flag is set from the max gpu skin bones, if true ImportLODData will be created
	bool bNeedToCreateImportData = false;

	//The following two flags are acting together when bNeedToCreateImportData == false.
	//When we found a LOD that is reduce inline, we want to convert the deprecated reduction source data
	//and create the import data from it.

	//If we found a LOD doing inline reduction this flag will be set to trtue
	bool bContainInlineReduction = false;
	//If we found a LOD doing inline reduction but with baked skinweight data this flag will be true.
	//If this flag is true we will not create the imported data, except if bNeedToCreateData is true
	//In case bNeedToCreateData is true, user will have to re-import the skinning data to comply with
	//the per platform max gpu bone.
	bool bCannotConvertSkinWeightProfile = false;

	for (int32 LodIndex = 0; LodIndex < GetLODNum(); LodIndex++)
	{
		const FSkeletalMeshLODModel* LODModel = &(GetImportedModel()->LODModels[LodIndex]);
		const FSkeletalMeshLODInfo* ThisLODInfo = GetLODInfo(LodIndex);
		check(ThisLODInfo);
		const bool bRawDataEmpty = IsLODImportedDataEmpty(LodIndex);
		const bool bRawBuildDataAvailable = IsLODImportedDataBuildAvailable(LodIndex);
		if (!bRawDataEmpty && bRawBuildDataAvailable)
		{
			continue;
		}
		
		//We create the import data only if the mesh was simplified and its doing inline reduction
		//Non simplified mesh without import data should not be converted because the simplification will kick in and the asset will be simplified (asset vertex and face count will change)
		const bool bReductionActive = IsReductionActive(LodIndex);
		const bool bInlineReduction = (ThisLODInfo->ReductionSettings.BaseLOD == LodIndex);
		if (bReductionActive && !bInlineReduction)
		{
			//Generated LOD (not inline) do not need imported data
			continue;
		}
		else if (bInlineReduction && (bReductionActive || ThisLODInfo->bHasBeenSimplified))
		{
			//If we cannot convert alternate skin weight data, we do not want to allow 
			if (GetImportedModel()->OriginalReductionSourceMeshData_DEPRECATED.IsValidIndex(LodIndex))
			{
				//Old inline reduced assets do not have the original imported skin weight data, it must be re-import if we want to convert it to import data
				bool bUseSkinWeightProfile = LODModel->SkinWeightProfiles.Num() > 0;
				FSkeletalMeshLODModel ReductionLODModel;
				TMap<FString, TArray<FMorphTargetDelta>> ReductionLODMorphTargetData;
				//Swap the LODModel pointer to the one we store when reducing, so we add the true import data
				GetImportedModel()->OriginalReductionSourceMeshData_DEPRECATED[LodIndex]->LoadReductionData(ReductionLODModel, ReductionLODMorphTargetData, this);

				if (bUseSkinWeightProfile && ReductionLODModel.SkinWeightProfiles.IsEmpty())
				{
					//If we cannot convert a particular LOD, prevent Creation of import data and exit the loop
					bCannotConvertSkinWeightProfile = true;
				}
			}
			bContainInlineReduction = true;
		}
		//See if the LODModel data use more bones then the chunking allow
		int32 MaxBoneperSection = 0;
		for(const FSkelMeshSection& Section : LODModel->Sections)
		{
			MaxBoneperSection = FMath::Max(MaxBoneperSection, Section.BoneMap.Num());
		}
		//If we use more bone then the minimum maxGPUSkinbone, we need to re-create de import data to be able to build the asset
		if (MaxBoneperSection > MinimumPerPlatformMaxGPUSkinBones)
		{
			bNeedToCreateImportData = true;
			break;
		}
	}

	if (bNeedToCreateImportData || (bContainInlineReduction && !bCannotConvertSkinWeightProfile))
	{
		if (!ensure(IsInGameThread()))
		{
			UE_ASSET_LOG(LogSkeletalMesh, Error, this, TEXT("USkeletalMesh::PostLoadEnsureImportDataExist() must be call on the game thread."));
			return;
		}
		if (bCannotConvertSkinWeightProfile)
		{
			UE_ASSET_LOG(LogSkeletalMesh, Warning, this, TEXT("USkeletalMesh::PostLoadEnsureImportDataExist() Cannot convert old alternate skin weight profiles data. Re-import all alternate profiles for this skeletal mesh."));
		}
		IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");
		//We create the import data for all LOD that do not have import data except for the generated LODs.
		{
			FScopedSkeletalMeshPostEditChange ScopedPostEditChange(this, false, false);
			MeshUtilities.CreateImportDataFromLODModel(this);
		}
#if WITH_EDITORONLY_DATA
		//If the import data is existing we want to turn use legacy derive data key to false
		SetUseLegacyMeshDerivedDataKey(false);
#endif
	}
}

void USkeletalMesh::PostLoadVerifyAndFixBadTangent()
{
	IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");
	bool bFoundBadTangents = false;
	for (int32 LodIndex = 0; LodIndex < GetLODNum(); LodIndex++)
	{
		if (!IsLODImportedDataEmpty(LodIndex))
		{
			//No need to verify skeletalmesh that have valid imported data, the tangents will always exist in this case
			continue;
		}
		const FSkeletalMeshLODInfo* LODInfoPtr = GetLODInfo(LodIndex);
		if (!LODInfoPtr || LODInfoPtr->bHasBeenSimplified)
		{
			//No need to validate simplified LOD
			continue;
		}

		auto ComputeTriangleTangent = [&MeshUtilities](const FSoftSkinVertex& VertexA, const FSoftSkinVertex& VertexB, const FSoftSkinVertex& VertexC, TArray<FVector3f>& OutTangents)
		{
			MeshUtilities.CalculateTriangleTangent(VertexA, VertexB, VertexC, OutTangents, FLT_MIN);
		};

		FSkeletalMeshLODModel& ThisLODModel = GetImportedModel()->LODModels[LodIndex];
		const int32 SectionNum = ThisLODModel.Sections.Num();
		TArray<FSoftSkinVertex> Vertices;
		TMap<int32, TArray<FVector3f>> TriangleTangents;

		for (int32 SectionIndex = 0; SectionIndex < SectionNum; ++SectionIndex)
		{
			FSkelMeshSection& Section = ThisLODModel.Sections[SectionIndex];
			const int32 NumVertices = Section.GetNumVertices();
			const int32 SectionBaseIndex = Section.BaseIndex;
			const int32 SectionNumTriangles = Section.NumTriangles;
			TArray<uint32>& IndexBuffer = ThisLODModel.IndexBuffer;
			//We inspect triangle per section so we need to reset the array when we start a new section.
			TriangleTangents.Empty(SectionNumTriangles);
			for (int32 FaceIndex = 0; FaceIndex < SectionNumTriangles; ++FaceIndex)
			{
				int32 BaseFaceIndexBufferIndex = SectionBaseIndex + (FaceIndex * 3);
				if (!ensure(IndexBuffer.IsValidIndex(BaseFaceIndexBufferIndex)) || !ensure(IndexBuffer.IsValidIndex(BaseFaceIndexBufferIndex + 2)))
				{
					break;
				}
				for (int32 Corner = 0; Corner < 3; ++Corner)
				{
					const int32 CornerIndexBufferIndex = BaseFaceIndexBufferIndex + Corner;
					ensure(IndexBuffer.IsValidIndex(CornerIndexBufferIndex));
					int32 VertexIndex = IndexBuffer[CornerIndexBufferIndex] - Section.BaseVertexIndex;
					ensure(Section.SoftVertices.IsValidIndex(VertexIndex));
					FSoftSkinVertex& SoftSkinVertex = Section.SoftVertices[VertexIndex];

					bool bNeedToOrthonormalize = false;

					//Make sure we have normalized tangents
					auto NormalizedTangent = [&bNeedToOrthonormalize, &bFoundBadTangents](FVector3f& Tangent)
					{
						if (Tangent.ContainsNaN() || Tangent.SizeSquared() < UE_THRESH_VECTOR_NORMALIZED)
						{
							//This is a degenerated tangent, we will set it to zero. It will be fix by the
							//FixTangent lambda function.
							Tangent = FVector3f::ZeroVector;
							//If we can fix this tangents, we have to orthonormalize the result
							bNeedToOrthonormalize = true;
							bFoundBadTangents = true;
							return false;
						}
						else if (!Tangent.IsNormalized())
						{
							//This is not consider has a bad normal since the tangent vector is not near zero.
							//We are just making sure the tangent is normalize.
							Tangent.Normalize();
						}
						return true;
					};

					/** Call this lambda only if you need to fix the tangent */
					auto FixTangent = [&IndexBuffer, &Section, &TriangleTangents, &ComputeTriangleTangent, &BaseFaceIndexBufferIndex](FVector3f& TangentA, const FVector3f& TangentB, const FVector3f& TangentC, const int32 Offset)
					{
						//If the two other axis are valid, fix the tangent with a cross product and normalize the answer.
						if (TangentB.IsNormalized() && TangentC.IsNormalized())
						{
							TangentA = FVector3f::CrossProduct(TangentB, TangentC);
							TangentA.Normalize();
							return true;
						}

						//We do not have any valid data to help us for fixing this normal so apply the triangle normals, this will create a faceted mesh but this is better then a black not shade mesh.
						TArray<FVector3f>& Tangents = TriangleTangents.FindOrAdd(BaseFaceIndexBufferIndex);
						if (Tangents.Num() == 0)
						{
							const int32 VertexIndex0 = IndexBuffer[BaseFaceIndexBufferIndex] - Section.BaseVertexIndex;
							const int32 VertexIndex1 = IndexBuffer[BaseFaceIndexBufferIndex + 1] - Section.BaseVertexIndex;
							const int32 VertexIndex2 = IndexBuffer[BaseFaceIndexBufferIndex + 2] - Section.BaseVertexIndex;
							if (!ensure(
								Section.SoftVertices.IsValidIndex(VertexIndex0) &&
								Section.SoftVertices.IsValidIndex(VertexIndex1) &&
								Section.SoftVertices.IsValidIndex(VertexIndex2) ) )
							{
								//We found bad vertex indices, we cannot compute this face tangents.
								return false;
							}
							ComputeTriangleTangent(Section.SoftVertices[VertexIndex0], Section.SoftVertices[VertexIndex1], Section.SoftVertices[VertexIndex2], Tangents);
							const FVector3f Axis[3] = { {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f} };
							if (!ensure(Tangents.Num() == 3))
							{
								Tangents.Empty(3);
								Tangents.AddZeroed(3);
							}
							for (int32 TangentIndex = 0; TangentIndex < Tangents.Num(); ++TangentIndex)
							{
								if (Tangents[TangentIndex].IsNearlyZero())
								{
									Tangents[TangentIndex] = Axis[TangentIndex];
								}
							}
							if (!ensure(Tangents.Num() == 3))
							{
								//We are not able to compute the triangle tangent, this is probably a degenerated triangle
								Tangents.Empty(3);

								Tangents.Add(Axis[0]);
								Tangents.Add(Axis[1]);
								Tangents.Add(Axis[2]);
							}
						}
						//Use the offset to know which tangent type we are setting (0: Tangent X, 1: bi-normal Y, 2: Normal Z)
						TangentA = Tangents[(Offset) % 3];
						return TangentA.IsNormalized();
					};

					//The SoftSkinVertex TangentZ is a FVector4 so we must use a temporary FVector to be able to pass reference
					FVector3f TangentZ = SoftSkinVertex.TangentZ;
					//Make sure the tangent space is normalize before fixing bad tangent, because we want to do a cross product
					//of 2 valid axis if possible. If not possible we will use the triangle normal which give a faceted triangle.
					bool ValidTangentX = NormalizedTangent(SoftSkinVertex.TangentX);
					bool ValidTangentY = NormalizedTangent(SoftSkinVertex.TangentY);
					bool ValidTangentZ = NormalizedTangent(TangentZ);

					if (!ValidTangentX)
					{
						ValidTangentX = FixTangent(SoftSkinVertex.TangentX, SoftSkinVertex.TangentY, TangentZ, 0);
					}
					if (!ValidTangentY)
					{
						ValidTangentY = FixTangent(SoftSkinVertex.TangentY, TangentZ, SoftSkinVertex.TangentX, 1);
					}
					if (!ValidTangentZ)
					{
						ValidTangentZ = FixTangent(TangentZ, SoftSkinVertex.TangentX, SoftSkinVertex.TangentY, 2);
					}

					//Make sure the result tangent space is orthonormal, only if we succeed to fix all tangents
					if (bNeedToOrthonormalize && ValidTangentX && ValidTangentY && ValidTangentZ)
					{
						FVector3f::CreateOrthonormalBasis(
							SoftSkinVertex.TangentX,
							SoftSkinVertex.TangentY,
							TangentZ
						);
					}
					SoftSkinVertex.TangentZ = TangentZ;
				}
			}
		}
	}
	if (bFoundBadTangents)
	{
		//Notify the user that we have to fix the normals on this model.
		UE_ASSET_LOG(LogSkeletalMesh, Display, this, TEXT("Find and fix some bad tangent! please re-import this skeletal mesh asset to fix the issue. The shading of the skeletal mesh will be bad and faceted."));
	}
}

#endif // WITH_EDITOR

bool USkeletalMesh::IsPostLoadThreadSafe() const
{
	return false;	// PostLoad is not thread safe because of the call to InitMorphTargets, which can call VerifySmartName() that can mutate a shared map in the skeleton.
}

void USkeletalMesh::BeginPostLoadInternal(FSkinnedAssetPostLoadContext& Context)
{
#if WITH_EDITOR
	TRACE_CPUPROFILER_EVENT_SCOPE(UStaticMesh::BeginPostLoadInternal);

	SetInternalFlags(EInternalObjectFlags::Async);

	// Lock all properties that should not be modified/accessed during async post-load
	AcquireAsyncProperty();

	// This scope allows us to use any locked properties without causing stalls
	FSkinnedAssetAsyncBuildScope AsyncBuildScope(this);

	// Make sure the cloth assets have finished loading
	// TODO: Remove all UObject PostLoad dependencies.
	//       Even with these ConditionalPostLoad calls, the UObject PostLoads' order of execution cannot be guaranted.
	//       E.g. in some instance it has been found that the SkeletalMesh EndLoad can trigger a ConditionalPostLoad
	//       on the cloth assets even before reaching this point.
	//       In these occurences, the cloth asset's RF_NeedsPostLoad flag is already cleared despite its PostLoad still
	//       being un-executed, making the following block code ineffective.
	for (UClothingAssetBase* MeshClothingAsset : GetMeshClothingAssets())
	{
		MeshClothingAsset->ConditionalPostLoad();
	}

	// Make sure the mesh editor data object is a sub object of the skeletalmesh, rename it to change the owner to be the skeletalmesh.
	if (IsMeshEditorDataValid() && GetMeshEditorData().GetOuter() != this)
	{
		//Post load call so no need to: dirty, redirect, transact or reset the loader.
		const TCHAR* NewName = nullptr;
		GetMeshEditorData().Rename(NewName, this, REN_ForceNoResetLoaders | REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
		GetMeshEditorData().SetFlags(RF_Transactional);
	}

	if (!GetOutermost()->bIsCookedForEditor)
	{
		TArray<FSkeletalMeshLODInfo>& LODInfoArray = GetLODInfoArray();

		// If LODInfo is missing - create array of correct size.
		if (LODInfoArray.Num() != GetImportedModel()->LODModels.Num())
		{
			LODInfoArray.Empty(GetImportedModel()->LODModels.Num());
			LODInfoArray.AddZeroed(GetImportedModel()->LODModels.Num());

			for (int32 i = 0; i < LODInfoArray.Num(); i++)
			{
				LODInfoArray[i].LODHysteresis = 0.02f;
			}
		}

		int32 TotalLODNum = LODInfoArray.Num();
		for (int32 LodIndex = 0; LodIndex < TotalLODNum; LodIndex++)
		{
			FSkeletalMeshLODInfo& ThisLODInfo = LODInfoArray[LodIndex];
			FSkeletalMeshLODModel& ThisLODModel = GetImportedModel()->LODModels[LodIndex];

			if (ThisLODInfo.ReductionSettings.BonesToRemove_DEPRECATED.Num() > 0)
			{
				for (auto& BoneToRemove : ThisLODInfo.ReductionSettings.BonesToRemove_DEPRECATED)
				{
					AddBoneToReductionSetting(LodIndex, BoneToRemove.BoneName);
				}

				// since in previous system, we always removed from previous LOD, I'm adding this 
				// here for previous LODs
				for (int32 CurLodIndx = LodIndex + 1; CurLodIndx < TotalLODNum; ++CurLodIndx)
				{
					AddBoneToReductionSetting(CurLodIndx, ThisLODInfo.RemovedBones_DEPRECATED);
				}

				// we don't apply this change here, but this will be applied when you re-gen simplygon
				ThisLODInfo.ReductionSettings.BonesToRemove_DEPRECATED.Empty();
			}

			if (ThisLODInfo.ReductionSettings.BakePose_DEPRECATED != nullptr)
			{
				ThisLODInfo.BakePose = ThisLODInfo.ReductionSettings.BakePose_DEPRECATED;
				ThisLODInfo.ReductionSettings.BakePose_DEPRECATED = nullptr;
			}
		}

		// load LODinfo if using shared asset, it can override existing bone remove settings
		if (GetLODSettings() != nullptr)
		{
			//before we copy
			if (GetLinkerCustomVersion(FFortniteMainBranchObjectVersion::GUID) < FFortniteMainBranchObjectVersion::AddBakePoseOverrideForSkeletalMeshReductionSetting)
			{
				// if LODsetting doesn't have BakePose, but this does, we'll have to copy that to BakePoseOverride
				const int32 NumSettings = FMath::Min(GetLODSettings()->GetNumberOfSettings(), GetLODNum());
				for (int32 Index = 0; Index < NumSettings; ++Index)
				{
					const FSkeletalMeshLODGroupSettings& GroupSetting = GetLODSettings()->GetSettingsForLODLevel(Index);
					// if lod setting doesn't have bake pose, but this lod does, that means this bakepose has to move to BakePoseOverride
					// since we want to match what GroupSetting has
					if (GroupSetting.BakePose == nullptr && LODInfoArray[Index].BakePose)
					{
						// in this case,
						LODInfoArray[Index].BakePoseOverride = LODInfoArray[Index].BakePose;
						LODInfoArray[Index].BakePose = nullptr;
					}
				}
			}
			GetLODSettings()->SetLODSettingsToMesh(this);
		}

		if (GetLinkerUEVersion() < VER_UE4_SORT_ACTIVE_BONE_INDICES)
		{
			for (int32 LodIndex = 0; LodIndex < LODInfoArray.Num(); LodIndex++)
			{
				FSkeletalMeshLODModel& ThisLODModel = GetImportedModel()->LODModels[LodIndex];
				ThisLODModel.ActiveBoneIndices.Sort();
			}
		}

		// make sure older versions contain active bone indices with parents present
		// even if they're not skinned, missing matrix calculation will mess up skinned children
		if (GetLinkerCustomVersion(FFortniteMainBranchObjectVersion::GUID) < FFortniteMainBranchObjectVersion::EnsureActiveBoneIndicesToContainParents)
		{
			for (int32 LodIndex = 0; LodIndex < LODInfoArray.Num(); LodIndex++)
			{
				FSkeletalMeshLODModel& ThisLODModel = GetImportedModel()->LODModels[LodIndex];
				GetRefSkeleton().EnsureParentsExistAndSort(ThisLODModel.ActiveBoneIndices);
			}
		}

		if (GetLinkerCustomVersion(FEditorObjectVersion::GUID) < FEditorObjectVersion::SkeletalMeshMoveEditorSourceDataToPrivateAsset)
		{
			ReserveLODImportData(GetImportedModel()->LODModels.Num() - 1);
			for (int32 LODIndex = 0; LODIndex < GetImportedModel()->LODModels.Num(); ++LODIndex)
			{
				FSkeletalMeshLODModel& ThisLODModel = GetImportedModel()->LODModels[LODIndex];
				//We can have partial data if the asset was save after the split workflow implementation
				//Use the deprecated member to retrieve this data
				if (GetLinkerCustomVersion(FFortniteMainBranchObjectVersion::GUID) >= FFortniteMainBranchObjectVersion::NewSkeletalMeshImporterWorkflow)
				{
					//Get the deprecated data 
					FRawSkeletalMeshBulkData* RawSkeletalMeshBulkData_DEPRECATED = nullptr;
					{
						PRAGMA_DISABLE_DEPRECATION_WARNINGS
						RawSkeletalMeshBulkData_DEPRECATED = &ThisLODModel.GetRawSkeletalMeshBulkData_DEPRECATED();
						PRAGMA_ENABLE_DEPRECATION_WARNINGS
					}
					check(RawSkeletalMeshBulkData_DEPRECATED);
					if (!RawSkeletalMeshBulkData_DEPRECATED->IsEmpty())
					{
						FSkeletalMeshImportData SerializeMeshData;
						RawSkeletalMeshBulkData_DEPRECATED->LoadRawMesh(SerializeMeshData);
						SaveLODImportedData(LODIndex, SerializeMeshData);
					}
					//Get the FRawSkeletalMeshBulkData to set the geo and skinning version
					FRawSkeletalMeshBulkData& RawSkeletalMeshBulkData = GetMeshEditorData().GetLODImportedData(LODIndex);
					RawSkeletalMeshBulkData.GeoImportVersion = RawSkeletalMeshBulkData_DEPRECATED->GeoImportVersion;
					RawSkeletalMeshBulkData.SkinningImportVersion = RawSkeletalMeshBulkData_DEPRECATED->SkinningImportVersion;
					//Empty the DEPRECATED member
					FSkeletalMeshImportData EmptyMeshData;
					RawSkeletalMeshBulkData_DEPRECATED->SaveRawMesh(EmptyMeshData);
					RawSkeletalMeshBulkData_DEPRECATED->EmptyBulkData();
				}
				//Set the cache data into the LODModel
				FRawSkeletalMeshBulkData& RawSkeletalMeshBulkData = GetMeshEditorData().GetLODImportedData(LODIndex);
				ThisLODModel.bIsRawSkeletalMeshBulkDataEmpty = RawSkeletalMeshBulkData.IsEmpty();
				ThisLODModel.bIsBuildDataAvailable = RawSkeletalMeshBulkData.IsBuildDataAvailable();
				ThisLODModel.RawSkeletalMeshBulkDataID = RawSkeletalMeshBulkData.GetIdString();
			}
		}

		if (GetLinkerCustomVersion(FEditorObjectVersion::GUID) < FEditorObjectVersion::SkeletalMeshBuildRefactor)
		{
			CreateUserSectionsDataForLegacyAssets();
		}

		ValidateAllLodMaterialIndexes();

		PostLoadEnsureImportDataExist();
	}

#endif // #if WITH_EDITOR
}

#if WITH_EDITORONLY_DATA
void USkeletalMesh::DeclareConstructClasses(TArray<FTopLevelAssetPath>& OutConstructClasses, const UClass* SpecificSubclass)
{
	Super::DeclareConstructClasses(OutConstructClasses, SpecificSubclass);

	OutConstructClasses.Add(FTopLevelAssetPath(USkeletalMeshEditorData::StaticClass()));
}
#endif

void USkeletalMesh::PostLoad()
{
	LLM_SCOPE(ELLMTag::SkeletalMesh);
	Super::PostLoad();
}

void USkeletalMesh::ExecutePostLoadInternal(FSkinnedAssetPostLoadContext& Context)
{
#if WITH_EDITOR
	TRACE_CPUPROFILER_EVENT_SCOPE(USkeletalMesh::ExecutePostLoadInternal);

	// This scope allows us to use any locked properties without causing stalls
	FSkinnedAssetAsyncBuildScope AsyncBuildScope(this);

	if (!GetOutermost()->bIsCookedForEditor)
	{
		RemoveLegacyClothingSections();

		UpdateGenerateUpToData();

		PostLoadValidateUserSectionData();

		PostLoadVerifyAndFixBadTangent();

		// Fixup missing material slot names and import slot names, so that mesh editing 
		// preserves material assignments.
		IMeshUtilities* MeshUtilities = FModuleManager::Get().LoadModulePtr<IMeshUtilities>("MeshUtilities");
		if (MeshUtilities)
		{
			MeshUtilities->FixupMaterialSlotNames(this);
		}

		if (GetResourceForRendering() == nullptr)
		{
			CacheDerivedData(&Context);
			Context.bHasCachedDerivedData = true;
		}
	}
#endif // WITH_EDITOR
}

void USkeletalMesh::FinishPostLoadInternal(FSkinnedAssetPostLoadContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(USkeletalMesh::FinishPostLoadInternal);
	
#if WITH_EDITOR

	ClearInternalFlags(EInternalObjectFlags::Async);

	// This scope allows us to use any locked properties without causing stalls
	FSkinnedAssetAsyncBuildScope AsyncBuildScope(this);

	if (Context.bHasCachedDerivedData)
	{
		PostMeshCached.Broadcast(this);
	}

	//Make sure unused cloth are unbind
	if (GetMeshClothingAssets().Num() > 0)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UnbindUnusedCloths);

		TArray<UClothingAssetBase*> InUsedClothingAssets;
		GetClothingAssetsInUse(InUsedClothingAssets);
		//Look if we have some cloth binding to unbind
		for (UClothingAssetBase* MeshClothingAsset : GetMeshClothingAssets())
		{
			if (MeshClothingAsset == nullptr)
			{
				continue;
			}
			bool bFound = false;
			for (UClothingAssetBase* UsedMeshClothingAsset : InUsedClothingAssets)
			{
				if (UsedMeshClothingAsset->GetAssetGuid() == MeshClothingAsset->GetAssetGuid())
				{
					bFound = true;
					break;
				}
			}
			if (!bFound)
			{
				//No post edit change and no reregister, we just prevent the inner scope to call postedit change and reregister
				FScopedSkeletalMeshPostEditChange ScopedPostEditChange(this, false, false);
				//Make sure the asset is unbind, some old code path was allowing to have bind cloth asset not present in the imported model.
				//The old inline reduction code was not rebinding the cloth asset nor unbind it.
				MeshClothingAsset->UnbindFromSkeletalMesh(this);
			}
		}
	}
#endif // WITH_EDITOR

#if WITH_EDITORONLY_DATA
	if (GetLinkerCustomVersion(FRenderingObjectVersion::GUID) < FRenderingObjectVersion::FixedMeshUVDensity)
	{
		UpdateUVChannelData(true);
	}
#endif // WITH_EDITOR

#if WITH_EDITOR
	ApplyFinishBuildInternalData(&Context);
#endif
	// should do this before InitResources.
	InitMorphTargets();

	// initialize rendering resources
	if (FApp::CanEverRender())
	{
		InitResources();
	}
	else
	{
		// Update any missing data when cooking.
		UpdateUVChannelData(false);
	}

	CalculateInvRefMatrices();

#if WITH_EDITORONLY_DATA
	if (GetRetargetBasePose().Num() == 0 && !GetOutermost()->bIsCookedForEditor)
	{
		GetRetargetBasePose() = GetRefSkeleton().GetRefBonePose();
	}

	if (GetLinkerCustomVersion(FFortniteMainBranchObjectVersion::GUID) < FFortniteMainBranchObjectVersion::SupportVirtualBoneInRetargeting)
	{
		ReallocateRetargetBasePose();
	}
#endif

	// Bounds have been loaded - apply extensions.
	CalculateExtendedBounds();

#if WITH_EDITORONLY_DATA
	if (GetRequiresLODScreenSizeConversion() || GetRequiresLODHysteresisConversion())
	{
		// Convert screen area to screen size
		ConvertLegacyLODScreenSize();
	}

	// If inverse masses have never been cached, invalidate data so it will be recalculated
	if (GetLinkerCustomVersion(FSkeletalMeshCustomVersion::GUID) < FSkeletalMeshCustomVersion::CachedClothInverseMasses)
	{
		for (UClothingAssetBase* ClothingAsset : GetMeshClothingAssets())
		{
			if (ClothingAsset) 
			{
				ClothingAsset->InvalidateAllCachedData();
			}
		}
	}
#endif

	SetHasActiveClothingAssets(ComputeActiveClothingAssets());

#if WITH_EDITOR
	if (GetLinkerCustomVersion(FNiagaraObjectVersion::GUID) < FNiagaraObjectVersion::SkeletalMeshVertexSampling)
	{
		GetSamplingInfoInternal().BuildRegions(this);
		GetSamplingInfoInternal().BuildWholeMesh(this);
	}
#endif

#if !WITH_EDITOR
	RebuildSocketMap();
#endif // !WITH_EDITOR
#if WITH_EDITORONLY_DATA
	// Only call the setter if the value needs adjustment since we're
	// not supposed to modify this value during async build.
	if (GetUseLegacyMeshDerivedDataKey())
	{
		//Next postedit change will use the new ddc key scheme
		SetUseLegacyMeshDerivedDataKey(false);
	}
#endif

#if WITH_EDITORONLY_DATA
	FPerPlatformInt PerPlatformData = GetMinLod();
	FPerQualityLevelInt PerQualityLevelData = GetQualityLevelMinLod();

	// Convert PerPlatForm data to PerQuality if perQuality data have not been serialized.
	// Also test default value, since PerPLatformData can have Default !=0 and no PerPlaform data overrides.
	bool bConvertMinLODData = (PerQualityLevelData.PerQuality.Num() == 0 && PerQualityLevelData.Default == 0) && (PerPlatformData.PerPlatform.Num() != 0 || PerPlatformData.Default != 0);

	if (GEngine && GEngine->UseSkeletalMeshMinLODPerQualityLevels && bConvertMinLODData)
	{
		// get the platform groups
		const TArray<FName>& PlatformGroupNameArray = PlatformInfo::GetAllPlatformGroupNames();

		// Make sure all platforms and groups are known before updating any of them. Missing platforms would not properly be converted to PerQuality if some of them were known and others were not.
		bool bAllPlatformsKnown = true;
		for (const TPair<FName, int32>& Pair : PerPlatformData.PerPlatform)
		{
			const bool bIsPlatformGroup = PlatformGroupNameArray.Contains(Pair.Key);
			const bool bIsKnownPlatform = (FDataDrivenPlatformInfoRegistry::GetPlatformInfo(Pair.Key).IniPlatformName.IsNone() == false);
			if (!bIsPlatformGroup && !bIsKnownPlatform)
			{
				bAllPlatformsKnown = false;
				break;
			}
		}

		if (bAllPlatformsKnown)
		{
			//assign the default value
			PerQualityLevelData.Default = PerPlatformData.Default;

			// iterate over all platform and platform group entry: ex: XBOXONE = 2, CONSOLE=1, MOBILE = 3
			if (PerQualityLevelData.PerQuality.Num() == 0)
			{
				TMap<FName, int32> SortedPerPlatforms = PerPlatformData.PerPlatform;
				SortedPerPlatforms.KeySort([&](const FName& A, const FName& B) { return (PlatformGroupNameArray.Contains(A) > PlatformGroupNameArray.Contains(B)); });

				for (const TPair<FName, int32>& Pair : SortedPerPlatforms)
				{
					FSupportedQualityLevelArray QualityLevels;
					FString PlatformEntry = Pair.Key.ToString();

					QualityLevels = QualityLevelProperty::PerPlatformOverrideMapping(PlatformEntry);

					// we now have a range of quality levels supported on that platform or from that group
					// note: 
					// -platform group overrides will be applied first
					// -platform override sharing the same quality level will take the smallest MinLOD value between them
					// -ex: if XboxOne and PS4 maps to high and XboxOne MinLOD = 2 and PS4 MINLOD = 1, MINLOD 1 will be selected
					for (int32& QLKey : QualityLevels)
					{
						int32* Value = PerQualityLevelData.PerQuality.Find(QLKey);
						if (Value != nullptr)
						{
							*Value = FMath::Min(Pair.Value, *Value);
						}
						else
						{
							PerQualityLevelData.PerQuality.Add(QLKey, Pair.Value);
						}
					}
				}
			}
			SetQualityLevelMinLod(PerQualityLevelData);
		}
	}
#endif

	ReleaseAsyncProperty();
}

#if WITH_EDITORONLY_DATA

void USkeletalMesh::RebuildRefSkeletonNameToIndexMap()
{
	TArray<FBoneIndexType> DuplicateBones;
	// Make sure we have no duplicate bones. Some content got corrupted somehow. :(
	GetRefSkeleton().RemoveDuplicateBones(this, DuplicateBones);

	// If we have removed any duplicate bones, we need to fix up any broken LODs as well.
	// Duplicate bones are given from highest index to lowest.
	// so it's safe to decrease indices for children, we're not going to lose the index of the remaining duplicate bones.
	for (int32 Index = 0; Index < DuplicateBones.Num(); Index++)
	{
		const FBoneIndexType& DuplicateBoneIndex = DuplicateBones[Index];
		for (int32 LodIndex = 0; LodIndex < GetLODInfoArray().Num(); LodIndex++)
		{
			FSkeletalMeshLODModel& ThisLODModel = GetImportedModel()->LODModels[LodIndex];
			{
				int32 FoundIndex;
				if (ThisLODModel.RequiredBones.Find(DuplicateBoneIndex, FoundIndex))
				{
					ThisLODModel.RequiredBones.RemoveAt(FoundIndex, 1);
					// we need to shift indices of the remaining bones.
					for (int32 j = FoundIndex; j < ThisLODModel.RequiredBones.Num(); j++)
					{
						ThisLODModel.RequiredBones[j] = ThisLODModel.RequiredBones[j] - 1;
					}
				}
			}

			{
				int32 FoundIndex;
				if (ThisLODModel.ActiveBoneIndices.Find(DuplicateBoneIndex, FoundIndex))
				{
					ThisLODModel.ActiveBoneIndices.RemoveAt(FoundIndex, 1);
					// we need to shift indices of the remaining bones.
					for (int32 j = FoundIndex; j < ThisLODModel.ActiveBoneIndices.Num(); j++)
					{
						ThisLODModel.ActiveBoneIndices[j] = ThisLODModel.ActiveBoneIndices[j] - 1;
					}
				}
			}
		}
	}

	// Rebuild name table.
	GetRefSkeleton().RebuildNameToIndexMap();
}

#endif


void USkeletalMesh::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
#if WITH_EDITOR
	// Avoid accessing properties being compiled, this function will get called again after compilation is finished.
	if (IsCompiling())
	{
		return;
	}
#endif

	int32 NumTriangles = 0;
	int32 NumVertices = 0;
	FSkeletalMeshRenderData* SkelMeshRenderData = GetResourceForRendering();
	if (SkelMeshRenderData && SkelMeshRenderData->LODRenderData.Num() > 0)
	{
		const FSkeletalMeshLODRenderData& LODData = SkelMeshRenderData->LODRenderData[0];
		NumTriangles = LODData.GetTotalFaces();
		NumVertices = LODData.GetNumVertices();
	}
	
	int32 NumLODs = GetLODInfoArray().Num();

	OutTags.Add(FAssetRegistryTag("Vertices", FString::FromInt(NumVertices), FAssetRegistryTag::TT_Numerical));
	OutTags.Add(FAssetRegistryTag("Triangles", FString::FromInt(NumTriangles), FAssetRegistryTag::TT_Numerical));
	OutTags.Add(FAssetRegistryTag("LODs", FString::FromInt(NumLODs), FAssetRegistryTag::TT_Numerical));
	OutTags.Add(FAssetRegistryTag("Bones", FString::FromInt(GetRefSkeleton().GetRawBoneNum()), FAssetRegistryTag::TT_Numerical));
	OutTags.Add(FAssetRegistryTag("MorphTargets", FString::FromInt(GetMorphTargets().Num()), FAssetRegistryTag::TT_Numerical));
	OutTags.Add(FAssetRegistryTag("SkinWeightProfiles", FString::FromInt(GetSkinWeightProfiles().Num()), FAssetRegistryTag::TT_Numerical));

#if WITH_EDITORONLY_DATA
	if (GetAssetImportData())
	{
		OutTags.Add( FAssetRegistryTag(SourceFileTagName(), GetAssetImportData()->GetSourceData().ToJson(), FAssetRegistryTag::TT_Hidden) );
#if WITH_EDITOR
		GetAssetImportData()->AppendAssetRegistryTags(OutTags);
#endif
	}
#endif
	
	Super::GetAssetRegistryTags(OutTags);
}

#if WITH_EDITOR
void USkeletalMesh::GetAssetRegistryTagMetadata(TMap<FName, FAssetRegistryTagMetadata>& OutMetadata) const
{
	Super::GetAssetRegistryTagMetadata(OutMetadata);
	OutMetadata.Add("PhysicsAsset", FAssetRegistryTagMetadata().SetImportantValue(TEXT("None")));
}
#endif

void USkeletalMesh::DebugVerifySkeletalMeshLOD()
{
	TArray<FSkeletalMeshLODInfo>& LODInfoArray = GetLODInfoArray();
	// if LOD do not have displayfactor set up correctly
	if (LODInfoArray.Num() > 1)
	{
		for(int32 i=1; i< LODInfoArray.Num(); i++)
		{
			if (LODInfoArray[i].ScreenSize.Default <= 0.1f)
			{
				// too small
				UE_LOG(LogSkeletalMesh, Warning, TEXT("SkelMeshLOD (%s) : ScreenSize for LOD %d may be too small (%0.5f)"), *GetPathName(), i, LODInfoArray[i].ScreenSize.Default);
			}
		}
	}
	else
	{
		// no LODInfo
		UE_LOG(LogSkeletalMesh, Warning, TEXT("SkelMeshLOD (%s) : LOD does not exist"), *GetPathName());
	}
}

void USkeletalMesh::InitMorphTargetsAndRebuildRenderData()
{
#if WITH_EDITOR
	FScopedSkeletalMeshPostEditChange ScopedPostEditChange(this);
#endif
	
	// need to refresh the map
	InitMorphTargets();

	if (IsInGameThread())
	{
		MarkPackageDirty();
		// reset all morphtarget for all components
		for (TObjectIterator<USkeletalMeshComponent> It; It; ++It)
		{
			if (It->GetSkeletalMeshAsset() == this)
			{
				It->RefreshMorphTargets();
			}
		}
	}
}

bool USkeletalMesh::RegisterMorphTarget(UMorphTarget* MorphTarget, bool bInvalidateRenderData)
{
	if ( MorphTarget )
	{
		// if MorphTarget has SkelMesh, make sure you unregister before registering yourself
		if ( MorphTarget->BaseSkelMesh && MorphTarget->BaseSkelMesh!=this )
		{
			MorphTarget->BaseSkelMesh->UnregisterMorphTarget(MorphTarget);
		}

		// if the input morphtarget doesn't have valid data, do not add to the base morphtarget
		ensureMsgf(MorphTarget->HasValidData(), TEXT("RegisterMorphTarget: %s has empty data."), *MorphTarget->GetName());

		MorphTarget->BaseSkelMesh = this;

		bool bRegistered = false;
		const FName MorphTargetName = MorphTarget->GetFName();
		TArray<UMorphTarget*>& RegisteredMorphTargets = GetMorphTargets();
		for ( int32 Index = 0; Index < RegisteredMorphTargets.Num(); ++Index )
		{
			if (RegisteredMorphTargets[Index]->GetFName() == MorphTargetName )
			{
				UE_LOG( LogSkeletalMesh, Verbose, TEXT("RegisterMorphTarget: %s already exists, replacing"), *MorphTarget->GetName() );
				RegisteredMorphTargets[Index] = MorphTarget;
				bRegistered = true;
				break;
			}
		}

		if (!bRegistered)
		{
			RegisteredMorphTargets.Add( MorphTarget );
			bRegistered = true;
		}

		if (bRegistered && bInvalidateRenderData)
		{
			InitMorphTargetsAndRebuildRenderData();
		}
		return bRegistered;
	}
	return false;
}


void USkeletalMesh::UnregisterAllMorphTarget()
{
	GetMorphTargets().Empty();
	InitMorphTargetsAndRebuildRenderData();
}

void USkeletalMesh::UnregisterMorphTarget(UMorphTarget* MorphTarget)
{
	if ( MorphTarget )
	{
		// Do not remove with MorphTarget->GetFName(). The name might have changed
		// Search the value, and delete	
		for ( int32 I=0; I< GetMorphTargets().Num(); ++I)
		{
			if (GetMorphTargets()[I] == MorphTarget )
			{
				GetMorphTargets().RemoveAt(I);
				--I;
				InitMorphTargetsAndRebuildRenderData();
				return;
			}
		}
		UE_LOG( LogSkeletalMesh, Log, TEXT("UnregisterMorphTarget: %s not found."), *MorphTarget->GetName() );
	}
}

void USkeletalMesh::InitMorphTargets()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(USkeletalMesh::InitMorphTargets);
	GetMorphTargetIndexMap().Empty();

	TArray<UMorphTarget*>& MorphTargetsLocal = GetMorphTargets();
	for (int32 Index = 0; Index < MorphTargetsLocal.Num(); ++Index)
	{
		UMorphTarget* MorphTarget = MorphTargetsLocal[Index];
		// if we don't have a valid data, just remove it
		if (!MorphTarget->HasValidData())
		{
			MorphTargetsLocal.RemoveAt(Index);
			--Index;
			continue;
		}

		FName const ShapeName = MorphTarget->GetFName();
		if (GetMorphTargetIndexMap().Find(ShapeName) == nullptr)
		{
			GetMorphTargetIndexMap().Add(ShapeName, Index);

			// register as morphtarget curves
			if (GetSkeleton())
			{
				FSmartName CurveName;
				CurveName.DisplayName = ShapeName;
				
				// verify will make sure it adds to the curve if not found
				// the reason of using this is to make sure it works in editor/non-editor
				GetSkeleton()->VerifySmartName(USkeleton::AnimCurveMappingName, CurveName);
				GetSkeleton()->AccumulateCurveMetaData(ShapeName, false, true);
			}
		}
	}
}

UMorphTarget* USkeletalMesh::FindMorphTarget(FName MorphTargetName) const
{
	int32 Index;
	return FindMorphTargetAndIndex(MorphTargetName, Index);
}

UMorphTarget* USkeletalMesh::FindMorphTargetAndIndex(FName MorphTargetName, int32& OutIndex) const
{
	OutIndex = INDEX_NONE;
	if( MorphTargetName != NAME_None )
	{
		const int32* Found = GetMorphTargetIndexMap().Find(MorphTargetName);
		if (Found)
		{
			OutIndex = *Found;
			return GetMorphTargets()[*Found];
		}
	}

	return nullptr;
}

USkeletalMeshSocket* USkeletalMesh::FindSocket(FName InSocketName) const
{
	int32 DummyIdx;
	return FindSocketAndIndex(InSocketName, DummyIdx);
}

#if WITH_EDITOR
void USkeletalMesh::AddSocket(USkeletalMeshSocket* InSocket, bool bAddToSkeleton /*= false*/)
{
	if (InSocket)
	{
		if (InSocket->GetOuter() == this)
		{
			const FString SocketNameString = InSocket->SocketName.ToString();
			const FString TrimmedNameString = SocketNameString.TrimStartAndEnd();
			const FName TrimmedName = FName(*TrimmedNameString);
			if (!Sockets.ContainsByPredicate([TrimmedName](const TObjectPtr<USkeletalMeshSocket>& Socket)
			{
				return Socket->SocketName == TrimmedName;
			}))
			{
				const FReferenceSkeleton& ReferenceSkeleton = GetRefSkeleton();
				if (ReferenceSkeleton.FindBoneIndex(InSocket->BoneName) != INDEX_NONE)
				{				
					if (!TrimmedNameString.IsEmpty())
					{
						const FScopedTransaction Transaction(LOCTEXT("AddSocket", "Add Socket"));
						Modify();

						// Update socket name in place
						InSocket->SocketName = TrimmedName;					
						Sockets.Add(InSocket);
		    
						if (bAddToSkeleton)
						{
							USkeleton* CurrentSkeleton = GetSkeleton();				
							if (!CurrentSkeleton->Sockets.ContainsByPredicate([Name=InSocket->SocketName](const TObjectPtr<USkeletalMeshSocket>& Socket)
							{
								return Socket->SocketName == Name;
							}))
							{
								const FReferenceSkeleton& SkeletonReferenceSkeleton = CurrentSkeleton->GetReferenceSkeleton();
								if (SkeletonReferenceSkeleton.FindBoneIndex(InSocket->BoneName) != INDEX_NONE)
								{
									CurrentSkeleton->Modify();
			    
									USkeletalMeshSocket* NewSocket = DuplicateObject<USkeletalMeshSocket>(InSocket, CurrentSkeleton);
									check(NewSocket);
									CurrentSkeleton->Sockets.Add(NewSocket);
								}
								else
								{
									// Bone does not exists on Skeleton
									UE_LOG(LogSkeletalMesh, Warning, TEXT("Failed to add socket to Skeleton (%s) as the provided bone name %s does not exist."), *CurrentSkeleton->GetFullName(), *InSocket->BoneName.ToString());
								}
							}
						}
					}
					else
					{
						// Invalid socket name
						UE_LOG(LogSkeletalMesh, Warning, TEXT("Failed to add socket as the provided socket name %s is invalid."), *TrimmedNameString);
					}
				}
				else
				{
					// Bone does not exists
					UE_LOG(LogSkeletalMesh, Warning, TEXT("Failed to add socket as the provided bone name %s does not exist."), *InSocket->BoneName.ToString());
				}
			}
			else
			{
				// Socket already exists
				UE_LOG(LogSkeletalMesh, Warning, TEXT("Failed to add socket as a socket with name %s already exist."), *InSocket->BoneName.ToString());
			}
		}
		else
		{
			// Socket outer is not correct
			UE_LOG(LogSkeletalMesh, Warning, TEXT("Failed to add socket as the socket its outer should be %s but is %s."), *GetFullName(), *InSocket->GetOuter()->GetFullName());
		}
	}
}
#endif

#if !WITH_EDITOR

USkeletalMesh::FSocketInfo::FSocketInfo(const USkeletalMesh* InSkeletalMesh, USkeletalMeshSocket* InSocket, int32 InSocketIndex)
	: SocketLocalTransform(InSocket->GetSocketLocalTransform())
	, Socket(InSocket)
	, SocketIndex(InSocketIndex)
	, SocketBoneIndex(InSkeletalMesh->GetRefSkeleton().FindBoneIndex(InSocket->BoneName))
{}

#endif

USkeletalMeshSocket* USkeletalMesh::FindSocketAndIndex(FName InSocketName, int32& OutIndex) const
{
	OutIndex = INDEX_NONE;
	if (InSocketName == NAME_None)
	{
		return nullptr;
	}

#if !WITH_EDITOR
	check(!HasAnyFlags(RF_NeedPostLoad));

	const FSocketInfo* FoundSocketInfo = SocketMap.Find(InSocketName);
	if (FoundSocketInfo)
	{
		OutIndex = FoundSocketInfo->SocketIndex;
		return FoundSocketInfo->Socket;
	}
	return nullptr;
#endif

	for (int32 i = 0; i < Sockets.Num(); i++)
	{
		USkeletalMeshSocket* Socket = Sockets[i];
		if (Socket && Socket->SocketName == InSocketName)
		{
			OutIndex = i;
			return Socket;
		}
	}

	// If the socket isn't on the mesh, try to find it on the skeleton
	if (GetSkeleton())
	{
		USkeletalMeshSocket* SkeletonSocket = GetSkeleton()->FindSocketAndIndex(InSocketName, OutIndex);
		if (SkeletonSocket != nullptr)
		{
			OutIndex += Sockets.Num();
		}
		return SkeletonSocket;
	}

	return nullptr;
}

USkeletalMeshSocket* USkeletalMesh::FindSocketInfo(FName InSocketName, FTransform& OutTransform, int32& OutBoneIndex, int32& OutIndex) const
{
	OutIndex = INDEX_NONE;
	OutTransform = FTransform::Identity;
	OutBoneIndex = INDEX_NONE;

	if (InSocketName == NAME_None)
	{
		return nullptr;
	}

#if !WITH_EDITOR
	check(!HasAnyFlags(RF_NeedPostLoad));

	const FSocketInfo* FoundSocketInfo = SocketMap.Find(InSocketName);
	if (FoundSocketInfo)
	{
		OutTransform = FoundSocketInfo->SocketLocalTransform;
		OutIndex = FoundSocketInfo->SocketIndex;
		OutBoneIndex = FoundSocketInfo->SocketBoneIndex;
		return FoundSocketInfo->Socket;
	}
	return nullptr;
#endif

	for (int32 i = 0; i < Sockets.Num(); i++)
	{
		USkeletalMeshSocket* Socket = Sockets[i];
		if (Socket && Socket->SocketName == InSocketName)
		{
			OutIndex = i;
			OutTransform = Socket->GetSocketLocalTransform();
			OutBoneIndex = GetRefSkeleton().FindBoneIndex(Socket->BoneName);
			return Socket;
		}
	}

	// If the socket isn't on the mesh, try to find it on the skeleton
	if (GetSkeleton())
	{
		USkeletalMeshSocket* SkeletonSocket = GetSkeleton()->FindSocketAndIndex(InSocketName, OutIndex);
		if (SkeletonSocket != nullptr)
		{
			OutIndex += Sockets.Num();
			OutTransform = SkeletonSocket->GetSocketLocalTransform();
			OutBoneIndex = GetRefSkeleton().FindBoneIndex(SkeletonSocket->BoneName);
		}
		return SkeletonSocket;
	}

	return nullptr;
}

int32 USkeletalMesh::NumSockets() const
{
	return Sockets.Num() + (GetSkeleton() ? GetSkeleton()->Sockets.Num() : 0);
}

USkeletalMeshSocket* USkeletalMesh::GetSocketByIndex(int32 Index) const
{
	const int32 NumMeshSockets = Sockets.Num();
	if (Index < NumMeshSockets)
	{
		return Sockets[Index];
	}

	if (GetSkeleton() && (Index - NumMeshSockets) < GetSkeleton()->Sockets.Num())
	{
		return GetSkeleton()->Sockets[Index - NumMeshSockets];
	}

	return nullptr;
}

TMap<FVector3f, FColor> USkeletalMesh::GetVertexColorData(const uint32 PaintingMeshLODIndex) const
{
	TMap<FVector3f, FColor> VertexColorData;
#if WITH_EDITOR
	const FSkeletalMeshModel* SkeletalMeshModel = GetImportedModel();
	if (GetHasVertexColors() && SkeletalMeshModel && SkeletalMeshModel->LODModels.IsValidIndex(PaintingMeshLODIndex))
	{
		const TArray<FSkelMeshSection>& Sections = SkeletalMeshModel->LODModels[PaintingMeshLODIndex].Sections;

		for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
		{
			const TArray<FSoftSkinVertex>& SoftVertices = Sections[SectionIndex].SoftVertices;
			
			for (int32 VertexIndex = 0; VertexIndex < SoftVertices.Num(); ++VertexIndex)
			{
				const FVector3f& Position(SoftVertices[VertexIndex].Position);
				FColor& Color = VertexColorData.FindOrAdd(Position);
				Color = SoftVertices[VertexIndex].Color;
			}
		}
	}
#endif // #if WITH_EDITOR

	return VertexColorData;
}


void USkeletalMesh::RebuildSocketMap()
{
#if !WITH_EDITOR

	check(IsInGameThread());

	SocketMap.Reset();
	SocketMap.Reserve(Sockets.Num() + (GetSkeleton() ? GetSkeleton()->Sockets.Num() : 0));

	int32 SocketIndex;
	for (SocketIndex = 0; SocketIndex < Sockets.Num(); ++SocketIndex)
	{
		USkeletalMeshSocket* Socket = Sockets[SocketIndex];
		SocketMap.Add(Socket->SocketName, FSocketInfo(this, Socket, SocketIndex));
	}

	// If the socket isn't on the mesh, try to find it on the skeleton
	if (GetSkeleton())
	{
		for (SocketIndex = 0; SocketIndex < GetSkeleton()->Sockets.Num(); ++SocketIndex)
		{
			USkeletalMeshSocket* Socket = GetSkeleton()->Sockets[SocketIndex];
			if (!SocketMap.Contains(Socket->SocketName))
			{
				SocketMap.Add(Socket->SocketName, FSocketInfo(this, Socket, Sockets.Num() + SocketIndex));
			}
		}
	}

#endif // !WITH_EDITOR
}

FMatrix USkeletalMesh::GetRefPoseMatrix( int32 BoneIndex ) const
{
 	check( BoneIndex >= 0 && BoneIndex < GetRefSkeleton().GetRawBoneNum() );
	FTransform BoneTransform = GetRefSkeleton().GetRawRefBonePose()[BoneIndex];
	// Make sure quaternion is normalized!
	BoneTransform.NormalizeRotation();
	return BoneTransform.ToMatrixWithScale();
}

FMatrix USkeletalMesh::GetComposedRefPoseMatrix( FName InBoneName ) const
{
	FMatrix LocalPose( FMatrix::Identity );

	if ( InBoneName != NAME_None )
	{
		int32 BoneIndex = GetRefSkeleton().FindBoneIndex(InBoneName);
		if (BoneIndex != INDEX_NONE)
		{
			return GetComposedRefPoseMatrix(BoneIndex);
		}
		else
		{
			USkeletalMeshSocket const* Socket = FindSocket(InBoneName);

			if(Socket != NULL)
			{
				BoneIndex = GetRefSkeleton().FindBoneIndex(Socket->BoneName);

				if(BoneIndex != INDEX_NONE)
				{
					const FRotationTranslationMatrix SocketMatrix(Socket->RelativeRotation, Socket->RelativeLocation);
					LocalPose = SocketMatrix * GetComposedRefPoseMatrix(BoneIndex);
				}
			}
		}
	}

	return LocalPose;
}

FMatrix USkeletalMesh::GetComposedRefPoseMatrix(int32 InBoneIndex) const
{
	return GetCachedComposedRefPoseMatrices()[InBoneIndex];
}

TArray<USkeletalMeshSocket*>& USkeletalMesh::GetMeshOnlySocketList()
{
	return Sockets;
}

const TArray<USkeletalMeshSocket*>& USkeletalMesh::GetMeshOnlySocketList() const
{
	return Sockets;
}

#if WITH_EDITORONLY_DATA
void USkeletalMesh::MoveDeprecatedShadowFlagToMaterials()
{
	// First, the easy case where there's no LOD info (in which case, default to true!)
	if ( GetLODInfoArray().Num() == 0 )
	{
		for ( auto Material = GetMaterials().CreateIterator(); Material; ++Material )
		{
			Material->bEnableShadowCasting_DEPRECATED = true;
		}

		return;
	}
	
	TArray<bool> PerLodShadowFlags;
	bool bDifferenceFound = false;

	// Second, detect whether the shadow casting flag is the same for all sections of all lods
	for ( auto LOD = GetLODInfoArray().CreateConstIterator(); LOD; ++LOD )
	{
		if ( LOD->bEnableShadowCasting_DEPRECATED.Num() )
		{
			PerLodShadowFlags.Add( LOD->bEnableShadowCasting_DEPRECATED[0] );
		}

		if ( !AreAllFlagsIdentical( LOD->bEnableShadowCasting_DEPRECATED ) )
		{
			// We found a difference in the sections of this LOD!
			bDifferenceFound = true;
			break;
		}
	}

	if ( !bDifferenceFound && !AreAllFlagsIdentical( PerLodShadowFlags ) )
	{
		// Difference between LODs
		bDifferenceFound = true;
	}

	if ( !bDifferenceFound )
	{
		// All the same, so just copy the shadow casting flag to all materials
		for ( auto Material = GetMaterials().CreateIterator(); Material; ++Material )
		{
			Material->bEnableShadowCasting_DEPRECATED = PerLodShadowFlags.Num() ? PerLodShadowFlags[0] : true;
		}
	}
	else
	{
		FSkeletalMeshModel* Resource = GetImportedModel();
		check( Resource->LODModels.Num() == GetLODInfoArray().Num() );

		TArray<FSkeletalMaterial> NewMaterialArray;
		TArray<FSkeletalMaterial>& CurrentMaterials = GetMaterials();

		TArray<FSkeletalMeshLODInfo>& LODInfoArray = GetLODInfoArray();
		// There was a difference, so we need to build a new material list which has all the combinations of UMaterialInterface and shadow casting flag required
		for ( int32 LODIndex = 0; LODIndex < Resource->LODModels.Num(); ++LODIndex )
		{
			check( Resource->LODModels[LODIndex].Sections.Num() == LODInfoArray[LODIndex].bEnableShadowCasting_DEPRECATED.Num() );

			for ( int32 i = 0; i < Resource->LODModels[LODIndex].Sections.Num(); ++i )
			{
				NewMaterialArray.Add( FSkeletalMaterial(CurrentMaterials[ Resource->LODModels[LODIndex].Sections[i].MaterialIndex ].MaterialInterface, LODInfoArray[LODIndex].bEnableShadowCasting_DEPRECATED[i], false, NAME_None, NAME_None ) );
			}
		}

		// Reassign the materials array to the new one
		SetMaterials(NewMaterialArray);
		int32 NewIndex = 0;

		// Remap the existing LODModels to point at the correct new material index
		for ( int32 LODIndex = 0; LODIndex < Resource->LODModels.Num(); ++LODIndex )
		{
			check( Resource->LODModels[LODIndex].Sections.Num() == LODInfoArray[LODIndex].bEnableShadowCasting_DEPRECATED.Num() );

			for ( int32 i = 0; i < Resource->LODModels[LODIndex].Sections.Num(); ++i )
			{
				Resource->LODModels[LODIndex].Sections[i].MaterialIndex = NewIndex;
				++NewIndex;
			}
		}
	}
}

void USkeletalMesh::MoveMaterialFlagsToSections()
{
	//No LOD we cant set the value
	if (GetLODInfoArray().Num() == 0)
	{
		return;
	}

	TArray<FSkeletalMaterial>& CurrentMaterials = GetMaterials();
	for (FSkeletalMeshLODModel &StaticLODModel : GetImportedModel()->LODModels)
	{
		for (int32 SectionIndex = 0; SectionIndex < StaticLODModel.Sections.Num(); ++SectionIndex)
		{
			FSkelMeshSection &Section = StaticLODModel.Sections[SectionIndex];
			//Prior to FEditorObjectVersion::RefactorMeshEditorMaterials Material index match section index
			if (CurrentMaterials.IsValidIndex(SectionIndex))
			{
				Section.bCastShadow = CurrentMaterials[SectionIndex].bEnableShadowCasting_DEPRECATED;

				Section.bRecomputeTangent = CurrentMaterials[SectionIndex].bRecomputeTangent_DEPRECATED;
			}
			else
			{
				//Default cast shadow to true this is a fail safe code path it should not go here if the data
				//is valid
				Section.bCastShadow = true;
				//Recompute tangent is serialize prior to FEditorObjectVersion::RefactorMeshEditorMaterials
				// We just keep the serialize value
			}
		}
	}
}
#endif // WITH_EDITORONLY_DATA

#if WITH_EDITOR

FDelegateHandle USkeletalMesh::RegisterOnClothingChange(const FSimpleMulticastDelegate::FDelegate& InDelegate)
{
	return OnClothingChange.Add(InDelegate);
}

void USkeletalMesh::UnregisterOnClothingChange(const FDelegateHandle& InHandle)
{
	OnClothingChange.Remove(InHandle);
}

#endif

bool USkeletalMesh::AreAllFlagsIdentical( const TArray<bool>& BoolArray ) const
{
	if ( BoolArray.Num() == 0 )
	{
		return true;
	}

	for ( int32 i = 0; i < BoolArray.Num() - 1; ++i )
	{
		if ( BoolArray[i] != BoolArray[i + 1] )
		{
			return false;
		}
	}

	return true;
}

TArray<USkeletalMeshSocket*> USkeletalMesh::GetActiveSocketList() const
{
	TArray<USkeletalMeshSocket*> ActiveSockets = Sockets;

	// Then the skeleton sockets that aren't in the mesh
	if (GetSkeleton())
	{
		for (auto SkeletonSocketIt = GetSkeleton()->Sockets.CreateConstIterator(); SkeletonSocketIt; ++SkeletonSocketIt)
		{
			USkeletalMeshSocket* Socket = *(SkeletonSocketIt);

			if (!IsSocketOnMesh(Socket->SocketName))
			{
				ActiveSockets.Add(Socket);
			}
		}
	}
	return ActiveSockets;
}

bool USkeletalMesh::IsSocketOnMesh(const FName& InSocketName) const
{
	for(int32 SocketIdx=0; SocketIdx < Sockets.Num(); SocketIdx++)
	{
		USkeletalMeshSocket* Socket = Sockets[SocketIdx];

		if(Socket != NULL && Socket->SocketName == InSocketName)
		{
			return true;
		}
	}

	return false;
}

void USkeletalMesh::AllocateResourceForRendering()
{
	SetSkeletalMeshRenderData(MakeUnique<FSkeletalMeshRenderData>());
}

#if WITH_EDITOR
void USkeletalMesh::InvalidateDeriveDataCacheGUID()
{
	// Create new DDC guid
	GetImportedModel()->GenerateNewGUID();
}

namespace InternalSkeletalMeshHelper
{
	/**
	 * We want to recreate the LODMaterialMap correctly. The hypothesis is the original section will always be the same when we build the skeletalmesh
	 * Max GPU bone per section which drive the chunking which can generate different number of section but the number of original section will always be the same.
	 * So we simply reset the LODMaterialMap and rebuild it with the backup we took before building the skeletalmesh.
	 */
	void CreateLodMaterialMapBackup(const USkeletalMesh* SkeletalMesh, TMap<int32, TArray<int16>>& BackupSectionsPerLOD)
	{
		if (!ensure(SkeletalMesh != nullptr))
		{
			return;
		}
		BackupSectionsPerLOD.Reset();
		const FSkeletalMeshModel* ImportedModel = SkeletalMesh->GetImportedModel();
		if (!ImportedModel)
		{
			return;
		}
		//Create the backup
		for (int32 LODIndex = 0; LODIndex < SkeletalMesh->GetLODNum(); ++LODIndex)
		{
			const FSkeletalMeshLODInfo* LODInfoEntry = SkeletalMesh->GetLODInfo(LODIndex);
			//Do not backup/restore LODMaterialMap if...
			if (!ImportedModel->LODModels.IsValidIndex(LODIndex)
				|| LODInfoEntry == nullptr
				|| LODInfoEntry->LODMaterialMap.Num() == 0 //If there is no LODMaterialMap we have nothing to backup
				|| SkeletalMesh->IsReductionActive(LODIndex) //Reduction will manage the LODMaterialMap, avoid backup restore
				|| !SkeletalMesh->IsLODImportedDataBuildAvailable(LODIndex)) //Legacy asset are not build, avoid backup restore
			{
				continue;
			}
			const FSkeletalMeshLODModel& LODModel = ImportedModel->LODModels[LODIndex];
			TArray<int16>& BackupSections = BackupSectionsPerLOD.FindOrAdd(LODIndex);
			int32 SectionCount = LODModel.Sections.Num();
			BackupSections.Reserve(SectionCount);
			for (int32 SectionIndex = 0; SectionIndex < SectionCount; ++SectionIndex)
			{
				if (LODModel.Sections[SectionIndex].ChunkedParentSectionIndex == INDEX_NONE)
				{
					BackupSections.Add(LODInfoEntry->LODMaterialMap.IsValidIndex(SectionIndex) ? LODInfoEntry->LODMaterialMap[SectionIndex] : INDEX_NONE);
				}
			}
		}
	}

	void RestoreLodMaterialMapBackup(USkeletalMesh* SkeletalMesh, const TMap<int32, TArray<int16>>& BackupSectionsPerLOD)
	{
		if (!ensure(SkeletalMesh != nullptr))
		{
			return;
		}
		const FSkeletalMeshModel* ImportedModel = SkeletalMesh->GetImportedModel();
		if (!ImportedModel)
		{
			return;
		}

		for (int32 LODIndex = 0; LODIndex < SkeletalMesh->GetLODNum(); ++LODIndex)
		{
			FSkeletalMeshLODInfo* LODInfoEntry = SkeletalMesh->GetLODInfo(LODIndex);
			if (!ImportedModel->LODModels.IsValidIndex(LODIndex) || LODInfoEntry == nullptr)
			{
				continue;
			}
			const TArray<int16>* BackupSectionsPtr = BackupSectionsPerLOD.Find(LODIndex);
			if (!BackupSectionsPtr || BackupSectionsPtr->IsEmpty())
			{
				continue;
			}

			const TArray<int16>& BackupSections = *BackupSectionsPtr;
			const FSkeletalMeshLODModel& LODModel = ImportedModel->LODModels[LODIndex];
			LODInfoEntry->LODMaterialMap.Reset();
			const int32 SectionCount = LODModel.Sections.Num();
			for (int32 SectionIndex = 0; SectionIndex < SectionCount; ++SectionIndex)
			{
				const FSkelMeshSection& Section = LODModel.Sections[SectionIndex];
				int16 NewLODMaterialMapValue = INDEX_NONE;
				if (BackupSections.IsValidIndex(Section.OriginalDataSectionIndex))
				{
					NewLODMaterialMapValue = BackupSections[Section.OriginalDataSectionIndex];
				}
				LODInfoEntry->LODMaterialMap.Add(NewLODMaterialMapValue);
			}
		}
	}
} //namespace InternalSkeletalMeshHelper

void USkeletalMesh::CacheDerivedData(FSkinnedAssetCompilationContext* ContextPtr)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(USkeletalMesh::CacheDerivedData);
	check(ContextPtr);

	// Cache derived data for the running platform.
	ITargetPlatform* RunningPlatform = GetTargetPlatformManagerRef().GetRunningTargetPlatform();
	check(RunningPlatform);

	AllocateResourceForRendering();

	// Warn if the platform support minimal number of per vertex bone influences 
	ValidateBoneWeights(RunningPlatform);

	//LODMaterialMap from LODInfo is store in the uasset and not in the DDC, so we want to fix it here
	//to cover the post load and the post edit change. The build can change the number of section and LODMaterialMap is index per section
	//TODO, move LODMaterialmap functionality into the LODModel UserSectionsData which are index per original section (imported section).
	TMap<int32, TArray<int16>> BackupSectionsPerLOD;
	InternalSkeletalMeshHelper::CreateLodMaterialMapBackup(this, BackupSectionsPerLOD);

	GetSkeletalMeshRenderData()->Cache(RunningPlatform, this, ContextPtr);

	InternalSkeletalMeshHelper::RestoreLodMaterialMapBackup(this, BackupSectionsPerLOD);
}


void USkeletalMesh::ValidateBoneWeights(const ITargetPlatform* TargetPlatform)
{
	if (TargetPlatform->SupportsFeature(ETargetPlatformFeatures::MobileRendering))
	{
		if (!GetImportedModel())
		{
			return;
		}
		FSkeletalMeshRenderData* SkelMeshRenderData = GetResourceForRendering();

		int32 NumLODs = GetLODInfoArray().Num();
		int32 MinFirstLOD = GetMinLodIdx();
		int32 MaxNumLODs = FMath::Clamp<int32>(NumLODs - MinFirstLOD, SkelMeshRenderData->NumInlinedLODs, NumLODs);

		for (int32 LODIndex = 0; LODIndex < GetLODNum(); ++LODIndex)
		{
			if (!GetImportedModel()->LODModels.IsValidIndex(LODIndex))
			{
				continue;
			}
			const FSkeletalMeshLODModel& ImportLODModel = GetImportedModel()->LODModels[LODIndex];

			int32 MaxBoneInfluences = ImportLODModel.GetMaxBoneInfluences();

			for (int32 SectionIndex = 0; SectionIndex < ImportLODModel.Sections.Num(); ++SectionIndex)
			{
				const FSkelMeshSection & Section = ImportLODModel.Sections[SectionIndex];

				int MaxBoneInfluencesSection = Section.MaxBoneInfluences;
				if (MaxBoneInfluences > 12)
				{
					UE_LOG(LogSkeletalMesh, Warning, TEXT("Mesh: %s has more than 12 max bone influences, it has: %d"), *GetFullName(), MaxBoneInfluencesSection);
				}
			}
		}
	}
}

void USkeletalMesh::BeginCacheForCookedPlatformData(const ITargetPlatform* TargetPlatform)
{
	// Make sure to cache platform data so it doesn't happen lazily during serialization of the skeletal mesh
	constexpr bool bIsSerializeSaving = false;
	GetPlatformSkeletalMeshRenderData(this, TargetPlatform, bIsSerializeSaving);
	ValidateBoneWeights(TargetPlatform);
}

void USkeletalMesh::ClearAllCachedCookedPlatformData()
{
	GetResourceForRendering()->NextCachedRenderData.Reset();
	
	if (FApp::CanEverRender())
	{
		// We need to keep the ddc editor data LODModel for rendering; it can be different (The number of sections, the number of vertices, the number of morphtargets) because of chunking, build or reduction setting that are or will be per platform.
		//Normally this call should be able to read values out of ddc rather than rebuilding, because the ddc for the running platform was cached when we loaded the asset.
		ITargetPlatform* RunningPlatform = GetTargetPlatformManagerRef().GetRunningTargetPlatform();
		check(RunningPlatform != NULL);
		const FString RunningPlatformDerivedDataKey = BuildDerivedDataKey(RunningPlatform);
		FSkeletalMeshRenderData RunningPlatformSkeletalMeshRenderData;
		constexpr bool bIsSerializeSaving = false;
		CachePlatform(this, RunningPlatform, &RunningPlatformSkeletalMeshRenderData, bIsSerializeSaving);
	}
}

//Serialize the LODInfo and append the result to the KeySuffix to build the LODInfo part of the DDC KEY
//Note: this serializer is only used to build the mesh DDC key, no versioning is required
static void SerializeLODInfoForDDC(USkeletalMesh* SkeletalMesh, FString& KeySuffix)
{
	TArray<FSkeletalMeshLODInfo>& LODInfos = SkeletalMesh->GetLODInfoArray();
	const bool bIs16BitfloatBufferSupported = GVertexElementTypeSupport.IsSupported(VET_Half2);
	for (int32 LODIndex = 0; LODIndex < SkeletalMesh->GetLODNum(); ++LODIndex)
	{
		check(LODInfos.IsValidIndex(LODIndex));
		FSkeletalMeshLODInfo& LODInfo = LODInfos[LODIndex];
		bool bValidLODSettings = false;
		if (SkeletalMesh->GetLODSettings() != nullptr)
		{
			const int32 NumSettings = FMath::Min(SkeletalMesh->GetLODSettings()->GetNumberOfSettings(), SkeletalMesh->GetLODNum());
			if (LODIndex < NumSettings)
			{
				bValidLODSettings = true;
			}
		}
		const FSkeletalMeshLODGroupSettings* SkeletalMeshLODGroupSettings = bValidLODSettings ? &SkeletalMesh->GetLODSettings()->GetSettingsForLODLevel(LODIndex) : nullptr;
		LODInfo.BuildGUID = LODInfo.ComputeDeriveDataCacheKey(SkeletalMeshLODGroupSettings);
		KeySuffix += LODInfo.BuildGUID.ToString(EGuidFormats::Digits);
	}
}

extern int32 GStripSkeletalMeshLodsDuringCooking;
extern int32 GSkeletalMeshKeepMobileMinLODSettingOnDesktop;

FString USkeletalMesh::BuildDerivedDataKey(const ITargetPlatform* TargetPlatform)
{
	FString KeySuffix(TEXT(""));

	if (GetUseLegacyMeshDerivedDataKey())
	{
		//Old asset will have the same LOD settings for bUseFullPrecisionUVs. We can use the LOD 0
		const FSkeletalMeshLODInfo* BaseLODInfo = GetLODInfo(0);
		bool bUseFullPrecisionUVs = BaseLODInfo ? BaseLODInfo->BuildSettings.bUseFullPrecisionUVs : false;
		KeySuffix += GetImportedModel()->GetIdString();
		KeySuffix += (bUseFullPrecisionUVs || !GVertexElementTypeSupport.IsSupported(VET_Half2)) ? "1" : "0";

		//Dummy call to update the FSkeletalMeshLODModel::BuildStringID members.
		GetImportedModel()->GetLODModelIdString();

		//Dummy call to update the LODInfo::BuildGUID members.
		{
			FString TmpString;
			SerializeLODInfoForDDC(this, TmpString);
		}
	}
	else
	{
		FString TmpPartialKeySuffix;
		//Synchronize the user data that are part of the key
		GetImportedModel()->SyncronizeLODUserSectionsData();
		TmpPartialKeySuffix = GetImportedModel()->GetIdString();
		KeySuffix += TmpPartialKeySuffix;
		TmpPartialKeySuffix = GetImportedModel()->GetLODModelIdString();
		KeySuffix += TmpPartialKeySuffix;

		//Add the max gpu bone per section
		const int32 MaxGPUSkinBones = FGPUBaseSkinVertexFactory::GetMaxGPUSkinBones(TargetPlatform);
		KeySuffix += FString::FromInt(MaxGPUSkinBones);

		TmpPartialKeySuffix = TEXT("");
		SerializeLODInfoForDDC(this, TmpPartialKeySuffix);
		KeySuffix += TmpPartialKeySuffix;
	}

	KeySuffix += GetHasVertexColors() ? "1" : "0";
	KeySuffix += GetVertexColorGuid().ToString(EGuidFormats::Digits);

	if (GetEnableLODStreaming(TargetPlatform))
	{
		const int32 MaxStreamedLODs = GetMaxNumStreamedLODs(TargetPlatform);
		const int32 MaxOptionalLODs = GetMaxNumOptionalLODs(TargetPlatform);
		KeySuffix += *FString::Printf(TEXT("1%08x%08x"), MaxStreamedLODs, MaxOptionalLODs);
	}
	else
	{
		KeySuffix += TEXT("0zzzzzzzzzzzzzzzz");
	}

	if (TargetPlatform->GetPlatformInfo().PlatformGroupName == TEXT("Desktop")
		&& GStripSkeletalMeshLodsDuringCooking != 0
		&& GSkeletalMeshKeepMobileMinLODSettingOnDesktop != 0)
	{
		KeySuffix += TEXT("_MinMLOD");
	}

	IMeshBuilderModule::GetForPlatform(TargetPlatform).AppendToDDCKey(KeySuffix, true);
	const bool bUnlimitedBoneInfluences = FGPUBaseSkinVertexFactory::GetUnlimitedBoneInfluences();
	KeySuffix += bUnlimitedBoneInfluences ? "1" : "0";

#if PLATFORM_CPU_ARM_FAMILY
	// Separate out arm keys as x64 and arm64 clang do not generate the same data for a given
	// input. Add the arm specifically so that a) we avoid rebuilding the current DDC and
	// b) we can remove it once we get arm64 to be consistent.
	KeySuffix.Append(TEXT("_arm64"));
#endif

	return FDerivedDataCacheInterface::BuildCacheKey(
		TEXT("SKELETALMESH"),
		*GetSkeletalMeshDerivedDataVersion(),
		*KeySuffix
	);
}

FString USkeletalMesh::GetDerivedDataKey()
{
	// Cache derived data for the running platform.
	ITargetPlatform* RunningPlatform = GetTargetPlatformManagerRef().GetRunningTargetPlatform();
	check(RunningPlatform);

	return BuildDerivedDataKey(RunningPlatform);
}

int32 USkeletalMesh::ValidatePreviewAttachedObjects()
{
	int32 NumBrokenAssets = GetPreviewAttachedAssetContainer().ValidatePreviewAttachedObjects();

	if (NumBrokenAssets > 0)
	{
		MarkPackageDirty();
	}
	return NumBrokenAssets;
}

void USkeletalMesh::RemoveMeshSection(int32 InLodIndex, int32 InSectionIndex)
{
	// Need a mesh resource
	if (GetImportedModel() == nullptr)
	{
		UE_LOG(LogSkeletalMesh, Warning, TEXT("Failed to remove skeletal mesh section, ImportedResource is invalid."));
		return;
	}

	// Need a valid LOD
	if (!GetImportedModel()->LODModels.IsValidIndex(InLodIndex))
	{
		UE_LOG(LogSkeletalMesh, Warning, TEXT("Failed to remove skeletal mesh section, LOD%d does not exist in the mesh"), InLodIndex);
		return;
	}

	FSkeletalMeshLODModel& LodModel = GetImportedModel()->LODModels[InLodIndex];

	// Need a valid section
	if (!LodModel.Sections.IsValidIndex(InSectionIndex))
	{
		UE_LOG(LogSkeletalMesh, Warning, TEXT("Failed to remove skeletal mesh section, Section %d does not exist in LOD%d."), InSectionIndex, InLodIndex);
		return;
	}

	FSkelMeshSection& SectionToDisable = LodModel.Sections[InSectionIndex];
	
	//Get the UserSectionData
	FSkelMeshSourceSectionUserData& UserSectionToDisableData = LodModel.UserSectionsData.FindChecked(SectionToDisable.OriginalDataSectionIndex);

	if(UserSectionToDisableData.HasClothingData())
	{
		// Can't remove this, clothing currently relies on it
		UE_LOG(LogSkeletalMesh, Warning, TEXT("Failed to remove skeletal mesh section, clothing is currently bound to Lod%d Section %d, unbind clothing before removal."), InLodIndex, InSectionIndex);
		return;
	}

	if (!UserSectionToDisableData.bDisabled || !SectionToDisable.bDisabled)
	{
		//Scope a post edit change
		FScopedSkeletalMeshPostEditChange ScopedPostEditChange(this);
		// Valid to disable, dirty the mesh
		Modify();
		PreEditChange(nullptr);
		//Disable the section
		UserSectionToDisableData.bDisabled = true;
		SectionToDisable.bDisabled = true;
	}
}

#endif // #if WITH_EDITOR

void USkeletalMesh::ReleaseCPUResources()
{
	FSkeletalMeshRenderData* SkelMeshRenderData = GetResourceForRendering();
	if (SkelMeshRenderData)
	{
		for(int32 Index = 0; Index < SkelMeshRenderData->LODRenderData.Num(); ++Index)
		{
			if (!NeedCPUData(Index))
			{
				SkelMeshRenderData->LODRenderData[Index].ReleaseCPUResources();
			}
		}
	}
}

/** Allocate and initialise bone mirroring table for this skeletal mesh. Default is source = destination for each bone. */
void USkeletalMesh::InitBoneMirrorInfo()
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	TArray<FBoneMirrorInfo>& LocalSkelMirrorTable = GetSkelMirrorTable();
	LocalSkelMirrorTable.Empty(GetRefSkeleton().GetNum());
	LocalSkelMirrorTable.AddZeroed(GetRefSkeleton().GetNum());

	// By default, no bone mirroring, and source is ourself.
	for(int32 i=0; i< LocalSkelMirrorTable.Num(); i++)
	{
		LocalSkelMirrorTable[i].SourceIndex = i;
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

/** Utility for copying and converting a mirroring table from another SkeletalMesh. */
void USkeletalMesh::CopyMirrorTableFrom(USkeletalMesh* SrcMesh)
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	TArray<FBoneMirrorInfo>& SrcSkelMirrorTable = SrcMesh->GetSkelMirrorTable();
	TArray<FBoneMirrorInfo>& LocalSkelMirrorTable = GetSkelMirrorTable();
	// Do nothing if no mirror table in source mesh
	if(SrcSkelMirrorTable.Num() == 0)
	{
		return;
	}

	// First, allocate and default mirroring table.
	InitBoneMirrorInfo();

	// Keep track of which entries in the source we have already copied
	TArray<bool> EntryCopied;
	EntryCopied.AddZeroed(SrcSkelMirrorTable.Num() );

	// Mirror table must always be size of ref skeleton.
	check(SrcSkelMirrorTable.Num() == SrcMesh->GetRefSkeleton().GetNum());

	// Iterate over each entry in the source mesh mirror table.
	// We assume that the src table is correct, and don't check for errors here (ie two bones using the same one as source).
	for(int32 i=0; i< SrcSkelMirrorTable.Num(); i++)
	{
		if(!EntryCopied[i])
		{
			// Get name of source and dest bone for this entry in the source table.
			FName DestBoneName = SrcMesh->GetRefSkeleton().GetBoneName(i);
			int32 SrcBoneIndex = SrcSkelMirrorTable[i].SourceIndex;
			FName SrcBoneName = SrcMesh->GetRefSkeleton().GetBoneName(SrcBoneIndex);
			EAxis::Type FlipAxis = SrcSkelMirrorTable[i].BoneFlipAxis;

			// Look up bone names in target mesh (this one)
			int32 DestBoneIndexTarget = GetRefSkeleton().FindBoneIndex(DestBoneName);
			int32 SrcBoneIndexTarget = GetRefSkeleton().FindBoneIndex(SrcBoneName);

			// If both bones found, copy data to this mesh's mirror table.
			if( DestBoneIndexTarget != INDEX_NONE && SrcBoneIndexTarget != INDEX_NONE )
			{
				LocalSkelMirrorTable[DestBoneIndexTarget].SourceIndex = SrcBoneIndexTarget;
				LocalSkelMirrorTable[DestBoneIndexTarget].BoneFlipAxis = FlipAxis;


				LocalSkelMirrorTable[SrcBoneIndexTarget].SourceIndex = DestBoneIndexTarget;
				LocalSkelMirrorTable[SrcBoneIndexTarget].BoneFlipAxis = FlipAxis;

				// Flag entries as copied, so we don't try and do it again.
				EntryCopied[i] = true;
				EntryCopied[SrcBoneIndex] = true;
			}
		}
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
/** Utility for copying and converting a mirroring table from another SkeletalMesh. */
void USkeletalMesh::ExportMirrorTable(TArray<FBoneMirrorExport> &MirrorExportInfo) const
{
	const TArray<FBoneMirrorInfo>& LocalSkelMirrorTable = GetSkelMirrorTable();
	// Do nothing if no mirror table in source mesh
	if(LocalSkelMirrorTable.Num() == 0 )
	{
		return;
	}
	
	// Mirror table must always be size of ref skeleton.
	check(LocalSkelMirrorTable.Num() == GetRefSkeleton().GetNum());

	MirrorExportInfo.Empty(LocalSkelMirrorTable.Num());
	MirrorExportInfo.AddZeroed(LocalSkelMirrorTable.Num());

	// Iterate over each entry in the source mesh mirror table.
	// We assume that the src table is correct, and don't check for errors here (ie two bones using the same one as source).
	for(int32 i=0; i< LocalSkelMirrorTable.Num(); i++)
	{
		MirrorExportInfo[i].BoneName		= GetRefSkeleton().GetBoneName(i);
		MirrorExportInfo[i].SourceBoneName	= GetRefSkeleton().GetBoneName(LocalSkelMirrorTable[i].SourceIndex);
		MirrorExportInfo[i].BoneFlipAxis	= LocalSkelMirrorTable[i].BoneFlipAxis;
	}
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

PRAGMA_DISABLE_DEPRECATION_WARNINGS
/** Utility for copying and converting a mirroring table from another SkeletalMesh. */
void USkeletalMesh::ImportMirrorTable(const TArray<FBoneMirrorExport> &MirrorExportInfo)
{

	// Do nothing if no mirror table in source mesh
	if( MirrorExportInfo.Num() == 0 )
	{
		return;
	}

	// First, allocate and default mirroring table.
	InitBoneMirrorInfo();

	// Keep track of which entries in the source we have already copied
	TArray<bool> EntryCopied;
	EntryCopied.AddZeroed(GetRefSkeleton().GetNum() );

	TArray<FBoneMirrorInfo>& LocalSkelMirrorTable = GetSkelMirrorTable();
	// Mirror table must always be size of ref skeleton.
	check(LocalSkelMirrorTable.Num() == GetRefSkeleton().GetNum());

	// Iterate over each entry in the source mesh mirror table.
	// We assume that the src table is correct, and don't check for errors here (ie two bones using the same one as source).
	for(int32 i=0; i<MirrorExportInfo.Num(); i++)
	{
		FName DestBoneName	= MirrorExportInfo[i].BoneName;
		int32 DestBoneIndex	= GetRefSkeleton().FindBoneIndex(DestBoneName);

		if( DestBoneIndex != INDEX_NONE && !EntryCopied[DestBoneIndex] )
		{
			FName SrcBoneName	= MirrorExportInfo[i].SourceBoneName;
			int32 SrcBoneIndex	= GetRefSkeleton().FindBoneIndex(SrcBoneName);
			EAxis::Type FlipAxis		= MirrorExportInfo[i].BoneFlipAxis;

			// If both bones found, copy data to this mesh's mirror table.
			if( SrcBoneIndex != INDEX_NONE )
			{
				LocalSkelMirrorTable[DestBoneIndex].SourceIndex = SrcBoneIndex;
				LocalSkelMirrorTable[DestBoneIndex].BoneFlipAxis = FlipAxis;

				LocalSkelMirrorTable[SrcBoneIndex].SourceIndex = DestBoneIndex;
				LocalSkelMirrorTable[SrcBoneIndex].BoneFlipAxis = FlipAxis;

				// Flag entries as copied, so we don't try and do it again.
				EntryCopied[DestBoneIndex]	= true;
				EntryCopied[SrcBoneIndex]	= true;
			}
		}
	}
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

/** 
 *	Utility for checking that the bone mirroring table of this mesh is good.
 *	Return true if mirror table is OK, false if there are problems.
 *	@param	ProblemBones	Output string containing information on bones that are currently bad.
 */
bool USkeletalMesh::MirrorTableIsGood(FString& ProblemBones) const
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	TArray<int32>	BadBoneMirror;

	const TArray<FBoneMirrorInfo>& LocalSkelMirrorTable = GetSkelMirrorTable();

	for(int32 i=0; i< LocalSkelMirrorTable.Num(); i++)
	{
		int32 SrcIndex = LocalSkelMirrorTable[i].SourceIndex;
		if(LocalSkelMirrorTable[SrcIndex].SourceIndex != i)
		{
			BadBoneMirror.Add(i);
		}
	}

	if(BadBoneMirror.Num() > 0)
	{
		for(int32 i=0; i<BadBoneMirror.Num(); i++)
		{
			int32 BoneIndex = BadBoneMirror[i];
			FName BoneName = GetRefSkeleton().GetBoneName(BoneIndex);

			ProblemBones += FString::Printf( TEXT("%s (%d)\n"), *BoneName.ToString(), BoneIndex );
		}

		return false;
	}
	else
	{
		return true;
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void USkeletalMesh::CreateBodySetup()
{
	const USkeletalMesh* ConstThis = this;
	if (ConstThis->GetBodySetup() == nullptr)
	{
		UBodySetup* NewBodySetup = NewObject<UBodySetup>(this);
		NewBodySetup->bSharedCookedData = true;
		NewBodySetup->AddToCluster(this);
		SetBodySetup(NewBodySetup);
	}
}

#if WITH_EDITOR
void USkeletalMesh::BuildPhysicsData()
{
	CreateBodySetup();
	const USkeletalMesh* ConstThis = this;
	UBodySetup* LocalBodySetup = ConstThis->GetBodySetup();
	LocalBodySetup->CookedFormatData.FlushData();	//we need to force a re-cook because we're essentially re-creating the bodysetup so that it swaps whether or not it has a trimesh
	LocalBodySetup->InvalidatePhysicsData();
	LocalBodySetup->CreatePhysicsMeshes();
}
#endif

bool USkeletalMesh::ContainsPhysicsTriMeshData(bool InUseAllTriData) const
{
	return GetEnablePerPolyCollision();
}

bool USkeletalMesh::GetTriMeshSizeEstimates(struct FTriMeshCollisionDataEstimates& OutTriMeshEstimates, bool bInUseAllTriData) const
{
#if WITH_EDITORONLY_DATA
	if (GetResourceForRendering() && GetEnablePerPolyCollision())
	{
		OutTriMeshEstimates.VerticeCount = GetResourceForRendering()->LODRenderData[0].GetNumVertices();
	}
#endif // #if WITH_EDITORONLY_DATA

	return true;
}

bool USkeletalMesh::GetPhysicsTriMeshData(FTriMeshCollisionData* CollisionData, bool bInUseAllTriData)
{
#if WITH_EDITORONLY_DATA

	// Fail if no mesh or not per poly collision
	if (!GetResourceForRendering() || !GetEnablePerPolyCollision())
	{
		return false;
	}

	FSkeletalMeshRenderData* SkelMeshRenderData = GetResourceForRendering();
	const FSkeletalMeshLODRenderData& LODData = SkelMeshRenderData->LODRenderData[0];

	const TArray<int32>* MaterialMapPtr = nullptr;
	if (const FSkeletalMeshLODInfo* LODZeroInfo = GetLODInfo(0))
	{
		MaterialMapPtr = &LODZeroInfo->LODMaterialMap;
	}
	// Copy all verts into collision vertex buffer.
	CollisionData->Vertices.Empty();
	CollisionData->Vertices.AddUninitialized(LODData.GetNumVertices());

	for (uint32 VertIdx = 0; VertIdx < LODData.GetNumVertices(); ++VertIdx)
	{
		CollisionData->Vertices[VertIdx] = LODData.StaticVertexBuffers.PositionVertexBuffer.VertexPosition(VertIdx);
	}

	{
		// Copy indices into collision index buffer
		const FMultiSizeIndexContainer& IndexBufferContainer = LODData.MultiSizeIndexContainer;

		TArray<uint32> Indices;
		IndexBufferContainer.GetIndexBuffer(Indices);

		const uint32 NumTris = Indices.Num() / 3;
		CollisionData->Indices.Empty();
		CollisionData->Indices.Reserve(NumTris);

		FTriIndices TriIndex;
		for (int32 SectionIndex = 0; SectionIndex < LODData.RenderSections.Num(); ++SectionIndex)
		{
			const FSkelMeshRenderSection& Section = LODData.RenderSections[SectionIndex];
			const uint32 OnePastLastIndex = Section.BaseIndex + Section.NumTriangles * 3;
			uint16 MaterialIndex = Section.MaterialIndex;
			if (MaterialMapPtr)
			{
				if (MaterialMapPtr->IsValidIndex(SectionIndex))
				{
					const uint16 RemapMaterialIndex = static_cast<uint16>((*MaterialMapPtr)[SectionIndex]);
					if (GetMaterials().IsValidIndex(RemapMaterialIndex))
					{
						MaterialIndex = RemapMaterialIndex;
					}
				}
			}

			for (uint32 i = Section.BaseIndex; i < OnePastLastIndex; i += 3)
			{
				TriIndex.v0 = Indices[i];
				TriIndex.v1 = Indices[i + 1];
				TriIndex.v2 = Indices[i + 2];

				CollisionData->Indices.Add(TriIndex);
				CollisionData->MaterialIndices.Add(MaterialIndex);
			}
		}
	}

	CollisionData->bFlipNormals = true;
	CollisionData->bDeformableMesh = true;

	// We only have a valid TriMesh if the CollisionData has vertices AND indices. For meshes with disabled section collision, it
	// can happen that the indices will be empty, in which case we do not want to consider that as valid trimesh data
	return CollisionData->Vertices.Num() > 0 && CollisionData->Indices.Num() > 0;
#else // #if WITH_EDITORONLY_DATA
	return false;
#endif // #if WITH_EDITORONLY_DATA
}

void USkeletalMesh::AddAssetUserData(UAssetUserData* InUserData)
{
	if (InUserData != NULL)
	{
		UAssetUserData* ExistingData = GetAssetUserDataOfClass(InUserData->GetClass());
		if (ExistingData != NULL)
		{
			AssetUserData.Remove(ExistingData);
		}
		AssetUserData.Add(InUserData);
	}
}

UAssetUserData* USkeletalMesh::GetAssetUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass)
{
	for (int32 DataIdx = 0; DataIdx < AssetUserData.Num(); DataIdx++)
	{
		UAssetUserData* Datum = AssetUserData[DataIdx];
		if (Datum != NULL && Datum->IsA(InUserDataClass))
		{
			return Datum;
		}
	}
	return NULL;
}

void USkeletalMesh::RemoveUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass)
{
	for (int32 DataIdx = 0; DataIdx < AssetUserData.Num(); DataIdx++)
	{
		UAssetUserData* Datum = AssetUserData[DataIdx];
		if (Datum != NULL && Datum->IsA(InUserDataClass))
		{
			AssetUserData.RemoveAt(DataIdx);
			return;
		}
	}
}

const TArray<UAssetUserData*>* USkeletalMesh::GetAssetUserDataArray() const
{
	return &ToRawPtrTArrayUnsafe(AssetUserData);
}

////// SKELETAL MESH THUMBNAIL SUPPORT ////////

/** 
 * Returns a one line description of an object for viewing in the thumbnail view of the generic browser
 */
FString USkeletalMesh::GetDesc()
{
	FString DescString;

	FSkeletalMeshRenderData* Resource = GetResourceForRendering();
	if (Resource)
	{
		check(Resource->LODRenderData.Num() > 0);
		DescString = FString::Printf(TEXT("%d Triangles, %d Bones"), Resource->LODRenderData[0].GetTotalFaces(), GetRefSkeleton().GetRawBoneNum());
	}
	return DescString;
}

bool USkeletalMesh::IsSectionUsingCloth(int32 InSectionIndex, bool bCheckCorrespondingSections) const
{
	FSkeletalMeshRenderData* SkelMeshRenderData = GetResourceForRendering();
	if(SkelMeshRenderData)
	{
		for (FSkeletalMeshLODRenderData& LodData : SkelMeshRenderData->LODRenderData)
		{
			if(LodData.RenderSections.IsValidIndex(InSectionIndex))
			{
				FSkelMeshRenderSection* SectionToCheck = &LodData.RenderSections[InSectionIndex];
				return SectionToCheck->HasClothingData();
			}
		}
	}

	return false;
}

#if WITH_EDITOR
void USkeletalMesh::AddBoneToReductionSetting(int32 LODIndex, const TArray<FName>& BoneNames)
{
	TArray<FSkeletalMeshLODInfo>& LODInfoArray = GetLODInfoArray();
	if (LODInfoArray.IsValidIndex(LODIndex))
	{
		for (auto& BoneName : BoneNames)
		{
			LODInfoArray[LODIndex].BonesToRemove.AddUnique(BoneName);
		}
	}
}
void USkeletalMesh::AddBoneToReductionSetting(int32 LODIndex, FName BoneName)
{
	TArray<FSkeletalMeshLODInfo>& LODInfoArray = GetLODInfoArray();
	if (LODInfoArray.IsValidIndex(LODIndex))
	{
		LODInfoArray[LODIndex].BonesToRemove.AddUnique(BoneName);
	}
}
#endif // WITH_EDITOR

#if WITH_EDITORONLY_DATA
void USkeletalMesh::ConvertLegacyLODScreenSize()
{
	TArray<FSkeletalMeshLODInfo>& LODInfoArray = GetLODInfoArray();
	if (LODInfoArray.Num() == 1)
	{
		// Only one LOD
		LODInfoArray[0].ScreenSize = 1.0f;
	}
	else
	{
		// Use 1080p, 90 degree FOV as a default, as this should not cause runtime regressions in the common case.
		// LODs will appear different in Persona, however.
		const float HalfFOV = UE_PI * 0.25f;
		const float ScreenWidth = 1920.0f;
		const float ScreenHeight = 1080.0f;
		const FPerspectiveMatrix ProjMatrix(HalfFOV, ScreenWidth, ScreenHeight, 1.0f);
		FBoxSphereBounds Bounds = GetBounds();

		// Multiple models, we should have LOD screen area data.
		for (int32 LODIndex = 0; LODIndex < LODInfoArray.Num(); ++LODIndex)
		{
			FSkeletalMeshLODInfo& LODInfoEntry = LODInfoArray[LODIndex];

			if (GetRequiresLODScreenSizeConversion())
			{
				if (LODInfoEntry.ScreenSize.Default == 0.0f)
				{
					LODInfoEntry.ScreenSize.Default = 1.0f;
				}
				else
				{
					// legacy screen size was scaled by a fixed constant of 320.0f, so its kinda arbitrary. Convert back to distance based metric first.
					const float ScreenDepth = FMath::Max(ScreenWidth / 2.0f * ProjMatrix.M[0][0], ScreenHeight / 2.0f * ProjMatrix.M[1][1]) * Bounds.SphereRadius / (LODInfoEntry.ScreenSize.Default * 320.0f);

					// Now convert using the query function
					LODInfoEntry.ScreenSize.Default = ComputeBoundsScreenSize(FVector::ZeroVector, Bounds.SphereRadius, FVector(0.0f, 0.0f, ScreenDepth), ProjMatrix);
				}
			}

			if (GetRequiresLODHysteresisConversion())
			{
				if (LODInfoEntry.LODHysteresis != 0.0f)
				{
					// Also convert the hysteresis as if it was a screen size topo
					const float ScreenHysteresisDepth = FMath::Max(ScreenWidth / 2.0f * ProjMatrix.M[0][0], ScreenHeight / 2.0f * ProjMatrix.M[1][1]) * Bounds.SphereRadius / (LODInfoEntry.LODHysteresis * 320.0f);
					LODInfoEntry.LODHysteresis = ComputeBoundsScreenSize(FVector::ZeroVector, Bounds.SphereRadius, FVector(0.0f, 0.0f, ScreenHysteresisDepth), ProjMatrix);
				}
			}
		}
	}
}
#endif

class UNodeMappingContainer* USkeletalMesh::GetNodeMappingContainer(class UBlueprint* SourceAsset) const
{
	const TArray<UNodeMappingContainer*>& LocalNodeMappingData = GetNodeMappingData();
	for (int32 Index = 0; Index < LocalNodeMappingData.Num(); ++Index)
	{
		UNodeMappingContainer* Iter = LocalNodeMappingData[Index];
		if (Iter && Iter->GetSourceAssetSoftObjectPtr() == TSoftObjectPtr<UObject>(SourceAsset))
		{
			return Iter;
		}
	}

	return nullptr;
}

const UAnimSequence* USkeletalMesh::GetBakePose(int32 LODIndex) const
{
	const FSkeletalMeshLODInfo* LOD = GetLODInfo(LODIndex);
	if (LOD)
	{
		if (LOD->BakePoseOverride && GetSkeleton() && GetSkeleton() == LOD->BakePoseOverride->GetSkeleton())
		{
			return LOD->BakePoseOverride;
		}

		// we make sure bake pose uses same skeleton
		if (LOD->BakePose && GetSkeleton() && GetSkeleton() == LOD->BakePose->GetSkeleton())
		{
			return LOD->BakePose;
		}
	}

	return nullptr;
}

const USkeletalMeshLODSettings* USkeletalMesh::GetDefaultLODSetting() const
{ 
#if WITH_EDITORONLY_DATA
	if (GetLODSettings())
	{
		return GetLODSettings();
	}
#endif // WITH_EDITORONLY_DATA

	return GetDefault<USkeletalMeshLODSettings>();
}

bool USkeletalMesh::IsMaterialUsed(int32 MaterialIndex) const
{
	if (GIsEditor || !CVarSkeletalMeshLODMaterialReference.GetValueOnAnyThread())
	{
		return true;
	}

	const FSkeletalMeshRenderData* RenderData = GetSkeletalMeshRenderData();

	if (RenderData)
	{
		for (int32 LODIndex = 0; LODIndex < RenderData->LODRenderData.Num(); ++LODIndex)
		{
			const FSkeletalMeshLODRenderData& LODData = RenderData->LODRenderData[LODIndex];

			if (LODData.BuffersSize > 0)
			{
				const TArray<int32>& RemappedMaterialIndices = GetLODInfoArray()[LODIndex].LODMaterialMap;

				for (int32 SectionIndex = 0; SectionIndex < LODData.RenderSections.Num(); ++SectionIndex)
				{
					const FSkelMeshRenderSection& Section = LODData.RenderSections[SectionIndex];
					const int32 UsedMaterialIndex =
						SectionIndex < RemappedMaterialIndices.Num() && GetMaterials().IsValidIndex(RemappedMaterialIndices[SectionIndex]) ?
						RemappedMaterialIndices[SectionIndex] :
						Section.MaterialIndex;
					
					if (UsedMaterialIndex == MaterialIndex)
					{
						return true;
					}
				}
			}
		}
	}

	return false;
}

void USkeletalMesh::ReleaseSkinWeightProfileResources()
{
	// This assumes that skin weights buffers are not used anywhere
	if (FSkeletalMeshRenderData* RenderData = GetResourceForRendering())
	{
		for (FSkeletalMeshLODRenderData& LODData : RenderData->LODRenderData)
		{
			LODData.SkinWeightProfilesData.ReleaseResources();
		}
	}
}

FSkeletalMeshLODInfo& USkeletalMesh::AddLODInfo()
{
	TArray<FSkeletalMeshLODInfo>& LODInfoArray = GetLODInfoArray();
	int32 NewIndex = LODInfoArray.AddDefaulted(1);

	check(NewIndex != INDEX_NONE);

	const USkeletalMeshLODSettings* DefaultSetting = GetDefaultLODSetting();
	// if failed to get setting, that means, we don't have proper setting 
	// in that case, use last index setting
	if (!DefaultSetting->SetLODSettingsToMesh(this, NewIndex))
	{
		FSkeletalMeshLODInfo& NewLODInfo = LODInfoArray[NewIndex];
		if (NewIndex > 0)
		{
			// copy previous copy
			const int32 LastIndex = NewIndex - 1;
			NewLODInfo.ScreenSize.Default = LODInfoArray[LastIndex].ScreenSize.Default * 0.5f;
			NewLODInfo.LODHysteresis = LODInfoArray[LastIndex].LODHysteresis;
			NewLODInfo.BakePose = LODInfoArray[LastIndex].BakePose;
			NewLODInfo.BakePoseOverride = LODInfoArray[LastIndex].BakePoseOverride;
			NewLODInfo.BonesToRemove = LODInfoArray[LastIndex].BonesToRemove;
			NewLODInfo.BonesToPrioritize = LODInfoArray[LastIndex].BonesToPrioritize;
			NewLODInfo.SectionsToPrioritize = LODInfoArray[LastIndex].SectionsToPrioritize;
			// now find reduction setting
			for (int32 SubLOD = LastIndex; SubLOD >= 0; --SubLOD)
			{
				if (LODInfoArray[SubLOD].bHasBeenSimplified)
				{
					// copy from previous index of LOD info reduction setting
					// this may not match with previous copy - as we're only looking for simplified version
					NewLODInfo.ReductionSettings = LODInfoArray[SubLOD].ReductionSettings;
					// and make it 50 % of that
					NewLODInfo.ReductionSettings.NumOfTrianglesPercentage = FMath::Clamp(NewLODInfo.ReductionSettings.NumOfTrianglesPercentage * 0.5f, 0.f, 1.f);
					// increase maxdeviation, 1.5 is random number
					NewLODInfo.ReductionSettings.MaxDeviationPercentage = FMath::Clamp(NewLODInfo.ReductionSettings.MaxDeviationPercentage * 1.5f, 0.f, 1.f);
					break;
				}
			}

		}
		// if this is the first LOD, then just use default setting of the struct
	}

	return LODInfoArray[NewIndex];
}

void USkeletalMesh::RemoveLODInfo(int32 Index)
{
	TArray<FSkeletalMeshLODInfo>& LODInfoArray = GetLODInfoArray();
	if (LODInfoArray.IsValidIndex(Index))
	{
#if WITH_EDITOR
		if (IsMeshEditorDataValid())
		{
			GetMeshEditorDataObject()->RemoveLODImportedData(Index);
		}
		
		if (GetImportedModel()->InlineReductionCacheDatas.IsValidIndex(Index))
		{
			GetImportedModel()->InlineReductionCacheDatas.RemoveAt(Index);
		}
#endif // WITH_EDITOR
		LODInfoArray.RemoveAt(Index);
	}
}

void USkeletalMesh::ResetLODInfo()
{
	GetLODInfoArray().Reset();
}

#if WITH_EDITOR
bool USkeletalMesh::GetEnableLODStreaming(const ITargetPlatform* TargetPlatform) const
{
	if (NeverStream)
	{
		return false;
	}

	static auto* VarMeshStreaming = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MeshStreaming"));
	if (VarMeshStreaming && VarMeshStreaming->GetInt() == 0)
	{
		return false;

	}

	check(TargetPlatform);
	// Check whether the target platforms support LOD streaming. 
	// Even if it does, disable streaming if it has editor only data since most tools don't support mesh streaming.
	if (!TargetPlatform->SupportsFeature(ETargetPlatformFeatures::MeshLODStreaming) || TargetPlatform->HasEditorOnlyData())
	{
		return false;
	}

	if (GetOverrideLODStreamingSettings())
	{
		return GetSupportLODStreaming().GetValueForPlatform(*TargetPlatform->IniPlatformName());
	}
	else
	{
		return GetDefault<URendererSettings>()->bStreamSkeletalMeshLODs.GetValueForPlatform(*TargetPlatform->IniPlatformName());
	}
}

int32 USkeletalMesh::GetMaxNumStreamedLODs(const ITargetPlatform* TargetPlatform) const
{
	check(TargetPlatform);
	if (GetOverrideLODStreamingSettings())
	{
		return GetMaxNumStreamedLODs().GetValueForPlatform(*TargetPlatform->IniPlatformName());
	}
	else
	{
		return MAX_MESH_LOD_COUNT;
	}
}

int32 USkeletalMesh::GetMaxNumOptionalLODs(const ITargetPlatform* TargetPlatform) const
{
	check(TargetPlatform);
	if (GetOverrideLODStreamingSettings())
	{
		return GetMaxNumOptionalLODs().GetValueForPlatform(*TargetPlatform->IniPlatformName()) <= 0 ? 0 : MAX_MESH_LOD_COUNT;
	}
	else
	{
		return GetDefault<URendererSettings>()->bDiscardSkeletalMeshOptionalLODs.GetValueForPlatform(*TargetPlatform->IniPlatformName()) ? 0 : MAX_MESH_LOD_COUNT;
	}
}

void USkeletalMesh::BuildLODModel(const ITargetPlatform* TargetPlatform, int32 LODIndex)
{
	FSkeletalMeshModel* SkelMeshModel = GetImportedModel();
	check(SkelMeshModel);

	FSkeletalMeshLODInfo* LODInfoPtr = GetLODInfo(LODIndex);
	check(LODInfoPtr);

	//We want to avoid building a LOD if the LOD was generated from a previous LODIndex.
	const bool bIsGeneratedLodNotInline = (LODInfoPtr->bHasBeenSimplified && IsReductionActive(LODIndex) && GetReductionSettings(LODIndex).BaseLOD < LODIndex);

	//Make sure the LOD have all the data needed to be build
	const bool bRawDataEmpty = IsLODImportedDataEmpty(LODIndex);
	const bool bRawBuildDataAvailable = IsLODImportedDataBuildAvailable(LODIndex);

	//Build the source model before the render data, if we are a purely generated LOD we do not need to be build
	IMeshBuilderModule& MeshBuilderModule = IMeshBuilderModule::GetForPlatform(TargetPlatform);
	if (!bIsGeneratedLodNotInline && !bRawDataEmpty && bRawBuildDataAvailable)
	{
		LODInfoPtr->bHasBeenSimplified = false;
		const bool bRegenDepLODs = true;
		FSkeletalMeshBuildParameters BuildParameters(this, TargetPlatform, LODIndex, bRegenDepLODs);
		MeshBuilderModule.BuildSkeletalMesh(BuildParameters);
	}
	else
	{
		//We need to synchronize when we are generated mesh or if we have load an old asset that was not re-imported
		SkelMeshModel->LODModels[LODIndex].SyncronizeUserSectionsDataArray();
	}
}
#endif

void USkeletalMesh::SetLODSettings(USkeletalMeshLODSettings* InLODSettings)
{
	WaitUntilAsyncPropertyReleased(ESkeletalMeshAsyncProperties::LODSettings);
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
#if WITH_EDITORONLY_DATA
	LODSettings = InLODSettings;
	if (LODSettings)
	{
		LODSettings->SetLODSettingsToMesh(this);
	}
#endif // WITH_EDITORONLY_DATA
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void USkeletalMesh::SetDefaultAnimatingRig(TSoftObjectPtr<UObject> InAnimatingRig)
{
	WaitUntilAsyncPropertyReleased(ESkeletalMeshAsyncProperties::DefaultAnimationRig);
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
#if WITH_EDITORONLY_DATA
	DefaultAnimatingRig = InAnimatingRig;
#endif // WITH_EDITORONLY_DATA
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

TSoftObjectPtr<UObject> USkeletalMesh::GetDefaultAnimatingRig() const
{
	WaitUntilAsyncPropertyReleased(ESkeletalMeshAsyncProperties::DefaultAnimationRig);
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
#if WITH_EDITORONLY_DATA
	return DefaultAnimatingRig;
#else // WITH_EDITORONLY_DATA
	return nullptr;
#endif // WITH_EDITORONLY_DATA
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void USkeletalMesh::GetMappableNodeData(TArray<FName>& OutNames, TArray<FNodeItem>& OutNodeItems) const
{
	TArray<FTransform> ComponentSpaceRefPose;
#if WITH_EDITORONLY_DATA
	FAnimationRuntime::FillUpComponentSpaceTransformsRetargetBasePose(this, ComponentSpaceRefPose);
#else
	// hasn't tested this route, but we don't have retarget base pose if not editor, wonder we should to non-editor soon
	ensure(false);
	FAnimationRuntime::FillUpComponentSpaceTransforms(GetRefSkeleton(), GetRefSkeleton().GetRefBonePose(), ComponentSpaceRefPose);
#endif // 

	const int32 NumJoint = GetRefSkeleton().GetNum();
	// allocate buffer
	OutNames.Reset(NumJoint);
	OutNodeItems.Reset(NumJoint);

	if (NumJoint > 0)
	{
		OutNames.AddDefaulted(NumJoint);
		OutNodeItems.AddDefaulted(NumJoint);

		const TArray<FMeshBoneInfo> MeshBoneInfo = GetRefSkeleton().GetRefBoneInfo();
		for (int32 NodeIndex = 0; NodeIndex < NumJoint; ++NodeIndex)
		{
			OutNames[NodeIndex] = MeshBoneInfo[NodeIndex].Name;
			if (MeshBoneInfo[NodeIndex].ParentIndex != INDEX_NONE)
			{
				OutNodeItems[NodeIndex] = FNodeItem(MeshBoneInfo[MeshBoneInfo[NodeIndex].ParentIndex].Name, ComponentSpaceRefPose[NodeIndex]);
			}
			else
			{
				OutNodeItems[NodeIndex] = FNodeItem(NAME_None, ComponentSpaceRefPose[NodeIndex]);
			}
		}
	}
}

#if WITH_EDITORONLY_DATA
FText USkeletalMesh::GetSourceFileLabelFromIndex(int32 SourceFileIndex)
{
	int32 RealSourceFileIndex = SourceFileIndex == INDEX_NONE ? 0 : SourceFileIndex;
	return RealSourceFileIndex == 0 ? NSSkeletalMeshSourceFileLabels::GeoAndSkinningText() : RealSourceFileIndex == 1 ? NSSkeletalMeshSourceFileLabels::GeometryText() : NSSkeletalMeshSourceFileLabels::SkinningText();
}

#endif //WITH_EDITORONLY_DATA


TArray<FString> USkeletalMesh::K2_GetAllMorphTargetNames() const
{
	TArray<FString> Names;
	for (UMorphTarget* MorphTarget : GetMorphTargets())
	{
		Names.Add(MorphTarget->GetFName().ToString());
	}
	return Names;
}

int32 USkeletalMesh::GetMinLodIdx(bool bForceLowestLODIdx) const
{
	if (IsMinLodQualityLevelEnable())
	{
		return bForceLowestLODIdx ? GetQualityLevelMinLod().GetLowestValue() : GetQualityLevelMinLod().GetValue(GSkeletalMeshMinLodQualityLevel);
	}
	else
	{
		return GetMinLod().GetValue();
	}
}

int32 USkeletalMesh::GetDefaultMinLod() const
{
	if (IsMinLodQualityLevelEnable())
	{
		return GetQualityLevelMinLod().Default;
	}
	else
	{
		return GetMinLod().Default;
	}
}

void USkeletalMesh::SetMinLodIdx(int32 InMinLOD)
{
	if (IsMinLodQualityLevelEnable())
	{
		SetQualityLevelMinLod(InMinLOD);
	}
	else
	{
		SetMinLod(InMinLOD);
	}
}

bool USkeletalMesh::IsMinLodQualityLevelEnable() const
{
	return (GEngine && GEngine->UseSkeletalMeshMinLODPerQualityLevels);
}

int32 USkeletalMesh::GetPlatformMinLODIdx(const ITargetPlatform* TargetPlatform) const
{
#if WITH_EDITOR
	check(TargetPlatform);

	if (IsMinLodQualityLevelEnable())
	{
		// get all supported quality level from scalability + engine ini files
		return GetQualityLevelMinLod().GetValueForPlatform(TargetPlatform);
	}
	else
	{
		return GetMinLod().GetValueForPlatform(*TargetPlatform->IniPlatformName());
	}
#else
	return 0;
#endif
}

void USkeletalMesh::SetSkinWeightProfilesData(int32 LODIndex, FSkinWeightProfilesData& SkinWeightProfilesData)
{
#if !WITH_EDITOR
	if (GSkinWeightProfilesLoadByDefaultMode == 1)
	{
		// Only allow overriding the base buffer in non-editor builds as it could otherwise be serialized into the asset
		SkinWeightProfilesData.OverrideBaseBufferSkinWeightData(this, LODIndex);
	}
	else
#endif
	if (GSkinWeightProfilesLoadByDefaultMode == 3)
	{
		SkinWeightProfilesData.SetDynamicDefaultSkinWeightProfile(this, LODIndex, true);
	}
}

void USkeletalMesh::OnLodStrippingQualityLevelChanged(IConsoleVariable* Variable) {
#if WITH_EDITOR || PLATFORM_DESKTOP
	if (GEngine && GEngine->UseSkeletalMeshMinLODPerQualityLevels)
	{
		for (TObjectIterator<USkeletalMesh> It; It; ++It)
		{
			USkeletalMesh* SkeletalMesh = *It;
			if (SkeletalMesh && SkeletalMesh->GetQualityLevelMinLod().PerQuality.Num() > 0)
			{
				FSkinnedMeshComponentRecreateRenderStateContext Context(SkeletalMesh, false);
			}
		}
	}
#endif
}

void USkeletalMesh::WaitUntilAsyncPropertyReleased(ESkeletalMeshAsyncProperties AsyncProperties, ESkinnedAssetAsyncPropertyLockType LockType) const
{
	// Cast strongly-typed enum to uint64
	WaitUntilAsyncPropertyReleasedInternal((uint64)AsyncProperties, LockType);
}

FString USkeletalMesh::GetAsyncPropertyName(uint64 Property) const
{
	return StaticEnum<ESkeletalMeshAsyncProperties>()->GetNameByValue(Property).ToString();
}

/*-----------------------------------------------------------------------------
USkeletalMeshSocket
-----------------------------------------------------------------------------*/
USkeletalMeshSocket::USkeletalMeshSocket(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bForceAlwaysAnimated(true)
{
	RelativeScale = FVector(1.0f, 1.0f, 1.0f);
}

void USkeletalMeshSocket::InitializeSocketFromLocation(const class USkeletalMeshComponent* SkelComp, FVector WorldLocation, FVector WorldNormal)
{
	if (ensureAsRuntimeWarning(SkelComp))
	{
		BoneName = SkelComp->FindClosestBone(WorldLocation);
		if (BoneName != NAME_None)
		{
			SkelComp->TransformToBoneSpace(BoneName, WorldLocation, WorldNormal.Rotation(), RelativeLocation, RelativeRotation);
		}
	}
}

FVector USkeletalMeshSocket::GetSocketLocation(const class USkeletalMeshComponent* SkelComp) const
{
	if (ensureAsRuntimeWarning(SkelComp))
	{
		FMatrix SocketMatrix;
		if (GetSocketMatrix(SocketMatrix, SkelComp))
		{
			return SocketMatrix.GetOrigin();
		}

		// Fall back to MeshComp origin, so it's visible in case of failure.
		return SkelComp->GetComponentLocation();
	}
	return FVector(0.f);
}

bool USkeletalMeshSocket::GetSocketMatrix(FMatrix& OutMatrix, const class USkeletalMeshComponent* SkelComp) const
{
	const int32 BoneIndex = SkelComp ? SkelComp->GetBoneIndex(BoneName) : INDEX_NONE;
	if(BoneIndex != INDEX_NONE)
	{
		FMatrix BoneMatrix = SkelComp->GetBoneMatrix(BoneIndex);
		FScaleRotationTranslationMatrix RelSocketMatrix( RelativeScale, RelativeRotation, RelativeLocation );
		OutMatrix = RelSocketMatrix * BoneMatrix;
		return true;
	}

	return false;
}

FTransform USkeletalMeshSocket::GetSocketLocalTransform() const
{
	return FTransform(RelativeRotation, RelativeLocation, RelativeScale);
}

FTransform USkeletalMeshSocket::GetSocketTransform(const class USkeletalMeshComponent* SkelComp) const
{
	FTransform OutTM;

	const int32 BoneIndex = SkelComp ? SkelComp->GetBoneIndex(BoneName) : INDEX_NONE;
	if(BoneIndex != INDEX_NONE)
	{
		FTransform BoneTM = SkelComp->GetBoneTransform(BoneIndex);
		FTransform RelSocketTM( RelativeRotation, RelativeLocation, RelativeScale );
		OutTM = RelSocketTM * BoneTM;
	}

	return OutTM;
}

bool USkeletalMeshSocket::GetSocketMatrixWithOffset(FMatrix& OutMatrix, class USkeletalMeshComponent* SkelComp, const FVector& InOffset, const FRotator& InRotation) const
{
	const int32 BoneIndex = SkelComp ? SkelComp->GetBoneIndex(BoneName) : INDEX_NONE;
	if(BoneIndex != INDEX_NONE)
	{
		FMatrix BoneMatrix = SkelComp->GetBoneMatrix(BoneIndex);
		FScaleRotationTranslationMatrix RelSocketMatrix(RelativeScale, RelativeRotation, RelativeLocation);
		FRotationTranslationMatrix RelOffsetMatrix(InRotation, InOffset);
		OutMatrix = RelOffsetMatrix * RelSocketMatrix * BoneMatrix;
		return true;
	}

	return false;
}


bool USkeletalMeshSocket::GetSocketPositionWithOffset(FVector& OutPosition, class USkeletalMeshComponent* SkelComp, const FVector& InOffset, const FRotator& InRotation) const
{
	const int32 BoneIndex = SkelComp ? SkelComp->GetBoneIndex(BoneName) : INDEX_NONE;
	if(BoneIndex != INDEX_NONE)
	{
		FMatrix BoneMatrix = SkelComp->GetBoneMatrix(BoneIndex);
		FScaleRotationTranslationMatrix RelSocketMatrix(RelativeScale, RelativeRotation, RelativeLocation);
		FRotationTranslationMatrix RelOffsetMatrix(InRotation, InOffset);
		FMatrix SocketMatrix = RelOffsetMatrix * RelSocketMatrix * BoneMatrix;
		OutPosition = SocketMatrix.GetOrigin();
		return true;
	}

	return false;
}

/** 
 *	Utility to associate an actor with a socket
 *	
 *	@param	Actor			The actor to attach to the socket
 *	@param	SkelComp		The skeletal mesh component that the socket comes from
 *
 *	@return	bool			true if successful, false if not
 */
bool USkeletalMeshSocket::AttachActor(AActor* Actor, class USkeletalMeshComponent* SkelComp) const
{
	bool bAttached = false;
	if (ensureAlways(SkelComp))
	{
		// Don't support attaching to own socket
		if ((Actor != SkelComp->GetOwner()) && Actor->GetRootComponent())
		{
			FMatrix SocketTM;
			if (GetSocketMatrix(SocketTM, SkelComp))
			{
				Actor->Modify();

				Actor->SetActorLocation(SocketTM.GetOrigin(), false);
				Actor->SetActorRotation(SocketTM.Rotator());
				Actor->GetRootComponent()->AttachToComponent(SkelComp, FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketName);

	#if WITH_EDITOR
				if (GIsEditor)
				{
					Actor->PreEditChange(NULL);
					Actor->PostEditChange();
				}
	#endif // WITH_EDITOR

				bAttached = true;
			}
		}
	}
	return bAttached;
}

#if WITH_EDITOR
void USkeletalMeshSocket::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property)
	{
		ChangedEvent.Broadcast(this, PropertyChangedEvent.MemberProperty);
	}
}

void USkeletalMeshSocket::CopyFrom(const class USkeletalMeshSocket* OtherSocket)
{
	if (OtherSocket)
	{
		SocketName = OtherSocket->SocketName;
		BoneName = OtherSocket->BoneName;
		RelativeLocation = OtherSocket->RelativeLocation;
		RelativeRotation = OtherSocket->RelativeRotation;
		RelativeScale = OtherSocket->RelativeScale;
		bForceAlwaysAnimated = OtherSocket->bForceAlwaysAnimated;
	}
}

#endif

void USkeletalMeshSocket::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FFrameworkObjectVersion::GUID);

	if(Ar.CustomVer(FFrameworkObjectVersion::GUID) < FFrameworkObjectVersion::MeshSocketScaleUtilization)
	{
		// Set the relative scale to 1.0. As it was not used before this should allow existing data
		// to work as expected.
		RelativeScale = FVector(1.0f, 1.0f, 1.0f);
	}
}


/*-----------------------------------------------------------------------------
FSkeletalMeshSceneProxy
-----------------------------------------------------------------------------*/
#include "Engine/LevelStreaming.h"
#include "LevelUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkeletalMesh)

const FQuat SphylBasis(FVector(1.0f / FMath::Sqrt(2.0f), 0.0f, 1.0f / FMath::Sqrt(2.0f)), UE_PI);

/** 
 * Constructor. 
 * @param	Component - skeletal mesh primitive being added
 */
FSkeletalMeshSceneProxy::FSkeletalMeshSceneProxy(const USkinnedMeshComponent* Component, FSkeletalMeshRenderData* InSkelMeshRenderData)
		:	FPrimitiveSceneProxy(Component, Component->GetSkinnedAsset()->GetFName())
		,	Owner(Component->GetOwner())
		,	MeshObject(Component->MeshObject)
		,	SkeletalMeshRenderData(InSkelMeshRenderData)
		,	SkeletalMeshForDebug(Component->GetSkinnedAsset())
		,	PhysicsAssetForDebug(Component->GetPhysicsAsset())
		,	OverlayMaterial(Component->OverlayMaterial)
		,	OverlayMaterialMaxDrawDistance(Component->OverlayMaterialMaxDrawDistance)
#if RHI_RAYTRACING
		,	bAnySegmentUsesWorldPositionOffset(false)
#endif
		,	bForceWireframe(Component->bForceWireframe)
		,	bCanHighlightSelectedSections(Component->bCanHighlightSelectedSections)
		,	bRenderStatic(Component->bRenderStatic)
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		,	bDrawDebugSkeleton(Component->ShouldDrawDebugSkeleton())
#endif
		,	FeatureLevel(GetScene().GetFeatureLevel())
		,	bMaterialsNeedMorphUsage_GameThread(false)
		,	MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		,	DebugDrawColor(Component->GetDebugDrawColor())
#endif
#if WITH_EDITORONLY_DATA
		,	StreamingDistanceMultiplier(FMath::Max(0.0f, Component->StreamingDistanceMultiplier))
#endif
{
	check(MeshObject);
	check(SkeletalMeshRenderData);
	check(SkeletalMeshForDebug);

#if WITH_EDITOR
	PoseWatchDynamicData = nullptr;
#endif

	// Skeletal meshes DO deform internally, unless bRenderStatic is used to force static mesh behaviour.
	bHasDeformableMesh = !bRenderStatic;

	bIsCPUSkinned = MeshObject->IsCPUSkinned();

	bCastCapsuleDirectShadow = Component->bCastDynamicShadow && Component->CastShadow && Component->bCastCapsuleDirectShadow;
	bCastsDynamicIndirectShadow = Component->bCastDynamicShadow && Component->CastShadow && Component->bCastCapsuleIndirectShadow;

	DynamicIndirectShadowMinVisibility = FMath::Clamp(Component->CapsuleIndirectShadowMinVisibility, 0.0f, 1.0f);

	// Force inset shadows if capsule shadows are requested, as they can't be supported with full scene shadows
	bCastInsetShadow = bCastInsetShadow || bCastCapsuleDirectShadow;

	// Get the pre-skinned local bounds
	Component->GetPreSkinnedLocalBounds(PreSkinnedLocalBounds);

	const USkinnedMeshComponent* SkinnedMeshComponent = Cast<const USkinnedMeshComponent>(Component);
	if(SkinnedMeshComponent && SkinnedMeshComponent->bPerBoneMotionBlur)
	{
		bAlwaysHasVelocity = true;
	}

	// setup materials and performance classification for each LOD.
	extern bool GForceDefaultMaterial;
	bool bCastShadow = Component->CastShadow;
	bool bAnySectionCastsShadow = false;
	LODSections.Reserve(SkeletalMeshRenderData->LODRenderData.Num());
	LODSections.AddZeroed(SkeletalMeshRenderData->LODRenderData.Num());
	for(int32 LODIdx=0; LODIdx < SkeletalMeshRenderData->LODRenderData.Num(); LODIdx++)
	{
		const FSkeletalMeshLODRenderData& LODData = SkeletalMeshRenderData->LODRenderData[LODIdx];
		const FSkeletalMeshLODInfo& Info = *(Component->GetSkinnedAsset()->GetLODInfo(LODIdx));

		FLODSectionElements& LODSection = LODSections[LODIdx];

		// Presize the array
		LODSection.SectionElements.Empty(LODData.RenderSections.Num() );
		for(int32 SectionIndex = 0;SectionIndex < LODData.RenderSections.Num();SectionIndex++)
		{
			const FSkelMeshRenderSection& Section = LODData.RenderSections[SectionIndex];

			// If we are at a dropped LOD, route material index through the LODMaterialMap in the LODInfo struct.
			int32 UseMaterialIndex = Section.MaterialIndex;			
			{
				if(SectionIndex < Info.LODMaterialMap.Num() && Component->GetSkinnedAsset()->IsValidMaterialIndex(Info.LODMaterialMap[SectionIndex]))
				{
					UseMaterialIndex = Info.LODMaterialMap[SectionIndex];
					UseMaterialIndex = FMath::Clamp( UseMaterialIndex, 0, Component->GetSkinnedAsset()->GetNumMaterials());
				}
			}

			// If Section is hidden, do not cast shadow
			bool bSectionHidden = MeshObject->IsMaterialHidden(LODIdx,UseMaterialIndex);

			// If the material is NULL, or isn't flagged for use with skeletal meshes, it will be replaced by the default material.
			UMaterialInterface* Material = Component->GetMaterial(UseMaterialIndex);
			if (GForceDefaultMaterial && Material && !IsTranslucentBlendMode(Material->GetBlendMode()))
			{
				Material = UMaterial::GetDefaultMaterial(MD_Surface);
				MaterialRelevance |= Material->GetRelevance(FeatureLevel);
			}

			// if this is a clothing section, then enabled and will be drawn but the corresponding original section should be disabled
			bool bClothSection = Section.HasClothingData();

			bool bValidUsage = Material && Material->CheckMaterialUsage_Concurrent(MATUSAGE_SkeletalMesh);
			if (bClothSection)
			{
				bValidUsage &= Material && Material->CheckMaterialUsage_Concurrent(MATUSAGE_Clothing);
			}

			if(!Material || !bValidUsage)
			{
				UE_CLOG(Material && !bValidUsage, LogSkeletalMesh, Error,
					TEXT("Material with missing usage flag was applied to skeletal mesh %s"),
					*Component->GetSkinnedAsset()->GetPathName());

				Material = UMaterial::GetDefaultMaterial(MD_Surface);
				MaterialRelevance |= Material->GetRelevance(FeatureLevel);
			}

			bool bSectionCastsShadow = !bSectionHidden && bCastShadow &&
				(Component->GetSkinnedAsset()->IsValidMaterialIndex(UseMaterialIndex) == false || Section.bCastShadow);

			bAnySectionCastsShadow |= bSectionCastsShadow;

#if RHI_RAYTRACING
			bAnySegmentUsesWorldPositionOffset |= MaterialRelevance.bUsesWorldPositionOffset;
#endif

			LODSection.SectionElements.Add(
				FSectionElementInfo(
					Material,
					bSectionCastsShadow,
					UseMaterialIndex
					));
			MaterialsInUse_GameThread.Add(Material);
		}
	}

	if (OverlayMaterial != nullptr)
	{
		if (!OverlayMaterial->CheckMaterialUsage_Concurrent(MATUSAGE_SkeletalMesh))
		{
			OverlayMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
			UE_LOG(LogSkeletalMesh, Error, TEXT("Overlay material with missing usage flag was applied to skeletal mesh %s"),	*Component->GetSkinnedAsset()->GetPathName());
		}
	}

	bCastDynamicShadow = bCastDynamicShadow && bAnySectionCastsShadow;

	// Try to find a color for level coloration.
	if( Owner )
	{
		ULevel* Level = Owner->GetLevel();
		ULevelStreaming* LevelStreaming = FLevelUtils::FindStreamingLevel( Level );
		if ( LevelStreaming )
		{
			SetLevelColor(LevelStreaming->LevelColor);
		}
	}

	// Get a color for property coloration
	FColor NewPropertyColor;
	if (GEngine->GetPropertyColorationColor((UObject*)Component, NewPropertyColor))
	{
		SetPropertyColor(NewPropertyColor);
	}

	// Copy out shadow physics asset data
	if(SkinnedMeshComponent)
	{
		UPhysicsAsset* ShadowPhysicsAsset = SkinnedMeshComponent->GetSkinnedAsset()->GetShadowPhysicsAsset();

		if (ShadowPhysicsAsset
			&& SkinnedMeshComponent->CastShadow
			&& (SkinnedMeshComponent->bCastCapsuleDirectShadow || SkinnedMeshComponent->bCastCapsuleIndirectShadow))
		{
			for (int32 BodyIndex = 0; BodyIndex < ShadowPhysicsAsset->SkeletalBodySetups.Num(); BodyIndex++)
			{
				UBodySetup* BodySetup = ShadowPhysicsAsset->SkeletalBodySetups[BodyIndex];
				int32 BoneIndex = SkinnedMeshComponent->GetBoneIndex(BodySetup->BoneName);

				if (BoneIndex != INDEX_NONE)
				{
					const FMatrix& RefBoneMatrix = SkinnedMeshComponent->GetSkinnedAsset()->GetComposedRefPoseMatrix(BoneIndex);

					const int32 NumSpheres = BodySetup->AggGeom.SphereElems.Num();
					for (int32 SphereIndex = 0; SphereIndex < NumSpheres; SphereIndex++)
					{
						const FKSphereElem& SphereShape = BodySetup->AggGeom.SphereElems[SphereIndex];
						ShadowCapsuleData.Emplace(BoneIndex, FCapsuleShape(RefBoneMatrix.TransformPosition(SphereShape.Center), SphereShape.Radius, FVector(0.0f, 0.0f, 1.0f), 0.0f));
					}

					const int32 NumCapsules = BodySetup->AggGeom.SphylElems.Num();
					for (int32 CapsuleIndex = 0; CapsuleIndex < NumCapsules; CapsuleIndex++)
					{
						const FKSphylElem& SphylShape = BodySetup->AggGeom.SphylElems[CapsuleIndex];
						ShadowCapsuleData.Emplace(BoneIndex, FCapsuleShape(RefBoneMatrix.TransformPosition(SphylShape.Center), SphylShape.Radius, RefBoneMatrix.TransformVector((SphylShape.Rotation.Quaternion() * SphylBasis).Vector()), SphylShape.Length));
					}

					if (NumSpheres > 0 || NumCapsules > 0)
					{
						ShadowCapsuleBoneIndices.AddUnique(BoneIndex);
					}
				}
			}
		}
	}

	// Sort to allow merging with other bone hierarchies
	if (ShadowCapsuleBoneIndices.Num())
	{
		ShadowCapsuleBoneIndices.Sort();
	}

	EnableGPUSceneSupportFlags();
	const bool bUseGPUScene = UseGPUScene(GMaxRHIShaderPlatform, FeatureLevel);

	if (bUseGPUScene)
	{
		bSupportsInstanceDataBuffer = true;
		UpdateDefaultInstanceSceneData();
	}

#if RHI_RAYTRACING
	if (IsRayTracingEnabled(GetScene().GetShaderPlatform()))
	{
		if (bRenderStatic)
		{
			RayTracingGeometries.AddDefaulted(SkeletalMeshRenderData->LODRenderData.Num());
			for (int32 LODIndex = 0; LODIndex < SkeletalMeshRenderData->LODRenderData.Num(); LODIndex++)
			{
				ensure(SkeletalMeshRenderData->LODRenderData[LODIndex].NumReferencingStaticSkeletalMeshObjects > 0);
				RayTracingGeometries[LODIndex] = &SkeletalMeshRenderData->LODRenderData[LODIndex].StaticRayTracingGeometry;
			}
		}
	}
#endif
}


// FPrimitiveSceneProxy interface.

/** 
 * Iterates over sections,chunks,elements based on current instance weight usage 
 */
class FSkeletalMeshSectionIter
{
public:
	FSkeletalMeshSectionIter(const int32 InLODIdx, const FSkeletalMeshObject& InMeshObject, const FSkeletalMeshLODRenderData& InLODData, const FSkeletalMeshSceneProxy::FLODSectionElements& InLODSectionElements)
		: SectionIndex(0)
		, MeshObject(InMeshObject)
		, LODSectionElements(InLODSectionElements)
		, Sections(InLODData.RenderSections)
#if WITH_EDITORONLY_DATA
		, SectionIndexPreview(InMeshObject.SectionIndexPreview)
		, MaterialIndexPreview(InMeshObject.MaterialIndexPreview)
#endif
	{
		while (NotValidPreviewSection())
		{
			SectionIndex++;
		}
	}
	FORCEINLINE FSkeletalMeshSectionIter& operator++()
	{
		do 
		{
		SectionIndex++;
		} while (NotValidPreviewSection());
		return *this;
	}
	FORCEINLINE explicit operator bool() const
	{
		return ((SectionIndex < Sections.Num()) && LODSectionElements.SectionElements.IsValidIndex(GetSectionElementIndex()));
	}
	FORCEINLINE const FSkelMeshRenderSection& GetSection() const
	{
		return Sections[SectionIndex];
	}
	FORCEINLINE const int32 GetSectionElementIndex() const
	{
		return SectionIndex;
	}
	FORCEINLINE const FSkeletalMeshSceneProxy::FSectionElementInfo& GetSectionElementInfo() const
	{
		int32 SectionElementInfoIndex = GetSectionElementIndex();
		return LODSectionElements.SectionElements[SectionElementInfoIndex];
	}
	FORCEINLINE bool NotValidPreviewSection()
	{
#if WITH_EDITORONLY_DATA
		if (MaterialIndexPreview == INDEX_NONE)
		{
			int32 ActualPreviewSectionIdx = SectionIndexPreview;

			return	(SectionIndex < Sections.Num()) &&
				((ActualPreviewSectionIdx >= 0) && (ActualPreviewSectionIdx != SectionIndex));
		}
		else
		{
			int32 ActualPreviewMaterialIdx = MaterialIndexPreview;
			int32 ActualPreviewSectionIdx = INDEX_NONE;
			if (ActualPreviewMaterialIdx != INDEX_NONE && Sections.IsValidIndex(SectionIndex))
			{
				const FSkeletalMeshSceneProxy::FSectionElementInfo& SectionInfo = LODSectionElements.SectionElements[SectionIndex];
				if (SectionInfo.UseMaterialIndex == ActualPreviewMaterialIdx)
				{
					ActualPreviewSectionIdx = SectionIndex;
				}
			}

			return	(SectionIndex < Sections.Num()) &&
				((ActualPreviewMaterialIdx >= 0) && (ActualPreviewSectionIdx != SectionIndex));
		}
#else
		return false;
#endif
	}
private:
	int32 SectionIndex;
	const FSkeletalMeshObject& MeshObject;
	const FSkeletalMeshSceneProxy::FLODSectionElements& LODSectionElements;
	const TArray<FSkelMeshRenderSection>& Sections;
#if WITH_EDITORONLY_DATA
	const int32 SectionIndexPreview;
	const int32 MaterialIndexPreview;
#endif
};

#if WITH_EDITOR
HHitProxy* FSkeletalMeshSceneProxy::CreateHitProxies(UPrimitiveComponent* Component, TArray<TRefCountPtr<HHitProxy> >& OutHitProxies)
{
	if ( Component->GetOwner() )
	{
		if ( LODSections.Num() > 0 )
		{
			for ( int32 LODIndex = 0; LODIndex < SkeletalMeshRenderData->LODRenderData.Num(); LODIndex++ )
			{
				const FSkeletalMeshLODRenderData& LODData = SkeletalMeshRenderData->LODRenderData[LODIndex];

				FLODSectionElements& LODSection = LODSections[LODIndex];

				check(LODSection.SectionElements.Num() == LODData.RenderSections.Num());

				for ( int32 SectionIndex = 0; SectionIndex < LODData.RenderSections.Num(); SectionIndex++ )
				{
					HHitProxy* ActorHitProxy;

					int32 MaterialIndex = LODData.RenderSections[SectionIndex].MaterialIndex;
					if ( Component->GetOwner()->IsA(ABrush::StaticClass()) && Component->IsA(UBrushComponent::StaticClass()) )
					{
						ActorHitProxy = new HActor(Component->GetOwner(), Component, HPP_Wireframe, SectionIndex, MaterialIndex);
					}
					else
					{
						ActorHitProxy = new HActor(Component->GetOwner(), Component, Component->HitProxyPriority, SectionIndex, MaterialIndex);
					}

					// Set the hitproxy.
					check(LODSection.SectionElements[SectionIndex].HitProxy == NULL);
					LODSection.SectionElements[SectionIndex].HitProxy = ActorHitProxy;
					OutHitProxies.Add(ActorHitProxy);
				}
			}
		}
		else
		{
			return FPrimitiveSceneProxy::CreateHitProxies(Component, OutHitProxies);
		}
	}

	return NULL;
}
#endif

void FSkeletalMeshSceneProxy::DrawStaticElements(FStaticPrimitiveDrawInterface* PDI)
{
	if (!MeshObject || !bRenderStatic)
	{
		return;
	}

	if (!HasViewDependentDPG())
	{
		ESceneDepthPriorityGroup PrimitiveDPG = GetStaticDepthPriorityGroup();
		bool bUseSelectedMaterial = false;

		int32 NumLODs = SkeletalMeshRenderData->LODRenderData.Num();
		int32 ClampedMinLOD = 0; // TODO: MinLOD, Bias?

		for (int32 LODIndex = ClampedMinLOD; LODIndex < NumLODs; ++LODIndex)
		{
			const FSkeletalMeshLODRenderData& LODData = SkeletalMeshRenderData->LODRenderData[LODIndex];
			
			if (LODSections.Num() > 0 && LODData.GetNumVertices() > 0)
			{
				float ScreenSize = MeshObject->GetScreenSize(LODIndex);
				const FLODSectionElements& LODSection = LODSections[LODIndex];
				check(LODSection.SectionElements.Num() == LODData.RenderSections.Num());

				for (FSkeletalMeshSectionIter Iter(LODIndex, *MeshObject, LODData, LODSection); Iter; ++Iter)
				{
					const FSkelMeshRenderSection& Section = Iter.GetSection();
					const int32 SectionIndex = Iter.GetSectionElementIndex();
					const FSectionElementInfo& SectionElementInfo = Iter.GetSectionElementInfo();
					const FVertexFactory* VertexFactory = MeshObject->GetSkinVertexFactory(nullptr, LODIndex, SectionIndex);
				
					// If hidden skip the draw
					if (MeshObject->IsMaterialHidden(LODIndex, SectionElementInfo.UseMaterialIndex))
					{
						continue;
					}
					
					if (!VertexFactory)
					{
						// hide this part
						continue;
					}

				#if WITH_EDITOR
					if (GIsEditor)
					{
						bUseSelectedMaterial = (MeshObject->SelectedEditorSection == SectionIndex);
						PDI->SetHitProxy(SectionElementInfo.HitProxy);
					}
				#endif // WITH_EDITOR
								
					FMeshBatch MeshElement;
					FMeshBatchElement& BatchElement = MeshElement.Elements[0];
					MeshElement.DepthPriorityGroup = PrimitiveDPG;
					MeshElement.VertexFactory = MeshObject->GetSkinVertexFactory(nullptr, LODIndex, SectionIndex);
					MeshElement.MaterialRenderProxy = SectionElementInfo.Material->GetRenderProxy();
					MeshElement.ReverseCulling = IsLocalToWorldDeterminantNegative();
					MeshElement.CastShadow = SectionElementInfo.bEnableShadowCasting;
				#if RHI_RAYTRACING
					MeshElement.CastRayTracedShadow = MeshElement.CastShadow && bCastDynamicShadow;
				#endif
					MeshElement.Type = PT_TriangleList;
					MeshElement.LODIndex = LODIndex;
					MeshElement.SegmentIndex = SectionIndex;
						
					BatchElement.FirstIndex = Section.BaseIndex;
					BatchElement.MinVertexIndex = Section.BaseVertexIndex;
					BatchElement.MaxVertexIndex = LODData.GetNumVertices() - 1;
					BatchElement.NumPrimitives = Section.NumTriangles;
					BatchElement.IndexBuffer = LODData.MultiSizeIndexContainer.GetIndexBuffer();
													
					PDI->DrawMesh(MeshElement, ScreenSize);

					if (OverlayMaterial != nullptr)
					{
						FMeshBatch OverlayMeshBatch(MeshElement);
						OverlayMeshBatch.bOverlayMaterial = true;
						OverlayMeshBatch.CastShadow = false;
						OverlayMeshBatch.bSelectable = false;
						OverlayMeshBatch.MaterialRenderProxy = OverlayMaterial->GetRenderProxy();
						// make sure overlay is always rendered on top of base mesh
						OverlayMeshBatch.MeshIdInPrimitive += LODData.RenderSections.Num();
						// Reuse mesh ScreenSize as cull distance for an overlay. Overlay does not need to compute LOD so we can avoid adding new members into MeshBatch or MeshRelevance
						float OverlayMeshScreenSize = OverlayMaterialMaxDrawDistance;
						PDI->DrawMesh(OverlayMeshBatch, OverlayMeshScreenSize);
					}
				}
			}
		}
	}
}

void FSkeletalMeshSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FSkeletalMeshSceneProxy_GetMeshElements);
	GetMeshElementsConditionallySelectable(Views, ViewFamily, true, VisibilityMap, Collector);
}

void FSkeletalMeshSceneProxy::GetMeshElementsConditionallySelectable(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, bool bInSelectable, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	if( !MeshObject )
	{
		return;
	}	
	MeshObject->PreGDMECallback(ViewFamily.Scene->GetGPUSkinCache(), ViewFamily.FrameNumber);

	const FEngineShowFlags& EngineShowFlags = ViewFamily.EngineShowFlags;

	int32 FirstLODIdx = SkeletalMeshRenderData->GetFirstValidLODIdx(FMath::Max(SkeletalMeshRenderData->PendingFirstLODIdx, SkeletalMeshRenderData->CurrentFirstLODIdx));
	if (FirstLODIdx == INDEX_NONE)
	{
#if DO_CHECK
		UE_LOG(LogSkeletalMesh, Warning, TEXT("Skeletal mesh %s has no valid LODs for rendering."), *GetResourceName().ToString());
#endif
	}
	else
	{
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (VisibilityMap & (1 << ViewIndex))
			{
				const FSceneView* View = Views[ViewIndex];
				MeshObject->UpdateMinDesiredLODLevel(View, GetBounds(), ViewFamily.FrameNumber, FirstLODIdx);
			}
		}

		const int32 LODIndex = MeshObject->GetLOD();
		check(LODIndex < SkeletalMeshRenderData->LODRenderData.Num());
		const FSkeletalMeshLODRenderData& LODData = SkeletalMeshRenderData->LODRenderData[LODIndex];

		if (LODSections.Num() > 0 && LODIndex >= FirstLODIdx)
		{
			check(SkeletalMeshRenderData->LODRenderData[LODIndex].GetNumVertices() > 0);

			const FLODSectionElements& LODSection = LODSections[LODIndex];

			check(LODSection.SectionElements.Num() == LODData.RenderSections.Num());

			for (FSkeletalMeshSectionIter Iter(LODIndex, *MeshObject, LODData, LODSection); Iter; ++Iter)
			{
				const FSkelMeshRenderSection& Section = Iter.GetSection();
				const int32 SectionIndex = Iter.GetSectionElementIndex();
				const FSectionElementInfo& SectionElementInfo = Iter.GetSectionElementInfo();

				bool bSectionSelected = false;

#if WITH_EDITORONLY_DATA
				// TODO: This is not threadsafe! A render command should be used to propagate SelectedEditorSection to the scene proxy.
				if (MeshObject->SelectedEditorMaterial != INDEX_NONE)
				{
					bSectionSelected = (MeshObject->SelectedEditorMaterial == SectionElementInfo.UseMaterialIndex);
				}
				else
				{
					bSectionSelected = (MeshObject->SelectedEditorSection == SectionIndex);
				}
			
#endif
				// If hidden skip the draw
				if (MeshObject->IsMaterialHidden(LODIndex, SectionElementInfo.UseMaterialIndex) || Section.bDisabled)
				{
					continue;
				}

				GetDynamicElementsSection(Views, ViewFamily, VisibilityMap, LODData, LODIndex, SectionIndex, bSectionSelected, SectionElementInfo, bInSelectable, Collector);
			}
		}
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			if( PhysicsAssetForDebug )
			{
				DebugDrawPhysicsAsset(ViewIndex, Collector, ViewFamily.EngineShowFlags);
			}

			if (EngineShowFlags.MassProperties && DebugMassData.Num() > 0)
			{
				FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
				if (MeshObject->GetComponentSpaceTransforms())
				{
					const TArray<FTransform>& ComponentSpaceTransforms = *MeshObject->GetComponentSpaceTransforms();

					for (const FDebugMassData& DebugMass : DebugMassData)
					{
						if (ComponentSpaceTransforms.IsValidIndex(DebugMass.BoneIndex))
						{
							const FTransform BoneToWorld = ComponentSpaceTransforms[DebugMass.BoneIndex] * FTransform(GetLocalToWorld());
							DebugMass.DrawDebugMass(PDI, BoneToWorld);
						}
					}
				}
			}

			if (ViewFamily.EngineShowFlags.SkeletalMeshes)
			{
				RenderBounds(Collector.GetPDI(ViewIndex), ViewFamily.EngineShowFlags, GetBounds(), IsSelected());
			}

			if (ViewFamily.EngineShowFlags.Bones || bDrawDebugSkeleton)
			{
				DebugDrawSkeleton(ViewIndex, Collector, ViewFamily.EngineShowFlags);
			}

#if WITH_EDITOR
			DebugDrawPoseWatchSkeletons(ViewIndex, Collector, ViewFamily.EngineShowFlags);
#endif
		}
	}
#endif
}

void FSkeletalMeshSceneProxy::CreateBaseMeshBatch(const FSceneView* View, const FSkeletalMeshLODRenderData& LODData, const int32 LODIndex, const int32 SectionIndex, const FSectionElementInfo& SectionElementInfo, FMeshBatch& Mesh, ESkinVertexFactoryMode VFMode) const
{
	Mesh.VertexFactory = MeshObject->GetSkinVertexFactory(View, LODIndex, SectionIndex, VFMode);
	Mesh.MaterialRenderProxy = SectionElementInfo.Material->GetRenderProxy();
#if RHI_RAYTRACING
	Mesh.SegmentIndex = SectionIndex;
	Mesh.CastRayTracedShadow = SectionElementInfo.bEnableShadowCasting && bCastDynamicShadow;
#endif

	FMeshBatchElement& BatchElement = Mesh.Elements[0];
	BatchElement.FirstIndex = LODData.RenderSections[SectionIndex].BaseIndex;
	BatchElement.IndexBuffer = LODData.MultiSizeIndexContainer.GetIndexBuffer();
	BatchElement.MinVertexIndex = LODData.RenderSections[SectionIndex].GetVertexBufferIndex();
	BatchElement.MaxVertexIndex = LODData.RenderSections[SectionIndex].GetVertexBufferIndex() + LODData.RenderSections[SectionIndex].GetNumVertices() - 1;
#if RHI_RAYTRACING
	FGPUSkinBatchElementUserData* VertexFactoryUserData = FGPUSkinCache::GetFactoryUserData(VFMode == ESkinVertexFactoryMode::RayTracing ? MeshObject->GetSkinCacheEntryForRayTracing() : MeshObject->SkinCacheEntry, SectionIndex);
#else
	FGPUSkinBatchElementUserData* VertexFactoryUserData = FGPUSkinCache::GetFactoryUserData(MeshObject->SkinCacheEntry, SectionIndex);
#endif
	BatchElement.VertexFactoryUserData = VertexFactoryUserData;
	BatchElement.PrimitiveUniformBuffer = GetUniformBuffer();
	BatchElement.NumPrimitives = LODData.RenderSections[SectionIndex].NumTriangles;

	UpdateLooseParametersUniformBuffer(View, SectionIndex, Mesh, VertexFactoryUserData);
}

void FSkeletalMeshSceneProxy::UpdateLooseParametersUniformBuffer(const FSceneView* View, const int32 SectionIndex, const FMeshBatch& Mesh, const FGPUSkinBatchElementUserData* BatchUserData) const
{
	// Loose parameters uniform buffer is only needed for PassThroughVF
	if (!Mesh.VertexFactory || Mesh.VertexFactory->GetType() != &FGPUSkinPassthroughVertexFactory::StaticType)
	{
		return;
	}
	
	FLocalVertexFactoryLooseParameters Parameters;

	FVertexFactory* NonConstVertexFactory = const_cast<FVertexFactory*>(Mesh.VertexFactory);
	FGPUSkinPassthroughVertexFactory* PassthroughVertexFactory = static_cast<FGPUSkinPassthroughVertexFactory*>(NonConstVertexFactory);
	// Bone data is updated whenever animation triggers a dynamic update, animation can skip frames hence the frequency is not necessary every frame.
	// So check if bone data is updated this frame, if not then the previous frame data is stale and not suitable for motion blur.
	bool bBoneDataUpdatedThisFrame = (View->Family->FrameNumber == PassthroughVertexFactory->GetUpdatedFrameNumber());
	// If world is paused, use current frame bone matrices, so velocity is canceled and skeletal mesh isn't blurred from motion.
	bool bVerticesInMotion = !View->Family->bWorldIsPaused && bBoneDataUpdatedThisFrame;

	const bool bUsesSkinCache = BatchUserData != nullptr;
	if (bUsesSkinCache)
	{
		Parameters.GPUSkinPassThroughPreviousPositionBuffer = bVerticesInMotion ?
			FGPUSkinCache::GetPreviousPositionBuffer(BatchUserData->Entry, SectionIndex)->SRV :
			FGPUSkinCache::GetPositionBuffer(BatchUserData->Entry, SectionIndex)->SRV;
	}
	else // Mesh deformer
	{
		Parameters.GPUSkinPassThroughPreviousPositionBuffer = (bVerticesInMotion && PassthroughVertexFactory->PrevPositionRDG) ?
			PassthroughVertexFactory->PrevPositionRDG->GetOrCreateSRV(FRHIBufferSRVCreateInfo(PF_R32_FLOAT)) :
			PassthroughVertexFactory->GetPositionsSRV();
	}

	PassthroughVertexFactory->LooseParametersUniformBuffer.UpdateUniformBufferImmediate(Parameters);
}

uint8 FSkeletalMeshSceneProxy::GetCurrentFirstLODIdx_Internal() const
{
	return SkeletalMeshRenderData->CurrentFirstLODIdx;
}

bool FSkeletalMeshSceneProxy::GetCachedGeometry(FCachedGeometry& OutCachedGeometry) const 
{
	return MeshObject != nullptr && MeshObject->GetCachedGeometry(OutCachedGeometry); 
}

void FSkeletalMeshSceneProxy::GetDynamicElementsSection(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, 
	const FSkeletalMeshLODRenderData& LODData, const int32 LODIndex, const int32 SectionIndex, bool bSectionSelected,
	const FSectionElementInfo& SectionElementInfo, bool bInSelectable, FMeshElementCollector& Collector ) const
{
	const FSkelMeshRenderSection& Section = LODData.RenderSections[SectionIndex];

	//// If hidden skip the draw
	//if (Section.bDisabled || MeshObject->IsMaterialHidden(LODIndex,SectionElementInfo.UseMaterialIndex))
	//{
	//	return;
	//}

#if !WITH_EDITOR
	const bool bIsSelected = false;
#else // #if !WITH_EDITOR
	bool bIsSelected = IsSelected();

	// if the mesh isn't selected but the mesh section is selected in the AnimSetViewer, find the mesh component and make sure that it can be highlighted (ie. are we rendering for the AnimSetViewer or not?)
	if( !bIsSelected && bSectionSelected && bCanHighlightSelectedSections )
	{
		bIsSelected = true;
	}
#endif // #if WITH_EDITOR

	const bool bIsWireframe = ViewFamily.EngineShowFlags.Wireframe;

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			const FSceneView* View = Views[ViewIndex];

			FMeshBatch& Mesh = Collector.AllocateMesh();

			CreateBaseMeshBatch(View, LODData, LODIndex, SectionIndex, SectionElementInfo, Mesh);
			
			if(!Mesh.VertexFactory)
			{
				// hide this part
				continue;
			}

			Mesh.bWireframe |= bForceWireframe;
			Mesh.Type = PT_TriangleList;
			Mesh.bSelectable = bInSelectable;

			FMeshBatchElement& BatchElement = Mesh.Elements[0];

		#if WITH_EDITOR
			Mesh.BatchHitProxyId = SectionElementInfo.HitProxy ? SectionElementInfo.HitProxy->Id : FHitProxyId();

			if (bSectionSelected && bCanHighlightSelectedSections)
			{
				Mesh.bUseSelectionOutline = true;
			}
			else
			{
				Mesh.bUseSelectionOutline = !bCanHighlightSelectedSections && bIsSelected;
			}
		#endif

#if WITH_EDITORONLY_DATA
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if (bIsSelected)
			{
				if (ViewFamily.EngineShowFlags.VertexColors && AllowDebugViewmodes())
				{
					// Override the mesh's material with our material that draws the vertex colors
					UMaterial* VertexColorVisualizationMaterial = NULL;
					switch (GVertexColorViewMode)
					{
					case EVertexColorViewMode::Color:
						VertexColorVisualizationMaterial = GEngine->VertexColorViewModeMaterial_ColorOnly;
						break;

					case EVertexColorViewMode::Alpha:
						VertexColorVisualizationMaterial = GEngine->VertexColorViewModeMaterial_AlphaAsColor;
						break;

					case EVertexColorViewMode::Red:
						VertexColorVisualizationMaterial = GEngine->VertexColorViewModeMaterial_RedOnly;
						break;

					case EVertexColorViewMode::Green:
						VertexColorVisualizationMaterial = GEngine->VertexColorViewModeMaterial_GreenOnly;
						break;

					case EVertexColorViewMode::Blue:
						VertexColorVisualizationMaterial = GEngine->VertexColorViewModeMaterial_BlueOnly;
						break;
					}
					check(VertexColorVisualizationMaterial != NULL);

					auto VertexColorVisualizationMaterialInstance = new FColoredMaterialRenderProxy(
						VertexColorVisualizationMaterial->GetRenderProxy(),
						GetSelectionColor(FLinearColor::White, bSectionSelected, IsHovered())
					);

					Collector.RegisterOneFrameMaterialProxy(VertexColorVisualizationMaterialInstance);
					Mesh.MaterialRenderProxy = VertexColorVisualizationMaterialInstance;
				}
			}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
#endif // WITH_EDITORONLY_DATA

			BatchElement.MinVertexIndex = Section.BaseVertexIndex;
			Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
			Mesh.CastShadow = SectionElementInfo.bEnableShadowCasting;
			Mesh.bCanApplyViewModeOverrides = true;
			Mesh.bUseWireframeSelectionColoring = bIsSelected;

		#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			BatchElement.SkinCacheDebugColor = MeshObject->GetSkinCacheVisualizationDebugColor(View->CurrentGPUSkinCacheVisualizationMode, SectionIndex);
			BatchElement.VisualizeElementIndex = SectionIndex;
			Mesh.VisualizeLODIndex = LODIndex;
		#endif

			if (ensureMsgf(Mesh.MaterialRenderProxy, TEXT("GetDynamicElementsSection with invalid MaterialRenderProxy. Owner:%s LODIndex:%d UseMaterialIndex:%d"), *GetOwnerName().ToString(), LODIndex, SectionElementInfo.UseMaterialIndex))
			{
				Collector.AddMesh(ViewIndex, Mesh);
			}

			const int32 NumVertices = Section.GetNumVertices();
			INC_DWORD_STAT_BY(STAT_GPUSkinVertices,(uint32)(bIsCPUSkinned ? 0 : NumVertices));
			INC_DWORD_STAT_BY(STAT_SkelMeshTriangles,Mesh.GetNumPrimitives());
			INC_DWORD_STAT(STAT_SkelMeshDrawCalls);

			if (OverlayMaterial != nullptr)
			{
				const bool bHasOverlayCullDistance = 
					OverlayMaterialMaxDrawDistance > 1.f && 
					OverlayMaterialMaxDrawDistance != FLT_MAX && 
					!ViewFamily.EngineShowFlags.DistanceCulledPrimitives;
				
				bool bAddOverlay = true;
				if (bHasOverlayCullDistance)
				{
					// this is already combined with ViewDistanceScale
					float MaxDrawDistanceScale = GetCachedScalabilityCVars().SkeletalMeshOverlayDistanceScale;
					MaxDrawDistanceScale *= GetCachedScalabilityCVars().CalculateFieldOfViewDistanceScale(View->DesiredFOV);
					float DistanceSquared = (GetBounds().Origin - View->ViewMatrices.GetViewOrigin()).SizeSquared();
					if (DistanceSquared > FMath::Square(OverlayMaterialMaxDrawDistance * MaxDrawDistanceScale))
					{
						// distance culled
						bAddOverlay = false;
					}
				}
				
				if (bAddOverlay)
				{
					FMeshBatch& OverlayMeshBatch = Collector.AllocateMesh();
					OverlayMeshBatch = Mesh;
					OverlayMeshBatch.bOverlayMaterial = true;
					OverlayMeshBatch.CastShadow = false;
					OverlayMeshBatch.bSelectable = false;
					OverlayMeshBatch.MaterialRenderProxy = OverlayMaterial->GetRenderProxy();
					// make sure overlay is always rendered on top of base mesh
					OverlayMeshBatch.MeshIdInPrimitive += LODData.RenderSections.Num();
					Collector.AddMesh(ViewIndex, OverlayMeshBatch);
				
					INC_DWORD_STAT_BY(STAT_SkelMeshTriangles, OverlayMeshBatch.GetNumPrimitives());
					INC_DWORD_STAT(STAT_SkelMeshDrawCalls);
				}
			}
		}
	}
}

#if RHI_RAYTRACING
bool FSkeletalMeshSceneProxy::HasRayTracingRepresentation() const
{
	return bRenderStatic;
}

void FSkeletalMeshSceneProxy::GetDynamicRayTracingInstances(FRayTracingMaterialGatheringContext & Context, TArray<struct FRayTracingInstance>& OutRayTracingInstances)
{
	if (!CVarRayTracingSkeletalMeshes.GetValueOnRenderThread()
		|| !CVarRayTracingSupportSkeletalMeshes.GetValueOnRenderThread())
	{
		return;
	}

	// GetRayTracingGeometry()->IsInitialized() is checked as a workaround for UE-92634. FSkeletalMeshSceneProxy's resources may have already been released, but proxy has not removed yet)
	if (MeshObject->GetRayTracingGeometry() && MeshObject->GetRayTracingGeometry()->IsInitialized())
	{
		if(MeshObject->GetRayTracingGeometry()->RayTracingGeometryRHI.IsValid())
		{
			check(MeshObject->GetRayTracingGeometry()->Initializer.IndexBuffer.IsValid());
			
			FRayTracingInstance RayTracingInstance;
			RayTracingInstance.Geometry = MeshObject->GetRayTracingGeometry();

			// Setup materials for each segment
			const int32 LODIndex = MeshObject->GetRayTracingLOD();
			check(LODIndex < SkeletalMeshRenderData->LODRenderData.Num());
			const FSkeletalMeshLODRenderData& LODData = SkeletalMeshRenderData->LODRenderData[LODIndex];

			if (LODIndex < SkeletalMeshRenderData->CurrentFirstLODIdx)
			{
				// According to GetMeshElementsConditionallySelectable(), non-resident LODs should just be skipped
				return;
			}

			ensure(LODSections.Num() > 0);
			const FLODSectionElements& LODSection = LODSections[LODIndex];
			check(LODSection.SectionElements.Num() == LODData.RenderSections.Num());
				
			//#dxr_todo (UE-113617): verify why this condition is not fulfilled sometimes
			if(!ensure(LODSection.SectionElements.Num() == MeshObject->GetRayTracingGeometry()->Initializer.Segments.Num()))
			{
				return;
			}

		#if WITH_EDITORONLY_DATA
			int32 SectionIndexPreview = MeshObject->SectionIndexPreview;
			int32 MaterialIndexPreview = MeshObject->MaterialIndexPreview;
			MeshObject->SectionIndexPreview = INDEX_NONE;
			MeshObject->MaterialIndexPreview = INDEX_NONE;
		#endif

			uint32 TotalNumVertices = 0;

			for (FSkeletalMeshSectionIter Iter(LODIndex, *MeshObject, LODData, LODSection); Iter; ++Iter)
			{
				const FSkelMeshRenderSection& Section = Iter.GetSection();
				const int32 SectionIndex = Iter.GetSectionElementIndex();
				const FSectionElementInfo& SectionElementInfo = Iter.GetSectionElementInfo();

				FMeshBatch MeshBatch;
				CreateBaseMeshBatch(Context.ReferenceView, LODData, LODIndex, SectionIndex, SectionElementInfo, MeshBatch, ESkinVertexFactoryMode::RayTracing);

				RayTracingInstance.Materials.Add(MeshBatch);

				TotalNumVertices += Section.GetNumVertices();
			}

			RayTracingInstance.InstanceTransforms.Add(GetLocalToWorld());
			const uint32 VertexBufferStride = LODData.StaticVertexBuffers.PositionVertexBuffer.GetStride();

			const FVertexFactory* VertexFactory = MeshObject->GetSkinVertexFactory(Context.ReferenceView, LODIndex, 0, ESkinVertexFactoryMode::RayTracing);
			const FVertexFactoryType* VertexFactoryType = VertexFactory->GetType();
			if (bAnySegmentUsesWorldPositionOffset 
				&& ensureMsgf(VertexFactoryType->SupportsRayTracingDynamicGeometry(),
					TEXT("Mesh uses world position offset, but the vertex factory does not support ray tracing dynamic geometry. ")
					TEXT("MeshObject: %s, VertexFactory: %s."), *MeshObject->GetDebugName().ToString(), VertexFactoryType->GetName()))
			{
				TArray<FRayTracingGeometrySegment> GeometrySections;
				GeometrySections.Reserve(LODData.RenderSections.Num());

				for (FSkeletalMeshSectionIter Iter(LODIndex, *MeshObject, LODData, LODSection); Iter; ++Iter)
				{
					const FSkelMeshRenderSection& Section = Iter.GetSection();
					const FSectionElementInfo& SectionElementInfo = Iter.GetSectionElementInfo();

					FRayTracingGeometrySegment Segment;
					Segment.VertexBufferStride = VertexBufferStride;
					Segment.MaxVertices = TotalNumVertices;
					Segment.FirstPrimitive = Section.BaseIndex / 3;
					Segment.NumPrimitives = Section.NumTriangles;
					Segment.bEnabled = !MeshObject->IsMaterialHidden(LODIndex, SectionElementInfo.UseMaterialIndex) && !Section.bDisabled && Section.bVisibleInRayTracing;
					GeometrySections.Add(Segment);
				}

				MeshObject->GetRayTracingGeometry()->Initializer.Segments = GeometrySections;

				Context.DynamicRayTracingGeometriesToUpdate.Add(
					FRayTracingDynamicGeometryUpdateParams
					{
						RayTracingInstance.Materials,
						false,
						LODData.GetNumVertices(),
						LODData.GetNumVertices() * (uint32)sizeof(FVector3f),
						MeshObject->GetRayTracingGeometry()->Initializer.TotalPrimitiveCount,
						MeshObject->GetRayTracingGeometry(),
						MeshObject->GetRayTracingDynamicVertexBuffer(),
						true
					}
				);
			}

			RayTracingInstance.BuildInstanceMaskAndFlags(GetScene().GetFeatureLevel());

			OutRayTracingInstances.Add(RayTracingInstance);
			
		#if WITH_EDITORONLY_DATA
			MeshObject->SectionIndexPreview = SectionIndexPreview;
			MeshObject->MaterialIndexPreview = MaterialIndexPreview;
		#endif
		}
	}
}
#endif // RHI_RAYTRACING

SIZE_T FSkeletalMeshSceneProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

bool FSkeletalMeshSceneProxy::HasDynamicIndirectShadowCasterRepresentation() const
{
	return CastsDynamicShadow() && CastsDynamicIndirectShadow();
}

void FSkeletalMeshSceneProxy::GetShadowShapes(FVector PreViewTranslation, TArray<FCapsuleShape3f>& OutCapsuleShapes) const
{
	SCOPE_CYCLE_COUNTER(STAT_GetShadowShapes);

	const TArray<FMatrix44f>& ReferenceToLocalMatrices = MeshObject->GetReferenceToLocalMatrices();
	const FMatrix& ProxyLocalToWorld = GetLocalToWorld();

	int32 CapsuleIndex = OutCapsuleShapes.Num();
	OutCapsuleShapes.SetNum(OutCapsuleShapes.Num() + ShadowCapsuleData.Num(), false);

	for(const TPair<int32, FCapsuleShape>& CapsuleData : ShadowCapsuleData)
	{
		FMatrix ReferenceToWorld = ProxyLocalToWorld;
		if (ReferenceToLocalMatrices.IsValidIndex(CapsuleData.Key))
		{
			ReferenceToWorld = FMatrix(ReferenceToLocalMatrices[CapsuleData.Key]) * ProxyLocalToWorld;
		}
		const float MaxScale = ReferenceToWorld.GetScaleVector().GetMax();

		FCapsuleShape3f& NewCapsule = OutCapsuleShapes[CapsuleIndex++];

		NewCapsule.Center = (FVector4f)(ReferenceToWorld.TransformPosition(CapsuleData.Value.Center) + PreViewTranslation);
		NewCapsule.Radius = CapsuleData.Value.Radius * MaxScale;
		NewCapsule.Orientation = (FVector4f)ReferenceToWorld.TransformVector(CapsuleData.Value.Orientation).GetSafeNormal();
		NewCapsule.Length = CapsuleData.Value.Length * MaxScale;
	}
}

/**
 * Returns the world transform to use for drawing.
 * @param OutLocalToWorld - Will contain the local-to-world transform when the function returns.
 * @param OutWorldToLocal - Will contain the world-to-local transform when the function returns.
 */
bool FSkeletalMeshSceneProxy::GetWorldMatrices( FMatrix& OutLocalToWorld, FMatrix& OutWorldToLocal ) const
{
	OutLocalToWorld = GetLocalToWorld();
	if (OutLocalToWorld.GetScaledAxis(EAxis::X).IsNearlyZero(UE_SMALL_NUMBER) &&
		OutLocalToWorld.GetScaledAxis(EAxis::Y).IsNearlyZero(UE_SMALL_NUMBER) &&
		OutLocalToWorld.GetScaledAxis(EAxis::Z).IsNearlyZero(UE_SMALL_NUMBER))
	{
		return false;
	}
	OutWorldToLocal = GetLocalToWorld().InverseFast();
	return true;
}

/**
 * Relevance is always dynamic for skel meshes unless they are disabled
 */
FPrimitiveViewRelevance FSkeletalMeshSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = IsShown(View) && View->Family->EngineShowFlags.SkeletalMeshes;
	Result.bShadowRelevance = IsShadowCast(View);
	Result.bStaticRelevance = bRenderStatic && !IsRichView(*View->Family);
	Result.bDynamicRelevance = !Result.bStaticRelevance;
	Result.bRenderCustomDepth = ShouldRenderCustomDepth();
	Result.bRenderInMainPass = ShouldRenderInMainPass();
	Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
	Result.bTranslucentSelfShadow = bCastVolumetricTranslucentShadow;
	
	MaterialRelevance.SetPrimitiveViewRelevance(Result);

#if !UE_BUILD_SHIPPING
	Result.bSeparateTranslucency |= View->Family->EngineShowFlags.Constraints;
#endif

#if WITH_EDITOR
	//only check these in the editor
	if (Result.bStaticRelevance)
	{
		Result.bEditorStaticSelectionRelevance = (IsSelected() || IsHovered());

		Result.bEditorVisualizeLevelInstanceRelevance = IsEditingLevelInstanceChild();
	}
#endif

	Result.bVelocityRelevance = DrawsVelocity() && Result.bOpaque && Result.bRenderInMainPass;

	return Result;
}

bool FSkeletalMeshSceneProxy::CanBeOccluded() const
{
	return !MaterialRelevance.bDisableDepthTest && !MaterialRelevance.bPostMotionBlurTranslucency && !ShouldRenderCustomDepth();
}

bool FSkeletalMeshSceneProxy::IsUsingDistanceCullFade() const
{
	return MaterialRelevance.bUsesDistanceCullFade;
}

/** Util for getting LOD index currently used by this SceneProxy. */
int32 FSkeletalMeshSceneProxy::GetCurrentLODIndex()
{
	if(MeshObject)
	{
		return MeshObject->GetLOD();
	}
	else
	{
		return 0;
	}
}


/** 
 * Render physics asset for debug display
 */
void FSkeletalMeshSceneProxy::DebugDrawPhysicsAsset(int32 ViewIndex, FMeshElementCollector& Collector, const FEngineShowFlags& EngineShowFlags) const
{
	FMatrix ProxyLocalToWorld, WorldToLocal;
	if (!GetWorldMatrices(ProxyLocalToWorld, WorldToLocal))
	{
		return; // Cannot draw this, world matrix not valid
	}

	FMatrix ScalingMatrix = ProxyLocalToWorld;
	FVector TotalScale = ScalingMatrix.ExtractScaling();

	// Only if valid
	if( !TotalScale.IsNearlyZero() )
	{
		FTransform LocalToWorldTransform(ProxyLocalToWorld);

		TArray<FTransform>* BoneSpaceBases = MeshObject->GetComponentSpaceTransforms();
		if(BoneSpaceBases)
		{
			//TODO: These data structures are not double buffered. This is not thread safe!
			check(PhysicsAssetForDebug);
			if (EngineShowFlags.Collision && IsCollisionEnabled())
			{
				PhysicsAssetForDebug->GetCollisionMesh(ViewIndex, Collector, SkeletalMeshForDebug->GetRefSkeleton(), *BoneSpaceBases, LocalToWorldTransform, TotalScale);
			}
			if (EngineShowFlags.Constraints)
			{
				PhysicsAssetForDebug->DrawConstraints(ViewIndex, Collector, SkeletalMeshForDebug->GetRefSkeleton(), *BoneSpaceBases, LocalToWorldTransform, TotalScale.X);
			}
		}
	}
}

#if WITH_EDITOR
void FSkeletalMeshSceneProxy::DebugDrawPoseWatchSkeletons(int32 ViewIndex, FMeshElementCollector& Collector, const FEngineShowFlags& EngineShowFlags) const
{
	FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);

	if (PoseWatchDynamicData)
	{
		for (const FAnimNodePoseWatch& PoseWatch : PoseWatchDynamicData->PoseWatches)
		{
			SkeletalDebugRendering::DrawBonesFromPoseWatch(PDI, PoseWatch, true);
		}
	}
}
#endif

void FSkeletalMeshSceneProxy::DebugDrawSkeleton(int32 ViewIndex, FMeshElementCollector& Collector, const FEngineShowFlags& EngineShowFlags) const
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	FMatrix ProxyLocalToWorld, WorldToLocal;
	if (!GetWorldMatrices(ProxyLocalToWorld, WorldToLocal))
	{
		return; // Cannot draw this, world matrix not valid
	}

	FTransform LocalToWorldTransform(ProxyLocalToWorld);

	auto MakeRandomColorForSkeleton = [](uint32 InUID)
	{
		FRandomStream Stream((int32)InUID);
		const uint8 Hue = (uint8)(Stream.FRand()*255.f);
		return FLinearColor::MakeFromHSV8(Hue, 255, 255);
	};

	FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
	TArray<FTransform>& ComponentSpaceTransforms = *MeshObject->GetComponentSpaceTransforms();

	for (int32 Index = 0; Index < ComponentSpaceTransforms.Num(); ++Index)
	{
		const int32 ParentIndex = SkeletalMeshForDebug->GetRefSkeleton().GetParentIndex(Index);
		FVector Start, End;
		
		FLinearColor LineColor = DebugDrawColor.Get(MakeRandomColorForSkeleton(GetPrimitiveComponentId().PrimIDValue));
		const FTransform Transform = ComponentSpaceTransforms[Index] * LocalToWorldTransform;

		if (ParentIndex >= 0)
		{
			Start = (ComponentSpaceTransforms[ParentIndex] * LocalToWorldTransform).GetLocation();
			End = Transform.GetLocation();
		}
		else
		{
			Start = LocalToWorldTransform.GetLocation();
			End = Transform.GetLocation();
		}

		if(EngineShowFlags.Bones || bDrawDebugSkeleton)
		{
			if(CVarDebugDrawSimpleBones.GetValueOnRenderThread() != 0)
			{
				PDI->DrawLine(Start, End, LineColor, SDPG_Foreground, 0.0f, 1.0f);
			}
			else
			{
				SkeletalDebugRendering::DrawWireBone(PDI, Start, End, LineColor, SDPG_Foreground);
			}

			if(CVarDebugDrawBoneAxes.GetValueOnRenderThread() != 0)
			{
				SkeletalDebugRendering::DrawAxes(PDI, Transform, SDPG_Foreground);
			}
		}
	}
#endif
}

/**
* Updates morph material usage for materials referenced by each LOD entry
*
* @param bNeedsMorphUsage - true if the materials used by this skeletal mesh need morph target usage
*/
void FSkeletalMeshSceneProxy::UpdateMorphMaterialUsage_GameThread(TArray<UMaterialInterface*>& MaterialUsingMorphTarget)
{
	bool bNeedsMorphUsage = MaterialUsingMorphTarget.Num() > 0;
	if( bNeedsMorphUsage != bMaterialsNeedMorphUsage_GameThread )
	{
		// keep track of current morph material usage for the proxy
		bMaterialsNeedMorphUsage_GameThread = bNeedsMorphUsage;

		TSet<UMaterialInterface*> MaterialsToSwap;
		for (auto It = MaterialsInUse_GameThread.CreateConstIterator(); It; ++It)
		{
			UMaterialInterface* Material = *It;
			if (Material)
			{
				const bool bCheckSkelUsage = Material->CheckMaterialUsage_Concurrent(MATUSAGE_SkeletalMesh);
				if (!bCheckSkelUsage)
				{
					MaterialsToSwap.Add(Material);
				}
				else if(MaterialUsingMorphTarget.Contains(Material))
				{
					const bool bCheckMorphUsage = !bMaterialsNeedMorphUsage_GameThread || (bMaterialsNeedMorphUsage_GameThread && Material->CheckMaterialUsage_Concurrent(MATUSAGE_MorphTargets));
					// make sure morph material usage and default skeletal usage are both valid
					if (!bCheckMorphUsage)
					{
						MaterialsToSwap.Add(Material);
					}
				}
			}
		}

		// update the new LODSections on the render thread proxy
		if (MaterialsToSwap.Num())
		{
			TSet<UMaterialInterface*> InMaterialsToSwap = MaterialsToSwap;
			UMaterialInterface* DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
			ERHIFeatureLevel::Type InFeatureLevel = GetScene().GetFeatureLevel();
			FSkeletalMeshSceneProxy* SkelMeshSceneProxy = this;
			FMaterialRelevance DefaultRelevance = DefaultMaterial->GetRelevance(InFeatureLevel);
			ENQUEUE_RENDER_COMMAND(UpdateSkelProxyLODSectionElementsCmd)(
				[InMaterialsToSwap, DefaultMaterial, DefaultRelevance, InFeatureLevel, SkelMeshSceneProxy](FRHICommandList& RHICmdList)
				{
					for( int32 LodIdx=0; LodIdx < SkelMeshSceneProxy->LODSections.Num(); LodIdx++ )
					{
						FLODSectionElements& LODSection = SkelMeshSceneProxy->LODSections[LodIdx];
						for( int32 SectIdx=0; SectIdx < LODSection.SectionElements.Num(); SectIdx++ )
						{
							FSectionElementInfo& SectionElement = LODSection.SectionElements[SectIdx];
							if( InMaterialsToSwap.Contains(SectionElement.Material) )
							{
								// fallback to default material if needed
								SectionElement.Material = DefaultMaterial;
							}
						}
					}
					SkelMeshSceneProxy->MaterialRelevance |= DefaultRelevance;
				});
		}
	}
}

#if WITH_EDITORONLY_DATA

bool FSkeletalMeshSceneProxy::GetPrimitiveDistance(int32 LODIndex, int32 SectionIndex, const FVector& ViewOrigin, float& PrimitiveDistance) const
{

	if (FPrimitiveSceneProxy::GetPrimitiveDistance(LODIndex, SectionIndex, ViewOrigin, PrimitiveDistance))
	{
		const float OneOverDistanceMultiplier = 1.f / FMath::Max<float>(UE_SMALL_NUMBER, StreamingDistanceMultiplier);
		PrimitiveDistance *= OneOverDistanceMultiplier;
		return true;
	}
	return false;
}

bool FSkeletalMeshSceneProxy::GetMeshUVDensities(int32 LODIndex, int32 SectionIndex, FVector4& WorldUVDensities) const
{
	if (LODSections.IsValidIndex(LODIndex) && LODSections[LODIndex].SectionElements.IsValidIndex(SectionIndex))
	{
		// The LOD-section data is stored per material index as it is only used for texture streaming currently.
		const int32 MaterialIndex = LODSections[LODIndex].SectionElements[SectionIndex].UseMaterialIndex;
		if (SkeletalMeshRenderData && SkeletalMeshRenderData->UVChannelDataPerMaterial.IsValidIndex(MaterialIndex))
		{
			const float TransformScale = GetLocalToWorld().GetMaximumAxisScale();
			const float* LocalUVDensities = SkeletalMeshRenderData->UVChannelDataPerMaterial[MaterialIndex].LocalUVDensities;

			WorldUVDensities.Set(
				LocalUVDensities[0] * TransformScale,
				LocalUVDensities[1] * TransformScale,
				LocalUVDensities[2] * TransformScale,
				LocalUVDensities[3] * TransformScale);
			
			return true;
		}
	}
	return FPrimitiveSceneProxy::GetMeshUVDensities(LODIndex, SectionIndex, WorldUVDensities);
}

bool FSkeletalMeshSceneProxy::GetMaterialTextureScales(int32 LODIndex, int32 SectionIndex, const FMaterialRenderProxy* MaterialRenderProxy, FVector4f* OneOverScales, FIntVector4* UVChannelIndices) const
	{
	if (LODSections.IsValidIndex(LODIndex) && LODSections[LODIndex].SectionElements.IsValidIndex(SectionIndex))
	{
		const UMaterialInterface* Material = LODSections[LODIndex].SectionElements[SectionIndex].Material;
		if (Material)
		{
			// This is thread safe because material texture data is only updated while the renderthread is idle.
			for (const FMaterialTextureInfo& TextureData : Material->GetTextureStreamingData())
			{
				const int32 TextureIndex = TextureData.TextureIndex;
				if (TextureData.IsValid(true))
				{
					OneOverScales[TextureIndex / 4][TextureIndex % 4] = 1.f / TextureData.SamplingScale;
					UVChannelIndices[TextureIndex / 4][TextureIndex % 4] = TextureData.UVChannelIndex;
				}
			}
			for (const FMaterialTextureInfo& TextureData : Material->TextureStreamingDataMissingEntries)
			{
				const int32 TextureIndex = TextureData.TextureIndex;
				if (TextureIndex >= 0 && TextureIndex < TEXSTREAM_MAX_NUM_TEXTURES_PER_MATERIAL)
				{
					OneOverScales[TextureIndex / 4][TextureIndex % 4] = 1.f;
					UVChannelIndices[TextureIndex / 4][TextureIndex % 4] = 0;
				}
			}
			return true;
		}
	}
	return false;
}
#endif

void FSkeletalMeshSceneProxy::OnTransformChanged()
{
	// OnTransformChanged is called on the following frame after FSkeletalMeshObject::Update(), thus omit '+ 1' to frame number.
	MeshObject->SetTransform(GetLocalToWorld(), GetScene().GetFrameNumber());
	MeshObject->RefreshClothingTransforms(GetLocalToWorld(), GetScene().GetFrameNumber());

	// Update the default-instance
	UpdateDefaultInstanceSceneData();
}

FSkinnedMeshComponentRecreateRenderStateContext::FSkinnedMeshComponentRecreateRenderStateContext(USkeletalMesh* InSkeletalMesh, bool InRefreshBounds /*= false*/)
	: bRefreshBounds(InRefreshBounds)
{
	bool bRequireFlushRenderingCommands = false;
	for (TObjectIterator<USkeletalMeshComponent> It; It; ++It)
	{
		if (It->GetSkeletalMeshAsset() == InSkeletalMesh)
		{
			checkf(!It->IsUnreachable(), TEXT("%s"), *It->GetFullName());

			if (It->IsRenderStateCreated())
			{
				check(It->IsRegistered());
				It->DestroyRenderState_Concurrent();
				bRequireFlushRenderingCommands = true;
			}
				MeshComponents.Add(*It);
			}
		}

	// Flush the rendering commands generated by the detachments.
	// The static mesh scene proxies reference the UStaticMesh, and this ensures that they are cleaned up before the UStaticMesh changes.
	if (bRequireFlushRenderingCommands)
	{
		FlushRenderingCommands();
	}
}

FSkinnedMeshComponentRecreateRenderStateContext::~FSkinnedMeshComponentRecreateRenderStateContext()
{
	const int32 ComponentCount = MeshComponents.Num();
	for (int32 ComponentIndex = 0; ComponentIndex < ComponentCount; ++ComponentIndex)
	{
		if(USkinnedMeshComponent* Component = MeshComponents[ComponentIndex].Get())
		{
			if (bRefreshBounds)
			{
				Component->UpdateBounds();
			}

			if (Component->IsRegistered() && !Component->IsRenderStateCreated() && Component->ShouldCreateRenderState())
			{
				Component->CreateRenderState_Concurrent(nullptr);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////

FVector GetRefVertexLocationTyped(
	const USkeletalMesh* Mesh,
	const FSkelMeshRenderSection& Section,
	const FPositionVertexBuffer& PositionBuffer,
	const FSkinWeightVertexBuffer& SkinWeightVertexBuffer,
	const int32 VertIndex
)
{
	FVector SkinnedPos(0, 0, 0);

	// Do soft skinning for this vertex.
	int32 BufferVertIndex = Section.GetVertexBufferIndex() + VertIndex;
	int32 MaxBoneInfluences = SkinWeightVertexBuffer.GetMaxBoneInfluences();

#if !PLATFORM_LITTLE_ENDIAN
	// uint8[] elements in LOD.VertexBufferGPUSkin have been swapped for VET_UBYTE4 vertex stream use
	for (int32 InfluenceIndex = MAX_INFLUENCES - 1; InfluenceIndex >= MAX_INFLUENCES - MaxBoneInfluences; InfluenceIndex--)
#else
	for (int32 InfluenceIndex = 0; InfluenceIndex < MaxBoneInfluences; InfluenceIndex++)
#endif
	{
		const float	Weight = (float)SkinWeightVertexBuffer.GetBoneWeight(BufferVertIndex, InfluenceIndex) / 255.0f;
		{
			const FMatrix BoneTransformMatrix = FMatrix::Identity;
			SkinnedPos += BoneTransformMatrix.TransformPosition((FVector)PositionBuffer.VertexPosition(BufferVertIndex)) * Weight;
		}
	}

	return SkinnedPos;
}

FVector GetSkeletalMeshRefVertLocation(const USkeletalMesh* Mesh, const FSkeletalMeshLODRenderData& LODData, const FSkinWeightVertexBuffer& SkinWeightVertexBuffer, const int32 VertIndex)
{
	int32 SectionIndex;
	int32 VertIndexInChunk;
	LODData.GetSectionFromVertexIndex(VertIndex, SectionIndex, VertIndexInChunk);
	const FSkelMeshRenderSection& Section = LODData.RenderSections[SectionIndex];
	return GetRefVertexLocationTyped(Mesh, Section, LODData.StaticVertexBuffers.PositionVertexBuffer, SkinWeightVertexBuffer, VertIndexInChunk);
}

//GetRefTangentBasisTyped
void GetRefTangentBasisTyped(const USkeletalMesh* Mesh, const FSkelMeshRenderSection& Section, const FStaticMeshVertexBuffer& StaticVertexBuffer, const FSkinWeightVertexBuffer& SkinWeightVertexBuffer, const int32 VertIndex, FVector3f& OutTangentX, FVector3f& OutTangentY, FVector3f& OutTangentZ)
{
	OutTangentX = FVector3f::ZeroVector;
	OutTangentY = FVector3f::ZeroVector;
	OutTangentZ = FVector3f::ZeroVector;

	// Do soft skinning for this vertex.
	const int32 BufferVertIndex = Section.GetVertexBufferIndex() + VertIndex;
	const int32 MaxBoneInfluences = SkinWeightVertexBuffer.GetMaxBoneInfluences();

	const FVector3f VertexTangentX = StaticVertexBuffer.VertexTangentX(BufferVertIndex);
	const FVector3f VertexTangentY = StaticVertexBuffer.VertexTangentY(BufferVertIndex);
	const FVector3f VertexTangentZ = StaticVertexBuffer.VertexTangentZ(BufferVertIndex);

#if !PLATFORM_LITTLE_ENDIAN
	// uint8[] elements in LOD.VertexBufferGPUSkin have been swapped for VET_UBYTE4 vertex stream use
	for (int32 InfluenceIndex = MAX_INFLUENCES - 1; InfluenceIndex >= MAX_INFLUENCES - MaxBoneInfluences; InfluenceIndex--)
#else
	for (int32 InfluenceIndex = 0; InfluenceIndex < MaxBoneInfluences; InfluenceIndex++)
#endif
	{
		const float	Weight = (float)SkinWeightVertexBuffer.GetBoneWeight(BufferVertIndex, InfluenceIndex) / 255.0f;
		const FMatrix44f BoneTransformMatrix = FMatrix44f::Identity;
		OutTangentX += BoneTransformMatrix.TransformVector(VertexTangentX) * Weight;
		OutTangentY += BoneTransformMatrix.TransformVector(VertexTangentY) * Weight;
		OutTangentZ += BoneTransformMatrix.TransformVector(VertexTangentZ) * Weight;
	}
}

void GetSkeletalMeshRefTangentBasis(const USkeletalMesh* Mesh, const FSkeletalMeshLODRenderData& LODData, const FSkinWeightVertexBuffer& SkinWeightVertexBuffer, const int32 VertIndex, FVector3f& OutTangentX, FVector3f& OutTangentY, FVector3f& OutTangentZ)
{
	int32 SectionIndex;
	int32 VertIndexInChunk;
	LODData.GetSectionFromVertexIndex(VertIndex, SectionIndex, VertIndexInChunk);
	const FSkelMeshRenderSection& Section = LODData.RenderSections[SectionIndex];
	GetRefTangentBasisTyped(Mesh, Section, LODData.StaticVertexBuffers.StaticMeshVertexBuffer, SkinWeightVertexBuffer, VertIndexInChunk, OutTangentX, OutTangentY, OutTangentZ);
}

#undef LOCTEXT_NAMESPACE

