// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PrimitiveComponent.cpp: Primitive component implementation.
=============================================================================*/

#include "Components/PrimitiveComponent.h"

#include "Chaos/ChaosEngineInterface.h"
#include "Chaos/PhysicsObjectCollisionInterface.h"
#include "ChaosInterfaceWrapperCore.h"
#include "Collision/CollisionConversions.h"
#include "Engine/HitResult.h"
#include "Engine/Level.h"
#include "Engine/OverlapResult.h"
#include "Engine/Texture.h"
#include "GameFramework/DamageType.h"
#include "EngineStats.h"
#include "GameFramework/Pawn.h"
#include "HLOD/HLODBatchingPolicy.h"
#include "PSOPrecache.h"
#include "AI/NavigationSystemBase.h"
#include "AI/Navigation/NavigationRelevantData.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PhysicsVolume.h"
#include "GameFramework/WorldSettings.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/SkeletalMesh.h"
#include "HitProxies.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ContentStreaming.h"
#include "PropertyPairsMap.h"
#include "UnrealEngine.h"
#include "PhysicsEngine/BodySetup.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"
#include "Misc/MapErrors.h"
#include "CollisionDebugDrawingPublic.h"
#include "GameFramework/CheatManager.h"
#include "PhysicsEngine/PhysicsObjectExternalInterface.h"
#include "RenderingThread.h"
#include "Streaming/TextureStreamingHelpers.h"
#include "PrimitiveSceneProxy.h"
#include "SceneInterface.h"
#include "UObject/FortniteMainBranchObjectVersion.h"
#include "UObject/UE5PrivateFrostyStreamObjectVersion.h"
#include "UObject/ObjectSaveContext.h"
#include "Engine/DamageEvents.h"
#include "MeshUVChannelInfo.h"
#include "PrimitiveSceneDesc.h"
#include "PSOPrecacheMaterial.h"
#include "MaterialCachedData.h"
#include "MaterialShared.h"
#include "MarkActorRenderStateDirtyTask.h"

#if WITH_EDITOR
#include "Engine/LODActor.h"
#include "Interfaces/ITargetPlatform.h"
#include "Rendering/StaticLightingSystemInterface.h"
#else
#include "Components/InstancedStaticMeshComponent.h"
#endif // WITH_EDITOR

#if DO_CHECK
#include "Engine/StaticMesh.h"
#endif

#define LOCTEXT_NAMESPACE "PrimitiveComponent"

CSV_DEFINE_CATEGORY(PrimitiveComponent, false);

//////////////////////////////////////////////////////////////////////////
// Globals

namespace PrimitiveComponentStatics
{
	static const FText MobilityWarnText = LOCTEXT("InvalidMove", "move");
}

typedef TArray<const FOverlapInfo*, TInlineAllocator<8>> TInlineOverlapPointerArray;

DEFINE_LOG_CATEGORY_STATIC(LogPrimitiveComponent, Log, All);

static int32 AlwaysCreatePhysicsStateConversionHackCVar = 0;
static FAutoConsoleVariableRef CVarAlwaysCreatePhysicsStateConversionHack(
	TEXT("p.AlwaysCreatePhysicsStateConversionHack"),
	AlwaysCreatePhysicsStateConversionHackCVar,
	TEXT("Hack to convert actors with query and ignore all to always create physics."),
	ECVF_Default);

namespace PrimitiveComponentCVars
{
	int32 bAllowCachedOverlapsCVar = 1;
	static FAutoConsoleVariableRef CVarAllowCachedOverlaps(
		TEXT("p.AllowCachedOverlaps"),
		bAllowCachedOverlapsCVar,
		TEXT("Primitive Component physics\n")
		TEXT("0: disable cached overlaps, 1: enable (default)"),
		ECVF_Default);

	float InitialOverlapToleranceCVar = 0.0f;
	static FAutoConsoleVariableRef CVarInitialOverlapTolerance(
		TEXT("p.InitialOverlapTolerance"),
		InitialOverlapToleranceCVar,
		TEXT("Tolerance for initial overlapping test in PrimitiveComponent movement.\n")
		TEXT("Normals within this tolerance are ignored if moving out of the object.\n")
		TEXT("Dot product of movement direction and surface normal."),
		ECVF_Default);

	float HitDistanceToleranceCVar = 0.0f;
	static FAutoConsoleVariableRef CVarHitDistanceTolerance(
		TEXT("p.HitDistanceTolerance"),
		HitDistanceToleranceCVar,
		TEXT("Tolerance for hit distance for overlap test in PrimitiveComponent movement.\n")
		TEXT("Hits that are less than this distance are ignored."),
		ECVF_Default);

	int32 bEnableFastOverlapCheck = 1;
	static FAutoConsoleVariableRef CVarEnableFastOverlapCheck(TEXT("p.EnableFastOverlapCheck"), bEnableFastOverlapCheck, TEXT("Enable fast overlap check against sweep hits, avoiding UpdateOverlaps (for the swept component)."));
}

namespace PhysicsReplicationCVars
{
	namespace PredictiveInterpolationCVars
	{
		static bool bFakeTargetOnClientWakeUp = false;
		static FAutoConsoleVariableRef CVarFakeTargetOnClientWakeUp(TEXT("np2.PredictiveInterpolation.FakeTargetOnClientWakeUp"), bFakeTargetOnClientWakeUp, TEXT("When true, predictive interpolation will fake a replication target at the current transform marked as asleep, this target only apply if the client doesn't receive targets from the server. This stops the client from desyncing from the server if being woken up by mistake"));
	}
}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
int32 CVarShowInitialOverlaps = 0;
FAutoConsoleVariableRef CVarRefShowInitialOverlaps(
	TEXT("p.ShowInitialOverlaps"),
	CVarShowInitialOverlaps,
	TEXT("Show initial overlaps when moving a component, including estimated 'exit' direction.\n")
	TEXT(" 0:off, otherwise on"),
	ECVF_Cheat);
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

DEFINE_STAT(STAT_BeginComponentOverlap);
DEFINE_STAT(STAT_MoveComponent_FastOverlap);

DECLARE_CYCLE_STAT(TEXT("EndComponentOverlap"), STAT_EndComponentOverlap, STATGROUP_Game);
DECLARE_CYCLE_STAT(TEXT("PrimComp DispatchBlockingHit"), STAT_DispatchBlockingHit, STATGROUP_Game);

FOverlapInfo::FOverlapInfo(UPrimitiveComponent* InComponent, int32 InBodyIndex)
	: bFromSweep(false)
{
	if (InComponent)
	{
		OverlapInfo.HitObjectHandle = FActorInstanceHandle(InComponent->GetOwner(), InComponent, InBodyIndex);
	}
	OverlapInfo.Component = InComponent;
	OverlapInfo.Item = InBodyIndex;
}

// Helper for finding the index of an FOverlapInfo in an Array using the FFastOverlapInfoCompare predicate, knowing that at least one overlap is valid (non-null).
template<class AllocatorType>
FORCEINLINE_DEBUGGABLE int32 IndexOfOverlapFast(const TArray<FOverlapInfo, AllocatorType>& OverlapArray, const FOverlapInfo& SearchItem)
{
	return OverlapArray.IndexOfByPredicate(FFastOverlapInfoCompare(SearchItem));
}

// Version that works with arrays of pointers and pointers to search items.
template<class AllocatorType>
FORCEINLINE_DEBUGGABLE int32 IndexOfOverlapFast(const TArray<const FOverlapInfo*, AllocatorType>& OverlapPtrArray, const FOverlapInfo* SearchItem)
{
	return OverlapPtrArray.IndexOfByPredicate(FFastOverlapInfoCompare(*SearchItem));
}

// Helper for adding an FOverlapInfo uniquely to an Array, using IndexOfOverlapFast and knowing that at least one overlap is valid (non-null).
template<class AllocatorType>
FORCEINLINE_DEBUGGABLE void AddUniqueOverlapFast(TArray<FOverlapInfo, AllocatorType>& OverlapArray, FOverlapInfo& NewOverlap)
{
	if (IndexOfOverlapFast(OverlapArray, NewOverlap) == INDEX_NONE)
	{
		OverlapArray.Add(NewOverlap);
	}
}

template<class AllocatorType>
FORCEINLINE_DEBUGGABLE void AddUniqueOverlapFast(TArray<FOverlapInfo, AllocatorType>& OverlapArray, FOverlapInfo&& NewOverlap)
{
	if (IndexOfOverlapFast(OverlapArray, NewOverlap) == INDEX_NONE)
	{
		OverlapArray.Add(NewOverlap);
	}
}

// Helper to see if two components can possibly generate overlaps with each other.
FORCEINLINE_DEBUGGABLE static bool CanComponentsGenerateOverlap(const UPrimitiveComponent* MyComponent, /*const*/ UPrimitiveComponent* OtherComp)
{
	return OtherComp
		&& OtherComp->GetGenerateOverlapEvents()
		&& MyComponent
		&& MyComponent->GetGenerateOverlapEvents()
		&& MyComponent->GetCollisionResponseToComponent(OtherComp) == ECR_Overlap;
}

// Predicate to identify components from overlaps array that can overlap
struct FPredicateFilterCanOverlap
{
	FPredicateFilterCanOverlap(const UPrimitiveComponent& OwningComponent)
		: MyComponent(OwningComponent)
	{
	}

	bool operator() (const FOverlapInfo& Info) const
	{
		return CanComponentsGenerateOverlap(&MyComponent, Info.OverlapInfo.GetComponent());
	}

private:
	const UPrimitiveComponent& MyComponent;
};

// Predicate to identify components from overlaps array that can no longer overlap
struct FPredicateFilterCannotOverlap
{
	FPredicateFilterCannotOverlap(const UPrimitiveComponent& OwningComponent)
	: MyComponent(OwningComponent)
	{
	}

	bool operator() (const FOverlapInfo& Info) const
	{
		return !CanComponentsGenerateOverlap(&MyComponent, Info.OverlapInfo.GetComponent());
	}

private:
	const UPrimitiveComponent& MyComponent;
};

// Helper to initialize an array to point to data members in another array.
template <class ElementType, class AllocatorType1, class AllocatorType2>
FORCEINLINE_DEBUGGABLE static void GetPointersToArrayData(TArray<const ElementType*, AllocatorType1>& Pointers, const TArray<ElementType, AllocatorType2>& DataArray)
{
	const int32 NumItems = DataArray.Num();
	Pointers.SetNumUninitialized(NumItems);
	for (int32 i=0; i < NumItems; i++)
	{
		Pointers[i] = &(DataArray[i]);
	}
}

template <class ElementType, class AllocatorType1>
FORCEINLINE_DEBUGGABLE static void GetPointersToArrayData(TArray<const ElementType*, AllocatorType1>& Pointers, const TArrayView<const ElementType>& DataArray)
{
	const int32 NumItems = DataArray.Num();
	Pointers.SetNumUninitialized(NumItems);
	for (int32 i=0; i < NumItems; i++)
	{
		Pointers[i] = &(DataArray[i]);
	}
}

// Helper to initialize an array to point to data members in another array which satisfy a predicate.
template <class ElementType, class AllocatorType1, class AllocatorType2, typename PredicateT>
FORCEINLINE_DEBUGGABLE static void GetPointersToArrayDataByPredicate(TArray<const ElementType*, AllocatorType1>& Pointers, const TArray<ElementType, AllocatorType2>& DataArray, PredicateT Predicate)
{
	Pointers.Reserve(Pointers.Num() + DataArray.Num());
	for (const ElementType& Item : DataArray)
	{
		if (Invoke(Predicate, Item))
		{
			Pointers.Add(&Item);
		}
	}
}

