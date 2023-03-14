// Copyright Epic Games, Inc. All Rights Reserved.

#include "GroomComponent.h"
#include "GeometryCacheComponent.h"
#include "Materials/Material.h"
#include "MaterialShared.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "PrimitiveSceneProxy.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "HairStrandsRendering.h"
#include "HairCardsVertexFactory.h"
#include "HairStrandsVertexFactory.h"
#include "RayTracingInstance.h"
#include "RayTracingDefinitions.h"
#include "HAL/LowLevelMemTracker.h"
#include "HairStrandsInterface.h"
#include "UObject/UObjectIterator.h"
#include "GlobalShader.h"
#include "Components/SkeletalMeshComponent.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Engine/RendererSettings.h"
#include "Animation/AnimationSettings.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"
#include "Misc/MapErrors.h"
#include "NiagaraComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "GroomBindingBuilder.h"
#include "RenderTargetPool.h"
#include "GroomManager.h"
#include "GroomInstance.h"
#include "GroomCache.h"
#include "GroomCacheStreamingManager.h"
#include "GroomPluginSettings.h"
#include "Async/Async.h"
#include "Async/ParallelFor.h"
#include "PrimitiveSceneInfo.h"
#include "PSOPrecache.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GroomComponent)
LLM_DECLARE_TAG(Groom);

static int32 GHairEnableAdaptiveSubsteps = 0;  
static FAutoConsoleVariableRef CVarHairEnableAdaptiveSubsteps(TEXT("r.HairStrands.EnableAdaptiveSubsteps"), GHairEnableAdaptiveSubsteps, TEXT("Enable adaptive solver substeps"));
bool IsHairAdaptiveSubstepsEnabled() { return (GHairEnableAdaptiveSubsteps == 1); }

static int32 GHairBindingValidationEnable = 0;
static FAutoConsoleVariableRef CVarHairBindingValidationEnable(TEXT("r.HairStrands.BindingValidation"), GHairBindingValidationEnable, TEXT("Enable groom binding validation, which report error/warnings with details about the cause."));

static bool GUseGroomCacheStreaming = true;
static FAutoConsoleVariableRef CVarGroomCacheStreamingEnable(TEXT("GroomCache.EnableStreaming"), GUseGroomCacheStreaming, TEXT("Enable groom cache streaming and prebuffering. Do not switch while groom caches are in use."));

static bool GUseProxyLocalToWorld = true;
static FAutoConsoleVariableRef CVarUseProxyLocalToWorld(TEXT("r.HairStrands.UseProxyLocalToWorld"), GUseProxyLocalToWorld, TEXT("Enable the use of the groom proxy local to world instead of extracting it from the game thread."));

static bool GHairStrands_Streaming_Prediction = false;
static FAutoConsoleVariableRef CVarHairStrands_Streaming_Prediction(TEXT("r.HairStrands.Streaming.Prediction"), GHairStrands_Streaming_Prediction, TEXT("Enable LOD streaming prediction."));

static int32 GHairStrands_BoundsMode = 0;
static FAutoConsoleVariableRef CVarHairStrands_BoundsMode(TEXT("r.HairStrands.BoundMode"), GHairStrands_BoundsMode, TEXT("Define how hair bound are computed at runtime when attached to a skel. mesh.\n 0: Use skel.mesh extented with grooms bounds.\n 1: Use skel.mesh bounds.\n 2: Use skel.mesh extented with grooms bounds (conservative)"));

static int32 GHairStrands_UseAttachedSimulationComponents = 0;
static FAutoConsoleVariableRef CVarHairStrands_UseAttachedSimulationComponents(TEXT("r.HairStrands.UseAttachedSimulationComponents"), GHairStrands_UseAttachedSimulationComponents, TEXT("Boolean to check if we are using already attached niagara components for simulation (WIP)"));

#define LOCTEXT_NAMESPACE "GroomComponent"

#define USE_HAIR_TRIANGLE_STRIP 0

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

EHairStrandsDebugMode GetHairStrandsGeometryDebugMode(const FHairGroupInstance* Instance);

