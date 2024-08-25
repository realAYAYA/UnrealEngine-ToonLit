// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SceneCore.cpp: Core scene implementation.
=============================================================================*/

#include "SceneCore.h"
#include "SceneInterface.h"
#include "SceneManagement.h"
#include "Misc/ConfigCacheIni.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "DepthRendering.h"
#include "SceneHitProxyRendering.h"
#include "VelocityRendering.h"
#include "BasePassRendering.h"
#include "MobileBasePassRendering.h"
#include "RendererModule.h"
#include "ScenePrivate.h"
#include "Containers/AllocatorFixedSizeFreeList.h"
#include "MaterialShared.h"
#include "HAL/LowLevelMemTracker.h"

int32 GUnbuiltPreviewShadowsInGame = 1;
FAutoConsoleVariableRef CVarUnbuiltPreviewShadowsInGame(
	TEXT("r.Shadow.UnbuiltPreviewInGame"),
	GUnbuiltPreviewShadowsInGame,
	TEXT("Whether to render unbuilt preview shadows in game.  When enabled and lighting is not built, expensive preview shadows will be rendered in game.  When disabled, lighting in game and editor won't match which can appear to be a bug."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

/**
 * Fixed Size pool allocator for FLightPrimitiveInteractions
 */
#define FREE_LIST_GROW_SIZE ( 16384 / sizeof(FLightPrimitiveInteraction) )
TAllocatorFixedSizeFreeList<sizeof(FLightPrimitiveInteraction), FREE_LIST_GROW_SIZE> GLightPrimitiveInteractionAllocator;


uint32 FRendererModule::GetNumDynamicLightsAffectingPrimitive(const FPrimitiveSceneInfo* PrimitiveSceneInfo,const FLightCacheInterface* LCI)
{
	uint32 NumDynamicLights = 0;

	FLightPrimitiveInteraction *LightList = PrimitiveSceneInfo->LightList;
	while ( LightList )
	{
		const FLightSceneInfo* LightSceneInfo = LightList->GetLight();

		// Determine the interaction type between the mesh and the light.
		FLightInteraction LightInteraction = FLightInteraction::Dynamic();
		if(LCI)
		{
			LightInteraction = LCI->GetInteraction(LightSceneInfo->Proxy);
		}

		// Don't count light-mapped or irrelevant lights.
		if(LightInteraction.GetType() != LIT_CachedIrrelevant && LightInteraction.GetType() != LIT_CachedLightMap)
		{
			++NumDynamicLights;
		}

		LightList = LightList->GetNextLight();
	}

	return NumDynamicLights;
}

/*-----------------------------------------------------------------------------
	FLightPrimitiveInteraction
-----------------------------------------------------------------------------*/

/**
 * Custom new
 */
void* FLightPrimitiveInteraction::operator new(size_t Size)
{
	// doesn't support derived classes with a different size
	checkSlow(Size == sizeof(FLightPrimitiveInteraction));
	return GLightPrimitiveInteractionAllocator.Allocate();
	//return FMemory::Malloc(Size);
}

/**
 * Custom delete
 */
void FLightPrimitiveInteraction::operator delete(void* RawMemory)
{
	GLightPrimitiveInteractionAllocator.Free(RawMemory);
	//FMemory::Free(RawMemory);
}

/**
 * Initialize the memory pool with a default size from the ini file.
 * Called at render thread startup. Since the render thread is potentially
 * created/destroyed multiple times, must make sure we only do it once.
 */
void FLightPrimitiveInteraction::InitializeMemoryPool()
{
	static bool bAlreadyInitialized = false;
	if (!bAlreadyInitialized)
	{
		bAlreadyInitialized = true;
		int32 InitialBlockSize = 0;
		GConfig->GetInt(TEXT("MemoryPools"), TEXT("FLightPrimitiveInteractionInitialBlockSize"), InitialBlockSize, GEngineIni);
		GLightPrimitiveInteractionAllocator.Grow(InitialBlockSize);
	}
}

/**
* Returns current size of memory pool
*/
uint32 FLightPrimitiveInteraction::GetMemoryPoolSize()
{
	return GLightPrimitiveInteractionAllocator.GetAllocatedSize();
}

void FLightPrimitiveInteraction::Create(FLightSceneInfo* LightSceneInfo,FPrimitiveSceneInfo* PrimitiveSceneInfo)
{
	LLM_SCOPE(ELLMTag::SceneRender);

	// Attach the light to the primitive's static meshes.
	bool bDynamic = true;
	bool bRelevant = false;
	bool bIsLightMapped = true;
	bool bShadowMapped = false;

	// Determine the light's relevance to the primitive.
	check(PrimitiveSceneInfo->Proxy && LightSceneInfo->Proxy);
	PrimitiveSceneInfo->Proxy->GetLightRelevance(LightSceneInfo->Proxy, bDynamic, bRelevant, bIsLightMapped, bShadowMapped);

	// Mobile renders stationary and dynamic local lights as dynamic
	bDynamic |= (PrimitiveSceneInfo->Scene->GetShadingPath() == EShadingPath::Mobile && bShadowMapped && LightSceneInfo->Proxy->IsLocalLight());

	if (bRelevant && bDynamic
		// Don't let lights with static shadowing or static lighting affect primitives that should use static lighting, but don't have valid settings (lightmap res 0, etc)
		// This prevents those components with invalid lightmap settings from causing lighting to remain unbuilt after a build
		&& !(LightSceneInfo->Proxy->HasStaticShadowing() && PrimitiveSceneInfo->Proxy->HasStaticLighting() && !PrimitiveSceneInfo->Proxy->HasValidSettingsForStaticLighting()))
	{
		const bool bTranslucentObjectShadow = LightSceneInfo->Proxy->CastsTranslucentShadows() && PrimitiveSceneInfo->Proxy->CastsVolumetricTranslucentShadow();
		const bool bInsetObjectShadow = 
			// Currently only supporting inset shadows on directional lights, but could be made to work with any whole scene shadows
			LightSceneInfo->Proxy->GetLightType() == LightType_Directional
			&& PrimitiveSceneInfo->Proxy->CastsInsetShadow();

		// Movable directional lights determine shadow relevance dynamically based on the view and CSM settings. Interactions are only required for per-object cases.
		if (LightSceneInfo->Proxy->GetLightType() != LightType_Directional || LightSceneInfo->Proxy->HasStaticShadowing() || bTranslucentObjectShadow || bInsetObjectShadow)
		{
			// Create the light interaction.
			FLightPrimitiveInteraction* Interaction = new FLightPrimitiveInteraction(LightSceneInfo, PrimitiveSceneInfo, bDynamic, bIsLightMapped, bShadowMapped, bTranslucentObjectShadow, bInsetObjectShadow);
		} //-V773
	}
}

void FLightPrimitiveInteraction::Destroy(FLightPrimitiveInteraction* LightPrimitiveInteraction)
{
	delete LightPrimitiveInteraction;
}

extern bool ShouldCreateObjectShadowForStationaryLight(const FLightSceneInfo* LightSceneInfo, const FPrimitiveSceneProxy* PrimitiveSceneProxy, bool bInteractionShadowMapped);

static bool MobileRequiresStaticMeshUpdateOnLocalLightChange(const FStaticShaderPlatform Platform)
{
	extern bool MobileLocalLightsUseSinglePermutation();
	
	if (!IsMobileDeferredShadingEnabled(Platform))
	{
		return MobileForwardEnableLocalLights(Platform) && !MobileLocalLightsUseSinglePermutation();
	}
	return false;
}

FLightPrimitiveInteraction::FLightPrimitiveInteraction(
	FLightSceneInfo* InLightSceneInfo,
	FPrimitiveSceneInfo* InPrimitiveSceneInfo,
	bool bInIsDynamic,
	bool bInLightMapped,
	bool bInIsShadowMapped,
	bool bInHasTranslucentObjectShadow,
	bool bInHasInsetObjectShadow
	) :
	LightSceneInfo(InLightSceneInfo),
	PrimitiveSceneInfo(InPrimitiveSceneInfo),
	LightId(InLightSceneInfo->Id),
	bLightMapped(bInLightMapped),
	bIsDynamic(bInIsDynamic),
	bIsShadowMapped(bInIsShadowMapped),
	bUncachedStaticLighting(false),
	bHasTranslucentObjectShadow(bInHasTranslucentObjectShadow),
	bHasInsetObjectShadow(bInHasInsetObjectShadow),
	bSelfShadowOnly(false),
	bMobileDynamicLocalLight(false)
{
	// Determine whether this light-primitive interaction produces a shadow.
	if(PrimitiveSceneInfo->Proxy->HasStaticLighting())
	{
		const bool bHasStaticShadow =
			LightSceneInfo->Proxy->HasStaticShadowing() &&
			LightSceneInfo->Proxy->CastsStaticShadow() &&
			PrimitiveSceneInfo->Proxy->CastsStaticShadow();
		const bool bHasDynamicShadow =
			!LightSceneInfo->Proxy->HasStaticLighting() &&
			LightSceneInfo->Proxy->CastsDynamicShadow() &&
			PrimitiveSceneInfo->Proxy->CastsDynamicShadow();
		bCastShadow = bHasStaticShadow || bHasDynamicShadow;
	}
	else
	{
		bCastShadow = LightSceneInfo->Proxy->CastsDynamicShadow() && PrimitiveSceneInfo->Proxy->CastsDynamicShadow();
	}
	bNaniteMeshProxy = PrimitiveSceneInfo->Proxy->IsNaniteMesh();
	bProxySupportsGPUScene = PrimitiveSceneInfo->Proxy->SupportsGPUScene();

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if(bCastShadow && bIsDynamic)
	{
		// Determine the type of dynamic shadow produced by this light.
		if (PrimitiveSceneInfo->Proxy->HasStaticLighting()
			&& PrimitiveSceneInfo->Proxy->CastsStaticShadow()
			// Don't mark unbuilt for movable primitives which were built with lightmaps but moved into a new light's influence
			&& PrimitiveSceneInfo->Proxy->GetLightmapType() != ELightmapType::ForceSurface
			&& (LightSceneInfo->Proxy->HasStaticLighting() || (LightSceneInfo->Proxy->HasStaticShadowing() && !bInIsShadowMapped)))
		{
			// Update the game thread's counter of number of uncached static lighting interactions.
			bUncachedStaticLighting = true;

			if (!GUnbuiltPreviewShadowsInGame && !InLightSceneInfo->Scene->IsEditorScene())
			{
				bCastShadow = false;
			}
	#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			LightSceneInfo->NumUnbuiltInteractions++;
	#endif

	#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			FPlatformAtomics::InterlockedIncrement(&PrimitiveSceneInfo->Scene->NumUncachedStaticLightingInteractions);
	#endif

#if WITH_EDITOR
			PrimitiveSceneInfo->Proxy->NumUncachedStaticLightingInteractions++;
#endif
		}
	}
#endif

	bSelfShadowOnly = PrimitiveSceneInfo->Proxy->CastsSelfShadowOnly();

	if (bIsDynamic)
	{
		// Add the interaction to the light's interaction list.
		PrevPrimitiveLink = PrimitiveSceneInfo->Proxy->IsMeshShapeOftenMoving() ? &LightSceneInfo->DynamicInteractionOftenMovingPrimitiveList : &LightSceneInfo->DynamicInteractionStaticPrimitiveList;

		// mobile local lights with dynamic lighting
		if (PrimitiveSceneInfo->Scene->GetShadingPath() == EShadingPath::Mobile && LightSceneInfo->ShouldRenderLightViewIndependent())
		{
			const uint8 LightType = LightSceneInfo->Proxy->GetLightType();

			const bool bIsValidLightType = 
				   LightType == LightType_Rect
				|| LightType == LightType_Point
				|| LightType == LightType_Spot;

			if( bIsValidLightType )
			{
				bMobileDynamicLocalLight = true;
				PrimitiveSceneInfo->NumMobileDynamicLocalLights++;
				if (PrimitiveSceneInfo->NumMobileDynamicLocalLights == 1 && 
					MobileRequiresStaticMeshUpdateOnLocalLightChange(PrimitiveSceneInfo->Scene->GetShaderPlatform()))
				{
					// Update static meshes to choose the shader permutation with local lights.
					PrimitiveSceneInfo->RequestStaticMeshUpdate();
				}
			} 

			if (LightSceneInfo->Proxy->CastsModulatedShadows() && !LightSceneInfo->Proxy->UseCSMForDynamicObjects() && LightSceneInfo->Proxy->HasStaticShadowing())
			{
				// Force bCastInsetShadow to be enabled to cast modulated shadow on mobile
				PrimitiveSceneInfo->Proxy->bCastInsetShadow = true;
				bHasInsetObjectShadow = true;
			}
		}
	}

	FlushCachedShadowMapData();

	NextPrimitive = *PrevPrimitiveLink;
	if(*PrevPrimitiveLink)
	{
		(*PrevPrimitiveLink)->PrevPrimitiveLink = &NextPrimitive;
	}
	*PrevPrimitiveLink = this;

	// Add the interaction to the primitives' interaction list.
	PrevLightLink = &PrimitiveSceneInfo->LightList;
	NextLight = *PrevLightLink;
	if(*PrevLightLink)
	{
		(*PrevLightLink)->PrevLightLink = &NextLight;
	}
	*PrevLightLink = this;

	if (HasShadow()
		&& LightSceneInfo->bRecordInteractionShadowPrimitives
		&& (HasTranslucentObjectShadow() || HasInsetObjectShadow() || ShouldCreateObjectShadowForStationaryLight(LightSceneInfo, PrimitiveSceneInfo->Proxy, IsShadowMapped())))
	{
		if (LightSceneInfo->InteractionShadowPrimitives.Num() < 16)
		{
			LightSceneInfo->InteractionShadowPrimitives.Add(this);
		}
		else
		{
			LightSceneInfo->bRecordInteractionShadowPrimitives = false;
			LightSceneInfo->InteractionShadowPrimitives.Empty();
		}
	}
}

FLightPrimitiveInteraction::~FLightPrimitiveInteraction()
{
	check(IsInRenderingThread());

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// Update the game thread's counter of number of uncached static lighting interactions.
	if(bUncachedStaticLighting)
	{
		LightSceneInfo->NumUnbuiltInteractions--;
		FPlatformAtomics::InterlockedDecrement(&PrimitiveSceneInfo->Scene->NumUncachedStaticLightingInteractions);
#if WITH_EDITOR
		PrimitiveSceneInfo->Proxy->NumUncachedStaticLightingInteractions--;
#endif
	}
#endif

	FlushCachedShadowMapData();

	// Track mobile movable local light count
	if (bMobileDynamicLocalLight)
	{
		PrimitiveSceneInfo->NumMobileDynamicLocalLights--;
		if (PrimitiveSceneInfo->NumMobileDynamicLocalLights == 0 &&
			MobileRequiresStaticMeshUpdateOnLocalLightChange(PrimitiveSceneInfo->Scene->GetShaderPlatform()))
		{
			// Update static meshes to choose the shader permutation without local lights.
			PrimitiveSceneInfo->RequestStaticMeshUpdate();
		}
	}

	// Remove the interaction from the light's interaction list.
	if(NextPrimitive)
	{
		NextPrimitive->PrevPrimitiveLink = PrevPrimitiveLink;
	}
	*PrevPrimitiveLink = NextPrimitive;

	// Remove the interaction from the primitive's interaction list.
	if(NextLight)
	{
		NextLight->PrevLightLink = PrevLightLink;
	}
	*PrevLightLink = NextLight;

	LightSceneInfo->InteractionShadowPrimitives.RemoveSingleSwap(this);
}

void FLightPrimitiveInteraction::FlushCachedShadowMapData()
{
	if (LightSceneInfo && PrimitiveSceneInfo && PrimitiveSceneInfo->Proxy && PrimitiveSceneInfo->Scene)
	{
		if (bCastShadow && !PrimitiveSceneInfo->Proxy->IsMeshShapeOftenMoving())
		{
			TArray<FCachedShadowMapData>* CachedShadowMapDatas = PrimitiveSceneInfo->Scene->GetCachedShadowMapDatas(LightSceneInfo->Id);

			if (CachedShadowMapDatas)
			{
				for (auto& CachedShadowMapData : *CachedShadowMapDatas)
				{
					CachedShadowMapData.InvalidateCachedShadow();
				}
			}
		}
	}
}

/** Initialization constructor. */
FExponentialHeightFogSceneInfo::FExponentialHeightFogSceneInfo(const UExponentialHeightFogComponent* InComponent):
	Component(InComponent),
	FogMaxOpacity(InComponent->FogMaxOpacity),
	StartDistance(InComponent->StartDistance),
	FogCutoffDistance(InComponent->FogCutoffDistance),
	DirectionalInscatteringExponent(InComponent->DirectionalInscatteringExponent),
	DirectionalInscatteringStartDistance(InComponent->DirectionalInscatteringStartDistance),
	DirectionalInscatteringColor(InComponent->DirectionalInscatteringLuminance)
{
	FogData[0].Height = InComponent->GetComponentLocation().Z;
	FogData[1].Height = InComponent->GetComponentLocation().Z + InComponent->SecondFogData.FogHeightOffset;

	// Scale the densities back down to their real scale
	// Artists edit the densities scaled up so they aren't entering in minuscule floating point numbers
	FogData[0].Density = InComponent->FogDensity / 1000.0f;	
	FogData[0].HeightFalloff = InComponent->FogHeightFalloff / 1000.0f;
	FogData[1].Density = InComponent->SecondFogData.FogDensity / 1000.0f;
	FogData[1].HeightFalloff = InComponent->SecondFogData.FogHeightFalloff / 1000.0f;

	FogColor = InComponent->InscatteringColorCubemap ? InComponent->InscatteringTextureTint : InComponent->FogInscatteringLuminance;
	InscatteringColorCubemap = InComponent->InscatteringColorCubemap;
	InscatteringColorCubemapAngle = InComponent->InscatteringColorCubemapAngle * (PI / 180.f);
	FullyDirectionalInscatteringColorDistance = InComponent->FullyDirectionalInscatteringColorDistance;
	NonDirectionalInscatteringColorDistance = InComponent->NonDirectionalInscatteringColorDistance;

	bEnableVolumetricFog = InComponent->bEnableVolumetricFog;
	VolumetricFogScatteringDistribution = FMath::Clamp(InComponent->VolumetricFogScatteringDistribution, -.99f, .99f);
	VolumetricFogAlbedo = FLinearColor(InComponent->VolumetricFogAlbedo);
	VolumetricFogEmissive = InComponent->VolumetricFogEmissive;

	// Apply a scale so artists don't have to work with tiny numbers.  
	const float UnitScale = 1.0f / 10000.0f;
	VolumetricFogEmissive.R = FMath::Max(VolumetricFogEmissive.R * UnitScale, 0.0f);
	VolumetricFogEmissive.G = FMath::Max(VolumetricFogEmissive.G * UnitScale, 0.0f);
	VolumetricFogEmissive.B = FMath::Max(VolumetricFogEmissive.B * UnitScale, 0.0f);
	VolumetricFogExtinctionScale = FMath::Max(InComponent->VolumetricFogExtinctionScale, 0.0f);
	VolumetricFogDistance = FMath::Max(InComponent->VolumetricFogStartDistance + InComponent->VolumetricFogDistance, 0.0f);
	VolumetricFogStaticLightingScatteringIntensity = FMath::Max(InComponent->VolumetricFogStaticLightingScatteringIntensity, 0.0f);
	bOverrideLightColorsWithFogInscatteringColors = InComponent->bOverrideLightColorsWithFogInscatteringColors;
	bHoldout = InComponent->bHoldout;
	bRenderInMainPass = InComponent->bRenderInMainPass;

	VolumetricFogStartDistance = InComponent->VolumetricFogStartDistance;
	VolumetricFogNearFadeInDistance = InComponent->VolumetricFogNearFadeInDistance;

	SkyAtmosphereAmbientContributionColorScale = InComponent->SkyAtmosphereAmbientContributionColorScale;
}