template <class ElementType, class AllocatorType1, typename PredicateT>
FORCEINLINE_DEBUGGABLE static void GetPointersToArrayDataByPredicate(TArray<const ElementType*, AllocatorType1>& Pointers, const TArrayView<const ElementType>& DataArray, PredicateT Predicate)
{
	Pointers.Reserve(Pointers.Num() + DataArray.Num());
	for (const ElementType& Item : DataArray)
	{
		if (Invoke(Predicate, Item))
		{
			Pointers.Add(&Item);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// PRIMITIVE COMPONENT
///////////////////////////////////////////////////////////////////////////////

uint32 UPrimitiveComponent::GlobalOverlapEventsCounter = 0;

FName UPrimitiveComponent::RVTActorDescProperty(TEXT("RVT"));

UPrimitiveComponent::UPrimitiveComponent(FVTableHelper& Helper) : Super(Helper) { }

PRAGMA_DISABLE_DEPRECATION_WARNINGS;
UPrimitiveComponent::~UPrimitiveComponent() = default;
PRAGMA_ENABLE_DEPRECATION_WARNINGS;

UPrimitiveComponent::UPrimitiveComponent(const FObjectInitializer& ObjectInitializer /*= FObjectInitializer::Get()*/)
	: Super(ObjectInitializer)
{
	OcclusionBoundsSlack = 0.f;
	BoundsScale = 1.0f;
	MinDrawDistance = 0.0f;
	DepthPriorityGroup = SDPG_World;
	bAllowCullDistanceVolume = true;
	bUseAsOccluder = false;
	bReceivesDecals = true;
	CastShadow = false;
	bEmissiveLightSource = false;
	bCastDynamicShadow = true;
	bAffectDynamicIndirectLighting = true;
	bAffectDistanceFieldLighting = true;
	bCastStaticShadow = true;
	ShadowCacheInvalidationBehavior = EShadowCacheInvalidationBehavior::Auto;
	bCastVolumetricTranslucentShadow = false;
	bCastContactShadow = true;
	IndirectLightingCacheQuality = ILCQ_Point;
	bStaticWhenNotMoveable = true;
	bSelectable = true;
#if WITH_EDITORONLY_DATA
	bConsiderForActorPlacementWhenHidden = false;
#endif // WITH_EDITORONLY_DATA
	bFillCollisionUnderneathForNavmesh = false;
	AlwaysLoadOnClient = true;
	AlwaysLoadOnServer = true;
	SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	bAlwaysCreatePhysicsState = false;
	bVisibleInReflectionCaptures = true;
	bVisibleInRealTimeSkyCaptures = true;
	bVisibleInRayTracing = true;
	bRenderInMainPass = true;
	bRenderInDepthPass = true;
	VisibilityId = INDEX_NONE;
#if WITH_EDITORONLY_DATA
	CanBeCharacterBase_DEPRECATED = ECB_Yes;
#endif
	CanCharacterStepUpOn = ECB_Yes;
	CustomDepthStencilValue = 0;
	CustomDepthStencilWriteMask = ERendererStencilMask::ERSM_Default;
	RayTracingGroupId = FPrimitiveSceneProxy::InvalidRayTracingGroupId;
	RayTracingGroupCullingPriority = ERayTracingGroupCullingPriority::CP_4_DEFAULT;
	bRayTracingFarField = false;

	LDMaxDrawDistance = 0.f;
	CachedMaxDrawDistance = 0.f;

	bEnableAutoLODGeneration = true;
	HLODBatchingPolicy = EHLODBatchingPolicy::None;
	ExcludeFromHLODLevels = 0;

#if WITH_EDITORONLY_DATA
	bUseMaxLODAsImposter_DEPRECATED = false;
	bBatchImpostersAsInstances_DEPRECATED = false;
#endif
	bIsValidTextureStreamingBuiltData = false;
	bNeverDistanceCull = false;

	bUseEditorCompositing = false;
	bIsBeingMovedByEditor = false;

	SetGenerateOverlapEvents(true);
	bMultiBodyOverlap = false;
	bReturnMaterialOnMove = false;
	bCanEverAffectNavigation = false;
	bNavigationRelevant = false;

	bWantsOnUpdateTransform = true;

	bCachedAllCollideableDescendantsRelative = false;
	bAttachedToStreamingManagerAsStatic = false;
	bAttachedToStreamingManagerAsDynamic = false;
	bHandledByStreamingManagerAsDynamic = false;
	bIgnoreStreamingManagerUpdate = false;
	bAttachedToCoarseMeshStreamingManager = false;
	bBulkReregister = false;
	LastCheckedAllCollideableDescendantsTime = 0.f;

#if UE_WITH_PSO_PRECACHING
	bPSOPrecacheCalled = false;
	bPSOPrecacheRequestBoosted = false;
#endif // UE_WITH_PSO_PRECACHING
	
	bApplyImpulseOnDamage = true;
	bReplicatePhysicsToAutonomousProxy = true;

	bReceiveMobileCSMShadows = true;

#if WITH_EDITOR
	bAlwaysAllowTranslucentSelect = false;

	SelectionOutlineColorIndex = 0;
#endif

#if WITH_EDITORONLY_DATA
	HitProxyPriority = HPP_World;
#endif // WITH_EDITORONLY_DATA

	bIgnoreBoundsForEditorFocus = false;
	bVisibleInSceneCaptureOnly = false;
	bHiddenInSceneCapture = false;
}

bool UPrimitiveComponent::UsesOnlyUnlitMaterials() const
{
	return false;
}


bool UPrimitiveComponent::GetLightMapResolution( int32& Width, int32& Height ) const
{
	Width	= 0;
	Height	= 0;
	return false;
}


void UPrimitiveComponent::GetLightAndShadowMapMemoryUsage( int32& LightMapMemoryUsage, int32& ShadowMapMemoryUsage ) const
{
	LightMapMemoryUsage		= 0;
	ShadowMapMemoryUsage	= 0;
	return;
}

void UPrimitiveComponent::InvalidateLightingCacheDetailed(bool bInvalidateBuildEnqueuedLighting, bool bTranslationOnly) 
{
	// If a static lighting build has been enqueued for this primitive, don't stomp on its visibility ID.
	if (bInvalidateBuildEnqueuedLighting)
	{
		VisibilityId = INDEX_NONE;
	}

	Super::InvalidateLightingCacheDetailed(bInvalidateBuildEnqueuedLighting, bTranslationOnly);
}

bool UPrimitiveComponent::IsEditorOnly() const
{
	return Super::IsEditorOnly() || ((AlwaysLoadOnClient == false) && (AlwaysLoadOnServer == false));
}

bool UPrimitiveComponent::HasStaticLighting() const
{
	return ((Mobility == EComponentMobility::Static) || LightmapType == ELightmapType::ForceSurface) && SupportsStaticLighting();
}

void UPrimitiveComponent::GetStreamingRenderAssetInfo(FStreamingTextureLevelContext& LevelContext, TArray<FStreamingRenderAssetPrimitiveInfo>& OutStreamingRenderAssets) const
{
	if (CanSkipGetTextureStreamingRenderAssetInfo())
	{
		return;
	}

	if (CVarStreamingUseNewMetrics.GetValueOnGameThread() != 0)
	{
		LevelContext.BindBuildData(nullptr);

		TArray<UMaterialInterface*> UsedMaterials;
		GetUsedMaterials(UsedMaterials);

		if (UsedMaterials.Num())
		{
			// As we have no idea what this component is doing, we assume something very conservative
			// by specifying that the texture is stretched across the bounds. To do this, we use a density of 1
			// while also specifying the component scale as the bound radius. 
			// Note that material UV scaling will  still apply.
			static const FMeshUVChannelInfo UVChannelData(1.f);

			FPrimitiveMaterialInfo MaterialData;
			MaterialData.PackedRelativeBox = PackedRelativeBox_Identity;
			MaterialData.UVChannelData = &UVChannelData;

			while (UsedMaterials.Num())
			{
				UMaterialInterface* MaterialInterface = UsedMaterials[0];
				if (MaterialInterface)
				{
					MaterialData.Material = MaterialInterface;
					LevelContext.ProcessMaterial(Bounds, MaterialData, Bounds.SphereRadius, OutStreamingRenderAssets, bIsValidTextureStreamingBuiltData, this);
				}
				// Remove all instances of this material in case there were duplicates.
				UsedMaterials.RemoveSwap(MaterialInterface);
			}
		}
	}
}

bool UPrimitiveComponent::BuildTextureStreamingDataImpl(ETextureStreamingBuildType BuildType, EMaterialQualityLevel::Type QualityLevel, ERHIFeatureLevel::Type FeatureLevel, TSet<FGuid>& DependentResources, bool& bOutSupportsBuildTextureStreamingData)
{
	// Default implementation marks component as having invalid texture streaming built data
	bOutSupportsBuildTextureStreamingData = false;
	return true;
}

bool UPrimitiveComponent::BuildTextureStreamingData(ETextureStreamingBuildType BuildType, EMaterialQualityLevel::Type QualityLevel, ERHIFeatureLevel::Type FeatureLevel, TSet<FGuid>& DependentResources)
{
	bool bSupportsBuildTextureStreamingData = false;
	bIsValidTextureStreamingBuiltData = false;
#if WITH_EDITOR
	bIsActorTextureStreamingBuiltData = false;
#endif
	
	bool Result = BuildTextureStreamingDataImpl(BuildType, QualityLevel, FeatureLevel, DependentResources, bSupportsBuildTextureStreamingData);
	if (Result && bSupportsBuildTextureStreamingData)
	{
		if (BuildType == TSB_MapBuild)
		{
			bIsValidTextureStreamingBuiltData = true;
		}
#if WITH_EDITOR
		else if (BuildType == TSB_ActorBuild)
		{
			bIsActorTextureStreamingBuiltData = true;
		}
#endif
	}
	return Result;
}

void UPrimitiveComponent::GetStreamingRenderAssetInfoWithNULLRemoval(FStreamingTextureLevelContext& LevelContext, TArray<struct FStreamingRenderAssetPrimitiveInfo>& OutStreamingRenderAssets) const
{
	// Ignore components that are fully initialized but have no scene proxy (hidden primitive or non game primitive)
	if (!IsRegistered() || !IsRenderStateCreated() || SceneProxy)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UPrimitiveComponent::GetStreamingRenderAssetInfoWithNULLRemoval);

		GetStreamingRenderAssetInfo(LevelContext, OutStreamingRenderAssets);
		for (int32 Index = 0; Index < OutStreamingRenderAssets.Num(); Index++)
		{
			const FStreamingRenderAssetPrimitiveInfo& Info = OutStreamingRenderAssets[Index];
			if (!Info.RenderAsset || !Info.RenderAsset->IsStreamable())
			{
				OutStreamingRenderAssets.RemoveAtSwap(Index--);
			}
			else
			{
#if DO_CHECK
				ensure(Info.TexelFactor >= 0.f
					|| Info.RenderAsset->IsA<UStaticMesh>()
					|| Info.RenderAsset->IsA<USkeletalMesh>()
					|| (Info.RenderAsset->IsA<UTexture>() && Info.RenderAsset->GetLODGroupForStreaming() == TEXTUREGROUP_Terrain_Heightmap));
#endif

				// Other wise check that everything is setup right. If the component is not yet registered, then the bound data is irrelevant.
				const bool bCanBeStreamedByDistance = Info.CanBeStreamedByDistance(IsRegistered());

				if (!bForceMipStreaming && !bCanBeStreamedByDistance && Info.TexelFactor >= 0.f)
				{
					OutStreamingRenderAssets.RemoveAtSwap(Index--);
				}
			}
		}
	}
}

void UPrimitiveComponent::GetUsedTextures(TArray<UTexture*>& OutTextures, EMaterialQualityLevel::Type QualityLevel)
{
	// Get the used materials so we can get their textures
	TArray<UMaterialInterface*> UsedMaterials;
	GetUsedMaterials( UsedMaterials );

	TArray<UTexture*> UsedTextures;
	for( int32 MatIndex = 0; MatIndex < UsedMaterials.Num(); ++MatIndex )
	{
		// Ensure we don't have any NULL elements.
		if( UsedMaterials[ MatIndex ] )
		{
			auto World = GetWorld();

			UsedTextures.Reset();
			UsedMaterials[MatIndex]->GetUsedTextures(UsedTextures, QualityLevel, false, World ? World->GetFeatureLevel() : GMaxRHIFeatureLevel, false);

			for( int32 TextureIndex=0; TextureIndex<UsedTextures.Num(); TextureIndex++ )
			{
				OutTextures.AddUnique( UsedTextures[TextureIndex] );
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Render

void UPrimitiveComponent::CreateRenderState_Concurrent(FRegisterComponentContext* Context)
{
	// Make sure cached cull distance is up-to-date if its zero and we have an LD cull distance
	if( CachedMaxDrawDistance == 0.f && LDMaxDrawDistance > 0.f )
	{
		bool bNeverCull = bNeverDistanceCull || GetLODParentPrimitive();
		CachedMaxDrawDistance = bNeverCull ? 0.f : LDMaxDrawDistance;
	}

	// Always setup our ptr to the OwnerLastRenderTimer for rendering time feedback from the renderer
	// The owner can change after calls to OnRegister so we must resynchronize this value
	SceneData.OwnerLastRenderTimePtr = FActorLastRenderTime::GetPtr(GetOwner());

	Super::CreateRenderState_Concurrent(Context);

	UpdateBounds();

	// If the primitive isn't hidden and the detail mode setting allows it, add it to the scene.
	if (ShouldComponentAddToScene()
#if WITH_EDITOR
		// [HOTFIX] When force deleting an asset, a SceneProxy is set to null from a different thread unsafely, causing the old stale value of SceneProxy being read here from the cache.
		// We need to better investigate why this happens, but for now this prevents a crash from occurring.
		&& SceneProxy == nullptr
#endif
	)
	{
		if (Context != nullptr)
		{
			Context->AddPrimitive(this);
		}
		else
		{
			GetWorld()->Scene->AddPrimitive(this);
		}
	}

	ConditionalNotifyStreamingPrimitiveUpdated_Concurrent();
}

void UPrimitiveComponent::SendRenderTransform_Concurrent()
{
	UpdateBounds();

	// If the primitive isn't hidden update its transform.
	const bool bDetailModeAllowsRendering	= DetailMode <= GetCachedScalabilityCVars().DetailMode;
	if( bDetailModeAllowsRendering && (ShouldRender() || bCastHiddenShadow || bAffectIndirectLightingWhileHidden || bRayTracingFarField))
	{
		// Update the scene info's transform for this primitive.
		GetWorld()->Scene->UpdatePrimitiveTransform(this);
	}

	Super::SendRenderTransform_Concurrent();
}

void UPrimitiveComponent::OnRegister()
{
	// Both those are initalized before call Super::OnRegister since the primitive can be added to the scene
	// before this method completes, for example through FNiagaraSystem::PollForCompilationComplete()
	 
	// Setup our ptr to the OwnerLastRenderTimer for rendering time feedback from the renderer
	SceneData.OwnerLastRenderTimePtr = FActorLastRenderTime::GetPtr(GetOwner());
	
	// Deterministically track primitives via registration sequence numbers.
 	SceneData.RegistrationSerialNumber = FPrimitiveSceneInfoData::GetNextRegistrationSerialNumber(); 

	Super::OnRegister();
	
	if (bCanEverAffectNavigation)
	{
		const bool bNavRelevant = bNavigationRelevant = IsNavigationRelevant();
		if (bNavRelevant)
		{
			FNavigationSystem::OnComponentRegistered(*this);
		}
	}
	else
	{
		bNavigationRelevant = false;
	}

#if WITH_EDITOR
	// If still compiling, this will be called when compilation has finished.
	if (!IsCompiling() && HasValidSettingsForStaticLighting(false))
	{
		FStaticLightingSystemInterface::OnPrimitiveComponentRegistered.Broadcast(this);
	}
#endif

	// Update our Owner's LastRenderTime
	SetLastRenderTime(SceneData.LastRenderTime);
}


void UPrimitiveComponent::OnUnregister()
{
	SceneData.OwnerLastRenderTimePtr = nullptr;

	// If this is being garbage collected we don't really need to worry about clearing this
	if (!HasAnyFlags(RF_BeginDestroyed) && !IsUnreachable())
	{
		UWorld* World = GetWorld();
		if (World && World->Scene)
		{
			World->Scene->ReleasePrimitive(this);
		}
	}

	Super::OnUnregister();

	// Unregister only has effect on dynamic primitives (as static ones are handled when the level visibility changes).
	if (bAttachedToStreamingManagerAsDynamic || bAttachedToCoarseMeshStreamingManager)
	{
		IStreamingManager::Get().NotifyPrimitiveDetached(this);
	}

	if (bCanEverAffectNavigation)
	{
		FNavigationSystem::OnComponentUnregistered(*this);
	}

#if WITH_EDITOR
	FStaticLightingSystemInterface::OnPrimitiveComponentUnregistered.Broadcast(this);
#endif
}

FPrimitiveComponentInstanceData::FPrimitiveComponentInstanceData(const UPrimitiveComponent* SourceComponent)
	: FSceneComponentInstanceData(SourceComponent)
	, VisibilityId(SourceComponent->VisibilityId)
	, LODParent(SourceComponent->GetLODParentPrimitive())
{
	const_cast<UPrimitiveComponent*>(SourceComponent)->ConditionalUpdateComponentToWorld(); // sadness
	ComponentTransform = SourceComponent->GetComponentTransform();
}

void FPrimitiveComponentInstanceData::ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase)
{
	FSceneComponentInstanceData::ApplyToComponent(Component, CacheApplyPhase);

	UPrimitiveComponent* PrimitiveComponent = CastChecked<UPrimitiveComponent>(Component);

#if WITH_EDITOR
	// This is needed to restore transient collision profile data.
	PrimitiveComponent->UpdateCollisionProfile();
#endif // #if WITH_EDITOR
	PrimitiveComponent->SetLODParentPrimitive(LODParent);

	if (VisibilityId != INDEX_NONE && GetComponentTransform().Equals(PrimitiveComponent->GetComponentTransform(), 1.e-3f))
	{
		PrimitiveComponent->VisibilityId = VisibilityId;
	}

	if (Component->IsRegistered() && ((VisibilityId != INDEX_NONE) || SavedProperties.Num() > 0))
	{
		// This is needed to restore transient primitive data from serialized defaults
		PrimitiveComponent->ResetCustomPrimitiveData();
		Component->MarkRenderStateDirty();
	}
}

bool FPrimitiveComponentInstanceData::ContainsData() const
{
	return (Super::ContainsData() || LODParent || (VisibilityId != INDEX_NONE));
}

void FPrimitiveComponentInstanceData::AddReferencedObjects(FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(Collector);

	// if LOD Parent
	if (LODParent)
	{
		Collector.AddReferencedObject(LODParent);
	}
}

void FPrimitiveComponentInstanceData::FindAndReplaceInstances(const TMap<UObject*, UObject*>& OldToNewInstanceMap)
{
	FSceneComponentInstanceData::FindAndReplaceInstances(OldToNewInstanceMap);

	// if LOD Parent 
	if (LODParent)
	{
		if (UObject* const* NewLODParent = OldToNewInstanceMap.Find(LODParent))
		{
			LODParent = CastChecked<UPrimitiveComponent>(*NewLODParent, ECastCheckedType::NullAllowed);
		}
	}
}

TStructOnScope<FActorComponentInstanceData> UPrimitiveComponent::GetComponentInstanceData() const
{
	return MakeStructOnScope<FActorComponentInstanceData, FPrimitiveComponentInstanceData>(this);
}

void UPrimitiveComponent::OnAttachmentChanged()
{
	UWorld* World = GetWorld();
	if (World && World->Scene)
	{
		World->Scene->UpdatePrimitiveAttachment(this);
	}
}

void UPrimitiveComponent::DestroyRenderState_Concurrent()
{
	// Remove the primitive from the scene.
	UWorld* World = GetWorld();
	if(World && World->Scene)
	{
		World->Scene->RemovePrimitive(this);
	}

	Super::DestroyRenderState_Concurrent();
}


//////////////////////////////////////////////////////////////////////////
// Physics

void UPrimitiveComponent::OnCreatePhysicsState()
{
	Super::OnCreatePhysicsState();

	// if we have a scene, we don't want to disable all physics and we have no bodyinstance already
	if(!BodyInstance.IsValidBodyInstance())
	{
		//UE_LOG(LogPrimitiveComponent, Warning, TEXT("Creating Physics State (%s : %s)"), *GetNameSafe(GetOuter()),  *GetName());

		if (AlwaysCreatePhysicsStateConversionHackCVar > 0)
		{
			static FCollisionResponseContainer IgnoreAll(ECR_Ignore);
			if (BodyInstance.GetCollisionEnabled() == ECollisionEnabled::QueryOnly && BodyInstance.GetResponseToChannels() == IgnoreAll)
			{
				bAlwaysCreatePhysicsState = true;
				BodyInstance.SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}
		}

		UBodySetup* BodySetup = GetBodySetup();
		if(BodySetup)
		{
			// Create new BodyInstance at given location.
			FTransform BodyTransform = GetComponentTransform();

			// Here we make sure we don't have zero scale. This still results in a body being made and placed in
			// world (very small) but is consistent with a body scaled to zero.
			const FVector BodyScale = BodyTransform.GetScale3D();
			if(BodyScale.IsNearlyZero())
			{
				BodyTransform.SetScale3D(FVector(UE_KINDA_SMALL_NUMBER));
			}
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if ((BodyInstance.GetCollisionEnabled() != ECollisionEnabled::NoCollision) && (FMath::IsNearlyZero(BodyScale.X) || FMath::IsNearlyZero(BodyScale.Y) || FMath::IsNearlyZero(BodyScale.Z)))
			{
				UE_LOG(LogPhysics, Warning, TEXT("Scale for %s has a component set to zero, which will result in a bad body instance. Scale:%s"), *GetPathNameSafe(this), *BodyScale.ToString());
				
				// User warning has been output - fix up the scale to be valid for physics
				BodyTransform.SetScale3D(FVector(
					FMath::IsNearlyZero(BodyScale.X) ? UE_KINDA_SMALL_NUMBER : BodyScale.X,
					FMath::IsNearlyZero(BodyScale.Y) ? UE_KINDA_SMALL_NUMBER : BodyScale.Y,
					FMath::IsNearlyZero(BodyScale.Z) ? UE_KINDA_SMALL_NUMBER : BodyScale.Z
				));
			}
#endif

			// Create the body.
			BodyInstance.InitBody(BodySetup, BodyTransform, this, GetWorld()->GetPhysicsScene());		
#if UE_ENABLE_DEBUG_DRAWING
			SendRenderDebugPhysics();
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

#if WITH_EDITOR
			// Make sure we have a valid body instance here. As we do not keep BIs with no collision shapes at all,
			// we don't want to create cloth collision in these cases
			if (BodyInstance.IsValidBodyInstance())
			{
				const float RealMass = BodyInstance.GetBodyMass();
				const float CalcedMass = BodySetup->CalculateMass(this);
				float MassDifference =  RealMass - CalcedMass;
				if (RealMass > 1.0f && FMath::Abs(MassDifference) > 0.1f)
				{
					UE_LOG(LogPhysics, Log, TEXT("Calculated mass differs from real mass for %s:%s. Mass: %f  CalculatedMass: %f"),
						GetOwner() != NULL ? *GetOwner()->GetName() : TEXT("NoActor"),
						*GetName(), RealMass, CalcedMass);
				}
			}
#endif // WITH_EDITOR
		}
	}

	OnComponentPhysicsStateChanged.Broadcast(this, EComponentPhysicsStateChange::Created);
}	

void UPrimitiveComponent::EnsurePhysicsStateCreated()
{
	// if physics is created when it shouldn't OR if physics isn't created when it should
	// we should fix it up
	if ( IsPhysicsStateCreated() != ShouldCreatePhysicsState() )
	{
		RecreatePhysicsState();
	}
}



void UPrimitiveComponent::MarkChildPrimitiveComponentRenderStateDirty()
{
	// Go through all potential children and update them 
	TArray<USceneComponent*, TInlineAllocator<8>> ProcessStack;
	ProcessStack.Append(GetAttachChildren());

	// Walk down the tree updating
	while (ProcessStack.Num() > 0)
	{
		if (USceneComponent* Current = ProcessStack.Pop(EAllowShrinking::No))
		{
			if (UPrimitiveComponent* CurrentPrimitive = Cast<UPrimitiveComponent>(Current))
			{
				CurrentPrimitive->MarkRenderStateDirty();
			}

			ProcessStack.Append(Current->GetAttachChildren());
		}
	}
}


void UPrimitiveComponent::ConditionalNotifyStreamingPrimitiveUpdated_Concurrent() const
{
	// Components are either registered as static or dynamic in the streaming manager.
	// Static components are registered in batches the first frame the level becomes visible (or incrementally each frame when loaded but not yet visible). 
	// The level static streaming data is never updated after this, and gets reused whenever the level becomes visible again (after being hidden).
	// Dynamic components, on the other hand, are updated whenever their render states change.
	// The following logic handles all cases where static components should fallback on the dynamic path.
	// It is based on a design where each component must either have bHandledByStreamingManagerAsDynamic or bAttachedToStreamingManagerAsStatic set.
	// If this is not the case, then the component has never been handled before.
	// The bIgnoreStreamingManagerUpdate flag is used to prevent handling component that are already in the update list or that don't have streaming data.
	if (!bIgnoreStreamingManagerUpdate && (Mobility != EComponentMobility::Static || bHandledByStreamingManagerAsDynamic || (!bAttachedToStreamingManagerAsStatic && OwnerLevelHasRegisteredStaticComponentsInStreamingManager(GetOwner()))))
	{
		FStreamingManagerCollection* Collection = IStreamingManager::Get_Concurrent();
		if (Collection)
		{
			Collection->NotifyPrimitiveUpdated_Concurrent(this);
		}
	}
}

bool UPrimitiveComponent::IsWelded() const
{
	return BodyInstance.WeldParent != nullptr;
}

void UPrimitiveComponent::OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
	Super::OnUpdateTransform(UpdateTransformFlags, Teleport);

	// Always send new transform to physics
	if(bPhysicsStateCreated && !(UpdateTransformFlags & EUpdateTransformFlags::SkipPhysicsUpdate))
	{
		//If we update transform of welded bodies directly (i.e. on the actual component) we need to update the shape transforms of the parent.
		//If the parent is updated, any welded shapes are automatically updated so we don't need to do this physx update.
		//If the parent is updated and we are NOT welded, the child still needs to update physx
		const bool bTransformSetDirectly = !(UpdateTransformFlags & EUpdateTransformFlags::PropagateFromParent);
		if( bTransformSetDirectly || !IsWelded())
		{
			SendPhysicsTransform(Teleport);
		}
	}
}

void UPrimitiveComponent::SendPhysicsTransform(ETeleportType Teleport)
{
	BodyInstance.SetBodyTransform(GetComponentTransform(), Teleport);
	BodyInstance.UpdateBodyScale(GetComponentTransform().GetScale3D());
}

void UPrimitiveComponent::OnDestroyPhysicsState()
{
	// we remove welding related to this component
	UnWeldFromParent();
	UnWeldChildren();

	// Remove all user defined entities here
	TArray<Chaos::FPhysicsObject*> PhysicsObjects = GetAllPhysicsObjects();
	FPhysicsObjectExternalInterface::LockWrite(PhysicsObjects)->SetUserDefinedEntity(PhysicsObjects, nullptr);

	// clean up physics engine representation
	if(BodyInstance.IsValidBodyInstance())
	{
		// We tell the BodyInstance to shut down the physics-engine data.
		BodyInstance.TermBody();
	}

#if UE_ENABLE_DEBUG_DRAWING
	SendRenderDebugPhysics();
#endif

	Super::OnDestroyPhysicsState();

	OnComponentPhysicsStateChanged.Broadcast(this, EComponentPhysicsStateChange::Destroyed);
}

#if UE_ENABLE_DEBUG_DRAWING
static void AppendDebugMassData(UPrimitiveComponent* Component, TArray<FPrimitiveSceneProxy::FDebugMassData>& DebugMassData)
{
	if (!Component->IsWelded() && Component->Mobility != EComponentMobility::Static)
	{
		if (FBodyInstance* BI = Component->GetBodyInstance())
		{
			if (BI->IsValidBodyInstance())
			{
				DebugMassData.AddDefaulted();
				FPrimitiveSceneProxy::FDebugMassData& RootMassData = DebugMassData.Last();
				const FTransform MassToWorld = BI->GetMassSpaceToWorldSpace();

				RootMassData.LocalCenterOfMass = Component->GetComponentTransform().InverseTransformPosition(MassToWorld.GetLocation());
				RootMassData.LocalTensorOrientation = MassToWorld.GetRotation() * Component->GetComponentTransform().GetRotation().Inverse();
				RootMassData.MassSpaceInertiaTensor = BI->GetBodyInertiaTensor();
				RootMassData.BoneIndex = INDEX_NONE;
			}
		}
	}
}

void UPrimitiveComponent::SendRenderDebugPhysics(FPrimitiveSceneProxy* OverrideSceneProxy)
{
	// For bulk reregistering, this is handled in the FStaticMeshComponentBulkReregisterContext constructor / destructor
	if (bBulkReregister)
	{
		return;
	}

	FPrimitiveSceneProxy* UseSceneProxy = OverrideSceneProxy ? OverrideSceneProxy : SceneProxy;
	if (UseSceneProxy)
	{
		TArray<FPrimitiveSceneProxy::FDebugMassData> DebugMassData;
		AppendDebugMassData(this, DebugMassData);

		FPrimitiveSceneProxy* PassedSceneProxy = UseSceneProxy;
		TArray<FPrimitiveSceneProxy::FDebugMassData> UseDebugMassData = DebugMassData;
		ENQUEUE_RENDER_COMMAND(PrimitiveComponent_SendRenderDebugPhysics)(
			[UseSceneProxy, DebugMassData](FRHICommandList& RHICmdList)
			{
					UseSceneProxy->SetDebugMassData(DebugMassData);
			});
	}
}

void UPrimitiveComponent::BatchSendRenderDebugPhysics(TArrayView<UPrimitiveComponent*> InPrimitives)
{
	TArray<FPrimitiveSceneProxy*> SceneProxies;
	TArray<uint32> DebugMassCounts;
	TArray<FPrimitiveSceneProxy::FDebugMassData> DebugMassData;

	SceneProxies.Reserve(InPrimitives.Num());
	DebugMassCounts.Reserve(InPrimitives.Num());
	DebugMassData.Reserve(InPrimitives.Num());

	for (int32 PrimitiveIndex = 0; PrimitiveIndex < InPrimitives.Num(); PrimitiveIndex++)
	{
		if (InPrimitives[PrimitiveIndex]->SceneProxy)
		{
			SceneProxies.Add(InPrimitives[PrimitiveIndex]->SceneProxy);

			uint32 NumDebugMassDataBefore = DebugMassData.Num();
			AppendDebugMassData(InPrimitives[PrimitiveIndex], DebugMassData);
			DebugMassCounts.Add(DebugMassData.Num() - NumDebugMassDataBefore);
		}
	}

	if (SceneProxies.Num())
	{
		ENQUEUE_RENDER_COMMAND(PrimitiveComponent_BatchSendRenderDebugPhysics)(
			[SceneProxies = MoveTemp(SceneProxies), DebugMassCounts = MoveTemp(DebugMassCounts), DebugMassData = MoveTemp(DebugMassData)](FRHICommandList& RHICmdList)
		{
			TArray<FPrimitiveSceneProxy::FDebugMassData> SingleDebugMassData;
			uint32 DebugMassOffset = 0;

			for (int32 ProxyIndex = 0; ProxyIndex < SceneProxies.Num(); ProxyIndex++)
			{
				uint32 DebugMassCount = DebugMassCounts[ProxyIndex];
				SingleDebugMassData.SetNumUninitialized(DebugMassCount);
				for (uint32 DebugMassIndex = 0; DebugMassIndex < DebugMassCount; DebugMassIndex++)
				{
					SingleDebugMassData[DebugMassIndex] = DebugMassData[DebugMassOffset + DebugMassIndex];
				}

				SceneProxies[ProxyIndex]->SetDebugMassData(SingleDebugMassData);

				DebugMassOffset += DebugMassCount;
			}
		});
	}
}
#endif  // UE_ENABLE_DEBUG_DRAWING

FMatrix UPrimitiveComponent::GetRenderMatrix() const
{
	return GetComponentTransform().ToMatrixWithScale();
}

void UPrimitiveComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar.UsingCustomVersion(FRenderingObjectVersion::GUID);
	Ar.UsingCustomVersion(FFortniteMainBranchObjectVersion::GUID);
	Ar.UsingCustomVersion(FUE5PrivateFrostyStreamObjectVersion::GUID);

	// This causes other issues with blueprint components (FORT-506503)
	// See UStaticMeshComponent::Serialize for a workaround.
	// CollisionProfile serialization needs some cleanup (UE-163199)
	// 
	// as temporary fix for the bug TTP 299926
	// permanent fix is coming
	if (Ar.IsLoading() && IsTemplate())
	{
		BodyInstance.FixupData(this);
	}

	if (Ar.CustomVer(FRenderingObjectVersion::GUID) < FRenderingObjectVersion::ReplaceLightAsIfStatic)
	{
		if (bLightAsIfStatic_DEPRECATED)
		{
			LightmapType = ELightmapType::ForceSurface;
		}
	}

#if WITH_EDITORONLY_DATA
	if (Ar.CustomVer(FUE5PrivateFrostyStreamObjectVersion::GUID) < FUE5PrivateFrostyStreamObjectVersion::HLODBatchingPolicy)
	{
		if (bUseMaxLODAsImposter_DEPRECATED)
		{
			HLODBatchingPolicy = EHLODBatchingPolicy::MeshSection;
		}

		if (bBatchImpostersAsInstances_DEPRECATED)
		{
			HLODBatchingPolicy = EHLODBatchingPolicy::Instancing;
		}
	}

	PRAGMA_DISABLE_DEPRECATION_WARNINGS;
	if (Ar.IsLoading() && !ExcludeForSpecificHLODLevels_DEPRECATED.IsEmpty())
	{
		SetExcludeForSpecificHLODLevels(ExcludeForSpecificHLODLevels_DEPRECATED);
		ExcludeForSpecificHLODLevels_DEPRECATED.Empty();
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS;
#endif
}

#if WITH_EDITOR
void UPrimitiveComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	float NewCachedMaxDrawDistance = CachedMaxDrawDistance;
	bool bCullDistanceInvalidated = false;

	FProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if(PropertyThatChanged)
	{
		const FName PropertyName = PropertyThatChanged->GetFName();

		// CachedMaxDrawDistance needs to be set as if you have no cull distance volumes affecting this primitive component the cached value wouldn't get updated
		if (PropertyName == GET_MEMBER_NAME_CHECKED(UPrimitiveComponent, LDMaxDrawDistance))
		{
			NewCachedMaxDrawDistance = LDMaxDrawDistance;
			bCullDistanceInvalidated = true;
		}
		else if (PropertyName == GET_MEMBER_NAME_CHECKED(UPrimitiveComponent, bAllowCullDistanceVolume)
			|| PropertyName == GET_MEMBER_NAME_CHECKED(UPrimitiveComponent, bNeverDistanceCull)
			|| PropertyName == GET_MEMBER_NAME_CHECKED(UPrimitiveComponent, Mobility))
		{
			bCullDistanceInvalidated = true;
		}

		// we need to reregister the primitive if the min draw distance changed to propagate the change to the rendering thread
		if (PropertyName == GET_MEMBER_NAME_CHECKED(UPrimitiveComponent, MinDrawDistance))
		{
			MarkRenderStateDirty();
		}

		// In the light attachment as group has changed, we need to notify attachment children that they are invalid (they may have a new root)
		// Unless multiple roots are in the way, in either case, they need to work this out.
		if (PropertyName == GET_MEMBER_NAME_CHECKED(UPrimitiveComponent, bLightAttachmentsAsGroup))
		{
			MarkChildPrimitiveComponentRenderStateDirty();
		}
	}

	if (FProperty* MemberPropertyThatChanged = PropertyChangedEvent.MemberProperty)
	{
		const FName MemberPropertyName = MemberPropertyThatChanged->GetFName();

		// Reregister to get the custom primitive data to the proxy
		if (MemberPropertyName == GET_MEMBER_NAME_CHECKED(UPrimitiveComponent, CustomPrimitiveData))
		{
			ResetCustomPrimitiveData();
			MarkRenderStateDirty();
		}
	}

	if (LightmapType == ELightmapType::ForceSurface && GetStaticLightingType() == LMIT_None)
	{
		LightmapType = ELightmapType::Default;
	}

	if (bCullDistanceInvalidated)
	{
		// Directly use LD cull distance if cull distance volumes are disabled or if the primitive isn't static
		if (!bAllowCullDistanceVolume || Mobility != EComponentMobility::Static)
		{
			NewCachedMaxDrawDistance = LDMaxDrawDistance;
		}
		else if (UWorld* World = GetWorld())
		{
			const bool bUpdatedDrawDistances = World->UpdateCullDistanceVolumes(nullptr, this);

			// If volumes invalidated the distance, handle the desired distance against other sources
			if (bUpdatedDrawDistances)
			{
				if (LDMaxDrawDistance <= 0)
				{
					// Volume is the only controlling source, use directly
					NewCachedMaxDrawDistance = CachedMaxDrawDistance;
				}
				else
				{
					// Select the minimum desired value
					NewCachedMaxDrawDistance = FMath::Min(CachedMaxDrawDistance, LDMaxDrawDistance);
				}
			}
		}

		// Reattach to propagate cull distance change.
		SetCachedMaxDrawDistance(NewCachedMaxDrawDistance);
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);

	// update component, ActorComponent's property update locks navigation system 
	// so it needs to be called directly here
	if (PropertyThatChanged && PropertyThatChanged->GetFName() == GET_MEMBER_NAME_CHECKED(UPrimitiveComponent, bCanEverAffectNavigation))
	{
		HandleCanEverAffectNavigationChange();
	}

	// Whatever changed, ensure the streaming data gets refreshed.
	IStreamingManager::Get().NotifyPrimitiveUpdated(this);
}

bool UPrimitiveComponent::CanEditChange(const FProperty* InProperty) const
{
	bool bIsEditable = Super::CanEditChange( InProperty );
	if (bIsEditable && InProperty)
	{
		const FName PropertyName = InProperty->GetFName();

		static FName LightmassSettingsName = TEXT("LightmassSettings");
		static FName LightingChannelsName = GET_MEMBER_NAME_CHECKED(UPrimitiveComponent, LightingChannels);
		static FName SingleSampleShadowFromStationaryLightsName = GET_MEMBER_NAME_CHECKED(UPrimitiveComponent, bSingleSampleShadowFromStationaryLights);
		static FName IndirectLightingCacheQualityName = GET_MEMBER_NAME_CHECKED(UPrimitiveComponent, IndirectLightingCacheQuality);
		static FName CastCinematicShadowName = GET_MEMBER_NAME_CHECKED(UPrimitiveComponent, bCastCinematicShadow);
		static FName CastInsetShadowName = GET_MEMBER_NAME_CHECKED(UPrimitiveComponent, bCastInsetShadow);
		static FName CastShadowName = GET_MEMBER_NAME_CHECKED(UPrimitiveComponent, CastShadow);

		if (PropertyName == LightmassSettingsName)
		{
			return Mobility != EComponentMobility::Movable || LightmapType == ELightmapType::ForceSurface;
		}

		if (PropertyName == SingleSampleShadowFromStationaryLightsName)
		{
			return Mobility != EComponentMobility::Static;
		}

		if (PropertyName == CastCinematicShadowName)
		{
			return Mobility == EComponentMobility::Movable;
		}

		if (PropertyName == IndirectLightingCacheQualityName)
		{
			UWorld* World = GetWorld();
			AWorldSettings* WorldSettings = World ? World->GetWorldSettings() : NULL;
			const bool bILCRelevant = WorldSettings ? (WorldSettings->LightmassSettings.VolumeLightingMethod == VLM_SparseVolumeLightingSamples) : true;
			return bILCRelevant && Mobility == EComponentMobility::Movable;
		}

		if (PropertyName == GET_MEMBER_NAME_CHECKED(UPrimitiveComponent, LightmapType))
		{
			return IsStaticLightingAllowed();
		}

		if (PropertyName == CastInsetShadowName)
		{
			return !bSelfShadowOnly;
		}
	}

	return bIsEditable;
}

void UPrimitiveComponent::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	const FName NAME_Scale3D(TEXT("Scale3D"));
	const FName NAME_Scale(TEXT("Scale"));
	const FName NAME_Translation(TEXT("Translation"));

	for( FEditPropertyChain::TIterator It(PropertyChangedEvent.PropertyChain.GetHead()); It; ++It )
	{
		if( It->GetFName() == NAME_Scale3D		||
			It->GetFName() == NAME_Scale		||
			It->GetFName() == NAME_Translation	||
			It->GetFName() == NAME_Rotation)
		{
			UpdateComponentToWorld();
			break;
		}
	}

	Super::PostEditChangeChainProperty( PropertyChangedEvent );
}

void UPrimitiveComponent::CheckForErrors()
{
	AActor* Owner = GetOwner();

	if (CastShadow && bCastDynamicShadow && BoundsScale > 1.0f)
	{
		FMessageLog("MapCheck").PerformanceWarning()
			->AddToken(FUObjectToken::Create(Owner))
			->AddToken(FTextToken::Create(LOCTEXT( "MapCheck_Message_ShadowCasterUsingBoundsScale", "Actor casts dynamic shadows and has a BoundsScale greater than 1! This will have a large performance hit" )))
			->AddToken(FMapErrorToken::Create(FMapErrors::ShadowCasterUsingBoundsScale));
	}

	if (HasStaticLighting() && !HasValidSettingsForStaticLighting(true) && (!Owner || !Owner->IsA(AWorldSettings::StaticClass())))	// Ignore worldsettings
	{
		FMessageLog("MapCheck").Error()
			->AddToken(FUObjectToken::Create(Owner))
			->AddToken(FTextToken::Create(LOCTEXT( "MapCheck_Message_InvalidLightmapSettings", "Component is a static type but has invalid lightmap settings!  Indirect lighting will be black.  Common causes are lightmap resolution of 0, LightmapCoordinateIndex out of bounds." )))
			->AddToken(FMapErrorToken::Create(FMapErrors::StaticComponentHasInvalidLightmapSettings));
	}

	static const auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shadow.TranslucentPerObject.ProjectEnabled"));
	if (bCastVolumetricTranslucentShadow && CastShadow && bCastDynamicShadow && CVar && CVar->GetInt() == 0)
	{
		FMessageLog("MapCheck").Warning()
			->AddToken(FUObjectToken::Create(Owner))
			->AddToken(FTextToken::Create(LOCTEXT( "MapCheck_Message_NoTranslucentShadowSupport", "Component is a using CastVolumetricTranslucentShadow but this feature is disabled for the project! Turn on r.Shadow.TranslucentPerObject.ProjectEnabled in a project ini if required." )))
			->AddToken(FMapErrorToken::Create(FMapErrors::PrimitiveComponentHasInvalidTranslucentShadowSetting));
	}
}

void UPrimitiveComponent::GetActorDescProperties(FPropertyPairsMap& PropertyPairsMap) const
{
	Super::GetActorDescProperties(PropertyPairsMap);

	for (URuntimeVirtualTexture* RuntimeVirtualTexture : RuntimeVirtualTextures)
	{
		if (RuntimeVirtualTexture)
		{
			PropertyPairsMap.AddProperty(UPrimitiveComponent::RVTActorDescProperty);
			return;
		}
	}
}

void UPrimitiveComponent::UpdateCollisionProfile()
{
	BodyInstance.LoadProfileData(false);
}
#endif // WITH_EDITOR

void UPrimitiveComponent::ReceiveComponentDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (bApplyImpulseOnDamage)
	{
		UDamageType const* const DamageTypeCDO = DamageEvent.DamageTypeClass ? DamageEvent.DamageTypeClass->GetDefaultObject<UDamageType>() : GetDefault<UDamageType>();
		if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
		{
			FPointDamageEvent* const PointDamageEvent = (FPointDamageEvent*)&DamageEvent;
			if ((DamageTypeCDO->DamageImpulse > 0.f) && !PointDamageEvent->ShotDirection.IsNearlyZero())
			{
				if (IsSimulatingPhysics(PointDamageEvent->HitInfo.BoneName))
				{
					FVector const ImpulseToApply = PointDamageEvent->ShotDirection.GetSafeNormal() * DamageTypeCDO->DamageImpulse;
					AddImpulseAtLocation(ImpulseToApply, PointDamageEvent->HitInfo.ImpactPoint, PointDamageEvent->HitInfo.BoneName);
				}
			}
		}
		else if (DamageEvent.IsOfType(FRadialDamageEvent::ClassID))
		{
			FRadialDamageEvent* const RadialDamageEvent = (FRadialDamageEvent*)&DamageEvent;
			if (DamageTypeCDO->DamageImpulse > 0.f)
			{
				AddRadialImpulse(RadialDamageEvent->Origin, RadialDamageEvent->Params.OuterRadius, DamageTypeCDO->DamageImpulse, RIF_Linear, DamageTypeCDO->bRadialDamageVelChange);
			}
		}
	}
}