const FLinearColor GetHairGroupDebugColor(int32 GroupIt)
{
	static TArray<FLinearColor> IndexedColor;
	if (IndexedColor.Num() == 0)
	{
		IndexedColor.Add(FLinearColor(FColor::Cyan));
		IndexedColor.Add(FLinearColor(FColor::Magenta));
		IndexedColor.Add(FLinearColor(FColor::Orange));
		IndexedColor.Add(FLinearColor(FColor::Silver));
		IndexedColor.Add(FLinearColor(FColor::Emerald));
		IndexedColor.Add(FLinearColor(FColor::Red));
		IndexedColor.Add(FLinearColor(FColor::Green));
		IndexedColor.Add(FLinearColor(FColor::Blue));
		IndexedColor.Add(FLinearColor(FColor::Yellow));
		IndexedColor.Add(FLinearColor(FColor::Purple));
		IndexedColor.Add(FLinearColor(FColor::Turquoise));
	
	}
	if (GroupIt >= IndexedColor.Num())
	{
		IndexedColor.Add(FLinearColor::MakeRandomColor());
	}
	check(GroupIt < IndexedColor.Num());
	return IndexedColor[GroupIt];	
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static EHairBindingType ToHairBindingType(EGroomBindingType In)
{
	switch (In)
	{
	case EGroomBindingType::Rigid		: return EHairBindingType::Rigid;
	case EGroomBindingType::Skinning	: return EHairBindingType::Skinning;
	case EGroomBindingType::NoneBinding	: return EHairBindingType::NoneBinding;
	}
	return EHairBindingType::NoneBinding;
}

static EHairInterpolationType ToHairInterpolationType(EGroomInterpolationType In)
{
	switch (In)
	{
	case EGroomInterpolationType::None				: return EHairInterpolationType::NoneSkinning; 
	case EGroomInterpolationType::RigidTransform	: return EHairInterpolationType::RigidSkinning;
	case EGroomInterpolationType::OffsetTransform	: return EHairInterpolationType::OffsetSkinning;
	case EGroomInterpolationType::SmoothTransform	: return EHairInterpolationType::SmoothSkinning;
	}
	return EHairInterpolationType::NoneSkinning;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static FHairGroupDesc GetGroomGroupsDesc(const UGroomAsset* Asset, UGroomComponent* Component, uint32 GroupIndex)
{
	if (!Asset || GroupIndex >= uint32(Component->GroomGroupsDesc.Num()))
	{
		return FHairGroupDesc();
	}

	FHairGroupDesc O = Component->GroomGroupsDesc[GroupIndex];
	O.HairLength = Asset->HairGroupsData[GroupIndex].Strands.BulkData.MaxLength;
	O.LODBias 	 = Asset->EffectiveLODBias[GroupIndex] > 0 ? FMath::Max(O.LODBias, Asset->EffectiveLODBias[GroupIndex]) : O.LODBias;

	if (!O.HairWidth_Override)					{ O.HairWidth					= Asset->HairGroupsRendering[GroupIndex].GeometrySettings.HairWidth;					}
	if (!O.HairRootScale_Override)				{ O.HairRootScale				= Asset->HairGroupsRendering[GroupIndex].GeometrySettings.HairRootScale;				}
	if (!O.HairTipScale_Override)				{ O.HairTipScale				= Asset->HairGroupsRendering[GroupIndex].GeometrySettings.HairTipScale;					}
	if (!O.bSupportVoxelization_Override)		{ O.bSupportVoxelization		= Asset->HairGroupsRendering[GroupIndex].ShadowSettings.bVoxelize;						}
	if (!O.HairShadowDensity_Override)			{ O.HairShadowDensity			= Asset->HairGroupsRendering[GroupIndex].ShadowSettings.HairShadowDensity;				}
	if (!O.HairRaytracingRadiusScale_Override)	{ O.HairRaytracingRadiusScale	= Asset->HairGroupsRendering[GroupIndex].ShadowSettings.HairRaytracingRadiusScale;		}
	if (!O.bUseHairRaytracingGeometry_Override) { O.bUseHairRaytracingGeometry  = Asset->HairGroupsRendering[GroupIndex].ShadowSettings.bUseHairRaytracingGeometry;		}
	if (!O.bUseStableRasterization_Override)	{ O.bUseStableRasterization		= Asset->HairGroupsRendering[GroupIndex].AdvancedSettings.bUseStableRasterization;		}
	if (!O.bScatterSceneLighting_Override)		{ O.bScatterSceneLighting		= Asset->HairGroupsRendering[GroupIndex].AdvancedSettings.bScatterSceneLighting;		}

	return O;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * An material render proxy which overrides the debug mode parameter.
 */
class FHairDebugModeMaterialRenderProxy : public FMaterialRenderProxy
{
public:
	const FMaterialRenderProxy* const Parent;
	const float DebugMode;
	const float HairMinRadius;
	const float HairMaxRadius;
	const FVector HairGroupColor;

	FName DebugModeParamName;
	FName MinHairRadiusParamName;
	FName MaxHairRadiusParamName;
	FName HairGroupHairColorParamName;

	/** Initialization constructor. */
	FHairDebugModeMaterialRenderProxy(const FMaterialRenderProxy* InParent, float InMode, float InMinRadius, float InMaxRadius, FVector InGroupColor) :
		FMaterialRenderProxy(InParent->GetMaterialName()),
		Parent(InParent),
		DebugMode(InMode),
		HairMinRadius(InMinRadius),
		HairMaxRadius(InMaxRadius),
		HairGroupColor(InGroupColor),
		DebugModeParamName(NAME_FloatProperty),
		MinHairRadiusParamName(NAME_ByteProperty),
		MaxHairRadiusParamName(NAME_IntProperty),
		HairGroupHairColorParamName(NAME_VectorProperty)
	{}

	virtual const FMaterial* GetMaterialNoFallback(ERHIFeatureLevel::Type InFeatureLevel) const override
	{
		return Parent->GetMaterialNoFallback(InFeatureLevel);
	}

	virtual const FMaterialRenderProxy* GetFallback(ERHIFeatureLevel::Type InFeatureLevel) const override
	{
		return Parent->GetFallback(InFeatureLevel);
	}

	virtual bool GetParameterValue(EMaterialParameterType Type, const FHashedMaterialParameterInfo& ParameterInfo, FMaterialParameterValue& OutValue, const FMaterialRenderContext& Context) const
	{
		switch (Type)
		{
		case EMaterialParameterType::Vector:
			if (ParameterInfo.Name == HairGroupHairColorParamName)
			{
				OutValue = FVector3f(HairGroupColor);
				return true;
			}
			break;
		case EMaterialParameterType::Scalar:
			if (ParameterInfo.Name == DebugModeParamName)
			{
				OutValue = DebugMode;
				return true;
			}
			else if (ParameterInfo.Name == MinHairRadiusParamName)
			{
				OutValue = HairMinRadius;
				return true;
			}
			else if (ParameterInfo.Name == MaxHairRadiusParamName)
			{
				OutValue = HairMaxRadius;
				return true;
			}
			break;
		default:
			break;
		}
		return Parent->GetParameterValue(Type, ParameterInfo, OutValue, Context);
	}
};

enum class EHairMaterialCompatibility : uint8
{
	Valid,
	Invalid_UsedWithHairStrands,
	Invalid_ShadingModel,
	Invalid_BlendMode,
	Invalid_IsNull
};

static EHairMaterialCompatibility IsHairMaterialCompatible(UMaterialInterface* MaterialInterface, ERHIFeatureLevel::Type FeatureLevel, EHairGeometryType GeometryType)
{
	// Hair material and opaque material are enforced for strands material as the strands system is tailored for this type of shading
	// (custom packing of material attributes). However this is not needed/required for cards/meshes, when the relaxation for these type
	// of goemetry
	if (MaterialInterface)
	{
		if (GeometryType != Strands)
		{
			return EHairMaterialCompatibility::Valid;
		}
		const FMaterialRelevance Relevance = MaterialInterface->GetRelevance_Concurrent(FeatureLevel);
		const bool bIsRelevanceInitialized = Relevance.Raw != 0;
		if (bIsRelevanceInitialized && !Relevance.bHairStrands)
		{
			return EHairMaterialCompatibility::Invalid_UsedWithHairStrands;
		}
		if (!MaterialInterface->GetShadingModels().HasShadingModel(MSM_Hair) && GeometryType == EHairGeometryType::Strands)
		{
			return EHairMaterialCompatibility::Invalid_ShadingModel;
		}
		if (MaterialInterface->GetBlendMode() != BLEND_Opaque && MaterialInterface->GetBlendMode() != BLEND_Masked && GeometryType == EHairGeometryType::Strands)
		{
			return EHairMaterialCompatibility::Invalid_BlendMode;
		}
	}
	else
	{
		return EHairMaterialCompatibility::Invalid_IsNull;
	}

	return EHairMaterialCompatibility::Valid;
}

enum class EHairMeshBatchType
{
	Raster,
	Raytracing
};

/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
//  FStrandHairSceneProxy
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////

inline int32 GetMaterialIndexWithFallback(int32 SlotIndex)
{
	// Default policy: if no slot index has been bound, fallback on slot 0. If there is no
	// slot, the material will fallback on the default material.
	return SlotIndex != INDEX_NONE ? SlotIndex : 0;
}

static EHairGeometryType ToHairGeometryType(EGroomGeometryType Type)
{
	switch (Type)
	{
	case EGroomGeometryType::Strands: return EHairGeometryType::Strands;
	case EGroomGeometryType::Cards:   return EHairGeometryType::Cards;
	case EGroomGeometryType::Meshes:  return EHairGeometryType::Meshes;
	}
	return EHairGeometryType::NoneGeometry;
}

class FHairStrandsSceneProxy final : public FPrimitiveSceneProxy
{
public:
	SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}

	FHairStrandsSceneProxy(UGroomComponent* Component)
		: FPrimitiveSceneProxy(Component)
		, MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
	{
		// Forcing primitive uniform as we don't support robustly GPU scene data
		bVFRequiresPrimitiveUniformBuffer = true;
		bCastDeepShadow = true;

		HairGroupMaterialProxies.SetNum(Component->HairGroupInstances.Num());
		HairGroupInstances = Component->HairGroupInstances;
		check(Component);
		check(Component->GroomAsset);
		check(Component->GroomAsset->GetNumHairGroups() > 0);
		ComponentId = Component->ComponentId.PrimIDValue;
		Strands_DebugMaterial = Component->Strands_DebugMaterial;
		bAlwaysHasVelocity = false;
		if (IsHairStrandsBindingEnable() && Component->RegisteredMeshComponent)
		{
			bAlwaysHasVelocity = true;
		}

		check(Component->HairGroupInstances.Num());

		const ERHIFeatureLevel::Type FeatureLevel = GetScene().GetFeatureLevel();
		const EShaderPlatform ShaderPlatform = GetScene().GetShaderPlatform();

		const int32 GroupCount = Component->GroomAsset->GetNumHairGroups();
		check(Component->GroomAsset->HairGroupsData.Num() == Component->HairGroupInstances.Num());
		for (int32 GroupIt=0; GroupIt<GroupCount; GroupIt++)
		{
			const bool bIsVisible = Component->GroomAsset->HairGroupsInfo[GroupIt].bIsVisible;

			const FHairGroupData& InGroupData = Component->GroomAsset->HairGroupsData[GroupIt];
			FHairGroupInstance* HairInstance = Component->HairGroupInstances[GroupIt];
			check(HairInstance->HairGroupPublicData);
			HairInstance->bForceCards = Component->bUseCards;
			HairInstance->bUpdatePositionOffset = Component->RegisteredMeshComponent != nullptr;
			HairInstance->bCastShadow = Component->CastShadow;

			if (HairInstance->Strands.IsValid() && HairInstance->Strands.VertexFactory == nullptr)
			{
				HairInstance->Strands.VertexFactory = new FHairStrandsVertexFactory(HairInstance, FeatureLevel, "FStrandsHairSceneProxy");
			}

			if (HairInstance->Cards.IsValid())
			{
				const uint32 LODCount = HairInstance->Cards.LODs.Num();
				for (uint32 LODIt = 0; LODIt < LODCount; ++LODIt)
				{
					if (HairInstance->Cards.IsValid(LODIt))
					{
						if (HairInstance->Cards.LODs[LODIt].VertexFactory == nullptr) HairInstance->Cards.LODs[LODIt].VertexFactory = new FHairCardsVertexFactory(HairInstance, LODIt, EHairGeometryType::Cards, ShaderPlatform, FeatureLevel, "HairCardsVertexFactory");
					}
				}
			}

			if (HairInstance->Meshes.IsValid())
			{
				const uint32 LODCount = HairInstance->Meshes.LODs.Num();
				for (uint32 LODIt = 0; LODIt < LODCount; ++LODIt)
				{
					if (HairInstance->Meshes.IsValid(LODIt))
					{
						if (HairInstance->Meshes.LODs[LODIt].VertexFactory == nullptr) HairInstance->Meshes.LODs[LODIt].VertexFactory = new FHairCardsVertexFactory(HairInstance, LODIt, EHairGeometryType::Meshes, ShaderPlatform, FeatureLevel, "HairMeshesVertexFactory");
					}
				}
			}

			{
				// If one of the group has simulation enable, then we enable velocity rendering for meshes/cards
				if (IsHairStrandsSimulationEnable() && HairInstance->Guides.IsValid() && (HairInstance->Guides.bIsSimulationEnable || HairInstance->Guides.bIsDeformationEnable))
				{
					bAlwaysHasVelocity = true;
				}
			}

			// Material - Strands
			if (IsHairStrandsEnabled(EHairStrandsShaderType::Strands, ShaderPlatform))
			{
				const int32 SlotIndex = Component->GroomAsset->GetMaterialIndex(Component->GroomAsset->HairGroupsRendering[GroupIt].MaterialSlotName);
				const UMaterialInterface* Material = Component->GetMaterial(GetMaterialIndexWithFallback(SlotIndex), EHairGeometryType::Strands, true);
				HairGroupMaterialProxies[GroupIt].Strands = Material ? Material->GetRenderProxy() : nullptr;
			}

			// Material - Cards
			HairGroupMaterialProxies[GroupIt].Cards.Init(nullptr, InGroupData.Cards.LODs.Num());
			if (IsHairStrandsEnabled(EHairStrandsShaderType::Cards, ShaderPlatform))
			{
				uint32 CardsLODIndex = 0;
				for (const FHairGroupData::FCards::FLOD& LOD : InGroupData.Cards.LODs)
				{
					if (LOD.IsValid())
					{
						// Material
						int32 SlotIndex = INDEX_NONE;
						for (const FHairGroupsCardsSourceDescription& Desc : Component->GroomAsset->HairGroupsCards)
						{
							if (Desc.GroupIndex == GroupIt && Desc.LODIndex == CardsLODIndex)
							{
								SlotIndex = Component->GroomAsset->GetMaterialIndex(Desc.MaterialSlotName);
								break;
							}
						}
						const UMaterialInterface* Material = Component->GetMaterial(GetMaterialIndexWithFallback(SlotIndex), EHairGeometryType::Cards, true);
						HairGroupMaterialProxies[GroupIt].Cards[CardsLODIndex] = Material ? Material->GetRenderProxy() : nullptr;
					}
					++CardsLODIndex;
				}
			}

			// Material - Meshes
			HairGroupMaterialProxies[GroupIt].Meshes.Init(nullptr, InGroupData.Meshes.LODs.Num());
			if (IsHairStrandsEnabled(EHairStrandsShaderType::Meshes, ShaderPlatform))
			{
				uint32 MeshesLODIndex = 0;
				for (const FHairGroupData::FMeshes::FLOD& LOD : InGroupData.Meshes.LODs)
				{
					if (LOD.IsValid())
					{
						// Material
						int32 SlotIndex = INDEX_NONE;
						for (const FHairGroupsMeshesSourceDescription& Desc : Component->GroomAsset->HairGroupsMeshes)
						{
							if (Desc.GroupIndex == GroupIt && Desc.LODIndex == MeshesLODIndex)
							{
								SlotIndex = Component->GroomAsset->GetMaterialIndex(Desc.MaterialSlotName);
								break;
							}
						}
						const UMaterialInterface* Material = Component->GetMaterial(GetMaterialIndexWithFallback(SlotIndex), EHairGeometryType::Meshes, true);
						HairGroupMaterialProxies[GroupIt].Meshes[MeshesLODIndex] = Material ? Material->GetRenderProxy() : nullptr;
					}
					++MeshesLODIndex;
				}
			}
		}
	}

	virtual ~FHairStrandsSceneProxy()
	{
	}
	
	/**
	 *	Called when the rendering thread adds the proxy to the scene.
	 *	This function allows for generating renderer-side resources.
	 *	Called in the rendering thread.
	 */
	virtual void CreateRenderThreadResources() 
	{
		FPrimitiveSceneProxy::CreateRenderThreadResources();

		// Register the data to the scene
		FSceneInterface& LocalScene = GetScene();
		TArray<FHairGroupInstance*> LocalInstances = HairGroupInstances;
		for (FHairGroupInstance* Instance : LocalInstances)
		{
			if (Instance->IsValid() || Instance->Strands.ClusterCullingResource)
			{
				check(Instance->HairGroupPublicData != nullptr);
				Instance->AddRef();
				Instance->Debug.Proxy = this;
				LocalScene.AddHairStrands(Instance);
			}
		}
	}

	/**
	 *	Called when the rendering thread removes the proxy from the scene.
	 *	This function allows for removing renderer-side resources.
	 *	Called in the rendering thread.
	 */
	virtual void DestroyRenderThreadResources() override
	{
		FPrimitiveSceneProxy::DestroyRenderThreadResources();

		// Unregister the data to the scene
		FSceneInterface& LocalScene = GetScene();
		TArray<FHairGroupInstance*> LocalInstances = HairGroupInstances;
		for (FHairGroupInstance* Instance : LocalInstances)
		{
			if (Instance->IsValid() || Instance->Strands.ClusterCullingResource)
			{
				check(Instance->GetRefCount() > 0);
				LocalScene.RemoveHairStrands(Instance);
				Instance->Debug.Proxy = nullptr;
				Instance->Release();
			}
		}
	}

	virtual void OnTransformChanged() override
	{
		const FTransform RigidLocalToWorld = FTransform(GetLocalToWorld());
		for (FHairGroupInstance* Instance : HairGroupInstances)
		{
			Instance->Debug.RigidPreviousLocalToWorld = Instance->Debug.RigidCurrentLocalToWorld;
			Instance->Debug.RigidCurrentLocalToWorld = RigidLocalToWorld;
		}
	}

	FORCEINLINE bool UseProxyLocalToWorld(const FHairGroupInstance* Instance) const
	{
		return (GUseProxyLocalToWorld && (Instance->BindingType != EHairBindingType::Skinning));
	}

#if RHI_RAYTRACING
	virtual bool IsRayTracingRelevant() const { return true; }
	virtual bool IsRayTracingStaticRelevant() const { return false; }

	virtual void GetDynamicRayTracingInstances(FRayTracingMaterialGatheringContext & Context, TArray<struct FRayTracingInstance>& OutRayTracingInstances) override
	{
		if (!IsHairRayTracingEnabled() || HairGroupInstances.Num() == 0)
			return;

		const EShaderPlatform Platform = Context.ReferenceView->GetShaderPlatform();
		if (!IsHairStrandsEnabled(EHairStrandsShaderType::Strands, Platform) &&
			!IsHairStrandsEnabled(EHairStrandsShaderType::Cards, Platform) &&
			!IsHairStrandsEnabled(EHairStrandsShaderType::Meshes, Platform))
		{
			return;
		}

		const bool bWireframe = AllowDebugViewmodes() && Context.ReferenceViewFamily.EngineShowFlags.Wireframe;
		const uint32 ViewRayTracingMask = Context.ReferenceViewFamily.EngineShowFlags.PathTracing ? EHairViewRayTracingMask::PathTracing : EHairViewRayTracingMask::RayTracing;
		if (bWireframe)
			return;

		for (uint32 GroupIt = 0, GroupCount = HairGroupInstances.Num(); GroupIt < GroupCount; ++GroupIt)
		{
			FHairGroupInstance* Instance = HairGroupInstances[GroupIt];
			check(Instance->GetRefCount() > 0);

			FMatrix OverrideLocalToWorld = UseProxyLocalToWorld(Instance) ? GetLocalToWorld() : Instance->GetCurrentLocalToWorld().ToMatrixWithScale();

			const EHairGeometryType GeometryType = Instance->HairGroupPublicData->VFInput.GeometryType;
			const uint32 LODIndex = Instance->HairGroupPublicData->GetIntLODIndex();

			FHairStrandsRaytracingResource* RTGeometry = nullptr;
			uint8 RayTracingMask = RAY_TRACING_MASK_OPAQUE;
			uint32 InstanceViewRayTracingMask = EHairViewRayTracingMask::PathTracing | EHairViewRayTracingMask::RayTracing;
			switch (GeometryType)
			{
				case EHairGeometryType::Strands:
				{
					RTGeometry = Instance->Strands.RenRaytracingResource;
					InstanceViewRayTracingMask = Instance->Strands.ViewRayTracingMask;
					RayTracingMask = RAY_TRACING_MASK_THIN_SHADOW;
					break;
				}
				case EHairGeometryType::Cards:
				{
					RTGeometry = Instance->Cards.LODs[LODIndex].RaytracingResource;
					break;
				}
				case EHairGeometryType::Meshes:
				{
					RTGeometry = Instance->Meshes.LODs[LODIndex].RaytracingResource;
					break;
				}
			}

			// If the view and the instance raytracing mask don't match skip this instance.
			if ((InstanceViewRayTracingMask & ViewRayTracingMask) == 0)
			{
				continue;
			}

			if (RTGeometry && RTGeometry->RayTracingGeometry.RayTracingGeometryRHI.IsValid())
			{
				for (const FRayTracingGeometrySegment& Segment : RTGeometry->RayTracingGeometry.Initializer.Segments)
				{
					check(Segment.VertexBuffer.IsValid());
				}
				// ViewFamily.EngineShowFlags.PathTracing
				if (FMeshBatch* MeshBatch = CreateMeshBatch(Context.ReferenceView, Context.ReferenceViewFamily, Context.RayTracingMeshResourceCollector, EHairMeshBatchType::Raytracing, Instance, GroupIt, nullptr))
				{
					FRayTracingInstance RayTracingInstance;
					RayTracingInstance.Geometry = &RTGeometry->RayTracingGeometry;
					RayTracingInstance.Materials.Add(*MeshBatch);
					RayTracingInstance.InstanceTransforms.Add(OverrideLocalToWorld);
					RayTracingInstance.BuildInstanceMaskAndFlags(GetScene().GetFeatureLevel(), ERayTracingInstanceLayer::NearField, RayTracingMask);

					// If thin shadow is requested, we ensure that regular shadow mask is not added.
					// TODO: can this logic live in BuildInstanceMaskAndFlags instead?
					if (RayTracingMask == RAY_TRACING_MASK_THIN_SHADOW)
					{
						if (RayTracingInstance.Mask & RAY_TRACING_MASK_SHADOW)
						{
							// geometry casts shadows, make sure it is in only one of the shadow casting groups, so it can be treated seperately if desired
							RayTracingInstance.Mask &= ~RAY_TRACING_MASK_SHADOW;
							RayTracingInstance.Mask |= RAY_TRACING_MASK_THIN_SHADOW;
						}
						else
						{
							// if the geometry does not cast shadows, remove this flag
							RayTracingInstance.Mask &= ~RAY_TRACING_MASK_THIN_SHADOW;
						}
						// make sure geometry is only in the hair group
						RayTracingInstance.Mask &= ~RAY_TRACING_MASK_OPAQUE;
						RayTracingInstance.Mask &= ~RAY_TRACING_MASK_TRANSLUCENT;
						RayTracingInstance.Mask |= RAY_TRACING_MASK_HAIR_STRANDS;
					}

					OutRayTracingInstances.Add(RayTracingInstance);
				}
			}
		}
	}
#endif

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{
		const EShaderPlatform Platform = ViewFamily.GetShaderPlatform();
		if (!IsHairStrandsEnabled(EHairStrandsShaderType::Strands, Platform) &&
			!IsHairStrandsEnabled(EHairStrandsShaderType::Cards, Platform) &&
			!IsHairStrandsEnabled(EHairStrandsShaderType::Meshes, Platform))
		{
			return;
		}

		TArray<FHairGroupInstance*> Instances = HairGroupInstances;
		if (Instances.Num() == 0)
		{
			return;
		}

		const uint32 GroupCount = Instances.Num();

		QUICK_SCOPE_CYCLE_COUNTER(STAT_HairStrandsSceneProxy_GetDynamicMeshElements);

		// Need information back from the rendering thread to knwo which representation to use (strands/cards/mesh)
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			const FSceneView* View = Views[ViewIndex];
			if (View->bIsReflectionCapture)
			{
				continue;
			}

			if (IsShown(View) && (VisibilityMap & (1 << ViewIndex)))
			{
				for (uint32 GroupIt = 0; GroupIt < GroupCount; ++GroupIt)
				{
					check(Instances[GroupIt]->GetRefCount() > 0);

					FMaterialRenderProxy* Debug_MaterialProxy = nullptr;
					EHairStrandsDebugMode DebugMode = GetHairStrandsGeometryDebugMode(Instances[GroupIt]);
					if (AllowDebugViewmodes() && View->Family->EngineShowFlags.LODColoration)
					{
						DebugMode = EHairStrandsDebugMode::RenderLODColoration;
					}
					
					const bool bNeedDebugMaterial = 
						DebugMode != EHairStrandsDebugMode::NoneDebug &&
						DebugMode != EHairStrandsDebugMode::RenderHairControlPoints &&
						DebugMode != EHairStrandsDebugMode::RenderHairTangent;

					if (bNeedDebugMaterial)
					{
						float DebugModeScalar = 0;
						switch(DebugMode)
						{
						case EHairStrandsDebugMode::NoneDebug					: DebugModeScalar =99.f; break;
						case EHairStrandsDebugMode::SimHairStrands				: DebugModeScalar = 0.f; break;
						case EHairStrandsDebugMode::RenderHairStrands			: DebugModeScalar = 0.f; break;
						case EHairStrandsDebugMode::RenderHairRootUV			: DebugModeScalar = 1.f; break;
						case EHairStrandsDebugMode::RenderHairUV				: DebugModeScalar = 2.f; break;
						case EHairStrandsDebugMode::RenderHairSeed				: DebugModeScalar = 3.f; break;
						case EHairStrandsDebugMode::RenderHairDimension			: DebugModeScalar = 4.f; break;
						case EHairStrandsDebugMode::RenderHairRadiusVariation	: DebugModeScalar = 5.f; break;
						case EHairStrandsDebugMode::RenderHairRootUDIM			: DebugModeScalar = 6.f; break;
						case EHairStrandsDebugMode::RenderHairBaseColor			: DebugModeScalar = 7.f; break;
						case EHairStrandsDebugMode::RenderHairRoughness			: DebugModeScalar = 8.f; break;
						case EHairStrandsDebugMode::RenderVisCluster			: DebugModeScalar = 0.f; break;
						case EHairStrandsDebugMode::RenderHairGroup				: DebugModeScalar = 9.f; break;
						case EHairStrandsDebugMode::RenderLODColoration			: DebugModeScalar = 10.f; break;
						};

						// TODO: fix this as the radius is incorrect. This code run before the interpolation code, which is where HairRadius is updated.
						float HairMaxRadius = 0;
						for (FHairGroupInstance* Instance : Instances)
						{
							HairMaxRadius = FMath::Max(HairMaxRadius, Instance->Strands.Modifier.HairWidth * 0.5f);
						}
						
						// Reuse the HairMaxRadius field to send the LOD index instead of adding yet another variable
						if (DebugMode == EHairStrandsDebugMode::RenderLODColoration)
						{
							HairMaxRadius = Instances[GroupIt]->HairGroupPublicData ? Instances[GroupIt]->HairGroupPublicData->LODIndex : 0;
						}

						FVector HairColor = FVector::ZeroVector;
						if (DebugMode == EHairStrandsDebugMode::RenderHairGroup)
						{
							HairColor = FVector(GetHairGroupDebugColor(GroupIt));
						}
						else if (DebugMode == EHairStrandsDebugMode::RenderLODColoration)
						{
							int32 LODIndex = Instances[GroupIt]->HairGroupPublicData ? Instances[GroupIt]->HairGroupPublicData->LODIndex : 0;
							LODIndex = FMath::Clamp(LODIndex, 0, GEngine->LODColorationColors.Num() - 1);
							const FLinearColor LODColor = GEngine->LODColorationColors[LODIndex];
							HairColor = FVector(LODColor.R, LODColor.G, LODColor.B);
						}
						auto DebugMaterial = new FHairDebugModeMaterialRenderProxy(Strands_DebugMaterial ? Strands_DebugMaterial->GetRenderProxy() : nullptr, DebugModeScalar, 0, HairMaxRadius, HairColor);
						Collector.RegisterOneFrameMaterialProxy(DebugMaterial);
						Debug_MaterialProxy = DebugMaterial;
					}

					if (FMeshBatch* MeshBatch = CreateMeshBatch(View, ViewFamily, Collector, EHairMeshBatchType::Raster, Instances[GroupIt], GroupIt, Debug_MaterialProxy))
					{
						Collector.AddMesh(ViewIndex, *MeshBatch);
					}
					else
					{
						continue;
					}

				#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
					// Render bounds
					RenderBounds(Collector.GetPDI(ViewIndex), ViewFamily.EngineShowFlags, GetBounds(), IsSelected());
				#endif
				}
			}
		}
	}

	FMeshBatch* CreateMeshBatch(
		const FSceneView* View,
		const FSceneViewFamily& ViewFamily,
		FMeshElementCollector& Collector,
		const EHairMeshBatchType MeshBatchType,
		const FHairGroupInstance* Instance,
		uint32 GroupIndex,
		FMaterialRenderProxy* Debug_MaterialProxy) const
	{
		const EHairGeometryType GeometryType = Instance->GeometryType;
		if (GeometryType == EHairGeometryType::NoneGeometry)
		{
			return nullptr;
		}

		check(Instance->GetRefCount());

		const int32 IntLODIndex = Instance->HairGroupPublicData->GetIntLODIndex();
		const bool bIsVisible = Instance->HairGroupPublicData->GetLODVisibility();

		const FVertexFactory* VertexFactory = nullptr;
		FIndexBuffer* IndexBuffer = nullptr;
		const FMaterialRenderProxy* MaterialRenderProxy = Debug_MaterialProxy;

		uint32 NumPrimitive = 0;
		uint32 HairVertexCount = 0;
		uint32 MaxVertexIndex = 0;
		bool bUseCulling = false;
		bool bWireframe = false;
		if (GeometryType == EHairGeometryType::Meshes)
		{
			if (!Instance->Meshes.IsValid(IntLODIndex))
			{
				return nullptr;
			}
			VertexFactory = (FVertexFactory*)Instance->Meshes.LODs[IntLODIndex].GetVertexFactory();
			check(VertexFactory);
			HairVertexCount = Instance->Meshes.LODs[IntLODIndex].RestResource->GetPrimitiveCount() * 3;
			MaxVertexIndex = HairVertexCount;
			NumPrimitive = HairVertexCount / 3;
			IndexBuffer = &Instance->Meshes.LODs[IntLODIndex].RestResource->IndexBuffer;
			bUseCulling = false;
			if (MaterialRenderProxy == nullptr)
			{
				MaterialRenderProxy = HairGroupMaterialProxies[GroupIndex].Meshes[IntLODIndex];
			}
			bWireframe = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;
		}
		else if (GeometryType == EHairGeometryType::Cards)
		{
			if (!Instance->Cards.IsValid(IntLODIndex))
			{
				return nullptr;
			}

			VertexFactory = (FVertexFactory*)Instance->Cards.LODs[IntLODIndex].GetVertexFactory();
			check(VertexFactory);
			HairVertexCount = Instance->Cards.LODs[IntLODIndex].RestResource->GetPrimitiveCount() * 3;
			MaxVertexIndex = HairVertexCount;
			NumPrimitive = HairVertexCount / 3;
			IndexBuffer = &Instance->Cards.LODs[IntLODIndex].RestResource->RestIndexBuffer;
			bUseCulling = false;
			if (MaterialRenderProxy == nullptr)
			{
				MaterialRenderProxy = HairGroupMaterialProxies[GroupIndex].Cards[IntLODIndex];
			}
			bWireframe = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;
		}
		else // if (GeometryType == EHairGeometryType::Strands)
		{
			VertexFactory = (FVertexFactory*)Instance->Strands.VertexFactory;
			HairVertexCount = Instance->Strands.RestResource->GetVertexCount();
			#if USE_HAIR_TRIANGLE_STRIP
			MaxVertexIndex = HairVertexCount * 2;
			#else
			MaxVertexIndex = HairVertexCount * 6;
			#endif
			bUseCulling = Instance->Strands.bIsCullingEnabled;
			NumPrimitive = bUseCulling ? 0 : HairVertexCount * 2;
			if (MaterialRenderProxy == nullptr)
			{
				MaterialRenderProxy = HairGroupMaterialProxies[GroupIndex].Strands;
			}
			bWireframe = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;
		}

		if (MaterialRenderProxy == nullptr || !bIsVisible)
		{
			return nullptr;
		}

		// Invalid primitive setup. This can happens when the (procedural) resources are not ready.
		if (NumPrimitive == 0 && !bUseCulling)
		{
			return nullptr;
		}

		if (bWireframe)
		{
			FMaterialRenderProxy* ColoredMaterialRenderProxy = new FColoredMaterialRenderProxy( GEngine->WireframeMaterial ? GEngine->WireframeMaterial->GetRenderProxy() : NULL, FLinearColor(1.f, 0.5f, 0.f));
			Collector.RegisterOneFrameMaterialProxy(ColoredMaterialRenderProxy);
			MaterialRenderProxy = ColoredMaterialRenderProxy;
		}

		// Draw the mesh.
		FMeshBatch& Mesh = Collector.AllocateMesh();

		const bool bUseCardsOrMeshes = GeometryType == EHairGeometryType::Cards || GeometryType == EHairGeometryType::Meshes;
		Mesh.CastShadow = bUseCardsOrMeshes;
		Mesh.bUseForMaterial = MeshBatchType == EHairMeshBatchType::Raytracing || bUseCardsOrMeshes;
		Mesh.bUseForDepthPass = bUseCardsOrMeshes;
		Mesh.SegmentIndex = 0;
		#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		Mesh.VisualizeLODIndex = IntLODIndex;
		#endif

		FMeshBatchElement& BatchElement = Mesh.Elements[0];
		BatchElement.IndexBuffer = IndexBuffer;
		Mesh.bWireframe = bWireframe;
		Mesh.VertexFactory = VertexFactory;
		Mesh.MaterialRenderProxy = MaterialRenderProxy;
		bool bHasPrecomputedVolumetricLightmap;
		FMatrix PreviousLocalToWorld;
		int32 SingleCaptureIndex;
		bool bOutputVelocity = GeometryType == EHairGeometryType::Cards || GeometryType == EHairGeometryType::Meshes;

		FPrimitiveSceneInfo* PrimSceneInfo = GetPrimitiveSceneInfo();
		GetScene().GetPrimitiveUniformShaderParameters_RenderThread(PrimSceneInfo, bHasPrecomputedVolumetricLightmap, PreviousLocalToWorld, SingleCaptureIndex, bOutputVelocity);

		const bool bUseProxy = UseProxyLocalToWorld(Instance);

		FMatrix CurrentLocalToWorld = bUseProxy ? GetLocalToWorld() : Instance->GetCurrentLocalToWorld().ToMatrixWithScale();
		PreviousLocalToWorld = bUseProxy ? PreviousLocalToWorld : Instance->GetPreviousLocalToWorld().ToMatrixWithScale();

		FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer = Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
		DynamicPrimitiveUniformBuffer.Set(CurrentLocalToWorld, PreviousLocalToWorld, GetBounds(), GetLocalBounds(), true, false, bOutputVelocity);
		BatchElement.PrimitiveUniformBufferResource = &DynamicPrimitiveUniformBuffer.UniformBuffer; // automatic copy to the gpu scene buffer
		//primtiveid is set to 0

		BatchElement.FirstIndex = 0;
		BatchElement.NumInstances = 1;
		BatchElement.PrimitiveIdMode = GeometryType == EHairGeometryType::Strands ? PrimID_ForceZero : PrimID_DynamicPrimitiveShaderData;
		if (bUseCulling)
		{
			BatchElement.NumPrimitives = 0;
			BatchElement.IndirectArgsBuffer = bUseCulling ? Instance->HairGroupPublicData->GetDrawIndirectBuffer().Buffer->GetRHI() : nullptr;
			BatchElement.IndirectArgsOffset = 0;
		}
		else
		{
			BatchElement.NumPrimitives = NumPrimitive;
			BatchElement.IndirectArgsBuffer = nullptr;
			BatchElement.IndirectArgsOffset = 0;
		}

		// Setup our vertex factor custom data
		BatchElement.VertexFactoryUserData = const_cast<void*>(reinterpret_cast<const void*>(Instance->HairGroupPublicData));

		BatchElement.MinVertexIndex = 0;
		BatchElement.MaxVertexIndex = MaxVertexIndex;
		BatchElement.UserData = reinterpret_cast<void*>(uint64(ComponentId));
		Mesh.ReverseCulling = bUseCardsOrMeshes ? IsLocalToWorldDeterminantNegative() : false;
		Mesh.bDisableBackfaceCulling = GeometryType == EHairGeometryType::Strands;
		#if USE_HAIR_TRIANGLE_STRIP
		Mesh.Type = GeometryType == EHairGeometryType::Strands ? PT_TriangleStrip : PT_TriangleList;
		#else
		Mesh.Type = PT_TriangleList;
		#endif
		Mesh.DepthPriorityGroup = SDPG_World;
		Mesh.bCanApplyViewModeOverrides = false;
		Mesh.BatchHitProxyId = PrimSceneInfo->DefaultDynamicHitProxyId;

		return &Mesh;
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		bool bUseCardsOrMesh = false;
		for (FHairGroupInstance* Instance : HairGroupInstances)
		{
			check(Instance->GetRefCount());
			const EHairGeometryType GeometryType = Instance->GeometryType;
			bUseCardsOrMesh = bUseCardsOrMesh || GeometryType == EHairGeometryType::Cards || GeometryType == EHairGeometryType::Meshes;
		}

		FPrimitiveViewRelevance Result;
		Result.bHairStrands = IsShown(View);

		// Special pass for hair strands geometry (not part of the base pass, and shadowing is handlded in a custom fashion). When cards rendering is enabled we reusethe base pass
		Result.bDrawRelevance		= IsShown(View);
		Result.bRenderInMainPass	= bUseCardsOrMesh && ShouldRenderInMainPass();
		Result.bShadowRelevance		= IsShadowCast(View);
		Result.bDynamicRelevance	= bUseCardsOrMesh;
		Result.bRenderCustomDepth	= ShouldRenderCustomDepth();
		Result.bVelocityRelevance	= Result.bRenderInMainPass && bUseCardsOrMesh;
		Result.bUsesLightingChannels= GetLightingChannelMask() != GetDefaultLightingChannelMask();

		// Selection only
		#if WITH_EDITOR
		{
			Result.bEditorStaticSelectionRelevance = true;
		}
		#endif
		MaterialRelevance.SetPrimitiveViewRelevance(Result);
		return Result;
	}

	virtual uint32 GetMemoryFootprint(void) const override { return(sizeof(*this) + GetAllocatedSize()); }

	uint32 GetAllocatedSize(void) const { return(FPrimitiveSceneProxy::GetAllocatedSize()); }

	TArray<FHairGroupInstance*> HairGroupInstances;

	// Cache the material proxy to avoid race condition, when groom component's proxy is recreated, 
	// while another one is currently in flight for drawing.
	struct FHairGroupMaterialProxy
	{
		const FMaterialRenderProxy* Strands = nullptr;
		TArray<const FMaterialRenderProxy*> Cards;
		TArray<const FMaterialRenderProxy*> Meshes;
	};
	TArray<FHairGroupMaterialProxy> HairGroupMaterialProxies;
	
private:
	uint32 ComponentId = 0;
	FMaterialRelevance MaterialRelevance;
	UMaterialInterface* Strands_DebugMaterial = nullptr;
};

/** GroomCacheBuffers implementation that hold copies of the GroomCacheAnimationData needed for playback */
class FGroomCacheBuffers : public IGroomCacheBuffers
{
public:
	FGroomCacheBuffers(UGroomCache* InGroomCache)
	: GroomCache(InGroomCache)
	{
	}

	virtual ~FGroomCacheBuffers()
	{
	}

	virtual void Reset()
	{
	}

	virtual const FGroomCacheAnimationData& GetCurrentFrameBuffer() override
	{
		return CurrentFrame;
	}

	virtual const FGroomCacheAnimationData& GetNextFrameBuffer() override
	{
		return NextFrame;
	}

	virtual const FGroomCacheAnimationData& GetInterpolatedFrameBuffer() override
	{
		return InterpolatedFrame;
	}

	virtual int32 GetCurrentFrameIndex() const override
	{
		return CurrentFrameIndex;
	}

	virtual int32 GetNextFrameIndex() const override
	{
		return NextFrameIndex;
	}

	virtual float GetInterpolationFactor() const override
	{
		return InterpolationFactor;
	}

	virtual void UpdateBuffersAtTime(float Time, bool bIsLooping)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FGroomCacheBuffers::UpdateBuffersAtTime);

		// Find the frame indices and interpolation factor to interpolate between
		int32 FrameIndexA = 0;
		int32 FrameIndexB = 0;
		float OutInterpolationFactor = 0.0f;
		GroomCache->GetFrameIndicesAtTime(Time, bIsLooping, false, FrameIndexA, FrameIndexB, OutInterpolationFactor);

		// Update and cache the frame data as needed
		bool bComputeInterpolation = false;
		if (FrameIndexA != CurrentFrameIndex)
		{
			bComputeInterpolation = true;
			if (FrameIndexA == NextFrameIndex)
			{
				Swap(CurrentFrame, NextFrame);
				CurrentFrameIndex = NextFrameIndex;
			}
			else
			{
				GroomCache->GetGroomDataAtFrameIndex(FrameIndexA, CurrentFrame);
				CurrentFrameIndex = FrameIndexA;
			}
		}

		if (FrameIndexB != NextFrameIndex)
		{
			bComputeInterpolation = true;
			GroomCache->GetGroomDataAtFrameIndex(FrameIndexB, NextFrame);
			NextFrameIndex = FrameIndexB;
		}

		// Make sure the initial interpolated frame is populated with valid data
		if (InterpolatedFrame.GroupsData.Num() != CurrentFrame.GroupsData.Num())
		{
			InterpolatedFrame = CurrentFrame;
		}

		bComputeInterpolation = bComputeInterpolation || !FMath::IsNearlyEqual(InterpolationFactor, OutInterpolationFactor, KINDA_SMALL_NUMBER);

		// Do interpolation of vertex positions if needed
		if (bComputeInterpolation)
		{
			Interpolate(CurrentFrame, NextFrame, OutInterpolationFactor);
		}
	}

	void Interpolate(const FGroomCacheAnimationData& FrameA, const FGroomCacheAnimationData& FrameB, float InInterpolationFactor)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FGroomCacheBuffers::InterpolateCPU);

		InterpolationFactor = InInterpolationFactor;

		FScopeLock Lock(GetCriticalSection());

		const int32 NumGroups = FrameA.GroupsData.Num();
		for (int32 GroupIndex = 0; GroupIndex < NumGroups; ++GroupIndex)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FGroomCacheBuffers::InterpolateCPU_Group);

			const FGroomCacheGroupData& CurrentGroupData = FrameA.GroupsData[GroupIndex];
			const FGroomCacheGroupData& NextGroupData = FrameB.GroupsData[GroupIndex];
			FGroomCacheGroupData& InterpolatedGroupData = InterpolatedFrame.GroupsData[GroupIndex];
			const int32 NumVertices = CurrentGroupData.VertexData.PointsPosition.Num();
			const int32 NextNumVertices = NextGroupData.VertexData.PointsPosition.Num();

			// Update the bounding box used for hair strands rendering computation
			FVector InterpolatedCenter = FMath::Lerp(CurrentGroupData.BoundingBox.GetCenter(), NextGroupData.BoundingBox.GetCenter(), InterpolationFactor);
			InterpolatedGroupData.BoundingBox = CurrentGroupData.BoundingBox.MoveTo(InterpolatedCenter) + NextGroupData.BoundingBox.MoveTo(InterpolatedCenter);

			const bool bHasRadiusData = InterpolatedGroupData.VertexData.PointsRadius.Num() > 0;
			if (bHasRadiusData)
			{
				InterpolatedGroupData.StrandData.MaxRadius = FMath::Lerp(CurrentGroupData.StrandData.MaxRadius, NextGroupData.StrandData.MaxRadius, InterpolationFactor);
			}

			if (NumVertices == NextNumVertices)
			{
				// In case the topology is varying, make sure the interpolated group data can hold the required number of vertices
				InterpolatedGroupData.VertexData.PointsPosition.SetNum(NumVertices);

			// Parallel batched interpolation
			const int32 BatchSize = 1024;
			const int32 BatchCount = (NumVertices + BatchSize - 1) / BatchSize;

			ParallelFor(BatchCount, [&](int32 BatchIndex)
			{
				const int32 Start = BatchIndex * BatchSize;
				const int32 End = FMath::Min(Start + BatchSize, NumVertices); // one-past end index

				for (int32 VertexIndex = Start; VertexIndex < End; ++VertexIndex)
				{
					const FVector3f& CurrentPosition = CurrentGroupData.VertexData.PointsPosition[VertexIndex];
					const FVector3f& NextPosition = NextGroupData.VertexData.PointsPosition[VertexIndex];

					InterpolatedGroupData.VertexData.PointsPosition[VertexIndex] = FMath::Lerp(CurrentPosition, NextPosition, InterpolationFactor);

					if (bHasRadiusData)
					{
						const float CurrentRadius = CurrentGroupData.VertexData.PointsRadius[VertexIndex];
						const float NextRadius = NextGroupData.VertexData.PointsRadius[VertexIndex];

						InterpolatedGroupData.VertexData.PointsRadius[VertexIndex] = FMath::Lerp(CurrentRadius, NextRadius, InterpolationFactor);
					}
				}
			});
		}
			else
			{
				// Cannot interpolate, use the closest frame
				InterpolatedGroupData.VertexData.PointsPosition = InterpolationFactor < 0.5f ? CurrentGroupData.VertexData.PointsPosition : NextGroupData.VertexData.PointsPosition;

				if (bHasRadiusData)
				{
					InterpolatedGroupData.VertexData.PointsRadius = InterpolationFactor < 0.5f ? CurrentGroupData.VertexData.PointsRadius : NextGroupData.VertexData.PointsRadius;
				}
	}
		}
	}

	FBox GetBoundingBox()
	{
		// Approximate bounding box used for visibility culling
		FBox BBox(EForceInit::ForceInitToZero);
		for (const FGroomCacheGroupData& GroupData : GetCurrentFrameBuffer().GroupsData)
		{
			BBox += GroupData.BoundingBox;
		}

		for (const FGroomCacheGroupData& GroupData : GetNextFrameBuffer().GroupsData)
		{
			BBox += GroupData.BoundingBox;
		}

		return BBox;
	}