void UPrimitiveComponent::PostInitProperties()
{
	Super::PostInitProperties();

	// Apply any deferred collision profile name we have (set in the constructor).
	BodyInstance.ApplyDeferredCollisionProfileName();
}

void UPrimitiveComponent::PostLoad()
{
	Super::PostLoad();
	
	FPackageFileVersion const UEVersion = GetLinkerUEVersion();

	// as temporary fix for the bug TTP 299926
	// permanent fix is coming
	if (!IsTemplate())
	{
		BodyInstance.FixupData(this);
	}

#if WITH_EDITORONLY_DATA
	if (UEVersion < VER_UE4_RENAME_CANBECHARACTERBASE)
	{
		CanCharacterStepUpOn = CanBeCharacterBase_DEPRECATED;
	}
#endif

	// Make sure cached cull distance is up-to-date.
	if( LDMaxDrawDistance > 0.f )
	{
		// Directly use LD cull distance if cached one is not set.
		if( CachedMaxDrawDistance == 0.f )
		{
			CachedMaxDrawDistance = LDMaxDrawDistance;
		}
		// Use min of both if neither is 0. Need to check as 0 has special meaning.
		else
		{
			CachedMaxDrawDistance = FMath::Min( LDMaxDrawDistance, CachedMaxDrawDistance );
		}

		bool bNeverCull = bNeverDistanceCull || GetLODParentPrimitive();
		CachedMaxDrawDistance = bNeverCull ? 0.f : CachedMaxDrawDistance;
	} 

	if (LightmapType == ELightmapType::ForceSurface && GetStaticLightingType() == LMIT_None)
	{
		LightmapType = ELightmapType::Default;
	}

	// Setup the default here
	ResetCustomPrimitiveData();
}

void UPrimitiveComponent::PostDuplicate(bool bDuplicateForPIE)
{
	if (!bDuplicateForPIE)
	{
		VisibilityId = INDEX_NONE;
	}

	Super::PostDuplicate(bDuplicateForPIE);
}

static int32 GEnableAutoDetectNoStreamableTextures = 1;
static FAutoConsoleVariableRef CVarEnableAutoDetectNoStreamableTextures(
	TEXT("r.Streaming.EnableAutoDetectNoStreamableTextures"),
	GEnableAutoDetectNoStreamableTextures,
	TEXT("Enables auto-detection at cook time of primitive components with no streamable textures. Can also be turned-off at runtime to skip optimisation."),
	ECVF_Default
);

bool UPrimitiveComponent::CanSkipGetTextureStreamingRenderAssetInfo() const
{
#if WITH_EDITOR
	return false;
#else
	return GEnableAutoDetectNoStreamableTextures && bHasNoStreamableTextures;
#endif
}

#if WITH_EDITOR
/**
 * Called after importing property values for this object (paste, duplicate or .t3d import)
 * Allow the object to perform any cleanup for properties which shouldn't be duplicated or
 * are unsupported by the script serialization
 */
void UPrimitiveComponent::PostEditImport()
{
	Super::PostEditImport();

	VisibilityId = INDEX_NONE;

	// as temporary fix for the bug TTP 299926
	// permanent fix is coming
	if (IsTemplate()==false)
	{
		BodyInstance.FixupData(this);
	}

	// Setup the transient internal primitive data array here after import (to support duplicate/paste)
	ResetCustomPrimitiveData();
}

void UPrimitiveComponent::PreSave(const class ITargetPlatform* TargetPlatform)
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS;
	Super::PreSave(TargetPlatform);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS;
}

void UPrimitiveComponent::PreSave(FObjectPreSaveContext ObjectSaveContext)
{
	Super::PreSave(ObjectSaveContext);

	if (!IsTemplate() && ObjectSaveContext.IsCooking())
	{
		// Reset flag
		bHasNoStreamableTextures = false;

		if (!GEnableAutoDetectNoStreamableTextures || (Mobility != EComponentMobility::Static))
		{
			return;
		}

		TArray<UMaterialInterface*> Materials;
		GetUsedMaterials(Materials);
		if (Materials.IsEmpty())
		{
			return;
		}

		TSet<const UTexture*> Textures;
		for (UMaterialInterface* Material : Materials)
		{
			if (Material)
			{
				Material->GetReferencedTexturesAndOverrides(Textures);
			}
		}

		bool bHasStreamableTextures = false;
		for (const UTexture* Texture : Textures)
		{
			if (Texture && Texture->IsCandidateForTextureStreamingOnPlatformDuringCook(ObjectSaveContext.GetTargetPlatform()))
			{
				bHasStreamableTextures = true;
				break;
			}
		}

		bHasNoStreamableTextures = !bHasStreamableTextures;
	}
}
#endif

void UPrimitiveComponent::BeginDestroy()
{
	// Whether static or dynamic, all references need to be freed
	if (IsAttachedToStreamingManager() || bAttachedToCoarseMeshStreamingManager)
	{
		IStreamingManager::Get().NotifyPrimitiveDetached(this);
	}

	Super::BeginDestroy();

	// Use a fence to keep track of when the rendering thread executes this scene detachment.
	DetachFence.BeginFence();
	
#if !UE_STRIP_DEPRECATED_PROPERTIES
	AActor* Owner = GetOwner();
	if(Owner)
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		Owner->DetachFence.BeginFence();
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}
#endif
}

void UPrimitiveComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	// Prevent future overlap events. Any later calls to UpdateOverlaps will only allow this to end overlaps.
	SetGenerateOverlapEvents(false);

	// End all current overlaps
	if (OverlappingComponents.Num() > 0)
	{
		const bool bDoNotifies = true;
		const bool bSkipNotifySelf = false;
		ClearComponentOverlaps(bDoNotifies, bSkipNotifySelf);
	}

	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

bool UPrimitiveComponent::IsReadyForFinishDestroy()
{
	// Don't allow the primitive component to the purged until its pending scene detachments have completed.
	return Super::IsReadyForFinishDestroy() && DetachFence.IsFenceComplete();
}

void UPrimitiveComponent::FinishDestroy()
{
	// The detach fence has cleared so we better not be attached to the scene.
	check(SceneData.AttachmentCounter.GetValue() == 0);
	Super::FinishDestroy();
}

bool UPrimitiveComponent::NeedsLoadForClient() const
{
	if (!IsVisible() && !IsCollisionEnabled() && !AlwaysLoadOnClient)
	{
		return 0;
	}
	else
	{
		return Super::NeedsLoadForClient();
	}
}


bool UPrimitiveComponent::NeedsLoadForServer() const
{
	if(!IsCollisionEnabled() && !AlwaysLoadOnServer)
	{
		return 0;
	}
	else
	{
		return Super::NeedsLoadForServer();
	}
}

void UPrimitiveComponent::SetOwnerNoSee(bool bNewOwnerNoSee)
{
	if(bOwnerNoSee != bNewOwnerNoSee)
	{
		bOwnerNoSee = bNewOwnerNoSee;
		MarkRenderStateDirty();
	}
}

void UPrimitiveComponent::SetOnlyOwnerSee(bool bNewOnlyOwnerSee)
{
	if(bOnlyOwnerSee != bNewOnlyOwnerSee)
	{
		bOnlyOwnerSee = bNewOnlyOwnerSee;
		MarkRenderStateDirty();
	}
}

bool UPrimitiveComponent::ShouldComponentAddToScene() const
{
	bool bSceneAdd = USceneComponent::ShouldComponentAddToScene();

#if WITH_EDITOR
	AActor* Owner = GetOwner();
	const bool bIsHiddenInEditor = GIsEditor && Owner && Owner->IsHiddenEd();
#else
	const bool bIsHiddenInEditor = false;
#endif

	return bSceneAdd && (ShouldRender() || (bCastHiddenShadow && !bIsHiddenInEditor) || bAffectIndirectLightingWhileHidden || bRayTracingFarField);
}

bool UPrimitiveComponent::ShouldCreatePhysicsState() const
{
	if (IsBeingDestroyed())
	{
		return false;
	}

	bool bShouldCreatePhysicsState = IsRegistered() && (bAlwaysCreatePhysicsState || BodyInstance.GetCollisionEnabled() != ECollisionEnabled::NoCollision);

#if WITH_EDITOR
	if (BodyInstance.bSimulatePhysics)
	{
		if(UWorld* World = GetWorld())
		{
			if(World->IsGameWorld())
			{
				const ECollisionEnabled::Type CollisionEnabled = GetCollisionEnabled();
				if (CollisionEnabled == ECollisionEnabled::NoCollision || CollisionEnabled == ECollisionEnabled::QueryOnly)
				{
					FMessageLog("PIE").Warning(FText::Format(LOCTEXT("InvalidSimulateOptions", "Invalid Simulate Options: Body ({0}) is set to simulate physics but Collision Enabled is incompatible"),
						FText::FromString(GetReadableName())));
				}
			}
		}
	}

	// if it shouldn't create physics state, but if world wants to enable trace collision for components, allow it
	if (!bShouldCreatePhysicsState)
	{
		UWorld* World = GetWorld();
		if (World && World->bEnableTraceCollision && !GetComponentTransform().GetScale3D().IsNearlyZero())
		{
			bShouldCreatePhysicsState = true;
		}
	}
#endif
	return bShouldCreatePhysicsState;
}

bool UPrimitiveComponent::HasValidPhysicsState() const
{
	return BodyInstance.IsValidBodyInstance();
}

bool UPrimitiveComponent::IsComponentIndividuallySelected() const
{
	bool bIndividuallySelected = false;
#if WITH_EDITOR
	if(SelectionOverrideDelegate.IsBound())
	{
		bIndividuallySelected = SelectionOverrideDelegate.Execute(this);
	}
#endif
	return bIndividuallySelected;
}

bool UPrimitiveComponent::ShouldRenderSelected() const
{
	if (bSelectable)
	{
		if (const AActor* Owner = GetOwner())
		{
			return Owner->IsActorOrSelectionParentSelected();
		}
	}
	return false;
}

bool UPrimitiveComponent::GetLevelInstanceEditingState() const
{
#if WITH_EDITOR
	if (const AActor* Owner = GetOwner())
	{
		return Owner->IsInEditLevelInstanceHierarchy();
	}
#endif

	return false;
}

void UPrimitiveComponent::SetVisibleInRayTracing(bool bNewVisibleInRayTracing)
{
	if (bNewVisibleInRayTracing != bVisibleInRayTracing)
	{
		bVisibleInRayTracing = bNewVisibleInRayTracing;
		MarkRenderStateDirty();
	}
}

void UPrimitiveComponent::SetCastShadow(bool NewCastShadow)
{
	if(NewCastShadow != CastShadow)
	{
		CastShadow = NewCastShadow;
		MarkRenderStateDirty();
	}
}

void UPrimitiveComponent::SetEmissiveLightSource(bool NewEmissiveLightSource)
{
	if(NewEmissiveLightSource != bEmissiveLightSource)
	{
		bEmissiveLightSource = NewEmissiveLightSource;
		MarkRenderStateDirty();
	}
}

void UPrimitiveComponent::SetCastHiddenShadow(bool NewCastHiddenShadow)
{
	if (NewCastHiddenShadow != bCastHiddenShadow)
	{
		bCastHiddenShadow = NewCastHiddenShadow;
		MarkRenderStateDirty();
	}
}

void UPrimitiveComponent::SetCastInsetShadow(bool bInCastInsetShadow)
{
	if(bInCastInsetShadow != bCastInsetShadow)
	{
		bCastInsetShadow = bInCastInsetShadow;
		MarkRenderStateDirty();
	}
}

void UPrimitiveComponent::SetCastContactShadow(bool bInCastContactShadow)
{
	if (bInCastContactShadow != bCastContactShadow)
	{
		bCastContactShadow = bInCastContactShadow;
		MarkRenderStateDirty();
	}
}

void UPrimitiveComponent::SetLightAttachmentsAsGroup(bool bInLightAttachmentsAsGroup)
{
	if(bInLightAttachmentsAsGroup != bLightAttachmentsAsGroup)
	{
		bLightAttachmentsAsGroup = bInLightAttachmentsAsGroup;
		MarkRenderStateDirty();
		MarkChildPrimitiveComponentRenderStateDirty();
	}
}

void UPrimitiveComponent::SetExcludeFromLightAttachmentGroup(bool bInExcludeFromLightAttachmentGroup)
{
	if (bExcludeFromLightAttachmentGroup != bInExcludeFromLightAttachmentGroup)
	{
		bExcludeFromLightAttachmentGroup = bInExcludeFromLightAttachmentGroup;
		MarkRenderStateDirty();
	}
}

void UPrimitiveComponent::SetSingleSampleShadowFromStationaryLights(bool bNewSingleSampleShadowFromStationaryLights)
{
	if (bNewSingleSampleShadowFromStationaryLights != bSingleSampleShadowFromStationaryLights)
	{
		bSingleSampleShadowFromStationaryLights = bNewSingleSampleShadowFromStationaryLights;
		MarkRenderStateDirty();
	}
}

void UPrimitiveComponent::SetTranslucentSortPriority(int32 NewTranslucentSortPriority)
{
	if (NewTranslucentSortPriority != TranslucencySortPriority)
	{
		TranslucencySortPriority = NewTranslucentSortPriority;
		MarkRenderStateDirty();
	}
}

void UPrimitiveComponent::SetAffectDistanceFieldLighting(bool NewAffectDistanceFieldLighting)
{
	if(NewAffectDistanceFieldLighting != bAffectDistanceFieldLighting)
	{
		bAffectDistanceFieldLighting = NewAffectDistanceFieldLighting;
		MarkRenderStateDirty();
	}
}

void UPrimitiveComponent::SetTranslucencySortDistanceOffset(float NewTranslucencySortDistanceOffset)
{
	if ( !FMath::IsNearlyEqual(NewTranslucencySortDistanceOffset, TranslucencySortDistanceOffset) )
	{
		TranslucencySortDistanceOffset = NewTranslucencySortDistanceOffset;
		MarkRenderStateDirty();
	}
}

void UPrimitiveComponent::SetReceivesDecals(bool bNewReceivesDecals)
{
	if (bNewReceivesDecals != bReceivesDecals)
	{
		bReceivesDecals = bNewReceivesDecals;
		MarkRenderStateDirty();
	}
}

void UPrimitiveComponent::SetHoldout(bool bNewHoldout)
{
	if (bHoldout != bNewHoldout)
	{
		bHoldout = bNewHoldout;
		MarkRenderStateDirty();
	}
}

void UPrimitiveComponent::SetAffectDynamicIndirectLighting(bool bNewAffectDynamicIndirectLighting)
{
	if (bAffectDynamicIndirectLighting != bNewAffectDynamicIndirectLighting)
	{
		bAffectDynamicIndirectLighting = bNewAffectDynamicIndirectLighting;
		MarkRenderStateDirty();
	}
}


void UPrimitiveComponent::SetAffectIndirectLightingWhileHidden(bool bNewAffectIndirectLightingWhileHidden)
{
	if (bAffectIndirectLightingWhileHidden != bNewAffectIndirectLightingWhileHidden)
	{
		bAffectIndirectLightingWhileHidden = bNewAffectIndirectLightingWhileHidden;
		MarkRenderStateDirty();
	}
}


void UPrimitiveComponent::PushSelectionToProxy()
{
	//although this should only be called for attached components, some billboard components can get in without valid proxies
	if (SceneProxy)
	{
		SceneProxy->SetSelection_GameThread(ShouldRenderSelected(), IsComponentIndividuallySelected());
	}
}

void UPrimitiveComponent::PushLevelInstanceEditingStateToProxy(bool bInEditingState)
{
	//although this should only be called for attached components, some billboard components can get in without valid proxies
	if (SceneProxy)
	{
		SceneProxy->SetLevelInstanceEditingState_GameThread(bInEditingState);
	}
}

void UPrimitiveComponent::PushEditorVisibilityToProxy( uint64 InVisibility )
{
	//although this should only be called for attached components, some billboard components can get in without valid proxies
	if (SceneProxy)
	{
		SceneProxy->SetHiddenEdViews_GameThread( InVisibility );
	}
}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
void UPrimitiveComponent::PushPrimitiveColorToProxy(const FLinearColor& InPrimitiveColor)
{
	//although this should only be called for attached components, some billboard components can get in without valid proxies
	if (SceneProxy)
	{
		SceneProxy->SetPrimitiveColor_GameThread(InPrimitiveColor);
	}
}
#endif

#if WITH_EDITOR
uint64 UPrimitiveComponent::GetHiddenEditorViews() const
{
	const AActor* OwnerActor = GetOwner();
	return OwnerActor ? OwnerActor->HiddenEditorViews : 0;
}

void UPrimitiveComponent::SetIsBeingMovedByEditor(bool bIsBeingMoved)
{
	bIsBeingMovedByEditor = bIsBeingMoved;

	if (SceneProxy)
	{
		SceneProxy->SetIsBeingMovedByEditor_GameThread(bIsBeingMoved);
	}
}

void UPrimitiveComponent::SetSelectionOutlineColorIndex(uint8 InSelectionOutlineColorIndex)
{
	SelectionOutlineColorIndex = InSelectionOutlineColorIndex;
	
	if (SceneProxy)
	{
		SceneProxy->SetSelectionOutlineColorIndex_GameThread(InSelectionOutlineColorIndex);
	}
}

#endif// WITH_EDITOR

void UPrimitiveComponent::ResetSceneVelocity()
{
	if (SceneProxy)
	{
		SceneProxy->ResetSceneVelocity_GameThread();
	}
}

void UPrimitiveComponent::PushHoveredToProxy(const bool bInHovered)
{
	//although this should only be called for attached components, some billboard components can get in without valid proxies
	if (SceneProxy)
	{
		SceneProxy->SetHovered_GameThread(bInHovered);
	}
}

void UPrimitiveComponent::SetCullDistance(const float NewCullDistance)
{
	if (NewCullDistance >= 0.f && NewCullDistance != LDMaxDrawDistance)
	{
		const float OldLDMaxDrawDistance = LDMaxDrawDistance;

		LDMaxDrawDistance = NewCullDistance;
	
		if (CachedMaxDrawDistance == 0.f || LDMaxDrawDistance < CachedMaxDrawDistance)
		{
			SetCachedMaxDrawDistance(LDMaxDrawDistance);
		}
		else if (OldLDMaxDrawDistance == CachedMaxDrawDistance)
		{
			SetCachedMaxDrawDistance(LDMaxDrawDistance);

			if (UWorld* World = GetWorld())
			{
				World->UpdateCullDistanceVolumes(nullptr, this);
			}
		}
	}
}

void UPrimitiveComponent::SetCachedMaxDrawDistance(const float NewCachedMaxDrawDistance)
{
	bool bNeverCull = bNeverDistanceCull || GetLODParentPrimitive();
	float NewMaxDrawDistance = bNeverCull ? 0.f : NewCachedMaxDrawDistance;

	if( !FMath::IsNearlyEqual(CachedMaxDrawDistance, NewMaxDrawDistance) )
	{
		CachedMaxDrawDistance = NewMaxDrawDistance;
		
		if (GetScene() && SceneProxy)
		{
			GetScene()->UpdatePrimitiveDrawDistance(this, MinDrawDistance, NewMaxDrawDistance, GetVirtualTextureMainPassMaxDrawDistance());
		}
	}
}


void UPrimitiveComponent::SetDepthPriorityGroup(ESceneDepthPriorityGroup NewDepthPriorityGroup)
{
	if (DepthPriorityGroup != NewDepthPriorityGroup)
	{
		DepthPriorityGroup = NewDepthPriorityGroup;
		MarkRenderStateDirty();
	}
}



void UPrimitiveComponent::SetViewOwnerDepthPriorityGroup(
	bool bNewUseViewOwnerDepthPriorityGroup,
	ESceneDepthPriorityGroup NewViewOwnerDepthPriorityGroup
	)
{
	bUseViewOwnerDepthPriorityGroup = bNewUseViewOwnerDepthPriorityGroup;
	ViewOwnerDepthPriorityGroup = NewViewOwnerDepthPriorityGroup;
	MarkRenderStateDirty();
}

bool UPrimitiveComponent::IsWorldGeometry() const
{
	// if modify flag doesn't change, and 
	// it's saying it's movement is static, we considered to be world geom
	// @Todo collision: we could check if bEnableCollision==true here as well
	// but then if we disable collision, they just become non world geometry. 
	// not sure if that would be best way to do this yet
	return Mobility != EComponentMobility::Movable && GetCollisionObjectType()==ECC_WorldStatic;
}

ECollisionChannel UPrimitiveComponent::GetCollisionObjectType() const
{
	return ECollisionChannel(BodyInstance.GetObjectType());
}

void UPrimitiveComponent::SetBoundsScale(float NewBoundsScale)
{
	BoundsScale = NewBoundsScale;
	UpdateBounds();
	MarkRenderTransformDirty();
}

UMaterialInterface* UPrimitiveComponent::GetMaterial(int32 Index) const
{
	// This function should be overridden
	return nullptr;
}

int32 UPrimitiveComponent::GetMaterialIndex(FName MaterialSlotName) const
{
	// This function should be overridden
	return INDEX_NONE;
}

TArray<FName> UPrimitiveComponent::GetMaterialSlotNames() const
{
	// This function should be overridden
	return TArray<FName>();
}

bool UPrimitiveComponent::IsMaterialSlotNameValid(FName MaterialSlotName) const
{
	// This function should be overridden
	return false;
}

UMaterialInterface* UPrimitiveComponent::GetMaterialByName(FName MaterialSlotName) const
{
	return nullptr;
}

void UPrimitiveComponent::SetMaterial(int32 Index, UMaterialInterface* InMaterial)
{
	// This function should be overridden
}

void UPrimitiveComponent::SetMaterialByName(FName MaterialSlotName, class UMaterialInterface* Material)
{
	// This function should be overridden
}

int32 UPrimitiveComponent::GetNumMaterials() const
{
	// This function should be overridden
	return 0;
}

UMaterialInstanceDynamic* UPrimitiveComponent::CreateAndSetMaterialInstanceDynamic(int32 ElementIndex)
{
	UMaterialInterface* MaterialInstance = GetMaterial(ElementIndex);
	UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(MaterialInstance);

	if (MaterialInstance && !MID)
	{
		// Create and set the dynamic material instance.
		MID = UMaterialInstanceDynamic::Create(MaterialInstance, this);
		SetMaterial(ElementIndex, MID);
	}
	else if (!MaterialInstance)
	{
		UE_LOG(LogPrimitiveComponent, Warning, TEXT("CreateAndSetMaterialInstanceDynamic on %s: Material index %d is invalid."), *GetPathName(), ElementIndex);
	}

	return MID;
}

UMaterialInstanceDynamic* UPrimitiveComponent::CreateAndSetMaterialInstanceDynamicFromMaterial(int32 ElementIndex, class UMaterialInterface* Parent)
{
	if (Parent)
	{
		SetMaterial(ElementIndex, Parent);
		return CreateAndSetMaterialInstanceDynamic(ElementIndex);
	}

	return NULL;
}

UMaterialInstanceDynamic* UPrimitiveComponent::CreateDynamicMaterialInstance(int32 ElementIndex, class UMaterialInterface* SourceMaterial, FName OptionalName)
{
	if (SourceMaterial)
	{
		SetMaterial(ElementIndex, SourceMaterial);
	}

	UMaterialInterface* MaterialInstance = GetMaterial(ElementIndex);
	UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(MaterialInstance);

	if (MaterialInstance && !MID)
	{
		// Create and set the dynamic material instance.
		MID = UMaterialInstanceDynamic::Create(MaterialInstance, this, OptionalName);
		SetMaterial(ElementIndex, MID);
	}
	else if (!MaterialInstance)
	{
#if !(UE_BUILD_TEST || UE_BUILD_SHIPPING)
		UE_LOG(LogPrimitiveComponent, Warning, TEXT("CreateDynamicMaterialInstance on %s: Material index %d is invalid."), *GetPathName(), ElementIndex);
#endif
	}

	return MID;
}

void UPrimitiveComponent::ResetCustomPrimitiveData()
{
	CustomPrimitiveDataInternal.Data = CustomPrimitiveData.Data;
}

/** Attempt to set the primitive data and return true if successful */
bool SetPrimitiveData(FCustomPrimitiveData& PrimitiveData, int32 DataIndex, const TArray<float>& Values)
{
	// Can only set data on valid indices and only if there's actually any data to set
	if (DataIndex >= 0 && DataIndex < FCustomPrimitiveData::NumCustomPrimitiveDataFloats && Values.Num() > 0)
	{
		// Number of floats needed in the custom primitive data array
		const int32 NeededFloats = FMath::Min(DataIndex + Values.Num(), FCustomPrimitiveData::NumCustomPrimitiveDataFloats);

		// Number of value to copy into the custom primitive data array at index DataIndex. Capped to not overflow
		const int32 NumValuesToSet = FMath::Min(Values.Num(), FCustomPrimitiveData::NumCustomPrimitiveDataFloats - DataIndex);

		// If trying to set data on an index which doesn't exist yet, allocate up to it
		if (NeededFloats > PrimitiveData.Data.Num())
		{
			PrimitiveData.Data.SetNumZeroed(NeededFloats);
		}

		// Only update data if it has changed
		if (FMemory::Memcmp(&PrimitiveData.Data[DataIndex], Values.GetData(), NumValuesToSet * sizeof(float)) != 0)
		{
			FMemory::Memcpy(&PrimitiveData.Data[DataIndex], Values.GetData(), NumValuesToSet * sizeof(float));

			return true;
		}
	}

	return false;
}

void UPrimitiveComponent::SetCustomPrimitiveDataInternal(int32 DataIndex, const TArray<float>& Values)
{
	if (SetPrimitiveData(CustomPrimitiveDataInternal, DataIndex, Values))
	{
		UWorld* World = GetWorld();
		if (World && World->Scene)
		{
			World->Scene->UpdateCustomPrimitiveData(this);
		}
	}
}

void UPrimitiveComponent::SetDefaultCustomPrimitiveData(int32 DataIndex, const TArray<float>& Values)
{
	if (SetPrimitiveData(CustomPrimitiveData, DataIndex, Values))
	{
		ResetCustomPrimitiveData();
		MarkRenderStateDirty();
	}
}

int32 UPrimitiveComponent::GetCustomPrimitiveDataIndexForScalarParameter(FName ParameterName) const
{
	const int32 NumMaterials = GetNumMaterials();

	for (int32 i = 0; i < NumMaterials; ++i)
	{
		if (UMaterialInterface* Material = GetMaterial(i))
		{
			FMaterialParameterMetadata ParameterMetadata;
			if (Material->GetParameterValue(EMaterialParameterType::Scalar, FMemoryImageMaterialParameterInfo(ParameterName), ParameterMetadata, EMaterialGetParameterValueFlags::CheckAll))
			{
				return ParameterMetadata.PrimitiveDataIndex;
			}
		}
	}

	return INDEX_NONE;
}

int32 UPrimitiveComponent::GetCustomPrimitiveDataIndexForVectorParameter(FName ParameterName) const
{
	const int32 NumMaterials = GetNumMaterials();

	for (int32 i = 0; i < NumMaterials; ++i)
	{
		if (UMaterialInterface* Material = GetMaterial(i))
		{
			FMaterialParameterMetadata ParameterMetadata;
			if (Material->GetParameterValue(EMaterialParameterType::Vector, FMemoryImageMaterialParameterInfo(ParameterName), ParameterMetadata, EMaterialGetParameterValueFlags::CheckAll))
			{
				return ParameterMetadata.PrimitiveDataIndex;
			}
		}
	}

	return INDEX_NONE;
}

void UPrimitiveComponent::SetScalarParameterForCustomPrimitiveData(FName ParameterName, float Value)
{
	int32 PrimitiveDataIndex = GetCustomPrimitiveDataIndexForScalarParameter(ParameterName);

	if (PrimitiveDataIndex > INDEX_NONE)
	{
		SetCustomPrimitiveDataInternal(PrimitiveDataIndex, {Value});
	}
}

void UPrimitiveComponent::SetVectorParameterForCustomPrimitiveData(FName ParameterName, FVector4 Value)
{
	const int32 PrimitiveDataIndex = GetCustomPrimitiveDataIndexForVectorParameter(ParameterName);

	if (PrimitiveDataIndex > INDEX_NONE)
	{	// LWC_TODO: precision loss
		FVector4f ValueFlt(Value);
		SetCustomPrimitiveDataInternal(PrimitiveDataIndex, { ValueFlt.X, ValueFlt.Y, ValueFlt.Z, ValueFlt.W });
	}
}

void UPrimitiveComponent::SetCustomPrimitiveDataFloat(int32 DataIndex, float Value)
{
	SetCustomPrimitiveDataInternal(DataIndex, {Value});
}

void UPrimitiveComponent::SetCustomPrimitiveDataVector2(int32 DataIndex, FVector2D Value)
{
	// LWC_TODO: precision loss
	FVector2f ValueFlt(Value);
	SetCustomPrimitiveDataInternal(DataIndex, {ValueFlt.X, ValueFlt.Y});
}

void UPrimitiveComponent::SetCustomPrimitiveDataVector3(int32 DataIndex, FVector Value)
{
	// LWC_TODO: precision loss
	FVector3f ValueFlt(Value);
	SetCustomPrimitiveDataInternal(DataIndex, {ValueFlt.X, ValueFlt.Y, ValueFlt.Z});
}

void UPrimitiveComponent::SetCustomPrimitiveDataVector4(int32 DataIndex, FVector4 Value)
{
	// LWC_TODO: precision loss
	FVector4f ValueFlt(Value);
	SetCustomPrimitiveDataInternal(DataIndex, {ValueFlt.X, ValueFlt.Y, ValueFlt.Z, ValueFlt.W});
}

void UPrimitiveComponent::SetScalarParameterForDefaultCustomPrimitiveData(FName ParameterName, float Value)
{
	int32 PrimitiveDataIndex = GetCustomPrimitiveDataIndexForScalarParameter(ParameterName);

	if (PrimitiveDataIndex > INDEX_NONE)
	{
		SetDefaultCustomPrimitiveData(PrimitiveDataIndex, { Value });
	}
}

void UPrimitiveComponent::SetVectorParameterForDefaultCustomPrimitiveData(FName ParameterName, FVector4 Value)
{
	const int32 PrimitiveDataIndex = GetCustomPrimitiveDataIndexForVectorParameter(ParameterName);

	if (PrimitiveDataIndex > INDEX_NONE)
	{
		// LWC_TODO: precision loss
		FVector4f ValueFlt(Value);
		SetDefaultCustomPrimitiveData(PrimitiveDataIndex, { ValueFlt.X, ValueFlt.Y, ValueFlt.Z, ValueFlt.W });
	}
}

void UPrimitiveComponent::SetDefaultCustomPrimitiveDataFloat(int32 DataIndex, float Value)
{
	SetDefaultCustomPrimitiveData(DataIndex, { Value });
}

void UPrimitiveComponent::SetDefaultCustomPrimitiveDataVector2(int32 DataIndex, FVector2D Value)
{
	FVector2f ValueFlt(Value);
	SetDefaultCustomPrimitiveData(DataIndex, { ValueFlt.X, ValueFlt.Y });
}

void UPrimitiveComponent::SetDefaultCustomPrimitiveDataVector3(int32 DataIndex, FVector Value)
{
	FVector3f ValueFlt(Value);
	SetDefaultCustomPrimitiveData(DataIndex, { ValueFlt.X, ValueFlt.Y, ValueFlt.Z });
}

void UPrimitiveComponent::SetDefaultCustomPrimitiveDataVector4(int32 DataIndex, FVector4 Value)
{
	FVector4f ValueFlt(Value);
	SetDefaultCustomPrimitiveData(DataIndex, { ValueFlt.X, ValueFlt.Y, ValueFlt.Z, ValueFlt.W });
}

UMaterialInterface* UPrimitiveComponent::GetMaterialFromCollisionFaceIndex(int32 FaceIndex, int32& SectionIndex) const
{
	//This function should be overriden
	SectionIndex = 0;
	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
// MOVECOMPONENT PROFILING CODE

// LOOKING_FOR_PERF_ISSUES
#define PERF_MOVECOMPONENT_STATS 0

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) && PERF_MOVECOMPONENT_STATS
extern bool GShouldLogOutAFrameOfMoveComponent;

/** 
 *	Class to start/stop timer when it goes outside MoveComponent scope.
 *	We keep all results from different MoveComponent calls until we reach the top level, and then print them all out.
 *	That way we can show totals before breakdown, and not pollute timings with log time.
 */
class FScopedMoveCompTimer
{
	/** Struct to contain one temporary MoveActor timing, until we finish entire move and can print it out. */
	struct FMoveTimer
	{
		AActor* Actor;
		FVector Delta;
		double Time;
		int32 Depth;
		bool bDidLineCheck;
		bool bDidEncroachCheck;
	};

	/** Array of all moves within one 'outer' move. */
	static TArray<FMoveTimer> Moves;
	/** Current depth in movement hierarchy */
	static int32 Depth;

	/** Time that this scoped move started. */
	double	StartTime;
	/** Index into Moves array to put results of this MoveActor timing. */
	int32		MoveIndex;
public:

	/** If we did a line check during this MoveActor. */
	bool	bDidLineCheck;
	/** If we did an encroach check during this MoveActor. */
	bool	bDidEncroachCheck;

	FScopedMoveCompTimer(AActor* Actor, const FVector& Delta)
		: StartTime(0.0)
	    , MoveIndex(-1)
	    , bDidLineCheck(false)
		, bDidEncroachCheck(false)
	{
		if(GShouldLogOutAFrameOfMoveComponent)
		{
			// Add new entry to temp results array, and save actor and current stack depth
			MoveIndex = Moves.AddZeroed(1);
			Moves(MoveIndex).Actor = Actor;
			Moves(MoveIndex).Depth = Depth;
			Moves(MoveIndex).Delta = Delta;
			Depth++;

			StartTime = FPlatformTime::Seconds(); // Start timer.
		}
	}

	~FScopedMoveCompTimer()
	{
		if(GShouldLogOutAFrameOfMoveComponent)
		{
			// Record total time MoveActor took
			const double TakeTime = FPlatformTime::Seconds() - StartTime;

			check(Depth > 0);
			check(MoveIndex < Moves.Num());

			// Update entry with timing results
			Moves(MoveIndex).Time = TakeTime;
			Moves(MoveIndex).bDidLineCheck = bDidLineCheck;
			Moves(MoveIndex).bDidEncroachCheck = bDidEncroachCheck;

			Depth--;

			// Reached the top of the move stack again - output what we accumulated!
			if(Depth == 0)
			{
				for(int32 MoveIdx=0; MoveIdx<Moves.Num(); MoveIdx++)
				{
					const FMoveTimer& Move = Moves(MoveIdx);

					// Build indentation
					FString Indent;
					for(int32 i=0; i<Move.Depth; i++)
					{
						Indent += TEXT("  ");
					}

					UE_LOG(LogPrimitiveComponent, Log, TEXT("MOVE%s - %s %5.2fms (%f %f %f) %d %d %s"), *Indent, *Move.Actor->GetName(), Move.Time * 1000.f, Move.Delta.X, Move.Delta.Y, Move.Delta.Z, Move.bDidLineCheck, Move.bDidEncroachCheck, *Move.Actor->GetDetailedInfo());
				}

				// Clear moves array
				Moves.Reset();
			}
		}
	}

};

// Init statics
TArray<FScopedMoveCompTimer::FMoveTimer>	FScopedMoveCompTimer::Moves;
int32											FScopedMoveCompTimer::Depth = 0;

#endif //  !(UE_BUILD_SHIPPING || UE_BUILD_TEST) && PERF_MOVECOMPONENT_STATS


static void PullBackHit(FHitResult& Hit, const FVector& Start, const FVector& End, const float Dist)
{
	const float DesiredTimeBack = FMath::Clamp(0.1f, 0.1f/Dist, 1.f/Dist) + 0.001f;
	Hit.Time = FMath::Clamp( Hit.Time - DesiredTimeBack, 0.f, 1.f );
}

/**
 * PERF_ISSUE_FINDER
 *
 * MoveComponent should not take a long time to execute.  If it is then then there is probably something wrong.
 *
 * Turn this on to have the engine log out when a specific actor is taking longer than 
 * PERF_SHOW_MOVECOMPONENT_TAKING_LONG_TIME_AMOUNT to move.  This is a great way to catch cases where
 * collision has been enabled but it should not have been.  Or if a specific actor is doing something evil
 *
 **/
//#define PERF_SHOW_MOVECOMPONENT_TAKING_LONG_TIME 1
const static float PERF_SHOW_MOVECOMPONENT_TAKING_LONG_TIME_AMOUNT = 2.0f; // modify this value to look at larger or smaller sets of "bad" actors


static bool ShouldIgnoreHitResult(const UWorld* InWorld, FHitResult const& TestHit, FVector const& MovementDirDenormalized, const AActor* MovingActor, EMoveComponentFlags MoveFlags)
{
	if (TestHit.bBlockingHit)
	{
		// check "ignore bases" functionality
		if ( (MoveFlags & MOVECOMP_IgnoreBases) && MovingActor )	//we let overlap components go through because their overlap is still needed and will cause beginOverlap/endOverlap events
		{
			// ignore if there's a base relationship between moving actor and hit actor
			AActor const* const HitActor = TestHit.HitObjectHandle.FetchActor();
			if (HitActor)
			{
				if (MovingActor->IsBasedOnActor(HitActor) || HitActor->IsBasedOnActor(MovingActor))
				{
					return true;
				}
			}
		}
	
		// If we started penetrating, we may want to ignore it if we are moving out of penetration.
		// This helps prevent getting stuck in walls.
		if ( (TestHit.Distance < PrimitiveComponentCVars::HitDistanceToleranceCVar || TestHit.bStartPenetrating) && !(MoveFlags & MOVECOMP_NeverIgnoreBlockingOverlaps) )
		{
 			const float DotTolerance = PrimitiveComponentCVars::InitialOverlapToleranceCVar;

			// Dot product of movement direction against 'exit' direction
			const FVector MovementDir = MovementDirDenormalized.GetSafeNormal();
			const float MoveDot = (TestHit.ImpactNormal | MovementDir);

			const bool bMovingOut = MoveDot > DotTolerance;

	#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			{
				if (CVarShowInitialOverlaps != 0)
				{
					UE_LOG(LogTemp, Log, TEXT("Overlapping %s Dir %s Dot %f Normal %s Depth %f"), *GetNameSafe(TestHit.Component.Get()), *MovementDir.ToString(), MoveDot, *TestHit.ImpactNormal.ToString(), TestHit.PenetrationDepth);
					DrawDebugDirectionalArrow(InWorld, TestHit.TraceStart, TestHit.TraceStart + 30.f * TestHit.ImpactNormal, 5.f, bMovingOut ? FColor(64,128,255) : FColor(255,64,64), false, 4.f);
					if (TestHit.PenetrationDepth > UE_KINDA_SMALL_NUMBER)
					{
						DrawDebugDirectionalArrow(InWorld, TestHit.TraceStart, TestHit.TraceStart + TestHit.PenetrationDepth * TestHit.Normal, 5.f, FColor(64,255,64), false, 4.f);
					}
				}
			}
	#endif

			// If we are moving out, ignore this result!
			if (bMovingOut)
			{
				return true;
			}
		}
	}

	return false;
}

static FORCEINLINE_DEBUGGABLE bool ShouldIgnoreOverlapResult(const UWorld* World, const AActor* ThisActor, const UPrimitiveComponent& ThisComponent, const FActorInstanceHandle& OtherActor, const UPrimitiveComponent& OtherComponent, bool bCheckOverlapFlags)
{
	// Don't overlap with self
	if (&ThisComponent == &OtherComponent)
	{
		return true;
	}

	if (bCheckOverlapFlags)
	{
		// Both components must set GetGenerateOverlapEvents()
		if (!ThisComponent.GetGenerateOverlapEvents() || !OtherComponent.GetGenerateOverlapEvents())
		{
			return true;
		}
	}

	if (!ThisActor || !OtherActor)
	{
		return true;
	}

	if (!World || OtherActor == World->GetWorldSettings() || (OtherActor.GetCachedActor() && !OtherActor.GetCachedActor()->IsActorInitialized()))
	{
		return true;
	}

	return false;
}