protected:
	static FGroomCacheAnimationData EmptyFrame;

	UGroomCache* GroomCache;

	/** Used with synchronous loading */
	FGroomCacheAnimationData CurrentFrame;
	FGroomCacheAnimationData NextFrame;

	/** Used for CPU interpolation */
	FGroomCacheAnimationData InterpolatedFrame;

	int32 CurrentFrameIndex = -1;
	int32 NextFrameIndex = -1;
	float InterpolationFactor = 0.0f;
};

FGroomCacheAnimationData FGroomCacheBuffers::EmptyFrame;

class FGroomCacheStreamedBuffers : public FGroomCacheBuffers
{
public:
	FGroomCacheStreamedBuffers(UGroomCache* InGroomCache)
		: FGroomCacheBuffers(InGroomCache)
		, CurrentFramePtr(nullptr)
		, NextFramePtr(nullptr)
	{
	}

	virtual ~FGroomCacheStreamedBuffers()
	{
		ResetInternal();
	}

	virtual void Reset() override
	{
		ResetInternal();
	}

private:
	void ResetInternal()
	{
		// Unmap the frames that are currently mapped
		if (CurrentFrameIndex != -1)
		{
			IGroomCacheStreamingManager::Get().UnmapAnimationData(GroomCache, CurrentFrameIndex);
			CurrentFrameIndex = -1;
			CurrentFramePtr = nullptr;
		}

		if (NextFrameIndex != -1)
		{
			IGroomCacheStreamingManager::Get().UnmapAnimationData(GroomCache, NextFrameIndex);
			NextFrameIndex = -1;
			NextFramePtr = nullptr;
		}
	}

public:
	virtual const FGroomCacheAnimationData& GetCurrentFrameBuffer() override
	{
		if (CurrentFramePtr)
		{
			return *CurrentFramePtr;
		}
		return EmptyFrame;
	}

	virtual const FGroomCacheAnimationData& GetNextFrameBuffer() override
	{
		if (NextFramePtr)
		{
			return *NextFramePtr;
		}
		return EmptyFrame;
	}

	virtual void UpdateBuffersAtTime(float Time, bool bIsLooping) override
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FGroomCacheStreamedBuffers::UpdateBuffersAtTime);

		FScopeLock Lock(GetCriticalSection());

		// Find the frame indices and interpolation factor to interpolate between
		int32 FrameIndexA = 0;
		int32 FrameIndexB = 0;
		float OutInterpolationFactor = 0.0f;
		GroomCache->GetFrameIndicesAtTime(Time, bIsLooping, false, FrameIndexA, FrameIndexB, OutInterpolationFactor);

		// Update and cache the frame data as needed
		bool bComputeInterpolation = false;
		if (FrameIndexA != CurrentFrameIndex)
		{
			bComputeInterpolation = true;
			if (FrameIndexA == NextFrameIndex)
			{
				// NextFrame is mapped to increment its ref count, but this could fail if the frame was evicted
				// This can happen if the time jumps around as is the case when Sequencer generates thumbnails
				const FGroomCacheAnimationData* DataPtr = IGroomCacheStreamingManager::Get().MapAnimationData(GroomCache, NextFrameIndex);
				if (DataPtr)
				{
					IGroomCacheStreamingManager::Get().UnmapAnimationData(GroomCache, CurrentFrameIndex);
					CurrentFramePtr = NextFramePtr;
					CurrentFrameIndex = NextFrameIndex;
				}
			}
			else
			{
				const FGroomCacheAnimationData* DataPtr = IGroomCacheStreamingManager::Get().MapAnimationData(GroomCache, FrameIndexA);
				if (DataPtr)
				{
					IGroomCacheStreamingManager::Get().UnmapAnimationData(GroomCache, CurrentFrameIndex);
					CurrentFramePtr = DataPtr;
					CurrentFrameIndex = FrameIndexA;
				}
			}
		}

		if (FrameIndexB != NextFrameIndex)
		{
			bComputeInterpolation = true;

			const FGroomCacheAnimationData* DataPtr = IGroomCacheStreamingManager::Get().MapAnimationData(GroomCache, FrameIndexB);
			if (DataPtr)
			{
				IGroomCacheStreamingManager::Get().UnmapAnimationData(GroomCache, NextFrameIndex);
				NextFramePtr = DataPtr;
				NextFrameIndex = FrameIndexB;
			}
		}

		if (CurrentFramePtr == nullptr || NextFramePtr == nullptr)
		{
			return;
		}

		// Make sure the initial interpolated frame is populated with valid data
		if (InterpolatedFrame.GroupsData.Num() != CurrentFramePtr->GroupsData.Num())
		{
			InterpolatedFrame = *CurrentFramePtr;
		}

		bComputeInterpolation = bComputeInterpolation || !FMath::IsNearlyEqual(InterpolationFactor, OutInterpolationFactor, KINDA_SMALL_NUMBER);

		// Do interpolation of vertex positions if needed
		if (bComputeInterpolation)
		{
			Interpolate(*CurrentFramePtr, *NextFramePtr, OutInterpolationFactor);
		}
	}

private:
	/** Used with GroomCache streaming. Point to cached data in the manager */
	const FGroomCacheAnimationData* CurrentFramePtr;
	const FGroomCacheAnimationData* NextFramePtr;
};

/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
// UComponent
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