void UPrimitiveComponent::InitSweepCollisionParams(FCollisionQueryParams &OutParams, FCollisionResponseParams& OutResponseParam) const
{
	OutResponseParam.CollisionResponse = BodyInstance.GetResponseToChannels();
	OutParams.AddIgnoredActors(MoveIgnoreActors);
	OutParams.AddIgnoredComponents(MoveIgnoreComponents);
	OutParams.bTraceComplex = bTraceComplexOnMove;
	OutParams.bReturnPhysicalMaterial = bReturnMaterialOnMove;
	OutParams.IgnoreMask = GetMoveIgnoreMask();
}

void UPrimitiveComponent::SetMoveIgnoreMask(FMaskFilter InMoveIgnoreMask)
{
	if (ensure(InMoveIgnoreMask < (1 << NumExtraFilterBits))) // We only have a limited nubmer of bits for the mask. TODO: don't assert, and make this a nicer exposed value.
	{
		MoveIgnoreMask = InMoveIgnoreMask;
	}
}

bool UPrimitiveComponent::ShouldComponentIgnoreHitResult(FHitResult const& TestHit, EMoveComponentFlags MoveFlags)
{
	// Check if the hit actors root actor is in the ignore array
	if (MoveFlags & MOVECOMP_CheckBlockingRootActorInIgnoreList)
	{
		AActor const* const HitActor = TestHit.HitObjectHandle.FetchActor();
		if (HitActor)
		{
			if (USceneComponent* RootSceneComp = HitActor->GetRootComponent())
			{
				if (AActor* RootActor = RootSceneComp->GetAttachmentRootActor())
				{
					return MoveIgnoreActors.Contains(RootActor);
				}
			}
		}
	}

	return false;
}

FCollisionShape UPrimitiveComponent::GetCollisionShape(float Inflation) const
{
	// This is intended to be overridden by shape classes, so this is a simple, large bounding shape.
	FVector Extent = Bounds.BoxExtent + Inflation;
	if (Inflation < 0.f)
	{
		// Don't shrink below zero size.
		Extent = Extent.ComponentMax(FVector::ZeroVector);
	}

	return FCollisionShape::MakeBox(Extent);
}


bool UPrimitiveComponent::MoveComponentImpl( const FVector& Delta, const FQuat& NewRotationQuat, bool bSweep, FHitResult* OutHit, EMoveComponentFlags MoveFlags, ETeleportType Teleport)
{
	SCOPE_CYCLE_COUNTER(STAT_MoveComponentTime);
	CSV_SCOPED_TIMING_STAT(PrimitiveComponent, MoveComponentTime);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) && PERF_MOVECOMPONENT_STATS
	FScopedMoveCompTimer MoveTimer(this, Delta);
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) && PERF_MOVECOMPONENT_STATS

#if defined(PERF_SHOW_MOVECOMPONENT_TAKING_LONG_TIME) || LOOKING_FOR_PERF_ISSUES
	uint32 MoveCompTakingLongTime=0;
	CLOCK_CYCLES(MoveCompTakingLongTime);
#endif

	// static things can move before they are registered (e.g. immediately after streaming), but not after.
	if (!IsValid(this) || CheckStaticMobilityAndWarn(PrimitiveComponentStatics::MobilityWarnText))
	{
		if (OutHit)
		{
			OutHit->Init();
		}
		return false;
	}

	ConditionalUpdateComponentToWorld();

	// Set up
	const FVector TraceStart = GetComponentLocation();
	const FVector TraceEnd = TraceStart + Delta;
	float DeltaSizeSq = (TraceEnd - TraceStart).SizeSquared();				// Recalc here to account for precision loss of float addition
	const FQuat InitialRotationQuat = GetComponentTransform().GetRotation();

	// ComponentSweepMulti does nothing if moving < KINDA_SMALL_NUMBER in distance, so it's important to not try to sweep distances smaller than that. 
	const float MinMovementDistSq = (bSweep ? FMath::Square(4.f* UE_KINDA_SMALL_NUMBER) : 0.f);
	if (DeltaSizeSq <= MinMovementDistSq)
	{
		// Skip if no vector or rotation.
		if (NewRotationQuat.Equals(InitialRotationQuat, SCENECOMPONENT_QUAT_TOLERANCE))
		{
			// copy to optional output param
			if (OutHit)
			{
				OutHit->Init(TraceStart, TraceEnd);
			}
			return true;
		}
		DeltaSizeSq = 0.f;
	}

	const bool bSkipPhysicsMove = ((MoveFlags & MOVECOMP_SkipPhysicsMove) != MOVECOMP_NoFlags);

	// WARNING: HitResult is only partially initialized in some paths. All data is valid only if bFilledHitResult is true.
	FHitResult BlockingHit(NoInit);
	BlockingHit.bBlockingHit = false;
	BlockingHit.Time = 1.f;
	bool bFilledHitResult = false;
	bool bMoved = false;
	bool bIncludesOverlapsAtEnd = false;
	bool bRotationOnly = false;
	TInlineOverlapInfoArray PendingOverlaps;
	AActor* const Actor = GetOwner();

	if ( !bSweep )
	{
		// not sweeping, just go directly to the new transform
		bMoved = InternalSetWorldLocationAndRotation(TraceEnd, NewRotationQuat, bSkipPhysicsMove, Teleport);
		bRotationOnly = (DeltaSizeSq == 0);
		bIncludesOverlapsAtEnd = bRotationOnly && (AreSymmetricRotations(InitialRotationQuat, NewRotationQuat, GetComponentScale())) && IsQueryCollisionEnabled();
	}
	else
	{
		TArray<FHitResult> Hits;
		FVector NewLocation = TraceStart;

		// Perform movement collision checking if needed for this actor.
		const bool bCollisionEnabled = IsQueryCollisionEnabled();
		UWorld* const MyWorld = GetWorld();
		if (MyWorld && bCollisionEnabled && (DeltaSizeSq > 0.f))
		{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if(!IsRegistered() && !MyWorld->bIsTearingDown)
			{
				if (Actor)
				{
					ensureMsgf(IsRegistered(), TEXT("%s MovedComponent %s not registered during sweep (IsValid %d)"), *Actor->GetName(), *GetName(), IsValid(Actor));
				}
				else
				{ //-V523
					ensureMsgf(IsRegistered(), TEXT("Non-actor MovedComponent %s not registered during sweep"), *GetFullName());
				}
			}
#endif

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) && PERF_MOVECOMPONENT_STATS
			MoveTimer.bDidLineCheck = true;
#endif 
			static const FName TraceTagName = TEXT("MoveComponent");
			const bool bForceGatherOverlaps = !ShouldCheckOverlapFlagToQueueOverlaps(*this);
			FComponentQueryParams Params(SCENE_QUERY_STAT(MoveComponent), Actor);
			FCollisionResponseParams ResponseParam;
			InitSweepCollisionParams(Params, ResponseParam);
			Params.bIgnoreTouches |= !(GetGenerateOverlapEvents() || bForceGatherOverlaps);
			Params.TraceTag = TraceTagName;

			bool const bHadBlockingHit = MyWorld->ComponentSweepMulti(Hits, this, TraceStart, TraceEnd, InitialRotationQuat, Params);

			if (Hits.Num() > 0)
			{
				const float DeltaSize = FMath::Sqrt(DeltaSizeSq);
				for(int32 HitIdx=0; HitIdx<Hits.Num(); HitIdx++)
				{
					PullBackHit(Hits[HitIdx], TraceStart, TraceEnd, DeltaSize);
				}
			}

			// If we had a valid blocking hit, store it.
			// If we are looking for overlaps, store those as well.
			int32 FirstNonInitialOverlapIdx = INDEX_NONE;
			if (bHadBlockingHit || (GetGenerateOverlapEvents() || bForceGatherOverlaps))
			{
				int32 BlockingHitIndex = INDEX_NONE;
				float BlockingHitNormalDotDelta = UE_BIG_NUMBER;
				for( int32 HitIdx = 0; HitIdx < Hits.Num(); HitIdx++ )
				{
					const FHitResult& TestHit = Hits[HitIdx];

					if (TestHit.bBlockingHit)
					{
						if (!ShouldIgnoreHitResult(MyWorld, TestHit, Delta, Actor, MoveFlags) && !ShouldComponentIgnoreHitResult(TestHit, MoveFlags))
						{
							if (TestHit.bStartPenetrating)
							{
								// We may have multiple initial hits, and want to choose the one with the normal most opposed to our movement.
								const float NormalDotDelta = (TestHit.ImpactNormal | Delta);
								if (NormalDotDelta < BlockingHitNormalDotDelta)
								{
									BlockingHitNormalDotDelta = NormalDotDelta;
									BlockingHitIndex = HitIdx;
								}
							}
							else if (BlockingHitIndex == INDEX_NONE)
							{
								// First non-overlapping blocking hit should be used, if an overlapping hit was not.
								// This should be the only non-overlapping blocking hit, and last in the results.
								BlockingHitIndex = HitIdx;
								break;
							}
						}
					}
					else if (GetGenerateOverlapEvents() || bForceGatherOverlaps)
					{
						UPrimitiveComponent* OverlapComponent = TestHit.Component.Get();
						if (OverlapComponent && (OverlapComponent->GetGenerateOverlapEvents() || bForceGatherOverlaps))
						{
							if (!ShouldIgnoreOverlapResult(MyWorld, Actor, *this, TestHit.HitObjectHandle, *OverlapComponent, /*bCheckOverlapFlags=*/ !bForceGatherOverlaps))
							{
								// don't process touch events after initial blocking hits
								if (BlockingHitIndex >= 0 && TestHit.Time > Hits[BlockingHitIndex].Time)
								{
									break;
								}

								if (FirstNonInitialOverlapIdx == INDEX_NONE && TestHit.Time > 0.f)
								{
									// We are about to add the first non-initial overlap.
									FirstNonInitialOverlapIdx = PendingOverlaps.Num();
								}

								// cache touches
								AddUniqueOverlapFast(PendingOverlaps, FOverlapInfo(TestHit));
							}
						}
					}
				}

				// Update blocking hit, if there was a valid one.
				if (BlockingHitIndex >= 0)
				{
					BlockingHit = Hits[BlockingHitIndex];
					bFilledHitResult = true;
				}
			}
		
			// Update NewLocation based on the hit result
			if (!BlockingHit.bBlockingHit)
			{
				NewLocation = TraceEnd;
			}
			else
			{
				check(bFilledHitResult);
				NewLocation = TraceStart + (BlockingHit.Time * (TraceEnd - TraceStart));

				// Sanity check
				const FVector ToNewLocation = (NewLocation - TraceStart);
				if (ToNewLocation.SizeSquared() <= MinMovementDistSq)
				{
					// We don't want really small movements to put us on or inside a surface.
					NewLocation = TraceStart;
					BlockingHit.Time = 0.f;

					// Remove any pending overlaps after this point, we are not going as far as we swept.
					if (FirstNonInitialOverlapIdx != INDEX_NONE)
					{
						PendingOverlaps.SetNum(FirstNonInitialOverlapIdx, EAllowShrinking::No);
					}
				}
			}

			bIncludesOverlapsAtEnd = AreSymmetricRotations(InitialRotationQuat, NewRotationQuat, GetComponentScale());

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if (UCheatManager::IsDebugCapsuleSweepPawnEnabled() && BlockingHit.bBlockingHit && !IsZeroExtent())
			{
				// this is sole debug purpose to find how capsule trace information was when hit 
				// to resolve stuck or improve our movement system - To turn this on, use DebugCapsuleSweepPawn
				APawn const* const ActorPawn = (Actor ? Cast<APawn>(Actor) : NULL);
				if (ActorPawn && ActorPawn->Controller && ActorPawn->Controller->IsLocalPlayerController())
				{
					APlayerController const* const PC = CastChecked<APlayerController>(ActorPawn->Controller);
					if (PC->CheatManager)
					{
						FVector CylExtent = ActorPawn->GetSimpleCollisionCylinderExtent()*FVector(1.001f,1.001f,1.0f);							
						FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(CylExtent);
						PC->CheatManager->AddCapsuleSweepDebugInfo(TraceStart, TraceEnd, BlockingHit.ImpactPoint, BlockingHit.Normal, BlockingHit.ImpactNormal, BlockingHit.Location, CapsuleShape.GetCapsuleHalfHeight(), CapsuleShape.GetCapsuleRadius(), true, (BlockingHit.bStartPenetrating && BlockingHit.bBlockingHit) ? true: false);
					}
				}
			}
#endif
		}
		else if (DeltaSizeSq > 0.f)
		{
			// apply move delta even if components has collisions disabled
			NewLocation += Delta;
			bIncludesOverlapsAtEnd = false;
		}
		else if (DeltaSizeSq == 0.f && bCollisionEnabled)
		{
			bIncludesOverlapsAtEnd = AreSymmetricRotations(InitialRotationQuat, NewRotationQuat, GetComponentScale());
			bRotationOnly = true;
		}

		// Update the location.  This will teleport any child components as well (not sweep).
		bMoved = InternalSetWorldLocationAndRotation(NewLocation, NewRotationQuat, bSkipPhysicsMove, Teleport);
	}

	// Handle overlap notifications.
	if (bMoved)
	{
		if (IsDeferringMovementUpdates())
		{
			// Defer UpdateOverlaps until the scoped move ends.
			FScopedMovementUpdate* ScopedUpdate = GetCurrentScopedMovement();
			if (bRotationOnly && bIncludesOverlapsAtEnd)
			{
				ScopedUpdate->KeepCurrentOverlapsAfterRotation(bSweep);
			}
			else
			{
				ScopedUpdate->AppendOverlapsAfterMove(PendingOverlaps, bSweep, bIncludesOverlapsAtEnd);
			}
		}
		else
		{
			if (bIncludesOverlapsAtEnd)
			{
				TInlineOverlapInfoArray OverlapsAtEndLocation;
				bool bHasEndOverlaps = false;
				if (bRotationOnly)
				{
					bHasEndOverlaps = ConvertRotationOverlapsToCurrentOverlaps(OverlapsAtEndLocation, OverlappingComponents);
				}
				else
				{
					bHasEndOverlaps = ConvertSweptOverlapsToCurrentOverlaps(OverlapsAtEndLocation, PendingOverlaps, 0, GetComponentLocation(), GetComponentQuat());
				}
				TOverlapArrayView PendingOverlapsView(PendingOverlaps);
				TOverlapArrayView OverlapsAtEndView(OverlapsAtEndLocation);
				UpdateOverlaps(&PendingOverlapsView, true, bHasEndOverlaps ? &OverlapsAtEndView : nullptr);
			}
			else
			{
				TOverlapArrayView PendingOverlapsView(PendingOverlaps);
				UpdateOverlaps(&PendingOverlapsView, true, nullptr);
			}
		}
	}

	// Handle blocking hit notifications. Avoid if pending kill (which could happen after overlaps).
	const bool bAllowHitDispatch = !BlockingHit.bStartPenetrating || !(MoveFlags & MOVECOMP_DisableBlockingOverlapDispatch);
	if (BlockingHit.bBlockingHit && bAllowHitDispatch && IsValid(this))
	{
		check(bFilledHitResult);
		if (IsDeferringMovementUpdates())
		{
			FScopedMovementUpdate* ScopedUpdate = GetCurrentScopedMovement();
			ScopedUpdate->AppendBlockingHitAfterMove(BlockingHit);
		}
		else
		{
			DispatchBlockingHit(*Actor, BlockingHit);
		}
	}

#if defined(PERF_SHOW_MOVECOMPONENT_TAKING_LONG_TIME) || LOOKING_FOR_PERF_ISSUES
	UNCLOCK_CYCLES(MoveCompTakingLongTime);
	const float MSec = FPlatformTime::ToMilliseconds(MoveCompTakingLongTime);
	if( MSec > PERF_SHOW_MOVECOMPONENT_TAKING_LONG_TIME_AMOUNT )
	{
		if (GetOwner())
		{
			UE_LOG(LogPrimitiveComponent, Log, TEXT("%10f executing MoveComponent for %s owned by %s"), MSec, *GetName(), *GetOwner()->GetFullName() );
		}
		else
		{
			UE_LOG(LogPrimitiveComponent, Log, TEXT("%10f executing MoveComponent for %s"), MSec, *GetFullName() );
		}
	}
#endif

	// copy to optional output param
	if (OutHit)
	{
		if (bFilledHitResult)
		{
			*OutHit = BlockingHit;
		}
		else
		{
			OutHit->Init(TraceStart, TraceEnd);
		}
	}

	// Return whether we moved at all.
	return bMoved;
}


void UPrimitiveComponent::DispatchBlockingHit(AActor& Owner, FHitResult const& BlockingHit)
{
	SCOPE_CYCLE_COUNTER(STAT_DispatchBlockingHit);

	UPrimitiveComponent* const BlockingHitComponent = BlockingHit.Component.Get();
	if (BlockingHitComponent)
	{
		Owner.DispatchBlockingHit(this, BlockingHitComponent, true, BlockingHit);

		// Dispatch above could kill the component, so we need to check that.
		if (IsValid(BlockingHitComponent))
		{
			// BlockingHit.GetActor() could be marked for deletion in DispatchBlockingHit(), which would make the weak pointer return NULL.
			if (AActor* const BlockingHitActor = BlockingHit.HitObjectHandle.GetManagingActor())
			{
				BlockingHitActor->DispatchBlockingHit(BlockingHitComponent, this, false, BlockingHit);
			}
		}
	}
}

void UPrimitiveComponent::DispatchWakeEvents(ESleepEvent WakeEvent, FName BoneName)
{
	if (ShouldDispatchWakeEvents(BoneName))
	{
		if (WakeEvent == ESleepEvent::SET_Wakeup)
		{
			OnComponentWake.Broadcast(this, BoneName);
		}
		else
		{
			OnComponentSleep.Broadcast(this, BoneName);
		}
	}
	
	//now update children that are welded
	FBodyInstance* RootBI = GetBodyInstance(BoneName, false);
	for(USceneComponent* SceneComp : GetAttachChildren())
	{
		if(UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(SceneComp))
		{
			if(FBodyInstance* BI = PrimComp->GetBodyInstance(BoneName, false))
			{
				if(BI->WeldParent == RootBI)
				{
					PrimComp->DispatchWakeEvents(WakeEvent, BoneName);	
				}
			}
		}
	}

	if (PhysicsReplicationCVars::PredictiveInterpolationCVars::bFakeTargetOnClientWakeUp)
	{
		if (WakeEvent == ESleepEvent::SET_Wakeup && IsSimulatingPhysics())
		{
			AActor* Owner = GetOwner();
			if (Owner && Owner->GetRootComponent() == this)
			{
				Owner->SetFakeNetPhysicsState(/*bShouldSleep*/ true);
			}
		}
	}
}

bool UPrimitiveComponent::ShouldDispatchWakeEvents(FName BoneName) const
{
	FBodyInstance* RootBI = GetBodyInstance(BoneName, false);
	if (RootBI)
	{
		return RootBI->bGenerateWakeEvents;
	}
	return false;
}

void UPrimitiveComponent::GetNavigationData(FNavigationRelevantData& OutData) const
{
	if (bFillCollisionUnderneathForNavmesh)
	{
		FCompositeNavModifier CompositeNavModifier;
		CompositeNavModifier.SetFillCollisionUnderneathForNavmesh(bFillCollisionUnderneathForNavmesh);
		OutData.Modifiers.Add(CompositeNavModifier);
	}
}

bool UPrimitiveComponent::IsNavigationRelevant() const 
{ 
	if (!CanEverAffectNavigation())
	{
		return false;
	}

	if (HasCustomNavigableGeometry() >= EHasCustomNavigableGeometry::EvenIfNotCollidable)
	{
		return true;
	}

	const FCollisionResponseContainer& ResponseToChannels = GetCollisionResponseToChannels();
	return IsQueryCollisionEnabled() &&
		(ResponseToChannels.GetResponse(ECC_Pawn) == ECR_Block || ResponseToChannels.GetResponse(ECC_Vehicle) == ECR_Block);
}

UBodySetup* UPrimitiveComponent::GetNavigableGeometryBodySetup()
{
	return GetBodySetup();
}

FTransform UPrimitiveComponent::GetNavigableGeometryTransform() const
{
	return GetComponentTransform();
}

EHasCustomNavigableGeometry::Type UPrimitiveComponent::HasCustomNavigableGeometry() const
{
	return bHasCustomNavigableGeometry;
}

bool UPrimitiveComponent::DoCustomNavigableGeometryExport(FNavigableGeometryExport& GeomExport) const
{
	return true;
}

FBox UPrimitiveComponent::GetNavigationBounds() const
{
	// Return invalid box when retrieving NavigationBounds before they are being computed at component registration
	return bRegistered ? Bounds.GetBox() : FBox(ForceInit);
}

//////////////////////////////////////////////////////////////////////////
// COLLISION

extern float DebugLineLifetime;


bool UPrimitiveComponent::LineTraceComponent(struct FHitResult& OutHit, const FVector Start, const FVector End, const struct FCollisionQueryParams& Params)
{
	return LineTraceComponent(OutHit, Start, End, DefaultCollisionChannel, Params, FCollisionResponseParams::DefaultResponseParam, FCollisionObjectQueryParams::DefaultObjectQueryParam);
}