UGroomComponent::UGroomComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	bTickInEditor = true;
	bAutoActivate = true;
	bSelectable = true;
	RegisteredMeshComponent = nullptr;
	SkeletalPreviousPositionOffset = FVector::ZeroVector;
	InitializedResources = nullptr;
	Mobility = EComponentMobility::Movable;
	bIsGroomAssetCallbackRegistered = false;
	bIsGroomBindingAssetCallbackRegistered = false;
	SourceSkeletalMesh = nullptr;
	NiagaraComponents.Empty();
	PhysicsAsset = nullptr;
	bCanEverAffectNavigation = false;
	bValidationEnable = GHairBindingValidationEnable > 0;
	bRunning = true;
	bLooping = true;
	bManualTick = false;
	ElapsedTime = 0.0f;

	// Overlap events are expensive and not needed (at least at the moment) as we don't need to collide against other component.
	SetGenerateOverlapEvents(false);
	SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Strands_DebugMaterialRef(TEXT("/HairStrands/Materials/HairDebugMaterial.HairDebugMaterial"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Strands_DefaultMaterialRef(TEXT("/HairStrands/Materials/HairDefaultMaterial.HairDefaultMaterial"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Cards_DefaultMaterialRef(TEXT("/HairStrands/Materials/HairCardsDefaultMaterial.HairCardsDefaultMaterial"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Meshes_DefaultMaterialRef(TEXT("/HairStrands/Materials/HairMeshesDefaultMaterial.HairMeshesDefaultMaterial"));

	Strands_DebugMaterial   = Strands_DebugMaterialRef.Object;
	Strands_DefaultMaterial = Strands_DefaultMaterialRef.Object;
	Cards_DefaultMaterial = Cards_DefaultMaterialRef.Object;
	Meshes_DefaultMaterial = Meshes_DefaultMaterialRef.Object;

	AngularSpringsSystem = nullptr;
	CosseratRodsSystem = nullptr;

#if WITH_EDITORONLY_DATA
	GroomAssetBeingLoaded = nullptr;
	BindingAssetBeingLoaded = nullptr;
#endif
}

void UGroomComponent::UpdateHairGroupsDesc()
{
	if (!GroomAsset)
	{
		GroomGroupsDesc.Empty();
		return;
	}

	const uint32 GroupCount = GroomAsset->GetNumHairGroups();
	const bool bNeedResize = GroupCount != GroomGroupsDesc.Num();
	if (bNeedResize)
	{
		GroomGroupsDesc.Init(FHairGroupDesc(), GroupCount);
	}
}

bool UGroomComponent::IsSimulationEnable(int32 GroupIndex, int32 LODIndex) const
{
	bool bIsSimulationEnable = GroomAsset ? GroomAsset->IsSimulationEnable(GroupIndex, LODIndex) : false;
	if (SimulationSettings.bOverrideSettings)
	{
		bIsSimulationEnable = bIsSimulationEnable && SimulationSettings.SolverSettings.bEnableSimulation;
	}

	return bIsSimulationEnable;
}

void UGroomComponent::ReleaseHairSimulation()
{
	for (int32 CompIndex = 0; CompIndex < NiagaraComponents.Num(); ++CompIndex)
	{
		ReleaseHairSimulation(CompIndex);
	}
	NiagaraComponents.Empty();
}

void UGroomComponent::ReleaseHairSimulation(const int32 GroupIndex)
{
	if (GroupIndex < NiagaraComponents.Num())
	{
		TObjectPtr<UNiagaraComponent>& NiagaraComponent = NiagaraComponents[GroupIndex];
		if (NiagaraComponent && !NiagaraComponent->IsBeingDestroyed())
		{
			if (GetWorld() && NiagaraComponent->IsRegistered())
			{
				NiagaraComponent->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
				NiagaraComponent->UnregisterComponent();
			}
			NiagaraComponent->DestroyComponent();
			NiagaraComponent = nullptr;
		}
	}
}

void UGroomComponent::CreateHairSimulation(const int32 GroupIndex, const int32 LODIndex)
{
	if (GroupIndex < NiagaraComponents.Num())
	{
		TObjectPtr<UNiagaraComponent>& NiagaraComponent = NiagaraComponents[GroupIndex];
		if (GroomAsset && (GroupIndex < GroomAsset->HairGroupsPhysics.Num()) && (LODIndex < GroomAsset->HairGroupsLOD[GroupIndex].LODs.Num()) && IsSimulationEnable(GroupIndex, LODIndex))
		{
			if (!NiagaraComponent)
			{
				NiagaraComponent = NewObject<UNiagaraComponent>(this, NAME_None, RF_Transient);
				NiagaraComponent->bUseAttachParentBound = true;
			}
			if (GetWorld() && GetWorld()->bIsWorldInitialized)
			{
				if (!NiagaraComponent->IsRegistered())
				{
					NiagaraComponent->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);
					NiagaraComponent->RegisterComponentWithWorld(GetWorld());
				}

				if (!AngularSpringsSystem) AngularSpringsSystem = LoadObject<UNiagaraSystem>(nullptr, TEXT("/HairStrands/Emitters/StableSpringsSystem.StableSpringsSystem"));
				if (!CosseratRodsSystem) CosseratRodsSystem = LoadObject<UNiagaraSystem>(nullptr, TEXT("/HairStrands/Emitters/StableRodsSystem.StableRodsSystem"));

				UNiagaraSystem* NiagaraAsset =
					(GroomAsset->HairGroupsPhysics[GroupIndex].SolverSettings.NiagaraSolver == EGroomNiagaraSolvers::AngularSprings) ? ToRawPtr(AngularSpringsSystem) :
					(GroomAsset->HairGroupsPhysics[GroupIndex].SolverSettings.NiagaraSolver == EGroomNiagaraSolvers::CosseratRods) ? ToRawPtr(CosseratRodsSystem) :
					GroomAsset->HairGroupsPhysics[GroupIndex].SolverSettings.CustomSystem.LoadSynchronous();
				NiagaraComponent->SetVisibleFlag(SimulationSettings.SimulationSetup.bDebugSimulation);
				NiagaraComponent->SetAsset(NiagaraAsset);
				NiagaraComponent->ReinitializeSystem();
			}
		}
		else if (NiagaraComponent && !NiagaraComponent->IsBeingDestroyed())
		{
			NiagaraComponent->SetAsset(nullptr);
			NiagaraComponent->DeactivateImmediate();
		}
	}
}

void UGroomComponent::UpdateHairSimulation()
{
	const int32 NumGroups = GroomAsset ? GroomAsset->HairGroupsPhysics.Num() : 0;
	const int32 NumComponents = FMath::Max(NumGroups, NiagaraComponents.Num());

	NiagaraComponents.SetNumZeroed(NumComponents);
	if( GHairStrands_UseAttachedSimulationComponents == 1)
	{
		int32 CompIndex = 0;
		// Fill the niagara components with the already attached children if any
		for(const TObjectPtr<USceneComponent>& GroomChild : GetAttachChildren())
		{
			if(GroomChild->IsA<UNiagaraComponent>() && (CompIndex < NumComponents) && !NiagaraComponents[CompIndex])
			{
				NiagaraComponents[CompIndex++] = StaticCast<UNiagaraComponent*>(GroomChild);
			}
		}
	}
	const int32 LODInit = (LODForcedIndex != -1.f) ? LODForcedIndex : (LODPredictedIndex != -1.f) ? LODPredictedIndex : 0;
	for (int32 CompIndex = 0; CompIndex < NumComponents; ++CompIndex)
	{
		CreateHairSimulation(CompIndex, LODInit);
	}
	UpdateSimulatedGroups();
}

void UGroomComponent::SwitchSimulationLOD(const int32 PreviousLOD, const int32 CurrentLOD)
{
	if (GroomAsset && PreviousLOD != CurrentLOD)
	{
		bool bRequiresSimulationUpdate = false;
		for (int32 GroupIt = 0, GroupCount = GroomAsset->HairGroupsData.Num(); GroupIt < GroupCount; ++GroupIt)
		{
			if ((IsSimulationEnable(GroupIt, PreviousLOD) != IsSimulationEnable(GroupIt, CurrentLOD)))
			{
				// Sanity check. Simulation does not support immediate mode.
				check(LODSelectionType != EHairLODSelectionType::Immediate);

				CreateHairSimulation(GroupIt, CurrentLOD);
				bRequiresSimulationUpdate = true;
			}
		}
		if (bRequiresSimulationUpdate)
		{
			UpdateSimulatedGroups();
		}
	}
}

void UGroomComponent::SetGroomAsset(UGroomAsset* Asset)
{
	SetGroomAsset(Asset, BindingAsset);
}

void UGroomComponent::SetGroomAsset(UGroomAsset* Asset, UGroomBindingAsset* InBinding, const bool bUpdateSimulation)
{
	ReleaseResources();
	if (Asset && Asset->IsValid())
	{
		GroomAsset = Asset;
		LODForcedIndex = FMath::Clamp(LODForcedIndex, -1, GroomAsset->GetLODCount() - 1);

#if WITH_EDITORONLY_DATA
		if (InBinding && !InBinding->IsValid())
		{
			// The binding could be invalid if the groom asset was previously invalid.
			// This will re-fetch the binding data from the DDC to make it valid
			InBinding->InvalidateBinding();
		}
		GroomAssetBeingLoaded = nullptr;
		BindingAssetBeingLoaded = nullptr;
#endif
	}
#if WITH_EDITORONLY_DATA
	else if (Asset)
	{
		// The asset is still being loaded. This will allow the assets to be re-set once the groom is finished loading
		GroomAssetBeingLoaded = Asset;
		BindingAssetBeingLoaded = InBinding;
	}
#endif
	else
	{
		GroomAsset = nullptr;
	}
	if (BindingAsset != InBinding
#if WITH_EDITORONLY_DATA
		// With the groom still being loaded, the binding is still invalid
		&& !BindingAssetBeingLoaded
#endif
		)
	{
		BindingAsset = InBinding;
	}

	if (!UGroomBindingAsset::IsBindingAssetValid(BindingAsset, false, bValidationEnable) || !UGroomBindingAsset::IsCompatible(GroomAsset, BindingAsset, bValidationEnable))
	{
		BindingAsset = nullptr;
	}

	UpdateHairGroupsDesc();
	if (!GroomAsset || !GroomAsset->IsValid())
	{
		return;
	}
	InitResources();
	if(bUpdateSimulation) UpdateHairSimulation();
}

void UGroomComponent::SetStableRasterization(bool bEnable)
{
	for (FHairGroupDesc& HairDesc : GroomGroupsDesc)
	{
		HairDesc.bUseStableRasterization = bEnable;
		HairDesc.bUseStableRasterization_Override = true;
	}
	UpdateHairGroupsDescAndInvalidateRenderState();
}

void UGroomComponent::SetHairRootScale(float Scale)
{
	Scale = FMath::Clamp(Scale, 0.f, 10.f);
	for (FHairGroupDesc& HairDesc : GroomGroupsDesc)
	{
		HairDesc.HairRootScale = Scale;
		HairDesc.HairRootScale_Override = true;
	}
	UpdateHairGroupsDescAndInvalidateRenderState();
}

void UGroomComponent::SetHairWidth(float HairWidth)
{
	for (FHairGroupDesc& HairDesc : GroomGroupsDesc)
	{
		HairDesc.HairWidth = HairWidth;
		HairDesc.HairWidth_Override = true;
	}
	UpdateHairGroupsDescAndInvalidateRenderState();
}

void UGroomComponent::SetScatterSceneLighting(bool Enable)
{
	for (FHairGroupDesc& HairDesc : GroomGroupsDesc)
	{
		HairDesc.bScatterSceneLighting = Enable;
		HairDesc.bScatterSceneLighting_Override = true;
	}
	UpdateHairGroupsDescAndInvalidateRenderState();
}

void UGroomComponent::SetUseCards(bool InbUseCards)
{
	bUseCards = InbUseCards;
	ResetSimulation();
	UpdateHairGroupsDescAndInvalidateRenderState();
}

void UGroomComponent::SetHairLengthScale(float InLengthScale)
{
	for (FHairGroupDesc& HairDesc : GroomGroupsDesc)
	{
		HairDesc.HairLengthScale = InLengthScale;
	}
	UpdateHairGroupsDescAndInvalidateRenderState(false);
}

void UGroomComponent::SetHairLengthScaleEnable(bool bEnable)
{
	for (FHairGroupDesc& HairDesc : GroomGroupsDesc)
	{
		HairDesc.HairLengthScale_Override = bEnable;
	}
	UpdateHairGroupsDescAndInvalidateRenderState(true);
	InitResources();
}

#if WITH_EDITOR
void UGroomComponent::SetDebugMode(EHairStrandsDebugMode InMode)
{ 
	DebugMode = InMode; 
	UpdateHairGroupsDescAndInvalidateRenderState(true);
}
#endif

bool UGroomComponent::GetIsHairLengthScaleEnabled()
{
	bool IsEnabled = true;
	for (FHairGroupDesc& HairDesc : GroomGroupsDesc)
	{
		if (!HairDesc.HairLengthScale_Override)
		{
			IsEnabled = false;
		}
	}
	return IsEnabled;
}

void AddHairStreamingRequest(FHairGroupInstance* InInstance, int32 InLODIndex);

void UGroomComponent::SetForcedLOD(int32 CurrLODIndex)
{
	// Previous LOD state
	EHairLODSelectionType PrevLODSelectionType = LODSelectionType;
	float PrevLODIndex = -1;
	if (PrevLODSelectionType == EHairLODSelectionType::Forced)			{ PrevLODIndex = LODForcedIndex; }
	else if (PrevLODSelectionType == EHairLODSelectionType::Predicted)	{ PrevLODIndex = LODPredictedIndex; }

	// Current LOD state
	EHairLODSelectionType CurrLODSelectionType = EHairLODSelectionType::Forced;
	if (GroomAsset)
	{
		CurrLODIndex = FMath::Clamp(CurrLODIndex, -1, GroomAsset->GetLODCount() - 1);
	}

	// Reset to non-forced LOD, and change LOD selection type to Predicted/Immediate
	if (CurrLODIndex < 0)
	{
		// If one of the Group/LOD needs simulation, then we switch to predicted LOD to ensure LOD switch happen 
		// on the game-thread so that the simulation component can do work at transition-time
		bool bNeedSimulationNeed = false;
		if (GroomAsset)
		{
			for (int32 GroupIt = 0, GroupCount = GroomAsset->HairGroupsData.Num(); GroupIt < GroupCount; ++GroupIt)
			{
				for (uint32 LODIt = 0, LODCount = GroomAsset->GetLODCount(); LODIt < LODCount; ++LODIt)
				{
					if (IsSimulationEnable(GroupIt, LODIt))
					{
						bNeedSimulationNeed = true;
						break;
					}
				}
			}
		}
		CurrLODSelectionType = bNeedSimulationNeed ? EHairLODSelectionType::Predicted : EHairLODSelectionType::Immediate;
	}

	// Inform simulation about LOD switch.
	SwitchSimulationLOD(PrevLODIndex, CurrLODIndex);

	// Finally, update the forced LOD value and LOD selection type
	const bool bHasLODSwitch = CurrLODIndex != PrevLODIndex;
	LODForcedIndex	 = CurrLODIndex;
	LODSelectionType = CurrLODSelectionType;

	// Add streaming request for the subsequent LOD as prediction, only when going to a finer LOD
	const bool bPredictionLoad = GHairStrands_Streaming_Prediction && bHasLODSwitch && CurrLODIndex > 0 && CurrLODIndex < PrevLODIndex;

	// Do not invalidate completly the proxy, but just update LOD index on the rendering thread	
	if (FHairStrandsSceneProxy* GroomSceneProxy = (FHairStrandsSceneProxy*)SceneProxy)
	{
		const EHairLODSelectionType LocalLODSelectionType = LODSelectionType;
		ENQUEUE_RENDER_COMMAND(FHairComponentSendLODIndex)(
		[GroomSceneProxy, CurrLODIndex, LocalLODSelectionType, bHasLODSwitch, bPredictionLoad](FRHICommandListImmediate& RHICmdList)
		{
			for (FHairGroupInstance* Instance : GroomSceneProxy->HairGroupInstances)
			{
				Instance->Debug.LODForcedIndex = CurrLODIndex;
				Instance->Debug.LODSelectionTypeForDebug = LocalLODSelectionType;

				if (bHasLODSwitch)
				{
					AddHairStreamingRequest(Instance, CurrLODIndex);
				}

				if (bPredictionLoad)
				{
					AddHairStreamingRequest(Instance, CurrLODIndex-1);
				}
			}
		});
	}
}

int32 UGroomComponent::GetNumLODs() const
{
	return GroomAsset ? GroomAsset->GetLODCount() : 0;
}

int32 UGroomComponent::GetForcedLOD() const
{
	return int32(LODForcedIndex);
}

int32 UGroomComponent::GetDesiredSyncLOD() const
{
	return LODPredictedIndex;
}

void  UGroomComponent::SetSyncLOD(int32 InLODIndex)
{
	SetForcedLOD(InLODIndex);
}

int32 UGroomComponent::GetNumSyncLODs() const
{
	return GetNumLODs();
}

int32 UGroomComponent::GetCurrentSyncLOD() const
{
	return GetForcedLOD();
}

void UGroomComponent::SetBinding(UGroomBindingAsset* InBinding)
{
	SetBindingAsset(InBinding);
}

void UGroomComponent::SetBindingAsset(UGroomBindingAsset* InBinding)
{
	if (BindingAsset != InBinding)
	{
		const bool bIsValid = InBinding != nullptr ? UGroomBindingAsset::IsBindingAssetValid(BindingAsset, false, bValidationEnable) : true;
		if (bIsValid && UGroomBindingAsset::IsCompatible(GroomAsset, InBinding, bValidationEnable))
		{
			BindingAsset = InBinding;
			InitResources();
		}
	}
}

void UGroomComponent::SetPhysicsAsset(UPhysicsAsset* InPhysicsAsset)
{
	if (InPhysicsAsset && PhysicsAsset != InPhysicsAsset)
	{
		PhysicsAsset = InPhysicsAsset;
			
		ReleaseResources();
		UpdateHairGroupsDesc();
		InitResources();
		UpdateHairSimulation();
	}
}

void UGroomComponent::AddCollisionComponent(USkeletalMeshComponent* SkeletalMeshComponent)
{
	if(SkeletalMeshComponent && (CollisionComponents.Find(SkeletalMeshComponent) == INDEX_NONE))
	{
		CollisionComponents.Add(SkeletalMeshComponent);
	}
}

void UGroomComponent::ResetCollisionComponents()
{
	CollisionComponents.Reset();
}

void UGroomComponent::SetEnableSimulation(bool bInEnableSimulation)
{
	if (SimulationSettings.bOverrideSettings)
	{
		if (SimulationSettings.SolverSettings.bEnableSimulation != bInEnableSimulation)
		{
			SimulationSettings.SolverSettings.bEnableSimulation = bInEnableSimulation;

			ReleaseResources();
			UpdateHairGroupsDesc();
			InitResources();
			UpdateHairSimulation();
		}
	}
	else
	{
		UE_LOG(LogHairStrands, Warning, TEXT("[Groom] %s - To be able to enable/disable the simulation from component you must enable the overide settings boolean in the simulatin settings."), *GetPathName());
	}
}

void UGroomComponent::ResetSimulation()
{
	bResetSimulation = true;
	bInitSimulation = true;
}

void UGroomComponent::UpdateHairGroupsDescAndInvalidateRenderState(bool bInvalid)
{
	UpdateHairGroupsDesc();

	uint32 GroupIndex = 0;
	for (FHairGroupInstance* Instance : HairGroupInstances)
	{
		Instance->Strands.Modifier  = GetGroomGroupsDesc(GroomAsset, this, GroupIndex);
#if WITH_EDITORONLY_DATA
		Instance->Debug.DebugMode = DebugMode;
		Instance->HairGroupPublicData->DebugMode = DebugMode; // Replicated value for accessing it from the engine side. Refactor this.
#endif// #if WITH_EDITORONLY_DATA
		++GroupIndex;
	}
	if (bInvalid)
	{
		MarkRenderStateDirty();
	}
}

FPrimitiveSceneProxy* UGroomComponent::CreateSceneProxy()
{
	DeleteDeferredHairGroupInstances();

	if (!GroomAsset || GroomAsset->GetNumHairGroups() == 0 || HairGroupInstances.Num() == 0)
		return nullptr;

	bool bIsValid = false;
	for (FHairGroupInstance* Instance : HairGroupInstances)
	{
		bIsValid |= Instance->IsValid();
	}
	if (!bIsValid)
	{
		return nullptr;
	}
	return new FHairStrandsSceneProxy(this);
}


FBoxSphereBounds UGroomComponent::CalcBounds(const FTransform& InLocalToWorld) const
{
	if (GroomAsset && GroomAsset->GetNumHairGroups() > 0)
	{
		FBox LocalHairBound(EForceInit::ForceInitToZero);
		if (!GroomCacheBuffers.IsValid())
		{
			for (const FHairGroupData& GroupData : GroomAsset->HairGroupsData)
			{
				if (IsHairStrandsEnabled(EHairStrandsShaderType::Strands) && GroupData.Strands.HasValidData())
				{
					LocalHairBound += GroupData.Strands.GetBounds();
				}
				else if (IsHairStrandsEnabled(EHairStrandsShaderType::Cards) && GroupData.Cards.HasValidData())
				{
					LocalHairBound += GroupData.Cards.GetBounds();
				}
				else if (IsHairStrandsEnabled(EHairStrandsShaderType::Meshes) && GroupData.Meshes.HasValidData())
				{
					LocalHairBound += GroupData.Meshes.GetBounds();
				}
				else if (GroupData.Guides.HasValidData())
				{
					LocalHairBound += GroupData.Guides.GetBounds();
				}
			}
		}
		else
		{
			FGroomCacheBuffers* Buffers = static_cast<FGroomCacheBuffers*>(GroomCacheBuffers.Get());
			LocalHairBound = Buffers->GetBoundingBox();
		}

		// If the attachment is done onto a skel. mesh, by default (i.e., GHairStrands_BoundsMode == 0):
		// * If the attachment is simple, use the skel. mesh expanded by the groom bounds.
		// * If the attachment is 'relative' to the skel mesh (using a bone anchor, which is provided with AttachmentName), 
		//   we use the simple groom bound.
		// Otherwise:
		// * GHairStrands_BoundsMode=1 : use the skel mesh bounds
		// * GHairStrands_BoundsMode=2 : use the skel mesh bounds + groom bounds. This is more conservative compares to GHairStrands_BoundsMode=0.
		if (RegisteredMeshComponent && GHairStrands_BoundsMode == 1)
		{
			return RegisteredMeshComponent->Bounds;
		}
		else if (RegisteredMeshComponent && GHairStrands_BoundsMode == 2)
		{
			const FVector3d GroomExtends = LocalHairBound.GetExtent();
			const float BoundExtraRadius = 0.5f * static_cast<float>(FMath::Max(0.0, FMath::Max3(GroomExtends.X, GroomExtends.Y, GroomExtends.Z)));
			FBox EffectiveBound = RegisteredMeshComponent->Bounds.GetBox();
			EffectiveBound.Min -= FVector3d(BoundExtraRadius);
			EffectiveBound.Max += FVector3d(BoundExtraRadius);
			return FBoxSphereBounds(EffectiveBound);
		}
		else if (RegisteredMeshComponent && AttachmentName.IsEmpty())
		{
			const FBox LocalSkeletalBound = RegisteredMeshComponent->CalcBounds(FTransform::Identity).GetBox();

			// Compute an extra 'radius' which will be added to the skel. mesh bound. 
			// It is based on the different of the groom bound and the skel. bound at rest position
			float BoundExtraRadius = 0.f;
			if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(RegisteredMeshComponent))
			{
				FBoxSphereBounds RestBounds;
				SkeletalMeshComponent->GetPreSkinnedLocalBounds(RestBounds);
				const FBox RestBox = RestBounds.GetBox();
				const FVector MinDiff = LocalHairBound.Min - RestBox.Min;
				const FVector MaxDiff = LocalHairBound.Max - RestBox.Max;
				BoundExtraRadius = static_cast<float>(FMath::Max3(0.0, FMath::Max3(MinDiff.X, MinDiff.Y, MinDiff.Z), FMath::Max3(MaxDiff.X, MaxDiff.Y, MaxDiff.Z)));
			}
			
			FBox LocalBound(EForceInit::ForceInitToZero);
			LocalBound += LocalSkeletalBound.Min - BoundExtraRadius;
			LocalBound += LocalSkeletalBound.Max + BoundExtraRadius;
			return FBoxSphereBounds(LocalBound.TransformBy(InLocalToWorld));
		}
		else
		{
			return FBoxSphereBounds(LocalHairBound.TransformBy(InLocalToWorld));
		}
	}
	else
	{
		FBoxSphereBounds LocalBounds(EForceInit::ForceInitToZero);
		return FBoxSphereBounds(LocalBounds.TransformBy(InLocalToWorld));
	}
}

/* Return the material slot index corresponding to the material name */
int32 UGroomComponent::GetMaterialIndex(FName MaterialSlotName) const
{
	if (GroomAsset)
	{
		return GroomAsset->GetMaterialIndex(MaterialSlotName);
	}

	return INDEX_NONE;
}

bool UGroomComponent::IsMaterialSlotNameValid(FName MaterialSlotName) const
{
	return GetMaterialIndex(MaterialSlotName) != INDEX_NONE;
}

TArray<FName> UGroomComponent::GetMaterialSlotNames() const
{
	TArray<FName> MaterialNames;
	if (GroomAsset)
	{
		MaterialNames = GroomAsset->GetMaterialSlotNames();
	}

	return MaterialNames;
}

int32 UGroomComponent::GetNumMaterials() const
{
	if (GroomAsset)
	{
		return FMath::Max(GroomAsset->HairGroupsMaterials.Num(), 1);
	}
	return 1;
}

UMaterialInterface* UGroomComponent::GetMaterial(int32 ElementIndex, EHairGeometryType GeometryType, bool bUseDefaultIfIncompatible) const
{
	UMaterialInterface* OverrideMaterial = Super::GetMaterial(ElementIndex);

	bool bUseHairDefaultMaterial = false;

	if (!OverrideMaterial && GroomAsset && ElementIndex < GroomAsset->HairGroupsMaterials.Num())
	{
		if (UMaterialInterface* Material = GroomAsset->HairGroupsMaterials[ElementIndex].Material)
		{
			OverrideMaterial = Material;
		}
		else if (bUseDefaultIfIncompatible)
		{
			bUseHairDefaultMaterial = true;
		}
	}

	if (bUseDefaultIfIncompatible)
	{
		const ERHIFeatureLevel::Type FeatureLevel = GetScene() ? GetScene()->GetFeatureLevel() : ERHIFeatureLevel::Num;
		if (FeatureLevel != ERHIFeatureLevel::Num && IsHairMaterialCompatible(OverrideMaterial, FeatureLevel, GeometryType) != EHairMaterialCompatibility::Valid)
		{
			bUseHairDefaultMaterial = true;
		}
	}

	if (bUseHairDefaultMaterial)
	{
		if (GeometryType == EHairGeometryType::Strands)
		{
			OverrideMaterial = Strands_DefaultMaterial;
		}
		else if (GeometryType == EHairGeometryType::Cards)
		{
			OverrideMaterial = Cards_DefaultMaterial;
		}
		else if (GeometryType == EHairGeometryType::Meshes)
		{
			OverrideMaterial = Meshes_DefaultMaterial;
		}
	}

	return OverrideMaterial;
}

EHairGeometryType UGroomComponent::GetMaterialGeometryType(int32 ElementIndex) const
{
	if (!GroomAsset)
	{
		// If we don't know, enforce strands, as it has the most requirement.
		return EHairGeometryType::Strands;
	}

	const EShaderPlatform Platform = GetScene() ? GetScene()->GetShaderPlatform() : EShaderPlatform::SP_NumPlatforms;
	for (uint32 GroupIt = 0, GroupCount = GroomAsset->HairGroupsRendering.Num(); GroupIt < GroupCount; ++GroupIt)
	{
		// Material - Strands
		const FHairGroupData& InGroupData = GroomAsset->HairGroupsData[GroupIt];
		if (IsHairStrandsEnabled(EHairStrandsShaderType::Strands, Platform))
		{
			const int32 SlotIndex = GroomAsset->GetMaterialIndex(GroomAsset->HairGroupsRendering[GroupIt].MaterialSlotName);
			if (SlotIndex == ElementIndex)
			{
				return EHairGeometryType::Strands;
			}
		}

		// Material - Cards
		if (IsHairStrandsEnabled(EHairStrandsShaderType::Cards, Platform))
		{
			uint32 CardsLODIndex = 0;
			for (const FHairGroupData::FCards::FLOD& LOD : InGroupData.Cards.LODs)
			{
				if (LOD.IsValid())
				{
					// Material
					int32 SlotIndex = INDEX_NONE;
					for (const FHairGroupsCardsSourceDescription& Desc : GroomAsset->HairGroupsCards)
					{
						if (Desc.GroupIndex == GroupIt && Desc.LODIndex == CardsLODIndex)
						{
							SlotIndex = GroomAsset->GetMaterialIndex(Desc.MaterialSlotName);
							break;
						}
					}
					if (SlotIndex == ElementIndex)
					{
						return EHairGeometryType::Cards;
					}
				}
				++CardsLODIndex;
			}
		}

		// Material - Meshes
		if (IsHairStrandsEnabled(EHairStrandsShaderType::Meshes, Platform))
		{
			uint32 MeshesLODIndex = 0;
			for (const FHairGroupData::FMeshes::FLOD& LOD : InGroupData.Meshes.LODs)
			{
				if (LOD.IsValid())
				{
					// Material
					int32 SlotIndex = INDEX_NONE;
					for (const FHairGroupsMeshesSourceDescription& Desc : GroomAsset->HairGroupsMeshes)
					{
						if (Desc.GroupIndex == GroupIt && Desc.LODIndex == MeshesLODIndex)
						{
							SlotIndex = GroomAsset->GetMaterialIndex(Desc.MaterialSlotName);
							break;
						}
					}
					if (SlotIndex == ElementIndex)
					{
						return EHairGeometryType::Meshes;
					}
				}
				++MeshesLODIndex;
			}
		}
	}
	// If we don't know, enforce strands, as it has the most requirement.
	return EHairGeometryType::Strands;
}

UMaterialInterface* UGroomComponent::GetMaterial(int32 ElementIndex) const
{
	const EHairGeometryType GeometryType = GetMaterialGeometryType(ElementIndex);
	return GetMaterial(ElementIndex, GeometryType, true);
}

FHairStrandsRestResource* UGroomComponent::GetGuideStrandsRestResource(uint32 GroupIndex)
{
	if (!GroomAsset || GroupIndex >= uint32(GroomAsset->GetNumHairGroups()))
	{
		return nullptr;
	}

	if (!GroomAsset->HairGroupsData[GroupIndex].Guides.IsValid())
	{
		return nullptr;
	}
	return GroomAsset->HairGroupsData[GroupIndex].Guides.RestResource;
}

FHairStrandsDeformedResource* UGroomComponent::GetGuideStrandsDeformedResource(uint32 GroupIndex)
{
	if (GroupIndex >= uint32(HairGroupInstances.Num()))
	{
		return nullptr;
	}

	if (!HairGroupInstances[GroupIndex]->Guides.IsValid())
	{
		return nullptr;
	}
	return HairGroupInstances[GroupIndex]->Guides.DeformedResource;
}

FHairStrandsRestRootResource* UGroomComponent::GetGuideStrandsRestRootResource(uint32 GroupIndex)
{
	if (GroupIndex >= uint32(HairGroupInstances.Num()))
	{
		return nullptr;
	}

	if (!HairGroupInstances[GroupIndex]->Guides.IsValid())
	{
		return nullptr;
	}
	return HairGroupInstances[GroupIndex]->Guides.RestRootResource;
}

FHairStrandsDeformedRootResource* UGroomComponent::GetGuideStrandsDeformedRootResource(uint32 GroupIndex)
{
	if (GroupIndex >= uint32(HairGroupInstances.Num()))
	{
		return nullptr;
	}

	if (!HairGroupInstances[GroupIndex]->Guides.IsValid())
	{
		return nullptr;
	}
	return HairGroupInstances[GroupIndex]->Guides.DeformedRootResource;
}

void UGroomComponent::UpdateSimulatedGroups()
{
	if (HairGroupInstances.Num()>0)
	{
		const uint32 Id = ComponentId.PrimIDValue;

		const bool bIsStrandsEnabled = IsHairStrandsEnabled(EHairStrandsShaderType::Strands);

		// For now use the force LOD to drive enabling/disabling simulation & RBF
		const int32 LODIndex = GetForcedLOD();

		TArray<FHairGroupInstance*> LocalInstances = HairGroupInstances;
		UGroomAsset* LocalGroomAsset = GroomAsset;
		UGroomBindingAsset* LocalBindingAsset = BindingAsset;
		ENQUEUE_RENDER_COMMAND(FHairStrandsTick_UEnableSimulatedGroups)(
			[LocalInstances, LocalGroomAsset, LocalBindingAsset, Id, bIsStrandsEnabled, LODIndex](FRHICommandListImmediate& RHICmdList)
		{
			int32 GroupIt = 0;
			for (FHairGroupInstance* Instance : LocalInstances)
			{
				Instance->Strands.HairInterpolationType = EHairInterpolationType::NoneSkinning;
				if (bIsStrandsEnabled && LocalGroomAsset)
				{
					Instance->Strands.HairInterpolationType = ToHairInterpolationType(LocalGroomAsset->HairInterpolationType);
				}
				if (Instance->Guides.IsValid())
				{
					check(Instance->HairGroupPublicData);
					Instance->Guides.bIsSimulationEnable	 = Instance->HairGroupPublicData->IsSimulationEnable(LODIndex);
					Instance->Guides.bHasGlobalInterpolation = Instance->HairGroupPublicData->IsGlobalInterpolationEnable(LODIndex);
					Instance->Guides.bIsDeformationEnable = Instance->HairGroupPublicData->bIsDeformationEnable;
				}
				++GroupIt;
			}
		});
	}
}

void UGroomComponent::OnChildDetached(USceneComponent* ChildComponent)
{}

void UGroomComponent::OnChildAttached(USceneComponent* ChildComponent)
{

}

static UGeometryCacheComponent* ValidateBindingAsset(
	UGroomAsset* GroomAsset, 
	UGroomBindingAsset* BindingAsset, 
	UGeometryCacheComponent* GeometryCacheComponent, 
	bool bIsBindingReloading, 
	bool bValidationEnable, 
	const USceneComponent* Component)
{
	if (!GroomAsset || !BindingAsset || !GeometryCacheComponent)
	{
		return nullptr;
	}

	bool bHasValidSectionCount = GeometryCacheComponent && GeometryCacheComponent->GetNumMaterials() < int32(GetHairStrandsMaxSectionCount());

	// Report warning if the section count is larger than the supported count
	if (GeometryCacheComponent && !bHasValidSectionCount)
	{
		FString Name = "";
		if (Component->GetOwner())
		{
			Name += Component->GetOwner()->GetName() + "/";
		}
		Name += Component->GetName() + "/" + GroomAsset->GetName();

		UE_LOG(LogHairStrands, Warning, TEXT("[Groom] %s - Groom is bound to a GeometryCache which has too many sections (%d), which is higher than the maximum supported for hair binding (%d). The groom binding will be disbled on this component."), *Name, GeometryCacheComponent->GetNumMaterials(), GetHairStrandsMaxSectionCount());
	}

	const bool bIsBindingCompatible =
		UGroomBindingAsset::IsCompatible(GeometryCacheComponent ? GeometryCacheComponent->GeometryCache : nullptr, BindingAsset, bValidationEnable) &&
		UGroomBindingAsset::IsCompatible(GroomAsset, BindingAsset, bValidationEnable) &&
		UGroomBindingAsset::IsBindingAssetValid(BindingAsset, bIsBindingReloading, bValidationEnable);

	return bIsBindingCompatible ? GeometryCacheComponent : nullptr;
}

// Return a non-null skeletal mesh Component if the binding asset is compatible with the current component
static USkeletalMeshComponent* ValidateBindingAsset(
	UGroomAsset* GroomAsset,
	UGroomBindingAsset* BindingAsset,
	USkeletalMeshComponent* SkeletalMeshComponent,
	bool bIsBindingReloading,
	bool bValidationEnable,
	const USceneComponent* Component)
{
	if (!GroomAsset || !BindingAsset || !SkeletalMeshComponent)
	{
		return nullptr;
	}

	// Optional advanced check
	bool bHasValidSectionCount = SkeletalMeshComponent && SkeletalMeshComponent->GetNumMaterials() < int32(GetHairStrandsMaxSectionCount());
	if (SkeletalMeshComponent && SkeletalMeshComponent->GetSkeletalMeshAsset() && SkeletalMeshComponent->GetSkeletalMeshAsset()->GetResourceForRendering())
	{
		const FSkeletalMeshRenderData* RenderData = SkeletalMeshComponent->GetSkeletalMeshAsset()->GetResourceForRendering();
		const int32 MaxSectionCount = GetHairStrandsMaxSectionCount();
		// Check that all LOD are below the number sections
		const uint32 MeshLODCount = RenderData->LODRenderData.Num();
		for (uint32 LODIt = 0; LODIt < MeshLODCount; ++LODIt)
		{
			if (RenderData->LODRenderData[LODIt].RenderSections.Num() >= MaxSectionCount)
			{
				bHasValidSectionCount = false;
			}
		}
	}

	// Report warning if the skeletal section count is larger than the supported count
	if (SkeletalMeshComponent && !bHasValidSectionCount)
	{
		FString Name = "";
		if (Component->GetOwner())
		{
			Name += Component->GetOwner()->GetName() + "/";
		}
		Name += Component->GetName() + "/" + GroomAsset->GetName();

		UE_LOG(LogHairStrands, Warning, TEXT("[Groom] %s - Groom is bound to a skeletal mesh which has too many sections (%d), which is higher than the maximum supported for hair binding (%d). The groom binding will be disbled on this component."), *Name, SkeletalMeshComponent->GetNumMaterials(), GetHairStrandsMaxSectionCount());
	}

	const bool bIsBindingCompatible =
		UGroomBindingAsset::IsCompatible(SkeletalMeshComponent ? SkeletalMeshComponent->GetSkeletalMeshAsset() : nullptr, BindingAsset, bValidationEnable) &&
		UGroomBindingAsset::IsCompatible(GroomAsset, BindingAsset, bValidationEnable) &&
		UGroomBindingAsset::IsBindingAssetValid(BindingAsset, bIsBindingReloading, bValidationEnable);

	if (!bIsBindingCompatible)
	{
		return nullptr;
	}

	// Validate against cards data
	for (int32 GroupIt = 0, GroupCount = GroomAsset->HairGroupsData.Num(); GroupIt < GroupCount; ++GroupIt)
	{
		FHairGroupData& GroupData = GroomAsset->HairGroupsData[GroupIt];
		uint32 CardsLODIndex = 0;
		for (FHairGroupData::FCards::FLOD& LOD : GroupData.Cards.LODs)
		{
			if (LOD.IsValid())
			{
				const bool bIsCardsBindingCompatible =
					bIsBindingCompatible &&
					BindingAsset &&
					GroupIt < BindingAsset->HairGroupResources.Num() &&
					CardsLODIndex < uint32(BindingAsset->HairGroupResources[GroupIt].CardsRootResources.Num()) &&
					BindingAsset->HairGroupResources[GroupIt].CardsRootResources[CardsLODIndex] != nullptr &&
					((SkeletalMeshComponent && SkeletalMeshComponent->GetSkeletalMeshAsset()) ? SkeletalMeshComponent->GetSkeletalMeshAsset()->GetLODInfoArray().Num() == BindingAsset->HairGroupResources[GroupIt].CardsRootResources[CardsLODIndex]->BulkData.MeshProjectionLODs.Num() : false);

				if (!bIsCardsBindingCompatible)
				{
					return nullptr;
				}
			}
			CardsLODIndex++;
		}
	}

	return SkeletalMeshComponent;
}

static UMeshComponent* ValidateBindingAsset(
	UGroomAsset* GroomAsset,
	UGroomBindingAsset* BindingAsset,
	UMeshComponent* MeshComponent,
	bool bIsBindingReloading,
	bool bValidationEnable,
	const USceneComponent* Component)
{
	if (!GroomAsset || !BindingAsset || !MeshComponent)
	{
		return nullptr;
	}

	if (BindingAsset->GroomBindingType == EGroomBindingMeshType::SkeletalMesh)
	{
		return ValidateBindingAsset(GroomAsset, BindingAsset, Cast<USkeletalMeshComponent>(MeshComponent), bIsBindingReloading, bValidationEnable, Component);
	}
	return ValidateBindingAsset(GroomAsset, BindingAsset, Cast<UGeometryCacheComponent>(MeshComponent), bIsBindingReloading, bValidationEnable, Component);
}

static EGroomGeometryType GetEffectiveGeometryType(EGroomGeometryType Type, bool bUseCards)
{
	return Type == EGroomGeometryType::Strands && (!IsHairStrandsEnabled(EHairStrandsShaderType::Strands) || bUseCards) ? EGroomGeometryType::Cards : Type;
}

void UGroomComponent::InitResources(bool bIsBindingReloading)
{
	LLM_SCOPE_BYTAG(Groom);

	ReleaseResources();
	bInitSimulation = true;
	bResetSimulation = true;

	UpdateHairGroupsDesc();

	if (!GroomAsset || GroomAsset->GetNumHairGroups() == 0)
	{
		return;
	}

	InitializedResources = GroomAsset;

	// 1. Check if we need any kind of binding data, simulation data, or RBF data
	//
	//					  Requires    	| Requires     | Requires
	//         Features | Skeletal mesh	| Binding data | Guides
	// -----------------|---------------|--------------|-------------
	//    Rigid binding | No			| No		   | No
	// Skinning binding | Yes			| Yes		   | No
	//       Simulation | No			| No		   | Yes
	//              RBF | Yes		    | Yes		   | Yes
	//
	bool bHasNeedSkinningBinding = false;			   
	bool bHasNeedSkeletalMesh = false;
	TArray<bool> bHasNeedGlobalDeformation;
	TArray<bool> bHasNeedSimulation;
	TArray<bool> bHasNeedDeformation;
	bHasNeedGlobalDeformation.Init(false, GroomAsset->HairGroupsData.Num());
	bHasNeedSimulation.Init(false, GroomAsset->HairGroupsData.Num());
	bHasNeedDeformation.Init(false, GroomAsset->HairGroupsData.Num());
	
	bool bHasAnyNeedSimulation = false;
	bool bHasAnyNeedDeformation = false;
	bool bHasAnyNeedGlobalDeformation = false;
	for (int32 GroupIt = 0, GroupCount = GroomAsset->HairGroupsData.Num(); GroupIt < GroupCount; ++GroupIt)
	{
		for (uint32 LODIt = 0, LODCount = GroomAsset->GetLODCount(); LODIt < LODCount; ++LODIt)
		{
			const EGroomGeometryType GeometryType = GetEffectiveGeometryType(GroomAsset->GetGeometryType(GroupIt, LODIt), bUseCards);
			const EGroomBindingType BindingType = GroomAsset->GetBindingType(GroupIt, LODIt);

			// Note on Global Deformation:
			// * Global deformation require to have skinning binding
			// * Force global interpolation to be enable for meshes with skinning binding as we use RBF defomation for 'sticking' meshes onto skel. mesh surface			
			bHasNeedSkeletalMesh				= bHasNeedSkeletalMesh || BindingType == EGroomBindingType::Rigid;
			bHasNeedSkinningBinding				= bHasNeedSkinningBinding || BindingType == EGroomBindingType::Skinning;
			bHasNeedSimulation[GroupIt]			= bHasNeedSimulation[GroupIt] || IsSimulationEnable(GroupIt, LODIt);
			bHasNeedGlobalDeformation[GroupIt]	= bHasNeedGlobalDeformation[GroupIt] || (BindingType == EGroomBindingType::Skinning && GroomAsset->IsGlobalInterpolationEnable(GroupIt, LODIt));

			bHasAnyNeedSimulation				= bHasAnyNeedSimulation			|| bHasNeedSimulation[GroupIt];
			bHasAnyNeedGlobalDeformation		= bHasAnyNeedGlobalDeformation	|| bHasNeedGlobalDeformation[GroupIt];
		}
		bHasNeedDeformation[GroupIt] = GroomAsset->IsDeformationEnable(GroupIt);
		bHasAnyNeedDeformation = bHasAnyNeedDeformation || bHasNeedDeformation[GroupIt];
	}
	const bool bHasNeedBindingData = bHasNeedSkinningBinding || bHasAnyNeedGlobalDeformation;

	// LOD Selection type
	// By default set to immediate, meaning the LOD selection will be done on the rendering thread based on the screen coverage of the groom
	// If simulation is enabled, then the LOD selection will be done on the game thread based on feedback from the rendering thread.
	// If LODForcedIndex is >= 0 it means it has been set by a previous SetForcedLOD call. LODSelectionType is set to Forced in such a case, to applied this 'cached' value
	if (LODForcedIndex >= 0)
	{
		LODSelectionType = EHairLODSelectionType::Forced;
	}
	else
	{
		LODSelectionType = (bHasAnyNeedSimulation || bHasAnyNeedDeformation) ? EHairLODSelectionType::Predicted : EHairLODSelectionType::Immediate;
	}

	// 2. Insure that the binding asset is compatible, otherwise no binding
	UMeshComponent* ParentMeshComponent = GetAttachParent() ? Cast<UMeshComponent>(GetAttachParent()) : nullptr;
	UMeshComponent* ValidatedMeshComponent = nullptr;
	if (bHasNeedBindingData && ParentMeshComponent)
	{
		ValidatedMeshComponent = ValidateBindingAsset(GroomAsset, BindingAsset, ParentMeshComponent, bIsBindingReloading, bValidationEnable, this);
		if (ValidatedMeshComponent)
		{
			if (BindingAsset && 
				((BindingAsset->GroomBindingType == EGroomBindingMeshType::SkeletalMesh && Cast<USkeletalMeshComponent>(ValidatedMeshComponent)->GetSkeletalMeshAsset() == nullptr) ||
				(BindingAsset->GroomBindingType == EGroomBindingMeshType::GeometryCache && Cast<UGeometryCacheComponent>(ValidatedMeshComponent)->GeometryCache == nullptr)))
			{
				ValidatedMeshComponent = nullptr;
			}
		}
	}
	else if (bHasNeedSkeletalMesh && ParentMeshComponent)
	{
		if (USkeletalMeshComponent* ParentSkelMeshComponent = Cast<USkeletalMeshComponent>(ParentMeshComponent))
		{
			ValidatedMeshComponent = ParentSkelMeshComponent->GetSkeletalMeshAsset() ? ParentMeshComponent : nullptr;
		}
	}

	// Grab deformed mesh if it exists 
	if(GroomAsset && GroomAsset->RiggedSkeletalMesh)
	{
		const USkeleton* TargetSkeleton = GroomAsset->RiggedSkeletalMesh->GetSkeleton();
		
		DeformedMeshComponent = nullptr;
		// Try to find the first component by walking the attachment hierarchy
		for (USceneComponent* Curr = this; Curr; Curr = Curr->GetAttachParent())
		{
			USkeletalMeshComponent* SkelMeshComp = Cast<USkeletalMeshComponent>(Curr);
			if (SkelMeshComp && SkelMeshComp->GetSkeletalMeshAsset() && SkelMeshComp->GetSkeletalMeshAsset()->GetSkeleton() == TargetSkeleton)
			{
				DeformedMeshComponent = SkelMeshComp;
				break;
			}
		}
		if (DeformedMeshComponent)
		{
			AddTickPrerequisiteComponent(DeformedMeshComponent);
		}
	}

	// Validate that if we are bound to a skel. mesh, and we have binding, and the binding type is set to skinning, that the skin cache is enabled for these
	if (bHasNeedSkinningBinding && ParentMeshComponent)
	{
		if (USkeletalMeshComponent* ParentSkelMeshComponent = Cast<USkeletalMeshComponent>(ParentMeshComponent))
		{
			if (BindingAsset)
			{
				const uint32 SkelLODCount = ParentSkelMeshComponent->GetNumLODs();
				for (int32 GroupIt = 0, GroupCount = GroomAsset->HairGroupsData.Num(); GroupIt < GroupCount; ++GroupIt)
				{
					for (uint32 LODIt = 0, LODCount = GroomAsset->GetLODCount(); LODIt < LODCount; ++LODIt)
					{
						const uint32 EffectiveLODIt = FMath::Clamp<uint32>(LODIt, 0, SkelLODCount - 1);
						const bool bSupportSkinCache = ParentSkelMeshComponent->IsSkinCacheAllowed(EffectiveLODIt);
						const EGroomBindingType BindingType = GroomAsset->GetBindingType(GroupIt, LODIt);
						const bool bIsVisible = GroomAsset->IsVisible(GroupIt, LODIt);

						if (BindingType == EGroomBindingType::Skinning && !bSupportSkinCache && bIsVisible)
						{
							UE_LOG(LogHairStrands, Warning, TEXT("[Groom] Groom asset (Group:%d/%d) is set to use Skinning at LOD %d/%d while the parent skel. mesh does not support skin. cache at this LOD - Groom:%s - Skel.Mesh:%s"),
								GroupIt,
								GroupCount,
								LODIt,
								LODCount,
								*GroomAsset->GetPathName(),
								*ParentMeshComponent->GetPathName());
						}
					}
				}
			}
		}
	}

	// Insure the ticking of the Groom component always happens after the skeletalMeshComponent.
	UGroomBindingAsset* LocalBindingAsset = nullptr;
	if (ValidatedMeshComponent)
	{
		RegisteredMeshComponent = ValidatedMeshComponent;
		AddTickPrerequisiteComponent(ValidatedMeshComponent);
		bUseAttachParentBound = false;
		if (bHasNeedBindingData)
		{
			LocalBindingAsset = BindingAsset;
			if (LocalBindingAsset && LocalBindingAsset->HairGroupResources.Num() != GroomAsset->HairGroupsData.Num())
			{
				LocalBindingAsset = nullptr;
			}
		}
	}

	// Update requirement based on binding asset availability
	if (LocalBindingAsset == nullptr)
	{
		bHasNeedSkinningBinding = false;
		bHasAnyNeedGlobalDeformation = false;
		bHasNeedGlobalDeformation.Init(false, GroomAsset->HairGroupsData.Num());
	}

	if (GroomCache)
	{
		GroomCacheBuffers = GUseGroomCacheStreaming ? MakeShared<FGroomCacheStreamedBuffers, ESPMode::ThreadSafe>(GroomCache) : MakeShared<FGroomCacheBuffers, ESPMode::ThreadSafe>(GroomCache);
		UpdateGroomCache(ElapsedTime);
	}
	else
	{
		GroomCacheBuffers.Reset();
	}

	for (int32 GroupIt = 0, GroupCount = GroomAsset->HairGroupsData.Num(); GroupIt < GroupCount; ++GroupIt)
	{
		FHairGroupInstance* HairGroupInstance = new FHairGroupInstance();
		HairGroupInstance->AddRef();
		HairGroupInstances.Add(HairGroupInstance);
		HairGroupInstance->Debug.ComponentId = ComponentId.PrimIDValue;
		HairGroupInstance->Debug.GroupIndex = GroupIt;
		HairGroupInstance->Debug.GroupCount = GroupCount;
		HairGroupInstance->Debug.GroomAssetName = GroomAsset->GetName();
		HairGroupInstance->Debug.MeshComponent = RegisteredMeshComponent;
		HairGroupInstance->Debug.GroomComponentForDebug = this;
		HairGroupInstance->Debug.GroomBindingType = BindingAsset ? BindingAsset->GroomBindingType : EGroomBindingMeshType::SkeletalMesh;
		HairGroupInstance->Debug.GroomCacheType = GroomCache ? GroomCache->GetType() : EGroomCacheType::None;
		HairGroupInstance->Debug.GroomCacheBuffers = GroomCacheBuffers;
		HairGroupInstance->Debug.LODForcedIndex = LODForcedIndex;
		HairGroupInstance->Debug.LODPredictedIndex = LODPredictedIndex;
		HairGroupInstance->Debug.LODSelectionTypeForDebug = LODSelectionType;
		HairGroupInstance->DeformedComponent = DeformedMeshComponent;
		HairGroupInstance->DeformedSection = GroomAsset->DeformedGroupSections.IsValidIndex(GroupIt) ? GroomAsset->DeformedGroupSections[GroupIt] : INDEX_NONE; 

		if (RegisteredMeshComponent)
		{
			HairGroupInstance->Debug.MeshComponentName = RegisteredMeshComponent->GetPathName();
		}
		HairGroupInstance->GeometryType = EHairGeometryType::NoneGeometry;
		HairGroupInstance->BindingType = EHairBindingType::NoneBinding;

		HairGroupInstance->Debug.SkinningCurrentLocalToWorld	= RegisteredMeshComponent ? RegisteredMeshComponent->GetComponentTransform() : FTransform::Identity;
		HairGroupInstance->Debug.SkinningPreviousLocalToWorld	= HairGroupInstance->Debug.SkinningCurrentLocalToWorld;
		HairGroupInstance->Debug.RigidCurrentLocalToWorld		= GetComponentTransform();
		HairGroupInstance->Debug.RigidPreviousLocalToWorld		= HairGroupInstance->Debug.RigidCurrentLocalToWorld;
		HairGroupInstance->LocalToWorld							= HairGroupInstance->GetCurrentLocalToWorld();

		FHairGroupData& GroupData = GroomAsset->HairGroupsData[GroupIt];

		const EHairInterpolationType HairInterpolationType = ToHairInterpolationType(GroomAsset->HairInterpolationType);

		// Initialize LOD screen size & visibility
		bool bNeedStrandsData = false;
		{
			HairGroupInstance->HairGroupPublicData = new FHairGroupPublicData(GroupIt);
			HairGroupInstance->HairGroupPublicData->Instance = HairGroupInstance;
			HairGroupInstance->HairGroupPublicData->bDebugDrawLODInfo = bPreviewMode;
			HairGroupInstance->HairGroupPublicData->DebugScreenSize = 0.f;
			HairGroupInstance->HairGroupPublicData->DebugGroupColor = GetHairGroupDebugColor(GroupIt);
			TArray<float> CPULODScreenSize;
			TArray<bool> LODVisibility;
			TArray<EHairGeometryType> LODGeometryTypes;
			const FHairGroupsLOD& GroupLOD = GroomAsset->HairGroupsLOD[GroupIt];
			for (uint32 LODIt = 0, LODCount = GroupLOD.LODs.Num(); LODIt < LODCount; ++LODIt)
			{
				const FHairLODSettings& LODSettings = GroupLOD.LODs[LODIt];
				EHairBindingType BindingType = ToHairBindingType(GroomAsset->GetBindingType(GroupIt, LODIt));
				if (BindingType == EHairBindingType::Skinning && !LocalBindingAsset)
				{
					BindingType = EHairBindingType::NoneBinding;
				}
				else if (BindingType == EHairBindingType::Rigid && !RegisteredMeshComponent)
				{
					BindingType = EHairBindingType::NoneBinding;
				}

				// * Force global interpolation to be enable for meshes with skinning binding as we use RBF defomation for 'sticking' meshes onto skel. mesh surface
				// * Global deformation are allowed only with 'Skinning' binding type
				const EHairGeometryType GeometryType = ToHairGeometryType(GetEffectiveGeometryType(GroomAsset->GetGeometryType(GroupIt, LODIt), bUseCards));
				const bool LODSimulation = IsSimulationEnable(GroupIt, LODIt);
				const bool LODGlobalInterpolation = LocalBindingAsset && BindingType == EHairBindingType::Skinning && GroomAsset->IsGlobalInterpolationEnable(GroupIt, LODIt);
				bNeedStrandsData = bNeedStrandsData || GeometryType == EHairGeometryType::Strands;

				CPULODScreenSize.Add(LODSettings.ScreenSize);
				LODVisibility.Add(LODSettings.bVisible);
				LODGeometryTypes.Add(GeometryType);
				HairGroupInstance->HairGroupPublicData->BindingTypes.Add(BindingType);
				HairGroupInstance->HairGroupPublicData->LODSimulations.Add(LODSimulation);
				HairGroupInstance->HairGroupPublicData->LODGlobalInterpolations.Add(LODGlobalInterpolation);
			}
			HairGroupInstance->HairGroupPublicData->bIsDeformationEnable = GroomAsset->IsDeformationEnable(GroupIt);
			HairGroupInstance->HairGroupPublicData->SetLODScreenSizes(CPULODScreenSize);
			HairGroupInstance->HairGroupPublicData->SetLODVisibilities(LODVisibility);
			HairGroupInstance->HairGroupPublicData->SetLODGeometryTypes(LODGeometryTypes);
		}

		FHairResourceName ResourceName(GetFName(), GroupIt);

		// Not needed anymore
		//const bool bDynamicResources = IsHairStrandsSimulationEnable() || IsHairStrandsBindingEnable();

		// Sim data
		// Simulation guides are used for two purposes:
		// * Physics simulation
		// * RBF deformation.
		// Therefore, even if simulation is disabled, we need to run partially the update if the binding system is enabled (skin deformation + RBF correction)
		const bool bNeedGuides = (GroupData.Guides.HasValidData() && (bHasNeedSimulation[GroupIt] || bHasNeedGlobalDeformation[GroupIt] || bHasNeedDeformation[GroupIt])) || (HairGroupInstance->Debug.GroomCacheType == EGroomCacheType::Guides);
		if (bNeedGuides)
		{
			HairGroupInstance->Guides.Data = &GroupData.Guides.BulkData;

			if (LocalBindingAsset)
			{
				check(RegisteredMeshComponent);
				check(LocalBindingAsset);
				check(GroupIt < LocalBindingAsset->HairGroupResources.Num());
				if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(RegisteredMeshComponent))
				{
					check(SkeletalMeshComponent->GetSkeletalMeshAsset() ? SkeletalMeshComponent->GetSkeletalMeshAsset()->GetLODInfoArray().Num() == LocalBindingAsset->HairGroupResources[GroupIt].SimRootResources->BulkData.MeshProjectionLODs.Num() : false);
				}

				HairGroupInstance->Guides.RestRootResource = LocalBindingAsset->HairGroupResources[GroupIt].SimRootResources;
				HairGroupInstance->Guides.DeformedRootResource = new FHairStrandsDeformedRootResource(HairGroupInstance->Guides.RestRootResource, EHairStrandsResourcesType::Guides, ResourceName);
			}

			// Lazy allocation of the guide resources
			HairGroupInstance->Guides.RestResource = GroomAsset->AllocateGuidesResources(GroupIt);
			check(GroupData.Guides.RestResource);

			// If guides are allocated, deformed resources are always needs since they are either used with simulation, or RBF deformation. Both are dynamics, and require deformed positions
			HairGroupInstance->Guides.DeformedResource = new FHairStrandsDeformedResource(GroupData.Guides.BulkData, EHairStrandsResourcesType::Guides, ResourceName);

			// Initialize the simulation and the global deformation to its default behavior by setting it with LODIndex = -1
			const int32 LODIndex = -1;
			HairGroupInstance->Guides.bIsSimulationEnable = IsSimulationEnable(GroupIt,LODIndex);
			HairGroupInstance->Guides.bHasGlobalInterpolation = LocalBindingAsset && GroomAsset->IsGlobalInterpolationEnable(GroupIt,LODIndex);
			HairGroupInstance->Guides.bIsDeformationEnable = GroomAsset->IsDeformationEnable(GroupIt);
		}

		// LODBias is in the Modifier which is needed for LOD selection regardless if the strands are there or not
		HairGroupInstance->Strands.Modifier = GetGroomGroupsDesc(GroomAsset, this, GroupIt);
		#if WITH_EDITORONLY_DATA
		HairGroupInstance->Debug.DebugMode = DebugMode;
		#endif// #if WITH_EDITORONLY_DATA

		// Strands data/resources
		if (bNeedStrandsData && GroupData.Strands.IsValid())
		{
			check(GroupIt < GroomGroupsDesc.Num());

			HairGroupInstance->Strands.Data = &GroupData.Strands.BulkData;

			// (Lazy) Allocate interpolation resources, only if guides are required
			if (bNeedGuides)
			{
				HairGroupInstance->Strands.InterpolationResource = GroomAsset->AllocateInterpolationResources(GroupIt);
				check(HairGroupInstance->Strands.InterpolationResource);
			}

			// Check if strands needs to load/allocate root resources
			bool bNeedRootResources = false;
			bool bNeedDynamicResources = false;
			{
				const FHairGroupsLOD& GroupLOD = GroomAsset->HairGroupsLOD[GroupIt];
				for (uint32 LODIt = 0, LODCount = GroupLOD.LODs.Num(); LODIt < LODCount; ++LODIt)
				{
					if (HairGroupInstance->HairGroupPublicData->GetGeometryType(LODIt) == EHairGeometryType::Strands)
					{
						if (LocalBindingAsset &&
							(HairGroupInstance->HairGroupPublicData->GetBindingType(LODIt) == EHairBindingType::Skinning ||
							 HairGroupInstance->HairGroupPublicData->IsGlobalInterpolationEnable(LODIt)))
						{
							bNeedRootResources = true;
						}

						if ((LocalBindingAsset && HairGroupInstance->HairGroupPublicData->GetBindingType(LODIt) == EHairBindingType::Skinning) ||
							HairGroupInstance->HairGroupPublicData->IsSimulationEnable(LODIt))
						{
							bNeedDynamicResources = true;
						}
						
					}
				}

				// If length scale override is enabled, the group will potentially be scaled, which will modified the strands position. 
				// For this reason we need to allocate deformed resources
				if (GroomGroupsDesc[GroupIt].HairLengthScale_Override)
				{
					bNeedDynamicResources = true;
				}

				// The strands GroomCache needs the Strands.DeformedResource, but the guides GroomCache also needs it for the HairTangentPass
				if (HairGroupInstance->Debug.GroomCacheType != EGroomCacheType::None)
				{
					bNeedDynamicResources = true;
				}
				// the bones deformation needs to setup the deformed resources
				if(HairGroupInstance->HairGroupPublicData->bIsDeformationEnable)
				{
					bNeedDynamicResources = true;
				}
			}

			#if RHI_RAYTRACING
			if (IsHairRayTracingEnabled() && HairGroupInstance->Strands.Modifier.bUseHairRaytracingGeometry && bVisibleInRayTracing)
			{
				HairGroupInstance->Strands.ViewRayTracingMask = EHairViewRayTracingMask::RayTracing | EHairViewRayTracingMask::PathTracing;
				if (bNeedDynamicResources)
				{
					// Allocate dynamic raytracing resources (owned by the groom component/instance)
					HairGroupInstance->Strands.RenRaytracingResource = new FHairStrandsRaytracingResource(GroupData.Strands.BulkData, ResourceName);
					HairGroupInstance->Strands.RenRaytracingResourceOwned = true;
				}
				else
				{
					// (Lazy) Allocate static raytracing resources (owned by the grooom asset)
					HairGroupInstance->Strands.RenRaytracingResourceOwned = false;
					HairGroupInstance->Strands.RenRaytracingResource = GroomAsset->AllocateStrandsRaytracingResources(GroupIt);
					check(HairGroupInstance->Strands.RenRaytracingResource);
				}
			}
			#endif

			if (bNeedRootResources)
			{
				check(LocalBindingAsset);
				check(RegisteredMeshComponent);
				check(GroupIt < LocalBindingAsset->HairGroupResources.Num());
				if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(RegisteredMeshComponent))
				{
					check(SkeletalMeshComponent->GetSkeletalMeshAsset() ? SkeletalMeshComponent->GetSkeletalMeshAsset()->GetLODInfoArray().Num() == LocalBindingAsset->HairGroupResources[GroupIt].RenRootResources->BulkData.MeshProjectionLODs.Num() : false);
				}

				HairGroupInstance->Strands.RestRootResource = LocalBindingAsset->HairGroupResources[GroupIt].RenRootResources;
				HairGroupInstance->Strands.DeformedRootResource = new FHairStrandsDeformedRootResource(HairGroupInstance->Strands.RestRootResource, EHairStrandsResourcesType::Strands, ResourceName);
			}

			HairGroupInstance->Strands.RestResource = GroupData.Strands.RestResource;
			if (bNeedDynamicResources)
			{
				HairGroupInstance->Strands.DeformedResource = new FHairStrandsDeformedResource(GroupData.Strands.BulkData, EHairStrandsResourcesType::Strands, ResourceName);
			} 

			// An empty groom doesn't have a ClusterCullingResource
			HairGroupInstance->Strands.ClusterCullingResource = GroupData.Strands.ClusterCullingResource;
			if (HairGroupInstance->Strands.ClusterCullingResource)
			{
				// This codes assumes strands LOD are contigus and the highest (i.e., 0...x). Change this code to something more robust
				check(HairGroupInstance->HairGroupPublicData);
				const int32 StrandsLODCount = GroupData.Strands.ClusterCullingResource->BulkData.CPULODScreenSize.Num();
				const TArray<float>& LODScreenSizes = HairGroupInstance->HairGroupPublicData->GetLODScreenSizes();
				const TArray<bool>& LODVisibilities = HairGroupInstance->HairGroupPublicData->GetLODVisibilities();
				check(StrandsLODCount <= LODScreenSizes.Num());
				check(StrandsLODCount <= LODVisibilities.Num());
				for (int32 LODIt = 0; LODIt < StrandsLODCount; ++LODIt)
				{
					// ClusterCullingData seriazlizes only the screen size related to strands geometry.
					// Other type of geometry are not serizalized, and so won't match
					if (GroomAsset->HairGroupsLOD[GroupIt].LODs[LODIt].GeometryType == EGroomGeometryType::Strands)
					{
						check(GroupData.Strands.ClusterCullingResource->BulkData.CPULODScreenSize[LODIt] == LODScreenSizes[LODIt]);
						check(GroupData.Strands.ClusterCullingResource->BulkData.LODVisibility[LODIt] == LODVisibilities[LODIt]);
					}
				}
				HairGroupInstance->HairGroupPublicData->SetClusters(HairGroupInstance->Strands.ClusterCullingResource->BulkData.ClusterCount, GroupData.Strands.BulkData.GetNumPoints());
				BeginInitResource(HairGroupInstance->HairGroupPublicData);
			}


			HairGroupInstance->Strands.HairInterpolationType = HairInterpolationType;
		}

		// Cards resources
		for (FHairGroupData::FCards::FLOD& LOD : GroupData.Cards.LODs)
		{
			const uint32 CardsLODIndex = HairGroupInstance->Cards.LODs.Num();
			FHairGroupInstance::FCards::FLOD& InstanceLOD = HairGroupInstance->Cards.LODs.AddDefaulted_GetRef();
			if (LOD.IsValid())
			{
				FHairResourceName LODResourceName(GetFName(), GroupIt, CardsLODIndex);

				const EHairBindingType BindingType	= HairGroupInstance->HairGroupPublicData->GetBindingType(CardsLODIndex);
				const bool bHasSimulation			= HairGroupInstance->HairGroupPublicData->IsSimulationEnable(CardsLODIndex);
				const bool bHasDeformation			= HairGroupInstance->HairGroupPublicData->bIsDeformationEnable;
				const bool bHasGlobalDeformation	= HairGroupInstance->HairGroupPublicData->IsGlobalInterpolationEnable(CardsLODIndex);
				const bool bNeedDeformedPositions	= bHasSimulation || bHasDeformation || bHasGlobalDeformation || BindingType == EHairBindingType::Skinning;
				const bool bNeedRootData			= bHasGlobalDeformation || BindingType == EHairBindingType::Skinning;

				// Sanity check
				if (bHasGlobalDeformation)
				{
					check(BindingType == EHairBindingType::Skinning);
				}

				InstanceLOD.Data = &LOD.BulkData;
				InstanceLOD.RestResource = LOD.RestResource;
				InstanceLOD.InterpolationResource = LOD.InterpolationResource;

				if (bNeedDeformedPositions)
				{
					InstanceLOD.DeformedResource = new FHairCardsDeformedResource(LOD.BulkData, false, LODResourceName);
				}

				#if RHI_RAYTRACING
				if (IsRayTracingEnabled() && bVisibleInRayTracing)
				{
					if (bNeedDeformedPositions)
					{
						// Allocate dynamic raytracing resources (owned by the groom component/instance)
						InstanceLOD.RaytracingResource = new FHairStrandsRaytracingResource(*InstanceLOD.Data, LODResourceName);
						InstanceLOD.RaytracingResourceOwned = true;
					}
					else
					{
						// (Lazy) Allocate static raytracing resources (owned by the grooom asset)
						InstanceLOD.RaytracingResourceOwned = false;
						InstanceLOD.RaytracingResource = GroomAsset->AllocateCardsRaytracingResources(GroupIt, CardsLODIndex);
						check(InstanceLOD.RaytracingResource);
					}
				}
				#endif

				// Strands data/resources
				if (bNeedDeformedPositions)
				{
					InstanceLOD.Guides.Data = &LOD.Guides.BulkData;
					InstanceLOD.Guides.InterpolationResource = LOD.Guides.InterpolationResource;

					if (bNeedRootData)
					{
						check(LocalBindingAsset);
						check(GroupIt < LocalBindingAsset->HairGroupResources.Num());
						if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(RegisteredMeshComponent))
						{
							check(SkeletalMeshComponent->GetSkeletalMeshAsset() ? SkeletalMeshComponent->GetSkeletalMeshAsset()->GetLODInfoArray().Num() == LocalBindingAsset->HairGroupResources[GroupIt].CardsRootResources[CardsLODIndex]->BulkData.MeshProjectionLODs.Num() : false);
						}

						InstanceLOD.Guides.RestRootResource = LocalBindingAsset->HairGroupResources[GroupIt].CardsRootResources[CardsLODIndex];
						InstanceLOD.Guides.DeformedRootResource = new FHairStrandsDeformedRootResource(InstanceLOD.Guides.RestRootResource, EHairStrandsResourcesType::Cards, LODResourceName);
					}

					InstanceLOD.Guides.RestResource = LOD.Guides.RestResource;
					{
						InstanceLOD.Guides.DeformedResource = new FHairStrandsDeformedResource(LOD.Guides.BulkData, EHairStrandsResourcesType::Cards, LODResourceName);
					}

					InstanceLOD.Guides.HairInterpolationType = HairInterpolationType;
				}
			}
		}

		// Meshes resources
		for (FHairGroupData::FMeshes::FLOD& LOD : GroupData.Meshes.LODs)
		{
			const int32 MeshLODIndex = HairGroupInstance->Meshes.LODs.Num();
			FHairGroupInstance::FMeshes::FLOD& InstanceLOD = HairGroupInstance->Meshes.LODs.AddDefaulted_GetRef();
			if (LOD.IsValid())
			{
				FHairResourceName LODResourceName(GetFName(), GroupIt, MeshLODIndex);

				const EHairBindingType BindingType = HairGroupInstance->HairGroupPublicData->GetBindingType(MeshLODIndex);
				const bool bHasGlobalDeformation   = HairGroupInstance->HairGroupPublicData->IsGlobalInterpolationEnable(MeshLODIndex);
				const bool bNeedDeformedPositions  = bHasGlobalDeformation && BindingType == EHairBindingType::Skinning;

				InstanceLOD.Data = &LOD.BulkData;
				InstanceLOD.RestResource = LOD.RestResource;
				if (bNeedDeformedPositions)
				{
					InstanceLOD.DeformedResource = new FHairMeshesDeformedResource(LOD.BulkData, true, LODResourceName);
				}

				#if RHI_RAYTRACING
				if (IsRayTracingEnabled() && bVisibleInRayTracing)
				{
					if (bNeedDeformedPositions)
					{
						// Allocate dynamic raytracing resources (owned by the groom component/instance)
						InstanceLOD.RaytracingResource = new FHairStrandsRaytracingResource(*InstanceLOD.Data, LODResourceName);
						InstanceLOD.RaytracingResourceOwned = true;
					}
					else
					{
						// (Lazy) Allocate static raytracing resources (owned by the grooom asset)
						InstanceLOD.RaytracingResourceOwned = false;
						InstanceLOD.RaytracingResource = GroomAsset->AllocateMeshesRaytracingResources(GroupIt, MeshLODIndex);
						check(InstanceLOD.RaytracingResource);
					}
				}
				#endif
			}
		}
	}

	// Does not run projection code when running with null RHI as this is not needed, and will crash as the skeletal GPU resources are not created
	if (GUsingNullRHI)
	{
		return;
	}
}