bool UPrimitiveComponent::LineTraceComponent(FHitResult& OutHit, const FVector Start, const FVector End, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams)
{
	bool bHaveHit = false;

	if (FBodyInstance* ThisBodyInstance = GetBodyInstance())
	{
		bHaveHit = ThisBodyInstance->LineTrace(OutHit, Start, End, Params.bTraceComplex, Params.bReturnPhysicalMaterial);
	}
	else
	{
		TArray<Chaos::FPhysicsObjectHandle> Objects = GetAllPhysicsObjects();
		FLockedReadPhysicsObjectExternalInterface Interface = FPhysicsObjectExternalInterface::LockRead(Objects);
		Objects = Objects.FilterByPredicate(
			[&Interface](Chaos::FPhysicsObjectHandle Handle)
			{
				return !Interface->AreAllDisabled({ &Handle, 1 });
			}
		);

		Chaos::FPhysicsObjectCollisionInterface_External CollisionInterface{ Interface.GetInterface() };
		ChaosInterface::FRaycastHit BestHit;
		if (CollisionInterface.LineTrace(Objects, Start, End, Params.bTraceComplex, BestHit))
		{
			bHaveHit = true;

			FCollisionFilterData QueryFilter;
			QueryFilter.Word1 = 0xFFFFF;
			ChaosInterface::SetFlags(BestHit, EHitFlags::Distance | EHitFlags::Normal | EHitFlags::Position);

			ConvertQueryImpactHit(GetWorld(), BestHit, OutHit, (End - Start).Size(), QueryFilter, Start, End, nullptr, FTransform{ Start }, true, Params.bReturnPhysicalMaterial);
		}
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (GetWorld()->DebugDrawSceneQueries(Params.TraceTag))
	{
		TArray<FHitResult> Hits;
		if (bHaveHit)
		{
			Hits.Add(OutHit);
		}

		DrawLineTraces(GetWorld(), Start, End, Hits, DebugLineLifetime);
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	return bHaveHit;
}

bool UPrimitiveComponent::SweepComponent(struct FHitResult& OutHit, const FVector Start, const FVector End, const FQuat& ShapeWorldRotation, const FCollisionShape &CollisionShape, bool bTraceComplex)
{
	if (FBodyInstance* ThisBodyInstance = GetBodyInstance())
	{
		return ThisBodyInstance->Sweep(OutHit, Start, End, ShapeWorldRotation, CollisionShape, bTraceComplex);
	}

	FCollisionQueryParams Params = FCollisionQueryParams::DefaultQueryParam;
	Params.bTraceComplex = bTraceComplex;

	if (CollisionShape.IsNearlyZero())
	{
		return LineTraceComponent(OutHit, Start, End, Params);
	}

	FPhysicsShapeAdapter_Chaos ShapeAdapter(ShapeWorldRotation, CollisionShape);
	return SweepComponent(OutHit, Start, End, ShapeWorldRotation, ShapeAdapter.GetGeometry(), DefaultCollisionChannel, Params, FCollisionResponseParams::DefaultResponseParam, FCollisionObjectQueryParams::DefaultObjectQueryParam);
}

bool UPrimitiveComponent::SweepComponent(FHitResult& OutHit, const FVector Start, const FVector End, const FQuat& ShapeWorldRotation, const FPhysicsGeometry& Geometry, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams)
{
	TArray<Chaos::FPhysicsObjectHandle> Objects = GetAllPhysicsObjects();
	FLockedReadPhysicsObjectExternalInterface Interface = FPhysicsObjectExternalInterface::LockRead(Objects);
	Objects = Objects.FilterByPredicate(
		[&Interface](Chaos::FPhysicsObjectHandle Handle)
		{
			return !Interface->AreAllDisabled({ &Handle, 1 });
		}
	);

	Chaos::FPhysicsObjectCollisionInterface_External CollisionInterface{ Interface.GetInterface() };
	ChaosInterface::FSweepHit BestHit;

	Chaos::FSweepParameters SweepParams;
	SweepParams.bSweepComplex = Params.bTraceComplex;

	// TODO: Expose this even further via parameters in the primitive component.
	// For now, having this be always true guarantees us identical behavior to tracing via the Chaos SQ
	// since TSQTraits::GetHitFlags() will always have the MTD flag on.
	SweepParams.bComputeMTD = true;
	if (CollisionInterface.ShapeSweep(Objects, Geometry, FTransform{ ShapeWorldRotation, Start }, End, SweepParams, BestHit))
	{
		FCollisionFilterData QueryFilter;
		QueryFilter.Word1 = 0xFFFFF;
		ChaosInterface::SetFlags(BestHit, EHitFlags::Distance | EHitFlags::Normal | EHitFlags::Position | EHitFlags::FaceIndex);

		bool bHasHit = false;
		ConvertTraceResults<ChaosInterface::FSweepHit>(bHasHit, GetWorld(), 1, &BestHit, (End - Start).Size(), QueryFilter, OutHit, Start, End, &Geometry, FTransform{ ShapeWorldRotation, Start }, 0.f, Params.bReturnFaceIndex, Params.bReturnPhysicalMaterial);
		return bHasHit;
	}

	return false;
}

bool UPrimitiveComponent::ComponentOverlapComponentImpl(class UPrimitiveComponent* PrimComp, const FVector Pos, const FQuat& Quat, const struct FCollisionQueryParams& Params)
{
	// if target is skeletalmeshcomponent and do not support singlebody physics
	USkeletalMeshComponent * OtherComp = Cast<USkeletalMeshComponent>(PrimComp);
	if (OtherComp)
	{
		UE_LOG(LogCollision, Warning, TEXT("ComponentOverlapMulti : (%s) Does not support skeletalmesh with Physics Asset"), *PrimComp->GetPathName());
		return false;
	}

	// if target is Instanced Static Meshes
	UInstancedStaticMeshComponent* InstancedStaticMesh = Cast<UInstancedStaticMeshComponent>(PrimComp);
	if (InstancedStaticMesh)
	{
		if (FBodyInstance* ThisBodyInstance = GetBodyInstance())
		{
			return ThisBodyInstance->OverlapTestForBodies(Pos, Quat, InstancedStaticMesh->InstanceBodies);
		}
		else
		{
			return false;
		}
	}

	FBodyInstance* BI = PrimComp->GetBodyInstance();
	FBodyInstance* ThisBodyInstance = GetBodyInstance();
	if(BI && ThisBodyInstance)
	{
		return BI->OverlapTestForBody(Pos, Quat, ThisBodyInstance);
	}

	TArray<Chaos::FPhysicsObjectHandle> InObjects = PrimComp->GetAllPhysicsObjects();
	TArray<Chaos::FPhysicsObjectHandle> ThisObjects = GetAllPhysicsObjects();

	FLockedReadPhysicsObjectExternalInterface Interface = FPhysicsObjectExternalInterface::LockRead(InObjects);
	InObjects = InObjects.FilterByPredicate(
		[&Interface](Chaos::FPhysicsObjectHandle Handle)
		{
			return !Interface->AreAllDisabled({ &Handle, 1 });
		}
	);

	ThisObjects = ThisObjects.FilterByPredicate(
		[&Interface](Chaos::FPhysicsObjectHandle Handle)
		{
			return !Interface->AreAllDisabled({ &Handle, 1 });
		}
	);

	Chaos::FPhysicsObjectCollisionInterface_External CollisionInterface{ Interface.GetInterface() };
	for (Chaos::FPhysicsObjectHandle InObject : InObjects)
	{
		for (Chaos::FPhysicsObjectHandle ThisObject : ThisObjects)
		{
			if (CollisionInterface.PhysicsObjectOverlap(InObject, FTransform::Identity, ThisObject, FTransform::Identity, Params.bTraceComplex))
			{
				return true;
			}
		}
	}
	return false;
}

bool UPrimitiveComponent::ComponentOverlapComponentWithResultImpl(const class UPrimitiveComponent* const PrimComp, const FVector& Pos, const FQuat& Rot, const FCollisionQueryParams& Params, TArray<FOverlapResult>& OutOverlap) const
{
	const FTransform InTransform{ Rot, Pos };
	TArray<Chaos::FPhysicsObjectHandle> InObjects = PrimComp->GetAllPhysicsObjects();
	TArray<Chaos::FPhysicsObjectHandle> ThisObjects = GetAllPhysicsObjects();

	FLockedReadPhysicsObjectExternalInterface Interface = FPhysicsObjectExternalInterface::LockRead(InObjects);
	InObjects = InObjects.FilterByPredicate(
		[&Interface](Chaos::FPhysicsObjectHandle Handle)
		{
			return !Interface->AreAllDisabled({ &Handle, 1 });
		}
	);

	ThisObjects = ThisObjects.FilterByPredicate(
		[&Interface](Chaos::FPhysicsObjectHandle Handle)
		{
			return !Interface->AreAllDisabled({ &Handle, 1 });
		}
	);

	Chaos::FPhysicsObjectCollisionInterface_External CollisionInterface{ Interface.GetInterface() };
	for (Chaos::FPhysicsObjectHandle InObject : InObjects)
	{
		for (Chaos::FPhysicsObjectHandle ThisObject : ThisObjects)
		{
			TArray<ChaosInterface::FOverlapHit> OverlapHits;
			if (CollisionInterface.PhysicsObjectOverlap(ThisObject, FTransform::Identity, InObject, InTransform, Params.bTraceComplex, OverlapHits))
			{
				TArray<FOverlapResult> Overlaps;

				FCollisionFilterData QueryFilter;
				QueryFilter.Word1 = 0xFFFFF;
				ConvertOverlapResults(OverlapHits.Num(), OverlapHits.GetData(), QueryFilter, Overlaps);

				if (!Overlaps.IsEmpty())
				{
					OutOverlap = Overlaps;
					return true;
				}
			}
		}
	}
	return false;
}

bool UPrimitiveComponent::OverlapComponent(const FVector& Pos, const FQuat& Rot, const struct FCollisionShape& CollisionShape) const
{
	if (FBodyInstance* ThisBodyInstance = GetBodyInstance())
	{
		return ThisBodyInstance->OverlapTest(Pos, Rot, CollisionShape);
	}

	TArray<FOverlapResult> NopResult;
	return OverlapComponentWithResult(Pos, Rot, CollisionShape, NopResult);
}

bool UPrimitiveComponent::OverlapComponentWithResult(const FVector& Pos, const FQuat& Rot, const FCollisionShape& CollisionShape, TArray<FOverlapResult>& OutOverlap) const
{
	FPhysicsShapeAdapter_Chaos ShapeAdapter(Rot, CollisionShape);
	return OverlapComponentWithResult(Pos, Rot, ShapeAdapter.GetGeometry(), DefaultCollisionChannel, FCollisionQueryParams::DefaultQueryParam, FCollisionResponseParams::DefaultResponseParam, FCollisionObjectQueryParams::DefaultObjectQueryParam, OutOverlap);
}

bool UPrimitiveComponent::OverlapComponentWithResult(const FVector& Pos, const FQuat& Rot, const FPhysicsGeometry& Geometry, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams, TArray<FOverlapResult>& OutOverlap) const
{
	TArray<Chaos::FPhysicsObjectHandle> Objects = GetAllPhysicsObjects();
	FLockedReadPhysicsObjectExternalInterface Interface = FPhysicsObjectExternalInterface::LockRead(Objects);
	Objects = Objects.FilterByPredicate(
		[&Interface](Chaos::FPhysicsObjectHandle Handle)
		{
			return !Interface->AreAllDisabled({ &Handle, 1 });
		}
	);

	Chaos::FPhysicsObjectCollisionInterface_External CollisionInterface{ Interface.GetInterface() };
	TArray<ChaosInterface::FOverlapHit> OverlapHits;
	if (CollisionInterface.ShapeOverlap(Objects, Geometry, FTransform{ Rot, Pos }, OverlapHits))
	{
		TArray<FOverlapResult> Overlaps;

		FCollisionFilterData QueryFilter;
		QueryFilter.Word1 = 0xFFFFF;
		ConvertOverlapResults(OverlapHits.Num(), OverlapHits.GetData(), QueryFilter, Overlaps);

		if (!Overlaps.IsEmpty())
		{
			OutOverlap = Overlaps;
			return true;
		}
	}

	return false;
}

bool UPrimitiveComponent::ComputePenetration(FMTDResult& OutMTD, const FCollisionShape & CollisionShape, const FVector& Pos, const FQuat& Rot)
{
	if(FBodyInstance* ComponentBodyInstance = GetBodyInstance())
	{
		return ComponentBodyInstance->OverlapTest(Pos, Rot, CollisionShape, &OutMTD);
	}

	return false;
}

bool UPrimitiveComponent::IsOverlappingComponent(const UPrimitiveComponent* OtherComp) const
{
	for (int32 i=0; i < OverlappingComponents.Num(); ++i)
	{
		if (OverlappingComponents[i].OverlapInfo.Component.Get() == OtherComp)
		{
			return true;
		}
	}
	return false;
}

bool UPrimitiveComponent::IsOverlappingComponent(const FOverlapInfo& Overlap) const
{
	return OverlappingComponents.Find(Overlap) != INDEX_NONE;
}

bool UPrimitiveComponent::IsOverlappingActor(const AActor* Other) const
{
	if (Other)
	{
		for (int32 OverlapIdx=0; OverlapIdx<OverlappingComponents.Num(); ++OverlapIdx)
		{
			UPrimitiveComponent const* const PrimComp = OverlappingComponents[OverlapIdx].OverlapInfo.Component.Get();
			if ( PrimComp && (PrimComp->GetOwner() == Other) )
			{
				return true;
			}
		}
	}

	return false;
}

bool UPrimitiveComponent::GetOverlapsWithActor(const AActor* Actor, TArray<FOverlapInfo>& OutOverlaps) const
{
	return GetOverlapsWithActor_Template(Actor, OutOverlaps);
}

bool UPrimitiveComponent::IsShown(const FEngineShowFlags& ShowFlags) const
{
	return true;
}

#if WITH_EDITOR
bool UPrimitiveComponent::ComponentIsTouchingSelectionBox(const FBox& InSelBBox, const FEngineShowFlags& ShowFlags, const bool bConsiderOnlyBSP, const bool bMustEncompassEntireComponent) const
{
	return IsShown(ShowFlags) && ComponentIsTouchingSelectionBox(InSelBBox, bConsiderOnlyBSP, bMustEncompassEntireComponent);
}

bool UPrimitiveComponent::ComponentIsTouchingSelectionBox(const FBox& InSelBBox, const bool bConsiderOnlyBSP, const bool bMustEncompassEntireComponent) const
{
	if (!bConsiderOnlyBSP)
	{
		const FBox& ComponentBounds = Bounds.GetBox();

		// Check the component bounds versus the selection box
		// If the selection box must encompass the entire component, then both the min and max vector of the bounds must be inside in the selection
		// box to be valid. If the selection box only has to touch the component, then it is sufficient to check if it intersects with the bounds.
		if ((!bMustEncompassEntireComponent && InSelBBox.Intersect(ComponentBounds))
			|| (bMustEncompassEntireComponent && InSelBBox.IsInside(ComponentBounds)))
		{
			return true;
		}
	}

	return false;
}

bool UPrimitiveComponent::ComponentIsTouchingSelectionFrustum(const FConvexVolume& InFrustum, const FEngineShowFlags& ShowFlags, const bool bConsiderOnlyBSP, const bool bMustEncompassEntireComponent) const
{
	return IsShown(ShowFlags) && ComponentIsTouchingSelectionFrustum(InFrustum, bConsiderOnlyBSP, bMustEncompassEntireComponent);
}

bool UPrimitiveComponent::ComponentIsTouchingSelectionFrustum(const FConvexVolume& InFrustum, const bool bConsiderOnlyBSP, const bool bMustEncompassEntireComponent) const
{
	if (!bConsiderOnlyBSP)
	{
		bool bIsFullyContained;
		if (InFrustum.IntersectBox(Bounds.Origin, Bounds.BoxExtent, bIsFullyContained))
		{
			return !bMustEncompassEntireComponent || bIsFullyContained;
		}
	}

	return false;
}
#endif



/** Used to determine if it is ok to call a notification on this object */
extern bool IsActorValidToNotify(AActor* Actor);

// @fixme, duplicated, make an inline member?
bool IsPrimCompValidAndAlive(UPrimitiveComponent* PrimComp)
{
	return IsValid(PrimComp);
}

bool AreActorsOverlapping(const AActor& A, const AActor& B)
{
	// Due to the implementation of IsOverlappingActor() that scans and queries all owned primitive components and their overlaps,
	// we guess that it's cheaper to scan the shorter of the lists.
	if (A.GetComponents().Num() <= B.GetComponents().Num())
	{
		return A.IsOverlappingActor(&B);
	}
	else
	{
		return B.IsOverlappingActor(&A);
	}
}


void UPrimitiveComponent::BeginComponentOverlap(const FOverlapInfo& OtherOverlap, bool bDoNotifies)
{
	SCOPE_CYCLE_COUNTER(STAT_BeginComponentOverlap);

	// If pending kill, we should not generate any new overlaps
	if (!IsValid(this))
	{
		return;
	}

	const bool bComponentsAlreadyTouching = (IndexOfOverlapFast(OverlappingComponents, OtherOverlap) != INDEX_NONE);
	if (!bComponentsAlreadyTouching)
	{
		UPrimitiveComponent* OtherComp = OtherOverlap.OverlapInfo.Component.Get();
		if (CanComponentsGenerateOverlap(this, OtherComp))
		{
			//UE_LOG(LogActor, Log, TEXT("BEGIN OVERLAP! Self=%s SelfComp=%s, Other=%s, OtherComp=%s"), *GetNameSafe(this), *GetNameSafe(MyComp), *GetNameSafe(OtherComp->GetOwner()), *GetNameSafe(OtherComp));
			GlobalOverlapEventsCounter++;			
			AActor* const OtherActor = OtherComp->GetOwner();
			AActor* const MyActor = GetOwner();

			const bool bSameActor = (MyActor == OtherActor);
			const bool bNotifyActorTouch = bDoNotifies && !bSameActor && !AreActorsOverlapping(*MyActor, *OtherActor);

			// Perform reflexive touch.
			OverlappingComponents.Add(OtherOverlap);												// already verified uniqueness above
			AddUniqueOverlapFast(OtherComp->OverlappingComponents, FOverlapInfo(this, INDEX_NONE));	// uniqueness unverified, so addunique
			
			const UWorld* World = GetWorld();
			const bool bLevelStreamingOverlap = (bDoNotifies && MyActor->bGenerateOverlapEventsDuringLevelStreaming && MyActor->IsActorBeginningPlayFromLevelStreaming());
			if (bDoNotifies && ((World && World->HasBegunPlay()) || bLevelStreamingOverlap))
			{
				// first execute component delegates
				if (IsValid(this))
				{
					OnComponentBeginOverlap.Broadcast(this, OtherActor, OtherComp, OtherOverlap.GetBodyIndex(), OtherOverlap.bFromSweep, OtherOverlap.OverlapInfo);
				}

				if (IsValid(OtherComp))
				{
					// Reverse normals for other component. When it's a sweep, we are the one that moved.
					OtherComp->OnComponentBeginOverlap.Broadcast(OtherComp, MyActor, this, INDEX_NONE, OtherOverlap.bFromSweep, OtherOverlap.bFromSweep ? FHitResult::GetReversedHit(OtherOverlap.OverlapInfo) : OtherOverlap.OverlapInfo);
				}

				// then execute actor notification if this is a new actor touch
				if (bNotifyActorTouch)
				{
					// First actor virtuals
					if (IsActorValidToNotify(MyActor))
					{
						MyActor->NotifyActorBeginOverlap(OtherActor);
					}

					if (IsActorValidToNotify(OtherActor))
					{
						OtherActor->NotifyActorBeginOverlap(MyActor);
					}

					// Then level-script delegates
					if (IsActorValidToNotify(MyActor))
					{
						MyActor->OnActorBeginOverlap.Broadcast(MyActor, OtherActor);
					}

					if (IsActorValidToNotify(OtherActor))
					{
						OtherActor->OnActorBeginOverlap.Broadcast(OtherActor, MyActor);
					}
				}
			}
		}
	}
}


void UPrimitiveComponent::EndComponentOverlap(const FOverlapInfo& OtherOverlap, bool bDoNotifies, bool bSkipNotifySelf)
{
	SCOPE_CYCLE_COUNTER(STAT_EndComponentOverlap);

	UPrimitiveComponent* OtherComp = OtherOverlap.OverlapInfo.Component.Get();
	if (OtherComp == nullptr)
	{
		return;
	}

	const int32 OtherOverlapIdx = IndexOfOverlapFast(OtherComp->OverlappingComponents, FOverlapInfo(this, INDEX_NONE));
	if (OtherOverlapIdx != INDEX_NONE)
	{
		OtherComp->OverlappingComponents.RemoveAtSwap(OtherOverlapIdx, 1, EAllowShrinking::No);
	}

	const int32 OverlapIdx = IndexOfOverlapFast(OverlappingComponents, OtherOverlap);
	if (OverlapIdx != INDEX_NONE)
	{
		//UE_LOG(LogActor, Log, TEXT("END OVERLAP! Self=%s SelfComp=%s, Other=%s, OtherComp=%s"), *GetNameSafe(this), *GetNameSafe(MyComp), *GetNameSafe(OtherActor), *GetNameSafe(OtherComp));
		GlobalOverlapEventsCounter++;
		OverlappingComponents.RemoveAtSwap(OverlapIdx, 1, EAllowShrinking::No);

		AActor* const MyActor = GetOwner();
		const UWorld* World = GetWorld();
		const bool bLevelStreamingOverlap = (bDoNotifies && MyActor && MyActor->bGenerateOverlapEventsDuringLevelStreaming && MyActor->IsActorBeginningPlayFromLevelStreaming());
		if (bDoNotifies && ((World && World->HasBegunPlay()) || bLevelStreamingOverlap))
		{
			AActor* const OtherActor = OtherComp->GetOwner();
			if (OtherActor)
			{
				if (!bSkipNotifySelf && IsPrimCompValidAndAlive(this))
				{
					OnComponentEndOverlap.Broadcast(this, OtherActor, OtherComp, OtherOverlap.GetBodyIndex());
				}

				if (IsPrimCompValidAndAlive(OtherComp))
				{
					OtherComp->OnComponentEndOverlap.Broadcast(OtherComp, MyActor, this, INDEX_NONE);
				}
	
				// if this was the last touch on the other actor by this actor, notify that we've untouched the actor as well
				const bool bSameActor = (MyActor == OtherActor);
				if (MyActor && !bSameActor && !AreActorsOverlapping(*MyActor, *OtherActor))
				{			
					if (IsActorValidToNotify(MyActor))
					{
						MyActor->NotifyActorEndOverlap(OtherActor);
						MyActor->OnActorEndOverlap.Broadcast(MyActor, OtherActor);
					}

					if (IsActorValidToNotify(OtherActor))
					{
						OtherActor->NotifyActorEndOverlap(MyActor);
						OtherActor->OnActorEndOverlap.Broadcast(OtherActor, MyActor);
					}
				}
			}
		}
	}
}

void UPrimitiveComponent::GetOverlappingActors(TArray<AActor*>& OutOverlappingActors, TSubclassOf<AActor> ClassFilter) const
{
	if (OverlappingComponents.Num() <= 12)
	{
		// TArray with fewer elements is faster than using a set (and having to allocate it).
		OutOverlappingActors.Reset(OverlappingComponents.Num());
		for (const FOverlapInfo& OtherOverlap : OverlappingComponents)
		{
			if (UPrimitiveComponent* OtherComponent = OtherOverlap.OverlapInfo.Component.Get())
			{
				AActor* OtherActor = OtherComponent->GetOwner();
				if (OtherActor && ((*ClassFilter) == nullptr || OtherActor->IsA(ClassFilter)))
				{
					OutOverlappingActors.AddUnique(OtherActor);
				}
			}
		}
	}
	else
	{
		// Fill set (unique)
		TSet<AActor*> OverlapSet;
		GetOverlappingActors(OverlapSet, ClassFilter);

		// Copy to array
		OutOverlappingActors.Reset(OverlapSet.Num());
		for (AActor* OverlappingActor : OverlapSet)
		{
			OutOverlappingActors.Add(OverlappingActor);
		}
	}
}

void UPrimitiveComponent::GetOverlappingActors(TSet<AActor*>& OutOverlappingActors, TSubclassOf<AActor> ClassFilter) const
{
	OutOverlappingActors.Reset();
	OutOverlappingActors.Reserve(OverlappingComponents.Num());

	for (const FOverlapInfo& OtherOverlap : OverlappingComponents)
	{
		if (UPrimitiveComponent* OtherComponent = OtherOverlap.OverlapInfo.Component.Get())
		{
			AActor* OtherActor = OtherComponent->GetOwner();
			if (OtherActor && ((*ClassFilter) == nullptr || OtherActor->IsA(ClassFilter)))
			{
				OutOverlappingActors.Add(OtherActor);
			}
		}
	}
}

void UPrimitiveComponent::GetOverlappingComponents(TArray<UPrimitiveComponent*>& OutOverlappingComponents) const
{
	if (OverlappingComponents.Num() <= 12)
	{
		// TArray with fewer elements is faster than using a set (and having to allocate it).
		OutOverlappingComponents.Reset(OverlappingComponents.Num());
		for (const FOverlapInfo& OtherOverlap : OverlappingComponents)
		{
			UPrimitiveComponent* const OtherComp = OtherOverlap.OverlapInfo.Component.Get();
			if (OtherComp)
			{
				OutOverlappingComponents.AddUnique(OtherComp);
			}
		}
	}
	else
	{
		// Fill set (unique)
		TSet<UPrimitiveComponent*> OverlapSet;
		GetOverlappingComponents(OverlapSet);
		
		// Copy to array
		OutOverlappingComponents.Reset(OverlapSet.Num());
		for (UPrimitiveComponent* OtherOverlap : OverlapSet)
		{
			OutOverlappingComponents.Add(OtherOverlap);
		}
	}
}

void UPrimitiveComponent::GetOverlappingComponents(TSet<UPrimitiveComponent*>& OutOverlappingComponents) const
{
	OutOverlappingComponents.Reset();
	OutOverlappingComponents.Reserve(OverlappingComponents.Num());

	for (const FOverlapInfo& OtherOverlap : OverlappingComponents)
	{
		UPrimitiveComponent* const OtherComp = OtherOverlap.OverlapInfo.Component.Get();
		if (OtherComp)
		{
			OutOverlappingComponents.Add(OtherComp);
		}
	}
}

bool UPrimitiveComponent::AreAllCollideableDescendantsRelative(bool bAllowCachedValue) const
{
	UPrimitiveComponent* MutableThis = const_cast<UPrimitiveComponent*>(this);
	if (GetAttachChildren().Num() > 0)
	{
		UWorld* MyWorld = GetWorld();
		check(MyWorld);

		// Throttle this test when it has been false in the past, since it rarely changes afterwards.
		if (bAllowCachedValue && !bCachedAllCollideableDescendantsRelative && MyWorld->TimeSince(LastCheckedAllCollideableDescendantsTime) < 1.f)
		{
			return false;
		}

		// Check all descendant PrimitiveComponents
		TInlineComponentArray<USceneComponent*> ComponentStack;
		const bool bForceGatherOverlaps = !ShouldCheckOverlapFlagToQueueOverlaps(*this);

		ComponentStack.Append(GetAttachChildren());
		while (ComponentStack.Num() > 0)
		{
			USceneComponent* const CurrentComp = ComponentStack.Pop(EAllowShrinking::No);
			if (CurrentComp)
			{
				// Is the component not using relative position?
				if (CurrentComp->IsUsingAbsoluteLocation() || CurrentComp->IsUsingAbsoluteRotation())
				{
					// Can we possibly collide with the component?
					UPrimitiveComponent* const CurrentPrimitive = Cast<UPrimitiveComponent>(CurrentComp);
					if (CurrentPrimitive && (CurrentPrimitive->GetGenerateOverlapEvents() || bForceGatherOverlaps) && CurrentPrimitive->IsQueryCollisionEnabled() && CurrentPrimitive->GetCollisionResponseToChannel(GetCollisionObjectType()) != ECR_Ignore)
					{
						MutableThis->bCachedAllCollideableDescendantsRelative = false;
						MutableThis->LastCheckedAllCollideableDescendantsTime = MyWorld->GetTimeSeconds();
						return false;
					}
				}

				ComponentStack.Append(CurrentComp->GetAttachChildren());
			}
		}
	}

	MutableThis->bCachedAllCollideableDescendantsRelative = true;
	return true;
}

void UPrimitiveComponent::BeginPlay()
{
	Super::BeginPlay();
	if(FBodyInstance* BI = GetBodyInstance(NAME_None, /*bGetWelded=*/ false))
	{
		if (BI->bSimulatePhysics && !BI->WeldParent)
		{
			//Since the object is physically simulated it can't be attached
			const bool bSavedDisableDetachmentUpdateOverlaps = bDisableDetachmentUpdateOverlaps;
			bDisableDetachmentUpdateOverlaps = true;
			DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
			bDisableDetachmentUpdateOverlaps = bSavedDisableDetachmentUpdateOverlaps;
		}
	}
	
}

void UPrimitiveComponent::IgnoreActorWhenMoving(AActor* Actor, bool bShouldIgnore)
{
	// Clean up stale references
	MoveIgnoreActors.RemoveSwap(nullptr);

	// Add/Remove the actor from the list
	if (Actor)
	{
		if (bShouldIgnore)
		{
			MoveIgnoreActors.AddUnique(Actor);
		}
		else
		{
			MoveIgnoreActors.RemoveSingleSwap(Actor);
		}
	}
}

TArray<AActor*> UPrimitiveComponent::CopyArrayOfMoveIgnoreActors()
{
	for (int32 Index = MoveIgnoreActors.Num() - 1; Index >=0; --Index)
	{
		const AActor* const MoveIgnoreActor = MoveIgnoreActors[Index];
		if (!IsValid(MoveIgnoreActor))
		{
			MoveIgnoreActors.RemoveAtSwap(Index,1,EAllowShrinking::No);
		}
	}
	return MoveIgnoreActors;
}

void UPrimitiveComponent::ClearMoveIgnoreActors()
{
	MoveIgnoreActors.Empty();
}

void UPrimitiveComponent::IgnoreComponentWhenMoving(UPrimitiveComponent* Component, bool bShouldIgnore)
{
	// Clean up stale references
	MoveIgnoreComponents.RemoveSwap(nullptr);

	// Add/Remove the component from the list
	if (Component)
	{
		if (bShouldIgnore)
		{
			MoveIgnoreComponents.AddUnique(Component);
		}
		else
		{
			MoveIgnoreComponents.RemoveSingleSwap(Component);
		}
	}
}

TArray<UPrimitiveComponent*> UPrimitiveComponent::CopyArrayOfMoveIgnoreComponents()
{
	for (int32 Index = MoveIgnoreComponents.Num() - 1; Index >= 0; --Index)
	{
		const UPrimitiveComponent* const MoveIgnoreComponent = MoveIgnoreComponents[Index];
		if (!IsValid(MoveIgnoreComponent))
		{
			MoveIgnoreComponents.RemoveAtSwap(Index, 1, EAllowShrinking::No);
		}
	}
	return MoveIgnoreComponents;
}

bool UPrimitiveComponent::UpdateOverlapsImpl(const TOverlapArrayView* NewPendingOverlaps, bool bDoNotifies, const TOverlapArrayView* OverlapsAtEndLocation)
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateOverlaps); 
	SCOPE_CYCLE_UOBJECT(ComponentScope, this);

	// if we haven't begun play, we're still setting things up (e.g. we might be inside one of the construction scripts)
	// so we don't want to generate overlaps yet. There is no need to update children yet either, they will update once we are allowed to as well.
	const AActor* const MyActor = GetOwner();
	if (MyActor && !MyActor->HasActorBegunPlay() && !MyActor->IsActorBeginningPlay())
	{
		return false;
	}

	bool bCanSkipUpdateOverlaps = true;

	// first, dispatch any pending overlaps
	if (GetGenerateOverlapEvents() && IsQueryCollisionEnabled())	//TODO: should modifying query collision remove from mayoverlapevents?
	{
		bCanSkipUpdateOverlaps = false;
		if (MyActor)
		{
			const FTransform PrevTransform = GetComponentTransform();
			// If we are the root component we ignore child components. Those children will update their overlaps when we descend into the child tree.
			// This aids an optimization in MoveComponent.
			const bool bIgnoreChildren = (MyActor->GetRootComponent() == this);

			if (NewPendingOverlaps)
			{
				// Note: BeginComponentOverlap() only triggers overlaps where GetGenerateOverlapEvents() is true on both components.
				const int32 NumNewPendingOverlaps = NewPendingOverlaps->Num();
				for (int32 Idx=0; Idx < NumNewPendingOverlaps; ++Idx)
				{
					BeginComponentOverlap( (*NewPendingOverlaps)[Idx], bDoNotifies );
				}
			}

			// now generate full list of new touches, so we can compare to existing list and determine what changed
			TInlineOverlapInfoArray OverlapMultiResult;
			TInlineOverlapPointerArray NewOverlappingComponentPtrs;

			// If pending kill, we should not generate any new overlaps. Also not if overlaps were just disabled during BeginComponentOverlap.
			if (IsValid(this) && GetGenerateOverlapEvents())
			{
				// Might be able to avoid testing for new overlaps at the end location.
				if (OverlapsAtEndLocation != nullptr && PrimitiveComponentCVars::bAllowCachedOverlapsCVar && PrevTransform.Equals(GetComponentTransform()))
				{
					UE_LOG(LogPrimitiveComponent, VeryVerbose, TEXT("%s->%s Skipping overlap test!"), *GetNameSafe(GetOwner()), *GetName());
					const bool bCheckForInvalid = (NewPendingOverlaps && NewPendingOverlaps->Num() > 0);
					if (bCheckForInvalid)
					{
						// BeginComponentOverlap may have disabled what we thought were valid overlaps at the end (collision response or overlap flags could change).
						GetPointersToArrayDataByPredicate(NewOverlappingComponentPtrs, *OverlapsAtEndLocation, FPredicateFilterCanOverlap(*this));
					}
					else
					{
						GetPointersToArrayData(NewOverlappingComponentPtrs, *OverlapsAtEndLocation);
					}
				}
				else
				{
					SCOPE_CYCLE_COUNTER(STAT_PerformOverlapQuery);
					UE_LOG(LogPrimitiveComponent, VeryVerbose, TEXT("%s->%s Performing overlaps!"), *GetNameSafe(GetOwner()), *GetName());
					UWorld* const MyWorld = GetWorld();
					TArray<FOverlapResult> Overlaps;
					// note this will optionally include overlaps with components in the same actor (depending on bIgnoreChildren). 
					FComponentQueryParams Params(SCENE_QUERY_STAT(UpdateOverlaps), bIgnoreChildren ? MyActor : nullptr);
					Params.bIgnoreBlocks = true;	//We don't care about blockers since we only route overlap events to real overlaps
					FCollisionResponseParams ResponseParam;
					InitSweepCollisionParams(Params, ResponseParam);
					ComponentOverlapMulti(Overlaps, MyWorld, GetComponentLocation(), GetComponentQuat(), GetCollisionObjectType(), Params);

					for (int32 ResultIdx=0; ResultIdx < Overlaps.Num(); ResultIdx++)
					{
						const FOverlapResult& Result = Overlaps[ResultIdx];

						UPrimitiveComponent* const HitComp = Result.Component.Get();
						if (HitComp && (HitComp != this) && HitComp->GetGenerateOverlapEvents())
						{
							const bool bCheckOverlapFlags = false; // Already checked above
							if (!ShouldIgnoreOverlapResult(MyWorld, MyActor, *this, Result.OverlapObjectHandle, *HitComp, bCheckOverlapFlags))
							{
								OverlapMultiResult.Emplace(HitComp, Result.ItemIndex);		// don't need to add unique unless the overlap check can return dupes
							}
						}
					}

					// Fill pointers to overlap results. We ensure below that OverlapMultiResult stays in scope so these pointers remain valid.
					GetPointersToArrayData(NewOverlappingComponentPtrs, OverlapMultiResult);
				}
			}

			// If we have any overlaps from BeginComponentOverlap() (from now or in the past), see if anything has changed by filtering NewOverlappingComponents
			if (OverlappingComponents.Num() > 0)
			{
				TInlineOverlapPointerArray OldOverlappingComponentPtrs;
				if (bIgnoreChildren)
				{
					GetPointersToArrayDataByPredicate(OldOverlappingComponentPtrs, OverlappingComponents, FPredicateOverlapHasDifferentActor(*MyActor));
				}
				else
				{
					GetPointersToArrayData(OldOverlappingComponentPtrs, OverlappingComponents);
				}

				// Now we want to compare the old and new overlap lists to determine 
				// what overlaps are in old and not in new (need end overlap notifies), and 
				// what overlaps are in new and not in old (need begin overlap notifies).
				// We do this by removing common entries from both lists, since overlapping status has not changed for them.
				// What is left over will be what has changed.
				for (int32 CompIdx=0; CompIdx < OldOverlappingComponentPtrs.Num() && NewOverlappingComponentPtrs.Num() > 0; ++CompIdx)
				{
					// RemoveAtSwap is ok, since it is not necessary to maintain order

					const FOverlapInfo* SearchItem = OldOverlappingComponentPtrs[CompIdx];
					const int32 NewElementIdx = IndexOfOverlapFast(NewOverlappingComponentPtrs, SearchItem);
					if (NewElementIdx != INDEX_NONE)
					{
						NewOverlappingComponentPtrs.RemoveAtSwap(NewElementIdx, 1, EAllowShrinking::No);
						OldOverlappingComponentPtrs.RemoveAtSwap(CompIdx, 1, EAllowShrinking::No);
						--CompIdx;
					}
				}

				const int32 NumOldOverlaps = OldOverlappingComponentPtrs.Num();
				if (NumOldOverlaps > 0)
				{
					// Now we have to make a copy of the overlaps because we can't keep pointers to them, that list is about to be manipulated in EndComponentOverlap().
					TInlineOverlapInfoArray OldOverlappingComponents;
					OldOverlappingComponents.SetNumUninitialized(NumOldOverlaps);
					for (int32 i=0; i < NumOldOverlaps; i++)
					{
						OldOverlappingComponents[i] = *(OldOverlappingComponentPtrs[i]);
					}

					// OldOverlappingComponents now contains only previous overlaps that are confirmed to no longer be valid.
					for (const FOverlapInfo& OtherOverlap : OldOverlappingComponents)
					{
						if (OtherOverlap.OverlapInfo.Component.IsValid())
						{
							EndComponentOverlap(OtherOverlap, bDoNotifies, false);
						}
						else
						{
							// Remove stale item. Reclaim memory only if it's getting large, to try to avoid churn but avoid bloating component's memory usage.
							const bool bAllowShrinking = (OverlappingComponents.Max() >= 24);
							const int32 StaleElementIndex = IndexOfOverlapFast(OverlappingComponents, OtherOverlap);
							if (StaleElementIndex != INDEX_NONE)
							{
								OverlappingComponents.RemoveAtSwap(StaleElementIndex, 1, bAllowShrinking ? EAllowShrinking::Yes : EAllowShrinking::No);
							}
						}
					}
				}
			}

			// Ensure these arrays are still in scope, because we kept pointers to them in NewOverlappingComponentPtrs.
			static_assert(sizeof(OverlapMultiResult) != 0, "Variable must be in this scope");
			static_assert(sizeof(*OverlapsAtEndLocation) != 0, "Variable must be in this scope");

			// NewOverlappingComponents now contains only new overlaps that didn't exist previously.
			for (const FOverlapInfo* NewOverlap : NewOverlappingComponentPtrs)
			{
				BeginComponentOverlap(*NewOverlap, bDoNotifies);
			}
		}
	}
	else
	{
		// GetGenerateOverlapEvents() is false or collision is disabled
		// End all overlaps that exist, in case GetGenerateOverlapEvents() was true last tick (i.e. was just turned off)
		if (OverlappingComponents.Num() > 0)
		{
			const bool bSkipNotifySelf = false;
			ClearComponentOverlaps(bDoNotifies, bSkipNotifySelf);
		}
	}

	// now update any children down the chain.
	// since on overlap events could manipulate the child array we need to take a copy
	// of it to avoid missing any children if one is removed from the middle
	TInlineComponentArray<USceneComponent*> AttachedChildren;
	AttachedChildren.Append(GetAttachChildren());

	for (USceneComponent* const ChildComp : AttachedChildren)
	{
		if (ChildComp)
		{
			// Do not pass on OverlapsAtEndLocation, it only applied to this component.
			bCanSkipUpdateOverlaps &= ChildComp->UpdateOverlaps(nullptr, bDoNotifies, nullptr);
		}
	}

	// Update physics volume using most current overlaps
	if (GetShouldUpdatePhysicsVolume())
	{
		UpdatePhysicsVolume(bDoNotifies);
		bCanSkipUpdateOverlaps = false;
	}

	return bCanSkipUpdateOverlaps;
}

void UPrimitiveComponent::SetGenerateOverlapEvents(bool bInGenerateOverlapEvents)
{
	if (bGenerateOverlapEvents != bInGenerateOverlapEvents)
	{
		bGenerateOverlapEvents = bInGenerateOverlapEvents;

		OnGenerateOverlapEventsChanged();
	}
}

void UPrimitiveComponent::OnGenerateOverlapEventsChanged()
{
	ClearSkipUpdateOverlaps();
}

void UPrimitiveComponent::SetLightingChannels(bool bChannel0, bool bChannel1, bool bChannel2)
{
	if (bChannel0 != LightingChannels.bChannel0 ||
		bChannel1 != LightingChannels.bChannel1 ||
		bChannel2 != LightingChannels.bChannel2)
	{
		LightingChannels.bChannel0 = bChannel0;
		LightingChannels.bChannel1 = bChannel1;
		LightingChannels.bChannel2 = bChannel2;
		if (SceneProxy)
		{
			SceneProxy->SetLightingChannels_GameThread(LightingChannels);
		}
		MarkRenderStateDirty();
	}
}

void UPrimitiveComponent::InvalidateLumenSurfaceCache()
{
	GetScene()->InvalidateLumenSurfaceCache_GameThread(this);
}

void UPrimitiveComponent::ClearComponentOverlaps(bool bDoNotifies, bool bSkipNotifySelf)
{
	if (OverlappingComponents.Num() > 0)
	{
		// Make a copy since EndComponentOverlap will remove items from OverlappingComponents.
		const TInlineOverlapInfoArray OverlapsCopy(OverlappingComponents);
		for (const FOverlapInfo& OtherOverlap : OverlapsCopy)
		{
			EndComponentOverlap(OtherOverlap, bDoNotifies, bSkipNotifySelf);
		}
	}
}

bool UPrimitiveComponent::ComponentOverlapMultiImpl(TArray<struct FOverlapResult>& OutOverlaps, const UWorld* World, const FVector& Pos, const FQuat& Quat, ECollisionChannel TestChannel, const struct FComponentQueryParams& Params, const struct FCollisionObjectQueryParams& ObjectQueryParams) const
{
	FComponentQueryParams ParamsWithSelf = Params;
	ParamsWithSelf.AddIgnoredComponent_LikelyDuplicatedRoot(this);
	OutOverlaps.Reset();
	return BodyInstance.OverlapMulti(OutOverlaps, World, /*pWorldToComponent=*/ nullptr, Pos, Quat, TestChannel, ParamsWithSelf, FCollisionResponseParams(GetCollisionResponseToChannels()), ObjectQueryParams);
}

const UPrimitiveComponent* UPrimitiveComponent::GetLightingAttachmentRoot() const
{
	// Exclude  from light attachment group whatever the parent says
	if (bExcludeFromLightAttachmentGroup)
	{
		return nullptr;
	}

	const USceneComponent* CurrentHead = this;

	while (CurrentHead)
	{
		// If the component has been marked to light itself and child attachments as a group, return it as root
		if (const UPrimitiveComponent* CurrentHeadPrim = Cast<UPrimitiveComponent>(CurrentHead))
		{
			if (CurrentHeadPrim->bLightAttachmentsAsGroup)
			{
				return CurrentHeadPrim;
			}
		}

		CurrentHead = CurrentHead->GetAttachParent();
	}

	return nullptr;
}

#if WITH_EDITOR
void UPrimitiveComponent::UpdateBounds()
{
	// Save old bounds
	FBoxSphereBounds OriginalBounds = Bounds;

	Super::UpdateBounds();

	if (IsRegistered() && (GetWorld() != nullptr) && !GetWorld()->IsGameWorld() && (!OriginalBounds.Origin.Equals(Bounds.Origin) || !OriginalBounds.BoxExtent.Equals(Bounds.BoxExtent)) )
	{
		if (!bIgnoreStreamingManagerUpdate && !bHandledByStreamingManagerAsDynamic && bAttachedToStreamingManagerAsStatic)
		{
			FStreamingManagerCollection* Collection = IStreamingManager::Get_Concurrent();
			if (Collection)
			{
				Collection->NotifyPrimitiveUpdated_Concurrent(this);
			}
		}
	}
}
#endif

void UPrimitiveComponent::UpdateOcclusionBoundsSlack(float NewSlack)
{
	if (SceneProxy && GetWorld() && GetWorld()->Scene && NewSlack != OcclusionBoundsSlack)
	{
		GetWorld()->Scene->UpdatePrimitiveOcclusionBoundsSlack(this, NewSlack);
		OcclusionBoundsSlack = NewSlack;
	}
}

void UPrimitiveComponent::UpdatePhysicsVolume( bool bTriggerNotifiers )
{
	if (GetShouldUpdatePhysicsVolume() && IsValid(this))
	{
		SCOPE_CYCLE_COUNTER(STAT_UpdatePhysicsVolume);
		if (UWorld* MyWorld = GetWorld())
		{
			if (MyWorld->GetNonDefaultPhysicsVolumeCount() == 0)
			{
				SetPhysicsVolume(MyWorld->GetDefaultPhysicsVolume(), bTriggerNotifiers);
			}
			else if (GetGenerateOverlapEvents() && IsQueryCollisionEnabled())
			{
				APhysicsVolume* BestVolume = MyWorld->GetDefaultPhysicsVolume();
				int32 BestPriority = BestVolume->Priority;

				for (auto CompIt = OverlappingComponents.CreateIterator(); CompIt; ++CompIt)
				{
					const FOverlapInfo& Overlap = *CompIt;
					UPrimitiveComponent* OtherComponent = Overlap.OverlapInfo.Component.Get();
					if (OtherComponent && OtherComponent->GetGenerateOverlapEvents())
					{
						APhysicsVolume* V = Cast<APhysicsVolume>(OtherComponent->GetOwner());
						if (V && V->Priority > BestPriority)
						{
							if (V->IsOverlapInVolume(*this))
							{
								BestPriority = V->Priority;
								BestVolume = V;
							}
						}
					}
				}

				SetPhysicsVolume(BestVolume, bTriggerNotifiers);
			}
			else
			{
				Super::UpdatePhysicsVolume(bTriggerNotifiers);
			}
		}
	}
}


void UPrimitiveComponent::DispatchMouseOverEvents(UPrimitiveComponent* CurrentComponent, UPrimitiveComponent* NewComponent)
{
	if (NewComponent)
	{
		AActor* NewOwner = NewComponent->GetOwner();

		bool bBroadcastComponentBegin = true;
		bool bBroadcastActorBegin = true;
		if (CurrentComponent)
		{
			AActor* CurrentOwner = CurrentComponent->GetOwner();

			if (NewComponent == CurrentComponent)
			{
				bBroadcastComponentBegin = false;
			}
			else
			{
				bBroadcastActorBegin = (NewOwner != CurrentOwner);

				if (IsValid(CurrentComponent))
				{
					CurrentComponent->OnEndCursorOver.Broadcast(CurrentComponent);
				}
				if (bBroadcastActorBegin && IsActorValidToNotify(CurrentOwner))
				{
					CurrentOwner->NotifyActorEndCursorOver();
					if (IsActorValidToNotify(CurrentOwner))
					{
						CurrentOwner->OnEndCursorOver.Broadcast(CurrentOwner);
					}
				}
			}
		}

		if (bBroadcastComponentBegin)
		{
			if (bBroadcastActorBegin && IsActorValidToNotify(NewOwner))
			{
				NewOwner->NotifyActorBeginCursorOver();
				if (IsActorValidToNotify(NewOwner))
				{
					NewOwner->OnBeginCursorOver.Broadcast(NewOwner);
				}
			}
			if (IsValid(NewComponent))
			{
				NewComponent->OnBeginCursorOver.Broadcast(NewComponent);
			}
		}
	}
	else if (CurrentComponent)
	{
		AActor* CurrentOwner = CurrentComponent->GetOwner();

		if (IsValid(CurrentComponent))
		{
			CurrentComponent->OnEndCursorOver.Broadcast(CurrentComponent);
		}

		if (IsActorValidToNotify(CurrentOwner))
		{
			CurrentOwner->NotifyActorEndCursorOver();
			if (IsActorValidToNotify(CurrentOwner))
			{
				CurrentOwner->OnEndCursorOver.Broadcast(CurrentOwner);
			}
		}
	}
}

void UPrimitiveComponent::DispatchTouchOverEvents(ETouchIndex::Type FingerIndex, UPrimitiveComponent* CurrentComponent, UPrimitiveComponent* NewComponent)
{
	if (NewComponent)
	{
		AActor* NewOwner = NewComponent->GetOwner();

		bool bBroadcastComponentBegin = true;
		bool bBroadcastActorBegin = true;
		if (CurrentComponent)
		{
			AActor* CurrentOwner = CurrentComponent->GetOwner();

			if (NewComponent == CurrentComponent)
			{
				bBroadcastComponentBegin = false;
			}
			else
			{
				bBroadcastActorBegin = (NewOwner != CurrentOwner);

				if (IsValid(CurrentComponent))
				{
					CurrentComponent->OnInputTouchLeave.Broadcast(FingerIndex, CurrentComponent);
				}
				if (bBroadcastActorBegin && IsActorValidToNotify(CurrentOwner))
				{
					CurrentOwner->NotifyActorOnInputTouchLeave(FingerIndex);
					if (IsActorValidToNotify(CurrentOwner))
					{
						CurrentOwner->OnInputTouchLeave.Broadcast(FingerIndex, CurrentOwner);
					}
				}
			}
		}

		if (bBroadcastComponentBegin)
		{
			if (bBroadcastActorBegin && IsActorValidToNotify(NewOwner))
			{
				NewOwner->NotifyActorOnInputTouchEnter(FingerIndex);
				if (IsActorValidToNotify(NewOwner))
				{
					NewOwner->OnInputTouchEnter.Broadcast(FingerIndex, NewOwner);
				}
			}
			if (IsValid(NewComponent))
			{
				NewComponent->OnInputTouchEnter.Broadcast(FingerIndex, NewComponent);
			}
		}
	}
	else if (CurrentComponent)
	{
		AActor* CurrentOwner = CurrentComponent->GetOwner();

		if (IsValid(CurrentComponent))
		{
			CurrentComponent->OnInputTouchLeave.Broadcast(FingerIndex, CurrentComponent);
		}

		if (IsActorValidToNotify(CurrentOwner))
		{
			CurrentOwner->NotifyActorOnInputTouchLeave(FingerIndex);
			if (IsActorValidToNotify(CurrentOwner))
			{
				CurrentOwner->OnInputTouchLeave.Broadcast(FingerIndex, CurrentOwner);
			}
		}
	}
}