template<typename T>
void InternalResourceRelease(T*& In)
{
	if (In)
	{
		In->ReleaseResource();
		delete In;
		In = nullptr;
	}
}

void UGroomComponent::ReleaseResources()
{

	FHairStrandsSceneProxy* GroomSceneProxy = (FHairStrandsSceneProxy*)SceneProxy;
	InitializedResources = nullptr;

	// Deferring instances deletion to insure scene proxy are done with the rendering data
	DeferredDeleteHairGroupInstances.Append(HairGroupInstances);
	HairGroupInstances.Empty();

	// Insure the ticking of the Groom component always happens after the skeletalMeshComponent.
	if (RegisteredMeshComponent)
	{
		RemoveTickPrerequisiteComponent(RegisteredMeshComponent);
	}
	SkeletalPreviousPositionOffset = FVector::ZeroVector;
	RegisteredMeshComponent = nullptr;

	GroomCacheBuffers.Reset();

	MarkRenderStateDirty();
}

void UGroomComponent::DeleteDeferredHairGroupInstances()
{
	FHairStrandsSceneProxy* GroomSceneProxy = (FHairStrandsSceneProxy*)SceneProxy;
	for (FHairGroupInstance* Instance : DeferredDeleteHairGroupInstances)
	{
		FHairGroupInstance* LocalInstance = Instance;
		ENQUEUE_RENDER_COMMAND(FHairStrandsBuffers)(
		[LocalInstance, GroomSceneProxy](FRHICommandListImmediate& RHICmdList)
		{
			// Sanity check
			check(LocalInstance->GetRefCount() == 1);

			// Guides
			if (LocalInstance->Guides.IsValid())
			{
				InternalResourceRelease(LocalInstance->Guides.DeformedRootResource);
				InternalResourceRelease(LocalInstance->Guides.DeformedResource);
			}

			// Strands
			if (LocalInstance->Strands.IsValid())
			{
				InternalResourceRelease(LocalInstance->Strands.DeformedRootResource);
				InternalResourceRelease(LocalInstance->Strands.DeformedResource);

				#if RHI_RAYTRACING
				if (LocalInstance->Strands.RenRaytracingResourceOwned)
				{
					InternalResourceRelease(LocalInstance->Strands.RenRaytracingResource);
				}
				#endif

				#if WITH_EDITOR
				LocalInstance->Strands.DebugAttributeBuffer.Release();
				#endif

				InternalResourceRelease(LocalInstance->Strands.VertexFactory);
			}

			// Cards
			{
				const uint32 CardLODCount = LocalInstance->Cards.LODs.Num();
				for (uint32 CardLODIt = 0; CardLODIt < CardLODCount; ++CardLODIt)
				{
					if (LocalInstance->Cards.IsValid(CardLODIt))
					{
						InternalResourceRelease(LocalInstance->Cards.LODs[CardLODIt].Guides.DeformedRootResource);
						InternalResourceRelease(LocalInstance->Cards.LODs[CardLODIt].Guides.DeformedResource);
						InternalResourceRelease(LocalInstance->Cards.LODs[CardLODIt].DeformedResource);
						#if RHI_RAYTRACING
						if (LocalInstance->Cards.LODs[CardLODIt].RaytracingResourceOwned)
						{
							InternalResourceRelease(LocalInstance->Cards.LODs[CardLODIt].RaytracingResource);
						}
						#endif

						InternalResourceRelease(LocalInstance->Cards.LODs[CardLODIt].VertexFactory);
					}
				}
			}

			// Meshes
			{
				const uint32 MeshesLODCount = LocalInstance->Meshes.LODs.Num();
				for (uint32 MeshesLODIt = 0; MeshesLODIt < MeshesLODCount; ++MeshesLODIt)
				{
					if (LocalInstance->Meshes.IsValid(MeshesLODIt))
					{
						InternalResourceRelease(LocalInstance->Meshes.LODs[MeshesLODIt].DeformedResource);
						#if RHI_RAYTRACING
						if (LocalInstance->Meshes.LODs[MeshesLODIt].RaytracingResourceOwned)
						{
							InternalResourceRelease(LocalInstance->Meshes.LODs[MeshesLODIt].RaytracingResource);
						}
						#endif

						InternalResourceRelease(LocalInstance->Meshes.LODs[MeshesLODIt].VertexFactory);
					}
				}
			}

			InternalResourceRelease(LocalInstance->HairGroupPublicData);
			LocalInstance->Release();
			delete LocalInstance;
		});
	}
	DeferredDeleteHairGroupInstances.Empty();
}