void UPrimitiveComponent::DispatchOnClicked(FKey ButtonPressed)
{
	if (IsActorValidToNotify(GetOwner()))
	{
		GetOwner()->NotifyActorOnClicked(ButtonPressed);
		if (IsActorValidToNotify(GetOwner()))
		{
			GetOwner()->OnClicked.Broadcast(GetOwner(), ButtonPressed);
		}
	}

	if (IsValid(this))
	{
		OnClicked.Broadcast(this, ButtonPressed);
	}
}

void UPrimitiveComponent::DispatchOnReleased(FKey ButtonReleased)
{
	if (IsActorValidToNotify(GetOwner()))
	{
		GetOwner()->NotifyActorOnReleased(ButtonReleased);
		if (IsActorValidToNotify(GetOwner()))
		{
			GetOwner()->OnReleased.Broadcast(GetOwner(), ButtonReleased);
		}
	}

	if (IsValid(this))
	{
		OnReleased.Broadcast(this, ButtonReleased);
	}
}

void UPrimitiveComponent::DispatchOnInputTouchBegin(const ETouchIndex::Type FingerIndex)
{
	if (IsActorValidToNotify(GetOwner()))
	{
		GetOwner()->NotifyActorOnInputTouchBegin(FingerIndex);
		if (IsActorValidToNotify(GetOwner()))
		{
			GetOwner()->OnInputTouchBegin.Broadcast(FingerIndex, GetOwner());
		}
	}

	if (IsValid(this))
	{
		OnInputTouchBegin.Broadcast(FingerIndex, this);
	}
}

void UPrimitiveComponent::DispatchOnInputTouchEnd(const ETouchIndex::Type FingerIndex)
{
	if (IsActorValidToNotify(GetOwner()))
	{
		GetOwner()->NotifyActorOnInputTouchEnd(FingerIndex);
		if (IsActorValidToNotify(GetOwner()))
		{
			GetOwner()->OnInputTouchEnd.Broadcast(FingerIndex, GetOwner());
		}
	}

	if (IsValid(this))
	{
		OnInputTouchEnd.Broadcast(FingerIndex, this);
	}
}

void UPrimitiveComponent::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	Super::GetResourceSizeEx(CumulativeResourceSize);

	if (BodyInstance.IsValidBodyInstance())
	{
		BodyInstance.GetBodyInstanceResourceSizeEx(CumulativeResourceSize);
	}
	if (SceneProxy)
	{
		CumulativeResourceSize.AddDedicatedSystemMemoryBytes(SceneProxy->GetMemoryFootprint());
	}

}

void UPrimitiveComponent::SetRenderCustomDepth(bool bValue)
{
	if( bRenderCustomDepth != bValue )
	{
		bRenderCustomDepth = bValue;
		MarkRenderStateDirty();
	}
}

void UPrimitiveComponent::SetCustomDepthStencilValue(int32 Value)
{
	// Clamping to currently usable stencil range (as specified in property UI and tooltips)
	int32 ClampedValue = FMath::Clamp(Value, 0, 255);

	if (CustomDepthStencilValue != ClampedValue)
	{
		CustomDepthStencilValue = ClampedValue;
		if (SceneProxy)
		{
			SceneProxy->SetCustomDepthStencilValue_GameThread(CustomDepthStencilValue);
		}
	}
}

void UPrimitiveComponent::SetCustomDepthStencilWriteMask(ERendererStencilMask WriteMask)
{
	if (CustomDepthStencilWriteMask != WriteMask)
	{
		CustomDepthStencilWriteMask = WriteMask;
		MarkRenderStateDirty();
	}
}

void UPrimitiveComponent::SetRenderInMainPass(bool bValue)
{
	if (bRenderInMainPass != bValue)
	{
		bRenderInMainPass = bValue;
		MarkRenderStateDirty();
	}
}

void UPrimitiveComponent::SetRenderInDepthPass(bool bValue)
{
	if (bRenderInDepthPass != bValue)
	{
		bRenderInDepthPass = bValue;
		MarkRenderStateDirty();
	}
}

void UPrimitiveComponent::SetVisibleInSceneCaptureOnly(bool bValue)
{
	if (bVisibleInSceneCaptureOnly != bValue)
	{
		bVisibleInSceneCaptureOnly = bValue;
		MarkRenderStateDirty();
	}
}

void UPrimitiveComponent::SetHiddenInSceneCapture(bool bValue)
{
	if (bHiddenInSceneCapture != bValue)
	{
		bHiddenInSceneCapture = bValue;
		MarkRenderStateDirty();
	}
}

void UPrimitiveComponent::SetLODParentPrimitive(UPrimitiveComponent * InLODParentPrimitive)
{
	if (LODParentPrimitive == InLODParentPrimitive)
	{
		return;
	}


#if WITH_EDITOR	
	const ALODActor* ParentLODActor = [&]() -> const ALODActor*
	{
		if (InLODParentPrimitive)
		{
			if (ALODActor* ParentActor = Cast<ALODActor>(InLODParentPrimitive->GetOwner()))
			{
				return ParentActor;
			}
		}

		return nullptr;
	}();

	if (!GIsEditor || !InLODParentPrimitive || ShouldGenerateAutoLOD(ParentLODActor ? ParentLODActor->LODLevel - 1 : INDEX_NONE))
#endif
	{
		LODParentPrimitive = InLODParentPrimitive;
		MarkRenderStateDirty();
	}
}

UPrimitiveComponent* UPrimitiveComponent::GetLODParentPrimitive() const
{
	return LODParentPrimitive;
}

#if WITH_EDITOR
const int32 UPrimitiveComponent::GetNumUncachedStaticLightingInteractions() const
{
	int32 NumUncachedStaticLighting = 0;

	NumUncachedStaticLighting += Super::GetNumUncachedStaticLightingInteractions();
	if (SceneProxy)
	{
		NumUncachedStaticLighting += SceneProxy->GetNumUncachedStaticLightingInteractions();
	}
	return NumUncachedStaticLighting;
}
#endif


bool UPrimitiveComponent::CanCharacterStepUp(APawn* Pawn) const
{
	if ( CanCharacterStepUpOn != ECB_Owner )
	{
		return CanCharacterStepUpOn == ECB_Yes;
	}
	else
	{	
		const AActor* Owner = GetOwner();
		return Owner && Owner->CanBeBaseForCharacter(Pawn);
	}
}

bool UPrimitiveComponent::CanEditSimulatePhysics()
{
	//Even if there's no collision but there is a body setup, we still let them simulate physics.
	// The object falls through the world - this behavior is debatable but what we decided on for now
	return GetBodySetup() != nullptr;
}

void UPrimitiveComponent::SetCustomNavigableGeometry(const EHasCustomNavigableGeometry::Type InType)
{
	bHasCustomNavigableGeometry = InType;
}
 
int32 UPrimitiveComponent::GetRayTracingGroupId() const
{
	if (RayTracingGroupId == FPrimitiveSceneProxy::InvalidRayTracingGroupId && GetOwner() != nullptr)
	{
		return GetOwner()->GetRayTracingGroupId();
	}
			
	return RayTracingGroupId;
}

bool UPrimitiveComponent::WasRecentlyRendered(float Tolerance /*= 0.2*/) const
{
	if (const UWorld* const World = GetWorld())
	{
		// Adjust tolerance, so visibility is not affected by bad frame rate / hitches.
		const float RenderTimeThreshold = FMath::Max(Tolerance, World->DeltaTimeSeconds + UE_KINDA_SMALL_NUMBER);

		// If the current cached value is less than the tolerance then we don't need to go look at the components
		return World->TimeSince(GetLastRenderTime()) <= RenderTimeThreshold;
	}
	return false;
}

void UPrimitiveComponent::SetLastRenderTime(float InLastRenderTime)
{
	SceneData.LastRenderTime = InLastRenderTime;
	if (AActor* Owner = GetOwner())
	{
		if (InLastRenderTime > Owner->GetLastRenderTime())
		{
			FActorLastRenderTime::Set(Owner, InLastRenderTime);
		}
	}
}

#if MESH_DRAW_COMMAND_STATS
void UPrimitiveComponent::SetMeshDrawCommandStatsCategory(FName StatsCategory)
{
	if (MeshDrawCommandStatsCategory != StatsCategory)
	{
		MeshDrawCommandStatsCategory = StatsCategory;
		MarkRenderStateDirty();
	}
}

FName UPrimitiveComponent::GetMeshDrawCommandStatsCategory() const
{
	// If a stats category isn't set on the component then use the component type.
	return MeshDrawCommandStatsCategory.IsNone() ? GetClass()->GetFName() : MeshDrawCommandStatsCategory;
}
#endif

void UPrimitiveComponent::SetupPrecachePSOParams(FPSOPrecacheParams& Params)
{
	Params.bRenderInMainPass = bRenderInMainPass;
	Params.bRenderInDepthPass = bRenderInDepthPass;
	Params.bStaticLighting = HasStaticLighting();
	Params.bAffectDynamicIndirectLighting = bAffectDynamicIndirectLighting;
	Params.bCastShadow = CastShadow;
	// Custom depth can be toggled at runtime with PSO precache call so assume it might be needed when depth pass is needed
	// Ideally precache those with lower priority and don't wait on these (UE-174426)
	Params.bRenderCustomDepth = bRenderCustomDepth;
	Params.bCastShadowAsTwoSided = bCastShadowAsTwoSided;
	Params.SetMobility(Mobility);	
	Params.SetStencilWriteMask(FRendererStencilMaskEvaluation::ToStencilMask(CustomDepthStencilWriteMask));

	TArray<UMaterialInterface*> UsedMaterials;
	GetUsedMaterials(UsedMaterials);
	for (const UMaterialInterface* MaterialInterface : UsedMaterials)
	{
		if (MaterialInterface)
		{
			if (MaterialInterface->GetRelevance_Concurrent(GMaxRHIFeatureLevel).bUsesWorldPositionOffset)
			{
				Params.bAnyMaterialHasWorldPositionOffset = true;
				break;
			}
		}
	}
}

void UPrimitiveComponent::PrecachePSOs()
{
#if UE_WITH_PSO_PRECACHING
	// Only request PSO precaching if app is rendering and per component PSO precaching is enabled
	// Also only request PSOs from game thread because TStrongObjectPtr is used on the material to make
	// it's not deleted via garbage collection when PSO precaching is still busy. TStrongObjectPtr can only
	// be constructed on the GameThread
	if (!FApp::CanEverRender() || !IsComponentPSOPrecachingEnabled() || !IsInGameThread())
	{
		return;
	}

	// clear the current request data
	MaterialPSOPrecacheRequestIDs.Empty();
	PSOPrecacheCompileEvent = nullptr;
	bPSOPrecacheRequestBoosted = false;

	// Collect the data from the derived classes
	FPSOPrecacheParams PSOPrecacheParams;
	SetupPrecachePSOParams(PSOPrecacheParams);
	FMaterialInterfacePSOPrecacheParamsList PSOPrecacheDataArray;
	CollectPSOPrecacheData(PSOPrecacheParams, PSOPrecacheDataArray);

	FGraphEventArray GraphEvents;
	PrecacheMaterialPSOs(PSOPrecacheDataArray, MaterialPSOPrecacheRequestIDs, GraphEvents);

	RequestRecreateRenderStateWhenPSOPrecacheFinished(GraphEvents);
#endif
}

void UPrimitiveComponent::RequestRecreateRenderStateWhenPSOPrecacheFinished(const FGraphEventArray& PSOPrecacheCompileEvents)
{
#if UE_WITH_PSO_PRECACHING
	// If the proxy creation strategy relies on knowing when the precached PSO has been compiled,
	// schedule a task to mark the render state dirty when all PSOs are compiled so the proxy gets recreated.
	if (UsePSOPrecacheRenderProxyDelay() && GetPSOPrecacheProxyCreationStrategy() != EPSOPrecacheProxyCreationStrategy::AlwaysCreate && !PSOPrecacheCompileEvents.IsEmpty())
	{
		PSOPrecacheCompileEvent = TGraphTask<FMarkActorRenderStateDirtyTask>::CreateTask(&PSOPrecacheCompileEvents).ConstructAndDispatchWhenReady(this);
	}

	bPSOPrecacheCalled = true;
#endif // UE_WITH_PSO_PRECACHING
}

bool UPrimitiveComponent::UsePSOPrecacheRenderProxyDelay() const
{
#if UE_WITH_PSO_PRECACHING
	return true;
#else
	return false;
#endif
}

bool UPrimitiveComponent::IsPSOPrecaching() const
{
#if UE_WITH_PSO_PRECACHING
	return PSOPrecacheCompileEvent && !PSOPrecacheCompileEvent->IsComplete();
#else
	return false;
#endif // UE_WITH_PSO_PRECACHING
}

bool UPrimitiveComponent::ShouldRenderProxyFallbackToDefaultMaterial() const
{
#if UE_WITH_PSO_PRECACHING
	return IsPSOPrecaching() && GetPSOPrecacheProxyCreationStrategy() == EPSOPrecacheProxyCreationStrategy::UseDefaultMaterialUntilPSOPrecached;
#else
	return false;
#endif // UE_WITH_PSO_PRECACHING
}

bool UPrimitiveComponent::CheckPSOPrecachingAndBoostPriority()
{
#if UE_WITH_PSO_PRECACHING
	ensure(!IsComponentPSOPrecachingEnabled() || bPSOPrecacheCalled);

	if (PSOPrecacheCompileEvent && !PSOPrecacheCompileEvent->IsComplete())
	{
		if (!bPSOPrecacheRequestBoosted)
		{
			BoostPSOPriority(MaterialPSOPrecacheRequestIDs);
			bPSOPrecacheRequestBoosted = true;
		}
	}
	else
	{
		PSOPrecacheCompileEvent = nullptr;
	}

	return IsPSOPrecaching();
#else
	return false;
#endif
}

FPrimitiveMaterialPropertyDescriptor UPrimitiveComponent::GetUsedMaterialPropertyDesc(ERHIFeatureLevel::Type FeatureLevel) const
{
	FPrimitiveMaterialPropertyDescriptor Result;
	TArray<UMaterialInterface*> UsedMaterials;
	GetUsedMaterials(UsedMaterials);

	const bool bUseTessellation = UseNaniteTessellation();

	for (const UMaterialInterface* MaterialInterface : UsedMaterials)
	{
		if (MaterialInterface)
		{
			FMaterialRelevance MaterialRelevance = MaterialInterface->GetRelevance_Concurrent(FeatureLevel);

			Result.bAnyMaterialHasWorldPositionOffset = Result.bAnyMaterialHasWorldPositionOffset || MaterialRelevance.bUsesWorldPositionOffset;

			if (MaterialInterface->HasPixelAnimation() && IsOpaqueOrMaskedBlendMode(MaterialInterface->GetBlendMode()))
			{
				Result.bAnyMaterialHasPixelAnimation = true;
			}

			if (bUseTessellation && MaterialRelevance.bUsesDisplacement)
			{
				FDisplacementScaling DisplacementScaling = MaterialInterface->GetDisplacementScaling();
			
				const float MinDisplacement = (0.0f - DisplacementScaling.Center) * DisplacementScaling.Magnitude;
				const float MaxDisplacement = (1.0f - DisplacementScaling.Center) * DisplacementScaling.Magnitude;

				Result.MinMaxMaterialDisplacement.X = FMath::Min(Result.MinMaxMaterialDisplacement.X, MinDisplacement);
				Result.MinMaxMaterialDisplacement.Y = FMath::Max(Result.MinMaxMaterialDisplacement.Y, MaxDisplacement);
			}

			Result.MaxWorldPositionOffsetDisplacement = FMath::Max(Result.MaxWorldPositionOffsetDisplacement, MaterialInterface->GetMaxWorldPositionOffsetDisplacement());

			const FMaterialCachedExpressionData& CachedMaterialData = MaterialInterface->GetCachedExpressionData();

			Result.bAnyMaterialHasPerInstanceRandom = Result.bAnyMaterialHasPerInstanceRandom || CachedMaterialData.bHasPerInstanceRandom;
			Result.bAnyMaterialHasPerInstanceCustomData = Result.bAnyMaterialHasPerInstanceCustomData || CachedMaterialData.bHasPerInstanceCustomData;
		}
	}

	return Result;
}

#if WITH_EDITOR
const bool UPrimitiveComponent::ShouldGenerateAutoLOD(const int32 HierarchicalLevelIndex) const
{	
	if (!IsHLODRelevant())
	{
		return false;
	}

	// bAllowSpecificExclusion
	bool bExcluded = false;
	if (HierarchicalLevelIndex < CHAR_BIT && IsExcludedFromHLODLevel(EHLODLevelExclusion(1 << HierarchicalLevelIndex)))
	{
		const TArray<struct FHierarchicalSimplification>& HLODSetup = GetOwner()->GetLevel()->GetWorldSettings()->GetHierarchicalLODSetup();
		if (HLODSetup.IsValidIndex(HierarchicalLevelIndex))
		{
			if (HLODSetup[HierarchicalLevelIndex].bAllowSpecificExclusion)
			{
				bExcluded = true;
			}
		}
	}

	return !bExcluded;
}

#endif

void UPrimitiveComponent::SetExcludeForSpecificHLODLevels(const TArray<int32>& InExcludeForSpecificHLODLevels)
{
	ExcludeFromHLODLevels = 0;
	for (int32 ExcludeFromLevel : InExcludeForSpecificHLODLevels)
	{
		if (ExcludeFromLevel < CHAR_BIT)
		{
			SetExcludedFromHLODLevel(EHLODLevelExclusion(1 << ExcludeFromLevel), true);
		}
	}
}

TArray<int32> UPrimitiveComponent::GetExcludeForSpecificHLODLevels() const
{
	TArray<int32> ExcludeFromLevels;

	for (int32 ExcludeFromLevel = 0; ExcludeFromLevel < CHAR_BIT; ExcludeFromLevel++)
	{
		if (IsExcludedFromHLODLevel(EHLODLevelExclusion(1 << ExcludeFromLevel)))
		{
			ExcludeFromLevels.Add(ExcludeFromLevel);
		}
	}

	return ExcludeFromLevels;
}

bool UPrimitiveComponent::IsExcludedFromHLODLevel(EHLODLevelExclusion HLODLevel) const
{
	return EnumHasAllFlags((EHLODLevelExclusion)ExcludeFromHLODLevels, HLODLevel);
}

void UPrimitiveComponent::SetExcludedFromHLODLevel(EHLODLevelExclusion HLODLevel, bool bExcluded)
{
	if (bExcluded)
	{
		EnumAddFlags((EHLODLevelExclusion&)ExcludeFromHLODLevels, HLODLevel);
	}
	else
	{
		EnumRemoveFlags((EHLODLevelExclusion&)ExcludeFromHLODLevels, HLODLevel);
	}
}

void UPrimitiveComponent::GetPrimitiveStats(FPrimitiveStats& PrimitiveStats) const
{
	// no default values returned
}

bool FActorPrimitiveComponentInterface::IsRenderStateCreated() const 
{
	return UPrimitiveComponent::GetPrimitiveComponent(this)->IsRenderStateCreated();
}

bool FActorPrimitiveComponentInterface::IsRenderStateDirty() const 
{
	return UPrimitiveComponent::GetPrimitiveComponent(this)->IsRenderStateDirty();
}

bool FActorPrimitiveComponentInterface::ShouldCreateRenderState() const 
{
	return UPrimitiveComponent::GetPrimitiveComponent(this)->ShouldCreateRenderState();
}

bool FActorPrimitiveComponentInterface::IsRegistered() const 
{
	return UPrimitiveComponent::GetPrimitiveComponent(this)->IsRegistered();
}

bool FActorPrimitiveComponentInterface::IsUnreachable() const 
{
	return UPrimitiveComponent::GetPrimitiveComponent(this)->IsUnreachable();
}

UWorld* FActorPrimitiveComponentInterface::GetWorld() const 
{
	return UPrimitiveComponent::GetPrimitiveComponent(this)->GetWorld();
}

FSceneInterface* FActorPrimitiveComponentInterface::GetScene() const 
{
	return UPrimitiveComponent::GetPrimitiveComponent(this)->GetScene();
}

FPrimitiveSceneProxy* FActorPrimitiveComponentInterface::GetSceneProxy() const 
{
	return UPrimitiveComponent::GetPrimitiveComponent(this)->SceneProxy;
}

void FActorPrimitiveComponentInterface::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const
{
	UPrimitiveComponent::GetPrimitiveComponent(this)->GetUsedMaterials(OutMaterials, bGetDebugMaterials);
}

void FActorPrimitiveComponentInterface::MarkRenderStateDirty()
{
	UPrimitiveComponent::GetPrimitiveComponent(this)->MarkRenderStateDirty();
}

void FActorPrimitiveComponentInterface::DestroyRenderState() 
{
	UPrimitiveComponent::GetPrimitiveComponent(this)->DestroyRenderState_Concurrent();
}

void FActorPrimitiveComponentInterface::CreateRenderState(FRegisterComponentContext* Context) 
{
	UPrimitiveComponent::GetPrimitiveComponent(this)->CreateRenderState_Concurrent(Context);
}

FString FActorPrimitiveComponentInterface::GetName() const 
{
	return UPrimitiveComponent::GetPrimitiveComponent(this)->GetName();
}

FString FActorPrimitiveComponentInterface::GetFullName() const 
{
	return UPrimitiveComponent::GetPrimitiveComponent(this)->GetFullName();
}

FTransform FActorPrimitiveComponentInterface::GetTransform() const 
{
	return UPrimitiveComponent::GetPrimitiveComponent(this)->GetComponentTransform();
}

FBoxSphereBounds FActorPrimitiveComponentInterface::GetBounds() const 
{
	return UPrimitiveComponent::GetPrimitiveComponent(this)->Bounds;
}

float FActorPrimitiveComponentInterface::GetLastRenderTimeOnScreen() const 
{
	return UPrimitiveComponent::GetPrimitiveComponent(this)->GetLastRenderTimeOnScreen();
}

void FActorPrimitiveComponentInterface::GetPrimitiveStats(FPrimitiveStats& PrimitiveStats) const
{
return UPrimitiveComponent::GetPrimitiveComponent(this)->GetPrimitiveStats(PrimitiveStats);
}


UObject* FActorPrimitiveComponentInterface::GetUObject() 
{
	return UPrimitiveComponent::GetPrimitiveComponent(this);
}

const UObject* FActorPrimitiveComponentInterface::GetUObject() const 
{
	return UPrimitiveComponent::GetPrimitiveComponent(this);
}

UObject* FActorPrimitiveComponentInterface::GetOwner() const 
{
	return UPrimitiveComponent::GetPrimitiveComponent(this)->GetOwner();
}

FString FActorPrimitiveComponentInterface::GetOwnerName() const 
{
	const UPrimitiveComponent* Component = UPrimitiveComponent::GetPrimitiveComponent(this);

#if ACTOR_HAS_LABELS
	return Component->GetOwner() ? Component->GetOwner()->GetActorNameOrLabel() : Component->GetName();
#else
	return Component->GetName();
#endif
}

FPrimitiveSceneProxy* FActorPrimitiveComponentInterface::CreateSceneProxy() 
{
	UPrimitiveComponent* Component = UPrimitiveComponent::GetPrimitiveComponent(this);
	check(Component->SceneProxy == nullptr && Component->SceneData.SceneProxy == nullptr);
	FPrimitiveSceneProxy* Proxy = Component->CreateSceneProxy();
	Component->SceneData.SceneProxy = Proxy;
	Component->SceneProxy = Proxy;
	return Proxy;
}

#if WITH_EDITOR
HHitProxy* FActorPrimitiveComponentInterface::CreateMeshHitProxy(int32 SectionIndex, int32 MaterialIndex) 
{
	UPrimitiveComponent* Component = UPrimitiveComponent::GetPrimitiveComponent(this);	
	return Component->CreateMeshHitProxy(SectionIndex, MaterialIndex);	
}
#endif

#undef LOCTEXT_NAMESPACE