void UGroomComponent::PostLoad()
{
	LLM_SCOPE_BYTAG(Groom);

	Super::PostLoad();

	if (GroomAsset)
	{
		// Make sure that the asset initialized its resources first since the component needs them to initialize its own resources
		GroomAsset->ConditionalPostLoad();
	}

	if (BindingAsset)
	{
		// Make sure that the asset initialized its resources first since the component needs them to initialize its own resources
		BindingAsset->ConditionalPostLoad();
	}

	// This call will handle the GroomAsset properly if it's still being loaded
	SetGroomAsset(GroomAsset, BindingAsset, false);

	PrecachePSOs();

#if WITH_EDITOR
	if (GroomAsset && !bIsGroomAssetCallbackRegistered)
	{
		// Delegate used for notifying groom data invalidation
		GroomAsset->GetOnGroomAssetChanged().AddUObject(this, &UGroomComponent::Invalidate);

		// Delegate used for notifying groom data & groom resoures invalidation
		GroomAsset->GetOnGroomAssetResourcesChanged().AddUObject(this, &UGroomComponent::InvalidateAndRecreate);

		bIsGroomAssetCallbackRegistered = true;
	}

	if (BindingAsset && !bIsGroomBindingAssetCallbackRegistered)
	{
		BindingAsset->GetOnGroomBindingAssetChanged().AddUObject(this, &UGroomComponent::InvalidateAndRecreate);
		bIsGroomBindingAssetCallbackRegistered = true;
	}

	// Do not validate the groom yet as the component count be loaded, but material/binding & co will be set later on
	// ValidateMaterials(false);
#endif
}

void UGroomComponent::PrecachePSOs()
{
	if (!IsComponentPSOPrecachingEnabled() || GroomAsset == nullptr)
	{
		return;
	}

	TArray<FHairVertexFactoryTypesPerMaterialData> VFsPerMaterials = GroomAsset->CollectVertexFactoryTypesPerMaterialData(GMaxRHIShaderPlatform);

	FPSOPrecacheParams PrecachePSOParams;
	SetupPrecachePSOParams(PrecachePSOParams);

	for (FHairVertexFactoryTypesPerMaterialData& VFsPerMaterial : VFsPerMaterials)
	{
		UMaterialInterface* MaterialInterface = GetMaterial(GetMaterialIndexWithFallback(VFsPerMaterial.MaterialIndex), VFsPerMaterial.HairGeometryType, true);
		if (MaterialInterface)
		{
			MaterialInterface->PrecachePSOs(VFsPerMaterial.VertexFactoryTypes, PrecachePSOParams);
		}
	}

}

#if WITH_EDITOR
void UGroomComponent::Invalidate()
{
	UpdateHairGroupsDescAndInvalidateRenderState();
	UpdateHairSimulation();
	ValidateMaterials(false);
}

void UGroomComponent::InvalidateAndRecreate()
{
	InitResources(true);
	UpdateHairSimulation();
	MarkRenderStateDirty();
}
#endif

void UGroomComponent::OnRegister()
{
	Super::OnRegister();
	UpdateHairGroupsDesc();

	if (GUseGroomCacheStreaming)
	{
		IGroomCacheStreamingManager::Get().RegisterComponent(this);
	}

	// Insure the parent skeletal mesh is the same than the registered skeletal mesh, and if not reinitialized resources
	// This can happens when the OnAttachment callback is not called, but the skeletal mesh change (e.g., the skeletal mesh get recompiled within a blueprint)
	UMeshComponent* MeshComponent = GetAttachParent() ? Cast<UMeshComponent>(GetAttachParent()) : nullptr;

	const bool bNeedInitialization = !InitializedResources || InitializedResources != GroomAsset || MeshComponent != RegisteredMeshComponent;
	if (bNeedInitialization)
	{
		InitResources();
	}
	else if (GroomCache)
	{
		// Buffers already initialized, just need to update them
		UpdateGroomCache(ElapsedTime);
	}
	UpdateHairSimulation();
}

void UGroomComponent::OnUnregister()
{
	Super::OnUnregister();
	ReleaseHairSimulation();

	if (GUseGroomCacheStreaming)
	{
		if (GroomCacheBuffers.IsValid())
		{
			// Reset the buffers so they can be updated at OnRegister
			FGroomCacheBuffers* Buffers = static_cast<FGroomCacheBuffers*>(GroomCacheBuffers.Get());
			Buffers->Reset();
		}
		IGroomCacheStreamingManager::Get().UnregisterComponent(this);
	}
}

void UGroomComponent::BeginDestroy()
{
	ReleaseResources();
	Super::BeginDestroy();
}

void UGroomComponent::FinishDestroy()
{
	DeleteDeferredHairGroupInstances();
	Super::FinishDestroy();
}

void UGroomComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	ReleaseResources();
	DeleteDeferredHairGroupInstances();

#if WITH_EDITOR
	if (bIsGroomAssetCallbackRegistered && GroomAsset)
	{
		GroomAsset->GetOnGroomAssetChanged().RemoveAll(this);
		GroomAsset->GetOnGroomAssetResourcesChanged().RemoveAll(this);
		bIsGroomAssetCallbackRegistered = false;
	}
	if (bIsGroomBindingAssetCallbackRegistered && BindingAsset)
	{
		BindingAsset->GetOnGroomBindingAssetChanged().RemoveAll(this);
		bIsGroomBindingAssetCallbackRegistered = false;
	}
#endif

	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void UGroomComponent::OnAttachmentChanged()
{
	Super::OnAttachmentChanged();
	if (GroomAsset && !IsBeingDestroyed() && HasBeenCreated() && IsValidChecked(this))
	{
		UMeshComponent* NewMeshComponent = Cast<UMeshComponent>(GetAttachParent());
		const bool bHasAttachmentChanged = RegisteredMeshComponent != NewMeshComponent;
		if (bHasAttachmentChanged)
		{
			InitResources();
		}
	}
}

// Override the DetachFromComponent so that we do not mark the actor as modified/dirty when attaching to a specific bone
void UGroomComponent::DetachFromComponent(const FDetachmentTransformRules& In)
{
	FDetachmentTransformRules DetachmentRules = In;
	DetachmentRules.bCallModify = false;
	Super::DetachFromComponent(DetachmentRules);
}

void UGroomComponent::UpdateGroomCache(float Time)
{
	if (GroomCache && GroomCacheBuffers.IsValid() && bRunning)
	{
		FGroomCacheBuffers* Buffers = static_cast<FGroomCacheBuffers*>(GroomCacheBuffers.Get());
		Buffers->UpdateBuffersAtTime(Time, bLooping);

		// Trigger an update of the bounds so that it follows the GroomCache
		if (IsInGameThread())
		{
			MarkRenderTransformDirty();
		}
		else
		{
			FWeakObjectPtr WeakComponent(this);
			AsyncTask(ENamedThreads::GameThread, [WeakComponent]()
			{
				if (UGroomComponent* Component = Cast<UGroomComponent>(WeakComponent.Get()))
				{
					Component->MarkRenderTransformDirty();
				}
			});
		}
	}
}

void UGroomComponent::SetGroomCache(UGroomCache* InGroomCache)
{
	if (GroomCache != InGroomCache)
	{
		ReleaseResources();
		ResetAnimationTime();

		if (GUseGroomCacheStreaming)
		{
			IGroomCacheStreamingManager::Get().UnregisterComponent(this);
			GroomCache = InGroomCache;
			IGroomCacheStreamingManager::Get().RegisterComponent(this);
		}
		else
		{
			GroomCache = InGroomCache;
		}

		InitResources();
	}
}

float UGroomComponent::GetGroomCacheDuration() const
{
	return GroomCache ? GroomCache->GetDuration() : 0.0f;
}

void UGroomComponent::SetManualTick(bool bInManualTick)
{
	bManualTick = bInManualTick;
}

bool UGroomComponent::GetManualTick() const
{
	return bManualTick;
}

void UGroomComponent::ResetAnimationTime()
{
	ElapsedTime = 0.0f;
	if (GroomCache && bRunning && GUseGroomCacheStreaming)
	{
		IGroomCacheStreamingManager::Get().PrefetchData(this);
	}
	UpdateGroomCache(ElapsedTime);
}

float UGroomComponent::GetAnimationTime() const
{
	return ElapsedTime;
}

void UGroomComponent::TickAtThisTime(const float Time, bool bInIsRunning, bool bInBackwards, bool bInIsLooping)
{
	if (GroomCache && bRunning && bManualTick)
	{
		float DeltaTime = Time - ElapsedTime;
		if (GUseGroomCacheStreaming)
		{
			// Scrubbing forward (or backward) can induce large (or negative) delta time, so force a prefetch
			if ((DeltaTime > GetDefault<UGroomPluginSettings>()->GroomCacheLookAheadBuffer) ||
				(DeltaTime < 0))
			{
				ElapsedTime = Time;
				IGroomCacheStreamingManager::Get().PrefetchData(this);

				// Update the buffers with the prefetched data right away
				UpdateGroomCache(Time);
				return;
			}
		}

		// Queue the update for the render thread to sync it with GeometryCache rendering
		ENQUEUE_RENDER_COMMAND(FGroomCacheUpdate)(
		[this, Time](FRHICommandList& RHICmdList)
		{
			ElapsedTime = Time;
			UpdateGroomCache(Time);
		});
	}
}

void UGroomComponent::BuildSimulationTransform(FTransform& SimulationTransform) const
{
	SimulationTransform = FTransform::Identity;
	if (SimulationSettings.SimulationSetup.bLocalSimulation)
	{
		const USkeletalMeshComponent* SkeletelMeshComponent = Cast<const USkeletalMeshComponent>(RegisteredMeshComponent);
		if (SkeletelMeshComponent && SkeletelMeshComponent->GetSkeletalMeshAsset() && !SimulationSettings.SimulationSetup.LocalBone.IsEmpty())
		{
			const FReferenceSkeleton& RefSkeleton = SkeletelMeshComponent->GetSkeletalMeshAsset()->GetRefSkeleton();
			const FName BoneName(SimulationSettings.SimulationSetup.LocalBone);
			const int32 BoneIndex = RefSkeleton.FindBoneIndex(BoneName);

			SimulationTransform = SkeletelMeshComponent->GetBoneTransform(BoneIndex);
		}
		else
		{
			SimulationTransform = GetComponentTransform();
		}
	}
}

UPhysicsAsset* UGroomComponent::BuildAndCollect(FTransform& BoneTransform, TArray<TWeakObjectPtr<USkeletalMeshComponent>>& SourceComponents, TArray<TWeakObjectPtr<UPhysicsAsset>>& PhysicsAssets) const
{
	BuildSimulationTransform(BoneTransform);

	for (auto& CollisionComponent : CollisionComponents)
	{
		if (CollisionComponent.IsValid() && CollisionComponent->GetPhysicsAsset())
		{
			SourceComponents.Add(CollisionComponent);
			PhysicsAssets.Add(CollisionComponent->GetPhysicsAsset());
		}
	}

	return PhysicsAsset;
}

void UGroomComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);	
	
	const uint32 Id = ComponentId.PrimIDValue;
	const ERHIFeatureLevel::Type FeatureLevel = GetWorld() ? ERHIFeatureLevel::Type(GetWorld()->FeatureLevel) : ERHIFeatureLevel::Num;

	// When a groom binding and simulation are disabled, and the groom component is parented with a skeletal mesh, we can optionally 
	// attach the groom to a particular socket/bone
	USkeletalMeshComponent* SkeletelMeshComponent = Cast<USkeletalMeshComponent>(RegisteredMeshComponent);
	if (SkeletelMeshComponent && SkeletelMeshComponent->GetSkeletalMeshAsset() && !AttachmentName.IsEmpty())
	{
		const FName BoneName(AttachmentName);
		if (GetAttachSocketName() != BoneName)
		{
			const int32 BoneIndex = SkeletelMeshComponent->GetBoneIndex(BoneName);
			if (BoneIndex != INDEX_NONE)
			{
				AttachToComponent(SkeletelMeshComponent, FAttachmentTransformRules(EAttachmentRule::KeepRelative, false), BoneName);
				const FMatrix BoneTransformRaw = SkeletelMeshComponent->GetSkeletalMeshAsset()->GetComposedRefPoseMatrix(BoneIndex);
				const FVector BoneLocation = BoneTransformRaw.GetOrigin();
				const FQuat BoneRotation = BoneTransformRaw.ToQuat();

				FTransform BoneTransform = FTransform::Identity;
				BoneTransform.SetLocation(BoneLocation);
				BoneTransform.SetRotation(BoneRotation);

				FTransform InvBoneTransform = BoneTransform.Inverse();
				SetRelativeLocation(InvBoneTransform.GetLocation());
				SetRelativeRotation(InvBoneTransform.GetRotation());
			}
		}
	}

	bResetSimulation = bInitSimulation;
	if (!bInitSimulation)
	{
		const bool bLocalSimulation = SimulationSettings.SimulationSetup.bLocalSimulation &&
			(SimulationSettings.SimulationSetup.LinearVelocityScale == 0.0) &&
			(SimulationSettings.SimulationSetup.AngularVelocityScale == 0.0);
		if (!bLocalSimulation)
		{
			if (USkeletalMeshComponent* ParentComp = Cast<USkeletalMeshComponent>(GetAttachParent()))
			{
				if (ParentComp->GetNumBones() > 0)
				{
					const int32 BoneIndex = FMath::Min(1, ParentComp->GetNumBones() - 1);
					const FMatrix NextBoneMatrix = ParentComp->GetBoneMatrix(BoneIndex);

					const float BoneDistance = FVector::DistSquared(PrevBoneMatrix.GetOrigin(), NextBoneMatrix.GetOrigin());
					if (SimulationSettings.SimulationSetup.TeleportDistance > 0.0 && BoneDistance >
						SimulationSettings.SimulationSetup.TeleportDistance * SimulationSettings.SimulationSetup.TeleportDistance)
					{
						bResetSimulation = true;
					}
					PrevBoneMatrix = NextBoneMatrix;
				}
			}
		}
		bResetSimulation |= SimulationSettings.SimulationSetup.bResetSimulation;
		if (!IsVisible() || (GetOwner() && GetOwner()->IsHidden()))
		{
			bResetSimulation = true;
		}
	}
	bInitSimulation = false;

	if (HairGroupInstances.Num() == 0)
	{
		return;
	}

	if (RegisteredMeshComponent)
	{
		// When a skeletal object with projection is enabled, activate the refresh of the bounding box to
		// insure the component/proxy bounding box always lies onto the actual skinned mesh
		MarkRenderTransformDirty();
	}

	// Tick GroomCache only when playing
	if (GroomCache && GetWorld()->AreActorsInitialized() && bRunning && !bManualTick)
	{
		ElapsedTime += DeltaTime;
		UpdateGroomCache(ElapsedTime);
	}

	// Based on LOD selection type pick the effect LOD index. 
	// When LOD prediction is used, we read from the rendering thread and use the smallest prediction accross all groups
	float EffectiveForceLOD = -1;
	if (LODSelectionType == EHairLODSelectionType::Predicted)
	{
		// 1. Compute the effective LOD (taking the min. LOD across all the groups)
		for (const FHairGroupInstance* Instance : HairGroupInstances)
		{
			/* /!\ Access rendering thread data at that point /!\ */
			const float LODPredictedIndex_Instance = Instance->Debug.LODPredictedIndex;
			if (LODPredictedIndex_Instance > 0)
			{
				EffectiveForceLOD = EffectiveForceLOD < 0 ? LODPredictedIndex_Instance : FMath::Min(LODPredictedIndex_Instance, EffectiveForceLOD);
			}
		}

		// 2. If there is a LOD change, update the simulation adequately 
		SwitchSimulationLOD(LODPredictedIndex, EffectiveForceLOD);

		// 3. Update global predicted index (used for SyncLOD API)
		LODPredictedIndex = EffectiveForceLOD;
	}
	else if (LODSelectionType == EHairLODSelectionType::Forced)
	{
		EffectiveForceLOD = LODForcedIndex;
	}

	const bool bSwapBuffer = GetHairSwapBufferType() == EHairBufferSwapType::Tick;
	FTransform SkelLocalToTransform = FTransform::Identity;	
	if (RegisteredMeshComponent)
	{
		SkelLocalToTransform = RegisteredMeshComponent->GetComponentTransform();
	}
	
	TArray<FHairGroupInstance*> LocalHairGroupInstances = HairGroupInstances;
	const EHairLODSelectionType LocalLODSelectionType = LODSelectionType;
	ENQUEUE_RENDER_COMMAND(FHairStrandsTick_TransformUpdate)(
		[Id, FeatureLevel, LocalHairGroupInstances, EffectiveForceLOD, LocalLODSelectionType, bSwapBuffer, SkelLocalToTransform](FRHICommandListImmediate& RHICmdList)
	{
		if (ERHIFeatureLevel::Num == FeatureLevel)
			return;

		for (FHairGroupInstance* Instance : LocalHairGroupInstances)
		{
			Instance->Debug.LODForcedIndex = EffectiveForceLOD;
			Instance->Debug.LODSelectionTypeForDebug = LocalLODSelectionType;
			if (LocalLODSelectionType == EHairLODSelectionType::Predicted && EffectiveForceLOD >= 0)
			{
				AddHairStreamingRequest(Instance, EffectiveForceLOD);
			}

			if (bSwapBuffer)
			{
				Instance->Debug.SkinningPreviousLocalToWorld = Instance->Debug.SkinningCurrentLocalToWorld;
				Instance->Debug.SkinningCurrentLocalToWorld  = SkelLocalToTransform;

				if (Instance->Guides.DeformedResource)  { Instance->Guides.DeformedResource->SwapBuffer(); }
				if (Instance->Strands.DeformedResource) { Instance->Strands.DeformedResource->SwapBuffer(); }
			}
		}
	});
	
	if(GetHairSwapBufferType() == EHairBufferSwapType::RenderFrame)
	{
		MarkRenderDynamicDataDirty();
	}
}

void UGroomComponent::SendRenderTransform_Concurrent()
{
	if (RegisteredMeshComponent)
	{
		if (ShouldComponentAddToScene() && ShouldRender())
		{
			GetWorld()->Scene->UpdatePrimitiveTransform(this);
		}
	}

	Super::SendRenderTransform_Concurrent();
}

void UGroomComponent::SendRenderDynamicData_Concurrent()
{
	Super::SendRenderDynamicData_Concurrent();

	if(GetHairSwapBufferType() == EHairBufferSwapType::RenderFrame)
	{
		TArray<FHairGroupInstance*> LocalHairGroupInstances = HairGroupInstances;
		ENQUEUE_RENDER_COMMAND(FHairStrandsTick_TransformUpdate)(
			[LocalHairGroupInstances](FRHICommandListImmediate& RHICmdList)
		{
			for (FHairGroupInstance* Instance : LocalHairGroupInstances)
			{
				if (Instance->Guides.DeformedResource)  { Instance->Guides.DeformedResource->SwapBuffer(); }
				if (Instance->Strands.DeformedResource) { Instance->Strands.DeformedResource->SwapBuffer(); }
			}
		});
	}
}

void UGroomComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const
{
	UMeshComponent::GetUsedMaterials(OutMaterials, bGetDebugMaterials);
	if (bGetDebugMaterials)
	{
		if (IsHairStrandsEnabled(EHairStrandsShaderType::Tool))
		{
			OutMaterials.Add(Strands_DebugMaterial);
		}
	}

	if (IsHairStrandsEnabled(EHairStrandsShaderType::Strands))
	{
		OutMaterials.Add(Strands_DefaultMaterial);
	}

	if (IsHairStrandsEnabled(EHairStrandsShaderType::Cards))
	{
		OutMaterials.Add(Cards_DefaultMaterial);
	}
	if (IsHairStrandsEnabled(EHairStrandsShaderType::Meshes))
	{
		OutMaterials.Add(Meshes_DefaultMaterial);
	}
}

#if WITH_EDITOR
void UGroomComponent::PreEditChange(FProperty* PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);

	const FName PropertyName = PropertyAboutToChange ? PropertyAboutToChange->GetFName() : NAME_None;
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UGroomComponent, GroomAsset))
	{
		// Remove the callback on the GroomAsset about to be replaced
		if (bIsGroomAssetCallbackRegistered && GroomAsset)
		{
			GroomAsset->GetOnGroomAssetChanged().RemoveAll(this);
			GroomAsset->GetOnGroomAssetResourcesChanged().RemoveAll(this);
		}
		bIsGroomAssetCallbackRegistered = false;
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UGroomComponent, BindingAsset))
	{
		// Remove the callback on the GroomAsset about to be replaced
		if (bIsGroomBindingAssetCallbackRegistered && BindingAsset)
		{
			BindingAsset->GetOnGroomBindingAssetChanged().RemoveAll(this);
		}
		bIsGroomBindingAssetCallbackRegistered = false;
	}
}

void UGroomComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	FProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	const FName PropertyName = PropertyThatChanged ? PropertyThatChanged->GetFName() : NAME_None;

	//  Init/release resources when setting the GroomAsset (or undoing)
	const bool bAssetChanged = PropertyName == GET_MEMBER_NAME_CHECKED(UGroomComponent, GroomAsset);
	const bool bSourceSkeletalMeshChanged = PropertyName == GET_MEMBER_NAME_CHECKED(UGroomComponent, SourceSkeletalMesh);
	const bool bBindingAssetChanged = PropertyName == GET_MEMBER_NAME_CHECKED(UGroomComponent, BindingAsset);
	const bool bIsBindingCompatible = UGroomBindingAsset::IsCompatible(GroomAsset, BindingAsset, bValidationEnable);
	const bool bEnableSolverChanged = PropertyName == GET_MEMBER_NAME_CHECKED(FHairSimulationSolver, bEnableSimulation);
	const bool bGroomCacheChanged = PropertyName == GET_MEMBER_NAME_CHECKED(UGroomComponent, GroomCache);
	const bool bEnableLengthScaleChanged = PropertyName == GET_MEMBER_NAME_CHECKED(FHairGroupDesc, HairLengthScale);
	if (!bIsBindingCompatible || !UGroomBindingAsset::IsBindingAssetValid(BindingAsset, false, bValidationEnable))
	{
		BindingAsset = nullptr;
	}

	if (GroomAsset && !GroomAsset->IsValid())
	{
		GroomAsset = nullptr;
	}

	// HairLengthScale_Override does not generate an edit event (only HairLengthScale does), so we manually go through 
	// all the groups to check if there is a need for reallocating the resources
	bool bEnableLengthScaleOverrideChanged = false;
	if (bEnableLengthScaleChanged && GroomAsset && !bAssetChanged)
	{
		for (const FHairGroupInstance* Instance : HairGroupInstances)
		{
			const FHairGroupDesc GroupDesc = GetGroomGroupsDesc(GroomAsset, this, Instance->Debug.GroupIndex);
			const bool bRecreate = Instance->Strands.DeformedResource == nullptr && GroupDesc.HairLengthScale_Override;
			if (bRecreate)
			{
				bEnableLengthScaleOverrideChanged = true;
				break;
			}
		}
	}

	bool bIsEventProcess = false;

	// If the raytracing flag change, then force to recreate resources
	// Update the raytracing resources only if the groom asset hasn't changed. Otherwise the group desc might be
	// invalid. If the groom asset has change, the raytracing resources will be correctly be rebuild witin the
	// InitResources function
	bool bRayTracingGeometryChanged = false;
	#if RHI_RAYTRACING
	if (GroomAsset && !bAssetChanged)
	{
		for (const FHairGroupInstance* Instance : HairGroupInstances)
		{
			const FHairGroupDesc GroupDesc = GetGroomGroupsDesc(GroomAsset, this, Instance->Debug.GroupIndex);
			const bool bRecreate =
				(Instance->Strands.RenRaytracingResource == nullptr && GroupDesc.bUseHairRaytracingGeometry) ||
				(Instance->Strands.RenRaytracingResource != nullptr && !GroupDesc.bUseHairRaytracingGeometry);
			if (bRecreate)
			{
				bRayTracingGeometryChanged = true;
				break;
			}
		}
	}
	#endif

	// If material is assigned to the groom from the viewport (i.e., drag&drop a material from the content brown onto the groom geometry, it results into a unknown property). There is other case 
	const bool bIsUnknown = PropertyThatChanged == nullptr;

	const bool bRecreateResources = bAssetChanged || bBindingAssetChanged || bGroomCacheChanged || bEnableLengthScaleOverrideChanged || bIsUnknown || bSourceSkeletalMeshChanged || bRayTracingGeometryChanged || bEnableSolverChanged;
	if (bRecreateResources)
	{
		// Release the resources before Super::PostEditChangeProperty so that they get
		// re-initialized in OnRegister
		ReleaseResources();
		bIsEventProcess = true;
	}

#if WITH_EDITOR
	if (bGroomCacheChanged)
	{
		ResetAnimationTime();
	}

	// Check to see if simulation is required, which is needed when using with the guides groom cache,
	// and enable it on all groom groups if it's not already
	const EGroomCacheType GroomCacheType = GroomCache ? GroomCache->GetType() : EGroomCacheType::None;
	const bool bSimulationRequired = GroomCacheType == EGroomCacheType::Guides;
	if (bSimulationRequired && GroomAsset)
	{
		bool bGroomAssetChanged = false;
		for (int32 Index = 0; Index < GroomAsset->GetNumHairGroups(); ++Index)
		{
			if (!IsSimulationEnable(Index, -1))
			{
				if (!bGroomAssetChanged)
				{
					GroomAsset->Modify();
					bGroomAssetChanged = true;
				}
				GroomAsset->HairGroupsPhysics[Index].SolverSettings.EnableSimulation = true;
			}
		}
		if (bGroomAssetChanged)
		{
			GroomAsset->CacheDerivedDatas();
		}
	}

	if (bAssetChanged)
	{
		if (GroomAsset)
		{
			// Set the callback on the new GroomAsset being assigned
			GroomAsset->GetOnGroomAssetChanged().AddUObject(this, &UGroomComponent::Invalidate);
			GroomAsset->GetOnGroomAssetResourcesChanged().AddUObject(this, &UGroomComponent::InvalidateAndRecreate);
			bIsGroomAssetCallbackRegistered = true;
		}
	}

	if (bBindingAssetChanged)
	{
		if (BindingAsset)
		{
			// Set the callback on the new GroomAsset being assigned
			BindingAsset->GetOnGroomBindingAssetChanged().AddUObject(this, &UGroomComponent::InvalidateAndRecreate);
			bIsGroomBindingAssetCallbackRegistered = true;
		}
	}
#endif

	if (PropertyName == GET_MEMBER_NAME_CHECKED(FHairGroupDesc, HairWidth) || 
		PropertyName == GET_MEMBER_NAME_CHECKED(FHairGroupDesc, HairRootScale) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(FHairGroupDesc, HairTipScale) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(FHairGroupDesc, HairShadowDensity) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(FHairGroupDesc, HairRaytracingRadiusScale) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(FHairGroupDesc, LODBias) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(FHairGroupDesc, bUseStableRasterization) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(FHairGroupDesc, bScatterSceneLighting) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(FHairGroupDesc, HairLengthScale))
	{
		UpdateHairGroupsDescAndInvalidateRenderState();
		bIsEventProcess = true;
	}

	if (bIsUnknown)
	{
		InitResources(false);
	}

	// Always call parent PostEditChangeProperty as parent expect such a call (would crash in certain case otherwise)
	//if (!bIsEventProcess)
	{
		Super::PostEditChangeProperty(PropertyChangedEvent);
	}

#if WITH_EDITOR
	ValidateMaterials(false);
#endif
}

bool UGroomComponent::CanEditChange(const FProperty* InProperty) const
{
	if (InProperty)
	{
		FString PropertyName = InProperty->GetName();

		if (FCString::Strcmp(*PropertyName, TEXT("HairRaytracingRadiusScale")) == 0)
		{
			bool bIsEditable = false;
			#if RHI_RAYTRACING
			if (IsHairRayTracingEnabled())
			{
				bIsEditable = true;
			}
			#endif
			return bIsEditable;
		}
	}

	return Super::CanEditChange(InProperty);
}
#endif

#if WITH_EDITOR

void UGroomComponent::ValidateMaterials(bool bMapCheck) const
{
	if (!GroomAsset)
		return;

	FString Name = "";
	if (GetOwner())
	{
		Name += GetOwner()->GetName() + "/";
	}
	Name += GetName() + "/" + GroomAsset->GetName();

	const ERHIFeatureLevel::Type FeatureLevel = GetScene() ? GetScene()->GetFeatureLevel() : ERHIFeatureLevel::Num;
	for (uint32 MaterialIt = 0, MaterialCount = GetNumMaterials(); MaterialIt < MaterialCount; ++MaterialIt)
	{
		// Do not fallback on default material, so that we can detect that a material is not valid, and we can emit warning/validation error for this material
		const EHairGeometryType GeometryType = GetMaterialGeometryType(MaterialIt);
		UMaterialInterface* OverrideMaterial = GetMaterial(MaterialIt, GeometryType, false);

		const EHairMaterialCompatibility Result = IsHairMaterialCompatible(OverrideMaterial, FeatureLevel, GeometryType);
		switch (Result)
		{
			case EHairMaterialCompatibility::Invalid_UsedWithHairStrands:
			{
				if (bMapCheck)
				{
					FMessageLog("MapCheck").Warning()
						->AddToken(FUObjectToken::Create(GroomAsset))
						->AddToken(FTextToken::Create(LOCTEXT("MapCheck_Message_HairStrandsMissingUseHairStrands", "Groom's material needs to enable the UseHairStrands option. Groom's material will be replaced with default hair strands shader in editor.")))
						->AddToken(FMapErrorToken::Create(FMapErrors::InvalidHairStrandsMaterial));
				}
				else
				{
					UE_LOG(LogHairStrands, Warning, TEXT("[Groom] %s - Groom's material needs to enable the UseHairStrands option. Groom's material will be replaced with default hair strands shader in editor."), *Name);
				}
			} break;
			case EHairMaterialCompatibility::Invalid_ShadingModel:
			{
				if (bMapCheck)
				{
					FMessageLog("MapCheck").Warning()
						->AddToken(FUObjectToken::Create(GroomAsset))
						->AddToken(FTextToken::Create(LOCTEXT("MapCheck_Message_HairStrandsInvalidShadingModel", "Groom's material needs to have Hair shading model. Groom's material will be replaced with default hair strands shader in editor.")))
						->AddToken(FMapErrorToken::Create(FMapErrors::InvalidHairStrandsMaterial));
				}
				else
				{
					UE_LOG(LogHairStrands, Warning, TEXT("[Groom] %s - Groom's material needs to have Hair shading model. Groom's material will be replaced with default hair strands shader in editor."), *Name);
				}
			}break;
			case EHairMaterialCompatibility::Invalid_BlendMode:
			{
				if (bMapCheck)
				{
					FMessageLog("MapCheck").Warning()
						->AddToken(FUObjectToken::Create(GroomAsset))
						->AddToken(FTextToken::Create(LOCTEXT("MapCheck_Message_HairStrandsInvalidBlendMode", "Groom's material needs to have Opaque blend mode. Groom's material will be replaced with default hair strands shader in editor.")))
						->AddToken(FMapErrorToken::Create(FMapErrors::InvalidHairStrandsMaterial));
				}
				else
				{
					UE_LOG(LogHairStrands, Warning, TEXT("[Groom] %s - Groom's material needs to have Opaque blend mode. Groom's material will be replaced with default hair strands shader in editor."), *Name);
				}
			}break;
			// Disable this warning as it is not really useful, and spams the log when editing parameters, since it is invoke on Invalidation & on PostEditChange
			#if 0
			case EHairMaterialCompatibility::Invalid_IsNull:
			{
				if (bMapCheck)
				{
					FMessageLog("MapCheck").Info()
						->AddToken(FUObjectToken::Create(GroomAsset))
						->AddToken(FTextToken::Create(LOCTEXT("MapCheck_Message_HairStrandsMissingMaterial", "Groom's material is not set and will fallback on default hair strands shader in editor.")))
						->AddToken(FMapErrorToken::Create(FMapErrors::InvalidHairStrandsMaterial));
				}
				else
				{
					UE_LOG(LogHairStrands, Warning, TEXT("[Groom] %s - Groom's material is not set and will fallback on default hair strands shader in editor."), *Name);
				}
			}break;
			#endif
		}
	}
}

void UGroomComponent::CheckForErrors()
{
	Super::CheckForErrors();

	const FCoreTexts& CoreTexts = FCoreTexts::Get();

	// Get the mesh owner's name.
	AActor* Owner = GetOwner();
	FString OwnerName(*(CoreTexts.None.ToString()));
	if (Owner)
	{
		OwnerName = Owner->GetName();
	}

	ValidateMaterials(true);
}
#endif

template<typename T>
void InternalAddDedicatedVideoMemoryBytes(FResourceSizeEx& CumulativeResourceSize, T Resource)
{
	if (Resource)
	{
		CumulativeResourceSize.AddDedicatedVideoMemoryBytes(Resource->GetResourcesSize());
	}
}

void UGroomComponent::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	for (const FHairGroupInstance* Instance : HairGroupInstances)
	{
		InternalAddDedicatedVideoMemoryBytes(CumulativeResourceSize, Instance->Guides.DeformedResource);
		InternalAddDedicatedVideoMemoryBytes(CumulativeResourceSize, Instance->Guides.DeformedRootResource);

		InternalAddDedicatedVideoMemoryBytes(CumulativeResourceSize, Instance->Strands.DeformedResource);
		InternalAddDedicatedVideoMemoryBytes(CumulativeResourceSize, Instance->Strands.DeformedRootResource);

		for (const FHairGroupInstance::FCards::FLOD& LOD : Instance->Cards.LODs)
		{
			InternalAddDedicatedVideoMemoryBytes(CumulativeResourceSize, LOD.DeformedResource);
		}

		for (const FHairGroupInstance::FMeshes::FLOD& LOD : Instance->Meshes.LODs)
		{
			InternalAddDedicatedVideoMemoryBytes(CumulativeResourceSize, LOD.DeformedResource);
		}
	}
}

uint32 UGroomComponent::GetResourcesSize() const
{
	uint32 Total = 0;
	for (const FHairGroupInstance* Instance : HairGroupInstances)
	{
		Total += Instance->HairGroupPublicData ? Instance->HairGroupPublicData->GetResourcesSize() : 0;

		Total += Instance->Guides.DeformedResource ? Instance->Guides.DeformedResource->GetResourcesSize() : 0;
		Total += Instance->Guides.DeformedRootResource ? Instance->Guides.DeformedRootResource->GetResourcesSize() : 0;

		Total += Instance->Strands.DeformedResource ? Instance->Strands.DeformedResource->GetResourcesSize() : 0;
		Total += Instance->Strands.DeformedRootResource ? Instance->Strands.DeformedRootResource->GetResourcesSize() : 0;
	#if RHI_RAYTRACING
		Total += (Instance->Strands.RenRaytracingResourceOwned && Instance->Strands.RenRaytracingResource) ? Instance->Strands.RenRaytracingResource->GetResourcesSize() : 0;
	#endif

		for (const FHairGroupInstance::FCards::FLOD& LOD : Instance->Cards.LODs)
		{
			Total += LOD.DeformedResource ? LOD.DeformedResource->GetResourcesSize() : 0;
		}

		for (const FHairGroupInstance::FMeshes::FLOD& LOD : Instance->Meshes.LODs)
		{
			Total += LOD.DeformedResource ? LOD.DeformedResource->GetResourcesSize() : 0;
		}
	}

	return Total;
}

/////////////////////////////////////////////////////////////////////////////////////////
// Asset dump function

void DumpLoadedGroomComponent(IConsoleVariable* InCVarPakTesterEnabled);

int32 GHairStrandsDump_GroomComponent = 0;
static FAutoConsoleVariableRef CVarHairStrandsDump_GroomComponent(
	TEXT("r.HairStrands.Dump.GroomComponent"),
	GHairStrandsDump_GroomComponent,
	TEXT("Dump information of all active groom components."),
	FConsoleVariableDelegate::CreateStatic(DumpLoadedGroomComponent));

void DumpLoadedGroomComponent(IConsoleVariable* InCVarPakTesterEnabled)
{
	const bool bDetails = true;
	const float ToMb = 1.f / 1000000.f;

	uint32 Total_Component = 0;
	uint32 Total_Group = 0;

	uint32 Total_GPUMemorySize_Common = 0;
	uint32 Total_GPUMemorySize_Guides = 0;
	uint32 Total_GPUMemorySize_Strands= 0;
	uint32 Total_GPUMemorySize_Cards  = 0;
	uint32 Total_GPUMemorySize_Meshes = 0;

	UE_LOG(LogHairStrands, Log, TEXT("[Groom] ##### UGroomComponent #####"));
	UE_LOG(LogHairStrands, Log, TEXT("----------------------------------------------------------------------------------------------------------------------------------------------------------------"));
	UE_LOG(LogHairStrands, Log, TEXT("--  No.  - LOD -          GPU (     Common|     Guides|    Strands|      Cards|     Meshes) - Asset Name "));
	UE_LOG(LogHairStrands, Log, TEXT("----------------------------------------------------------------------------------------------------------------------------------------------------------------"));
	FString OutputData;
	for (TObjectIterator<UGroomComponent> ComponentIt; ComponentIt; ++ComponentIt)
	{
		if (ComponentIt)
		{
			const uint32 GroupCount = ComponentIt->GetGroupCount();
			for (uint32 GroupIt = 0; GroupIt < GroupCount; ++GroupIt)
			{
				FHairGroupInstance* Instance = ComponentIt->GetGroupInstance(GroupIt);

				uint32 GPUMemorySize_Common = 0;
				GPUMemorySize_Common += Instance->HairGroupPublicData ? Instance->HairGroupPublicData->GetResourcesSize() : 0;

				uint32 GPUMemorySize_Guides = 0;
				GPUMemorySize_Guides += Instance->Guides.DeformedResource ? Instance->Guides.DeformedResource->GetResourcesSize() : 0;
				GPUMemorySize_Guides += Instance->Guides.DeformedRootResource ? Instance->Guides.DeformedRootResource->GetResourcesSize() : 0;

				uint32 GPUMemorySize_Strands = 0;
				GPUMemorySize_Strands += Instance->Strands.DeformedResource ? Instance->Strands.DeformedResource->GetResourcesSize() : 0;
				GPUMemorySize_Strands += Instance->Strands.DeformedRootResource ? Instance->Strands.DeformedRootResource->GetResourcesSize() : 0;
			#if RHI_RAYTRACING
				GPUMemorySize_Strands += (Instance->Strands.RenRaytracingResourceOwned && Instance->Strands.RenRaytracingResource) ? Instance->Strands.RenRaytracingResource->GetResourcesSize() : 0;
			#endif

				uint32 GPUMemorySize_Cards  = 0;
				for (const FHairGroupInstance::FCards::FLOD& LOD : Instance->Cards.LODs)
				{
					GPUMemorySize_Cards += LOD.DeformedResource ? LOD.DeformedResource->GetResourcesSize() : 0;
				}

				uint32 GPUMemorySize_Meshes = 0;
				for (const FHairGroupInstance::FMeshes::FLOD& LOD : Instance->Meshes.LODs)
				{
					GPUMemorySize_Meshes += LOD.DeformedResource ? LOD.DeformedResource->GetResourcesSize() : 0;
				}

				uint32 LODCount = Instance->Cards.LODs.Num();

				uint32 GPUMemorySize = 0;
				GPUMemorySize += GPUMemorySize_Common;
				GPUMemorySize += GPUMemorySize_Guides;
				GPUMemorySize += GPUMemorySize_Strands;
				GPUMemorySize += GPUMemorySize_Cards;
				GPUMemorySize += GPUMemorySize_Meshes;

				Total_GPUMemorySize_Common += GPUMemorySize_Common;
				Total_GPUMemorySize_Guides += GPUMemorySize_Guides;
				Total_GPUMemorySize_Strands+= GPUMemorySize_Strands;
				Total_GPUMemorySize_Cards  += GPUMemorySize_Cards;
				Total_GPUMemorySize_Meshes += GPUMemorySize_Meshes;

				if (bDetails)
				{
//					UE_LOG(LogHairStrands, Log, TEXT("--  No.  - LOD -    GPU Total (    Common|     Guides|    Strands|      Cards|     Meshes) - Asset Name "));
//					UE_LOG(LogHairStrands, Log, TEXT("-- 00/00 -  00 -  0000.0Mb (0000.0Mb |0000.0Mb|0000.0Mb |0000.0Mb) -  0000.0Mb (0000.0Mb|0000.0Mb|0000.0Mb|0000.0Mb) - AssetName"));
					UE_LOG(LogHairStrands, Log, TEXT("-- %2d/%2d -  %2d -  %9.3fMb (%9.3fMb|%9.3fMb|%9.3fMb|%9.3fMb|%9.3fMb) - %s"), 
						GroupIt,
						GroupCount,

						LODCount, 

						GPUMemorySize* ToMb,
						GPUMemorySize_Common * ToMb,
						GPUMemorySize_Guides * ToMb,
						GPUMemorySize_Strands* ToMb,
						GPUMemorySize_Cards  * ToMb,
						GPUMemorySize_Meshes * ToMb,
						GroupIt == 0 && ComponentIt->GroomAsset ? *ComponentIt->GroomAsset->GetPathName() : TEXT("."));
				}
			}

			Total_Component++;
			Total_Group += GroupCount;
		}
	}

	uint32 Total_GPUMemorySize = 0;
	Total_GPUMemorySize += Total_GPUMemorySize_Common;
	Total_GPUMemorySize += Total_GPUMemorySize_Guides;
	Total_GPUMemorySize += Total_GPUMemorySize_Strands;
	Total_GPUMemorySize += Total_GPUMemorySize_Cards;
	Total_GPUMemorySize += Total_GPUMemorySize_Meshes;

	UE_LOG(LogHairStrands, Log, TEXT("----------------------------------------------------------------------------------------------------------------------------------------------------------------"));
	UE_LOG(LogHairStrands, Log, TEXT("-- C:%3d|G:%3d -  %9.3fMb (%9.3fMb|%9.3fMb|%9.3fMb|%9.3fMb|%9.3fMb)"),
		Total_Component,
		Total_Group,

		Total_GPUMemorySize * ToMb,
		Total_GPUMemorySize_Common* ToMb,
		Total_GPUMemorySize_Guides * ToMb,
		Total_GPUMemorySize_Strands * ToMb,
		Total_GPUMemorySize_Cards * ToMb,
		Total_GPUMemorySize_Meshes * ToMb);
}

/////////////////////////////////////////////////////////////////////////////////////////

int32 GHairStrandsResetSimulationOnAllGroomComponents = 0;
void ResetSimulationOnAllGroomComponents(IConsoleVariable* InCVarPakTesterEnabled)
{
	if (GHairStrandsResetSimulationOnAllGroomComponents > 0)
	{
		for (TObjectIterator<UGroomComponent> HairStrandsComponentIt; HairStrandsComponentIt; ++HairStrandsComponentIt)
		{
			HairStrandsComponentIt->ResetSimulation();
		}
	}
	GHairStrandsResetSimulationOnAllGroomComponents = 0;
}

static FAutoConsoleVariableRef CVarHairStrandsResetSimulationOnAllGroomComponents(
	TEXT("r.HairStrands.Simulation.ResetAll"),
	GHairStrandsResetSimulationOnAllGroomComponents,
	TEXT("Reset hair strands simulation on all groom components."),
	FConsoleVariableDelegate::CreateStatic(ResetSimulationOnAllGroomComponents));

#if WITH_EDITORONLY_DATA
FGroomComponentRecreateRenderStateContext::FGroomComponentRecreateRenderStateContext(UGroomAsset* GroomAsset)
{
	if (!GroomAsset)
	{
		return;
	}

	for (TObjectIterator<UGroomComponent> HairStrandsComponentIt; HairStrandsComponentIt; ++HairStrandsComponentIt)
	{
		if (HairStrandsComponentIt->GroomAsset == GroomAsset ||
			HairStrandsComponentIt->GroomAssetBeingLoaded == GroomAsset) // A GroomAsset was set on the component while it was still loading
		{
			if (HairStrandsComponentIt->IsRenderStateCreated())
			{
				HairStrandsComponentIt->DestroyRenderState_Concurrent();
				GroomComponents.Add(*HairStrandsComponentIt);
			}
		}
	}

	// Flush the rendering commands generated by the detachments.
	FlushRenderingCommands();
}

FGroomComponentRecreateRenderStateContext::~FGroomComponentRecreateRenderStateContext()
{
	const int32 ComponentCount = GroomComponents.Num();
	for (int32 ComponentIndex = 0; ComponentIndex < ComponentCount; ++ComponentIndex)
	{
		UGroomComponent* GroomComponent = GroomComponents[ComponentIndex];

		if (GroomComponent->IsRegistered() && !GroomComponent->IsRenderStateCreated())
		{
			if (GroomComponent->GroomAssetBeingLoaded && GroomComponent->GroomAssetBeingLoaded->IsValid())
			{
				// Re-set the assets on the component now that they are loaded
				GroomComponent->SetGroomAsset(GroomComponent->GroomAssetBeingLoaded, GroomComponent->BindingAssetBeingLoaded);
			}
			else
			{
				GroomComponent->InitResources();
			}
			GroomComponent->CreateRenderState_Concurrent(nullptr);
		}
	}
}
#endif

#undef LOCTEXT_NAMESPACE

