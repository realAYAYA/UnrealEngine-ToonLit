// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MaterialInterface.cpp: UMaterialInterface implementation.
=============================================================================*/

#include "Materials/MaterialInterface.h"

#include "MeshUVChannelInfo.h"
#include "RenderingThread.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "PrimitiveViewRelevance.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "UObject/AssetRegistryTagsContext.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UE5ReleaseStreamObjectVersion.h"
#include "EditorFramework/AssetImportData.h"
#include "Engine/AssetUserData.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "ObjectCacheEventSink.h"
#include "Engine/SubsurfaceProfile.h"
#include "Engine/SpecularProfile.h"
#include "Engine/NeuralProfile.h"
#include "Interfaces/ITargetPlatform.h"
#include "Components/PrimitiveComponent.h"
#include "ContentStreaming.h"
#include "MeshBatch.h"
#include "Engine/Scene.h"
#include "RenderUtils.h"
#include "TextureCompiler.h"
#include "MaterialDomain.h"
#include "MaterialShaderQualitySettings.h"
#include "Materials/MaterialRenderProxy.h"
#include "ProfilingDebugging/CookStats.h"
#include "ShaderPlatformQualitySettings.h"
#include "ObjectCacheContext.h"
#include "MaterialCachedData.h"
#include "Components/DecalComponent.h"
#include "UObject/ArchiveCookContext.h"
#include "UObject/Package.h"
#include "ShaderCompiler.h"

#if WITH_EDITOR
#include "ObjectCacheEventSink.h"
#include "MaterialCachedHLSLTree.h"
#endif

#define LOCTEXT_NAMESPACE "MaterialInterface"

/**
 * This is used to deprecate data that has been built with older versions.
 * To regenerate the data, commands like "BUILDMATERIALTEXTURESTREAMINGDATA" can be used in the editor.
 * Ideally the data would be stored the DDC instead of the asset, but this is not yet  possible because it requires the GPU.
 */
#define MATERIAL_TEXTURE_STREAMING_DATA_VERSION 1

//////////////////////////////////////////////////////////////////////////

UEnum* UMaterialInterface::SamplerTypeEnum = nullptr;

static TAutoConsoleVariable<int32> CVarMaterialEnableNewHLSLGenerator(
	TEXT("r.MaterialEnableNewHLSLGenerator"),
	0,
	TEXT("Enables the new (WIP) material HLSL generator.\n")
	TEXT("0 - Don't allow\n")
	TEXT("1 - Allow if enabled by material\n")
	TEXT("2 - Force all materials to use new generator\n"),
	ECVF_RenderThreadSafe | ECVF_ReadOnly);

//////////////////////////////////////////////////////////////////////////

bool IsHairStrandsGeometrySupported(const EShaderPlatform Platform)
{
	check(Platform != SP_NumPlatforms);

	return FDataDrivenShaderPlatformInfo::GetSupportsHairStrandGeometry(Platform)
		&& IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
}

bool IsCompatibleWithHairStrands(const FMaterial* Material, const ERHIFeatureLevel::Type FeatureLevel)
{
	return
		ERHIFeatureLevel::SM5 <= FeatureLevel &&
		Material && Material->IsUsedWithHairStrands() && 
		IsOpaqueOrMaskedBlendMode(*Material);
}

bool IsCompatibleWithHairStrands(EShaderPlatform Platform, const FMaterialShaderParameters& Parameters)
{
	return
		IsHairStrandsGeometrySupported(Platform) &&
		Parameters.bIsUsedWithHairStrands &&
		IsOpaqueOrMaskedBlendMode(Parameters);
}

static EMaterialGetParameterValueFlags MakeParameterValueFlags(bool bOveriddenOnly)
{
	EMaterialGetParameterValueFlags Result = EMaterialGetParameterValueFlags::CheckInstanceOverrides;
	if (!bOveriddenOnly)
	{
		Result |= EMaterialGetParameterValueFlags::CheckNonOverrides;
	}
	return Result;
}

#if WITH_EDITORONLY_DATA

/** Deletes any invalid EditorOnlyData object with specified name that isn't the one assigned to specified interface. */
static void DisposeInvalidEditorOnlyData(UMaterialInterface* MaterialInterface, const FString& EditorOnlyDataName)
{
	UObject* EditorOnlyExisting = StaticFindObject(/*Class=*/ nullptr, MaterialInterface, *EditorOnlyDataName, true);
	if (EditorOnlyExisting && EditorOnlyExisting != MaterialInterface->GetEditorOnlyData())
	{
		FName UniqueDummyName = MakeUniqueObjectName(GetTransientPackage(), UMaterialInterfaceEditorOnlyData::StaticClass());
		EditorOnlyExisting->Rename(*UniqueDummyName.ToString(), GetTransientPackage(), REN_NonTransactional | REN_DontCreateRedirectors | REN_ForceNoResetLoaders);
		EditorOnlyExisting->MarkAsGarbage();
	}
}

#endif

//////////////////////////////////////////////////////////////////////////

/** Copies the material's relevance flags to a primitive's view relevance flags. */
void FMaterialRelevance::SetPrimitiveViewRelevance(FPrimitiveViewRelevance& OutViewRelevance) const
{
	OutViewRelevance.Raw = Raw;
}

//////////////////////////////////////////////////////////////////////////

#if WITH_EDITORONLY_DATA

namespace MaterialInterface
{
	FString GetEditorOnlyDataName(const TCHAR* InMaterialName)
	{
		return FString::Printf(TEXT("%sEditorOnlyData"), InMaterialName);
	}
}

#endif // WITH_EDITORONLY_DATA

UMaterialInterface::UMaterialInterface() = default;

UMaterialInterface::UMaterialInterface(FVTableHelper& Helper)
{
}

UMaterialInterface::~UMaterialInterface()
{
}

UMaterialInterface::UMaterialInterface(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		MaterialDomainString(MD_Surface); // find the enum for this now before we start saving
#if USE_EVENT_DRIVEN_ASYNC_LOAD_AT_BOOT_TIME
		if (!GIsInitialLoad || !GEventDrivenLoaderEnabled)
#endif
		{
			InitDefaultMaterials();
			AssertDefaultMaterialsExist();
		}

		if (SamplerTypeEnum == nullptr)
		{
			SamplerTypeEnum = StaticEnum<EMaterialSamplerType>();
			check(SamplerTypeEnum);
		}

		SetLightingGuid();
	}
}

void UMaterialInterface::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FUE5ReleaseStreamObjectVersion::GUID);

	if (Ar.IsCooking())
	{
		// Mark whether this material is part of the base game. This provides information on whether it is safe to be
		// used as a parent safely in child modules.
		bIncludedInBaseGame = Ar.GetCookContext()->GetCookingDLC() == UE::Cook::ECookingDLC::No;
	}

	Super::Serialize(Ar);

	bool bSavedCachedExpressionData = false;
	if (Ar.CustomVer(FUE5ReleaseStreamObjectVersion::GUID) >= FUE5ReleaseStreamObjectVersion::MaterialInterfaceSavedCachedData)
	{
		if ((Ar.IsCooking() || (FPlatformProperties::RequiresCookedData() && Ar.IsSaving() && (Ar.GetPortFlags() & PPF_Duplicate))) && (bool)CachedExpressionData)
		{
			bSavedCachedExpressionData = true;
		}

		Ar << bSavedCachedExpressionData;
	}

	if (bSavedCachedExpressionData)
	{
		if (Ar.IsLoading())
		{
			CachedExpressionData.Reset(new FMaterialCachedExpressionData());
			bLoadedCachedExpressionData = true;
#if WITH_EDITORONLY_DATA
			EditorOnlyData->CachedExpressionData = CachedExpressionData->EditorOnlyData;
#endif
		}
		else
		{
#if WITH_EDITOR
			CachedExpressionData->Validate(*this);
#endif
		}

		check(CachedExpressionData);
		UScriptStruct* Struct = FMaterialCachedExpressionData::StaticStruct();
		Struct->SerializeTaggedProperties(Ar, (uint8*)CachedExpressionData.Get(), Struct, nullptr);

#if WITH_EDITOR
		FObjectCacheEventSink::NotifyReferencedTextureChanged_Concurrent(this);
#endif
	}
    // we don't consider bLoadedCachedExpressionData here because in editor we call UpdateCachedExpressionData which can
    // create CachedExpressionData if it wasn't serialized
	else if (Ar.IsObjectReferenceCollector() && CachedExpressionData)
	{
 		UScriptStruct* Struct = FMaterialCachedExpressionData::StaticStruct();
 		Struct->SerializeTaggedProperties(Ar, (uint8*)CachedExpressionData.Get(), Struct, nullptr);
	}
}

void UMaterialInterface::PostLoad()
{
	Super::PostLoad();
#if USE_EVENT_DRIVEN_ASYNC_LOAD_AT_BOOT_TIME
	if (!GEventDrivenLoaderEnabled)
#endif
	{
		PostLoadDefaultMaterials();
	}

#if WITH_EDITORONLY_DATA
	if (TextureStreamingDataVersion != MATERIAL_TEXTURE_STREAMING_DATA_VERSION)
	{
		TextureStreamingData.Empty();
	}
#endif // WITH_EDITORONLY_DATA
}

#if WITH_EDITORONLY_DATA
void UMaterialInterface::DeclareConstructClasses(TArray<FTopLevelAssetPath>& OutConstructClasses, const UClass* SpecificSubclass)
{
	Super::DeclareConstructClasses(OutConstructClasses, SpecificSubclass);
	OutConstructClasses.Add(FTopLevelAssetPath(UMaterialInterfaceEditorOnlyData::StaticClass()));
}
#endif

const FMaterialCachedExpressionData& UMaterialInterface::GetCachedExpressionData(TMicRecursionGuard) const
{
	const FMaterialCachedExpressionData* LocalData = CachedExpressionData.Get();
	return LocalData ? *LocalData : FMaterialCachedExpressionData::EmptyData;
}

#if WITH_EDITOR
const FMaterialCachedHLSLTree& UMaterialInterface::GetCachedHLSLTree(TMicRecursionGuard) const
{
	check(IsUsingNewHLSLGenerator());
	const FMaterialCachedHLSLTree* LocalTree = CachedHLSLTree.Get();
	return LocalTree ? *LocalTree : FMaterialCachedHLSLTree::EmptyTree;
}
#endif // WITH_EDITOR

bool UMaterialInterface::IsUsingControlFlow() const
{
	const int CVarValue = CVarMaterialEnableNewHLSLGenerator.GetValueOnAnyThread();
	if (CVarValue == 0)
	{
		return false;
	}

	const UMaterial* BaseMaterial = GetMaterial_Concurrent();
	return BaseMaterial ? BaseMaterial->bEnableExecWire : false;
}

bool UMaterialInterface::IsUsingNewHLSLGenerator() const
{
	const int CVarValue = CVarMaterialEnableNewHLSLGenerator.GetValueOnAnyThread();
	if (CVarValue == 0)
	{
		return false;
	}
	else if (CVarValue == 2)
	{
		return true;
	}

	const UMaterial* BaseMaterial = GetMaterial_Concurrent();
	return BaseMaterial ? BaseMaterial->bEnableNewHLSLGenerator : false;
}

const FSubstrateCompilationConfig& UMaterialInterface::GetSubstrateCompilationConfig() const
{
	const UMaterial* BaseMaterial = GetMaterial_Concurrent();
	static FSubstrateCompilationConfig DefaultFSubstrateCompilationConfig = FSubstrateCompilationConfig();
	return BaseMaterial ? BaseMaterial->SubstrateCompilationConfig : DefaultFSubstrateCompilationConfig;
}

ENGINE_API void UMaterialInterface::SetSubstrateCompilationConfig(FSubstrateCompilationConfig& SubstrateCompilationConfig)
{
	UMaterial* BaseMaterial = GetMaterial();
	if (BaseMaterial)
	{
		BaseMaterial->SubstrateCompilationConfig = SubstrateCompilationConfig;
	}
}

void UMaterialInterface::GetQualityLevelUsage(TArray<bool, TInlineAllocator<EMaterialQualityLevel::Num> >& OutQualityLevelsUsed, EShaderPlatform ShaderPlatform, bool bCooking)
{
	OutQualityLevelsUsed = GetCachedExpressionData().QualityLevelsUsed;
	if (OutQualityLevelsUsed.Num() == 0)
	{
		OutQualityLevelsUsed.AddDefaulted(EMaterialQualityLevel::Num);
	}
	if (ShaderPlatform != SP_NumPlatforms)
	{
		const UShaderPlatformQualitySettings* MaterialQualitySettings = UMaterialShaderQualitySettings::Get()->GetShaderPlatformQualitySettings(ShaderPlatform);
		for (int32 Quality = 0; Quality < EMaterialQualityLevel::Num; ++Quality)
		{
			const FMaterialQualityOverrides& QualityOverrides = MaterialQualitySettings->GetQualityOverrides((EMaterialQualityLevel::Type)Quality);
			if (bCooking && QualityOverrides.bDiscardQualityDuringCook)
			{
				OutQualityLevelsUsed[Quality] = false;
			}
			else if (QualityOverrides.bEnableOverride &&
				QualityOverrides.HasAnyOverridesSet() &&
				QualityOverrides.CanOverride(ShaderPlatform))
			{
				OutQualityLevelsUsed[Quality] = true;
			}
		}
	}

#if WITH_EDITOR
	// if we specified -CacheMaterialQuality= on the cmd line we should only cook that quality.
	static const int32 MaterialQualityLevel = GetCmdLineMaterialQualityToCache();
	if (MaterialQualityLevel != INDEX_NONE)
	{
		// the format is only valid if it is a desired format for this platform.
		if (OutQualityLevelsUsed.IsValidIndex(MaterialQualityLevel))
		{
			// only cache the format specified on the command line.
			OutQualityLevelsUsed.SetNumZeroed(OutQualityLevelsUsed.Num());
			OutQualityLevelsUsed[MaterialQualityLevel] = true;
		}
	}
#endif
}

TArrayView<const TObjectPtr<UObject>> UMaterialInterface::GetReferencedTextures() const
{
	return GetCachedExpressionData().ReferencedTextures;
}

#if WITH_EDITOR
void UMaterialInterface::GetReferencedTexturesAndOverrides(TSet<const UTexture*>& InOutTextures) const
{
	for (UObject* UsedObject : GetCachedExpressionData().ReferencedTextures)
	{
		if (const UTexture* UsedTexture = Cast<UTexture>(UsedObject))
		{
			InOutTextures.Add(UsedTexture);
		}
	}
}
#endif // WITH_EDITOR

void UMaterialInterface::GetUsedTexturesAndIndices(TArray<UTexture*>& OutTextures, TArray< TArray<int32> >& OutIndices, EMaterialQualityLevel::Type QualityLevel, ERHIFeatureLevel::Type FeatureLevel) const
{
	GetUsedTextures(OutTextures, QualityLevel, false, FeatureLevel, false);
	OutIndices.AddDefaulted(OutTextures.Num());
}

#if WITH_EDITORONLY_DATA
bool UMaterialInterface::GetStaticSwitchParameterValue(const FHashedMaterialParameterInfo& ParameterInfo, bool& OutValue, FGuid& OutExpressionGuid, bool bOveriddenOnly /*= false*/) const
{
	FMaterialParameterMetadata Result;
	if (GetParameterValue(EMaterialParameterType::StaticSwitch, ParameterInfo, Result, MakeParameterValueFlags(bOveriddenOnly)))
	{
		OutExpressionGuid = Result.ExpressionGuid;
		OutValue = Result.Value.AsStaticSwitch();
		return true;
	}
	return false;
}

bool UMaterialInterface::GetStaticComponentMaskParameterValue(const FHashedMaterialParameterInfo& ParameterInfo, bool& R, bool& G, bool& B, bool& A, FGuid& OutExpressionGuid, bool bOveriddenOnly /*= false*/) const
{
	FMaterialParameterMetadata Result;
	if (GetParameterValue(EMaterialParameterType::StaticSwitch, ParameterInfo, Result, MakeParameterValueFlags(bOveriddenOnly)))
	{
		OutExpressionGuid = Result.ExpressionGuid;
		R = Result.Value.Bool[0];
		G = Result.Value.Bool[1];
		B = Result.Value.Bool[2];
		A = Result.Value.Bool[3];
		return true;
	}
	return false;
}
#endif // WITH_EDITORONLY_DATA

FMaterialRelevance UMaterialInterface::GetRelevance_Internal(const UMaterial* Material, ERHIFeatureLevel::Type InFeatureLevel) const
{
	if(Material)
	{
		const FMaterialResource* MaterialResource = GetMaterialResource(InFeatureLevel);

		// If material is invalid e.g. unparented instance, fallback to the passed in material
		bool bIsValidMaterialResource = (MaterialResource != nullptr) && (MaterialResource->GetMaterial() != nullptr);
		if (!bIsValidMaterialResource && (Material != nullptr))
		{
			MaterialResource = Material->GetMaterialResource(InFeatureLevel);
		}

		if (MaterialResource == nullptr)
		{
			return FMaterialRelevance();
		}

		const bool bIsMobile = InFeatureLevel <= ERHIFeatureLevel::ES3_1;
		const bool bUsesSingleLayerWaterMaterial = MaterialResource->GetShadingModels().HasShadingModel(MSM_SingleLayerWater);
		const bool bIsSinglePassWaterTranslucent = bIsMobile && bUsesSingleLayerWaterMaterial;
		const bool bIsMobilePixelProjectedTranslucent = MaterialResource->IsUsingPlanarForwardReflections() 
														&& IsUsingMobilePixelProjectedReflection(GetFeatureLevelShaderPlatform(InFeatureLevel));

		// Note that even though XX_GameThread() api is called, this function can be called on non game thread via 
		// GetRelevance_Concurrent()
		bool bUsesAnisotropy = MaterialResource->GetShadingModels().HasAnyShadingModel({ MSM_DefaultLit, MSM_ClearCoat }) && 
			MaterialResource->MaterialUsesAnisotropy_GameThread();

		const EBlendMode BlendMode = (EBlendMode)GetBlendMode();
		const bool bIsTranslucent = IsTranslucentBlendMode(BlendMode) || bIsSinglePassWaterTranslucent || bIsMobilePixelProjectedTranslucent; // We want meshes with water materials to be scheduled for translucent pass on mobile. And we also have to render the meshes used for mobile pixel projection reflection in translucent pass.

		EMaterialDomain Domain = (EMaterialDomain)MaterialResource->GetMaterialDomain();
		bool bDecal = (Domain == MD_DeferredDecal);

		// Determine the material's view relevance.
		FMaterialRelevance MaterialRelevance;

		MaterialRelevance.ShadingModelMask = GetShadingModels().GetShadingModelField();
		MaterialRelevance.CustomDepthStencilUsageMask = MaterialResource->GetCustomDepthStencilUsageMask_GameThread();
		MaterialRelevance.bDecal = bDecal;

		// Check whether the material can be drawn in the separate translucency pass as per FMaterialResource::IsTranslucencyAfterDOFEnabled and IsMobileSeparateTranslucencyEnabled
		EMaterialTranslucencyPass TranslucencyPass = MTP_BeforeDOF;
		const bool bSupportsSeparateTranslucency = Material->MaterialDomain != MD_UI && Material->MaterialDomain != MD_DeferredDecal;
		if (bIsTranslucent && bSupportsSeparateTranslucency)
		{
			if (bIsMobile)
			{
				if (Material->bEnableMobileSeparateTranslucency)
				{
					TranslucencyPass = MTP_AfterDOF;
				}
			}
			else
			{
				TranslucencyPass = Material->TranslucencyPass;
			}
		}			

		// If dual blending is supported, and we are rendering post-DOF translucency, then we also need to render a second pass to the modulation buffer.
		// The modulation buffer can also be used for regular modulation shaders after DoF.
		const bool bMaterialSeparateModulation = MaterialResource->IsDualBlendingEnabled(GShaderPlatformForFeatureLevel[InFeatureLevel]) || IsModulateBlendMode(BlendMode);

		// Encode Substrate BSDF into a mask where each bit correspond to a number of BSDF (1-8)
		const uint8 SubstrateBSDFCount = FMath::Max(MaterialResource->MaterialGetSubstrateClosureCount_GameThread(), uint8(1u));
		const uint8 SubstrateBSDFCountMask = 1u << uint8(FMath::Min(SubstrateBSDFCount - 1, 8));
		const uint8 SubstrateUintPerPixel = FMath::Max(MaterialResource->MaterialGetSubstrateUintPerPixel_GameThread(), uint8(1u));

		MaterialRelevance.bOpaque = !bIsTranslucent;
		MaterialRelevance.bMasked = IsMasked();
		MaterialRelevance.bDistortion = MaterialResource->IsDistorted();
		MaterialRelevance.bHairStrands = IsCompatibleWithHairStrands(MaterialResource, InFeatureLevel);
		MaterialRelevance.bTwoSided = MaterialResource->IsTwoSided();
		MaterialRelevance.bSeparateTranslucency = bIsTranslucent && (TranslucencyPass == MTP_AfterDOF);
		MaterialRelevance.bTranslucencyModulate = bMaterialSeparateModulation;
		MaterialRelevance.bPostMotionBlurTranslucency = (TranslucencyPass == MTP_AfterMotionBlur);
		MaterialRelevance.bNormalTranslucency = bIsTranslucent && (TranslucencyPass == MTP_BeforeDOF);
		MaterialRelevance.bDisableDepthTest = bIsTranslucent && Material->bDisableDepthTest;		
		MaterialRelevance.bUsesSceneColorCopy = bIsTranslucent && MaterialResource->RequiresSceneColorCopy_GameThread();
		MaterialRelevance.bOutputsTranslucentVelocity = Material->IsTranslucencyWritingVelocity();
		MaterialRelevance.bUsesGlobalDistanceField = MaterialResource->UsesGlobalDistanceField_GameThread();
		MaterialRelevance.bUsesWorldPositionOffset = MaterialResource->MaterialUsesWorldPositionOffset_GameThread();
		MaterialRelevance.bUsesDisplacement = MaterialResource->MaterialUsesDisplacement_GameThread();
		MaterialRelevance.bUsesPixelDepthOffset = MaterialResource->MaterialUsesPixelDepthOffset_GameThread();
		ETranslucencyLightingMode TranslucencyLightingMode = MaterialResource->GetTranslucencyLightingMode();
		MaterialRelevance.bTranslucentSurfaceLighting = bIsTranslucent && (TranslucencyLightingMode == TLM_SurfacePerPixelLighting || TranslucencyLightingMode == TLM_Surface);
		MaterialRelevance.bUsesSceneDepth = MaterialResource->MaterialUsesSceneDepthLookup_GameThread();
		MaterialRelevance.bHasVolumeMaterialDomain = MaterialResource->IsVolumetricPrimitive();
		MaterialRelevance.bUsesDistanceCullFade = MaterialResource->MaterialUsesDistanceCullFade_GameThread();
		MaterialRelevance.bUsesSkyMaterial = Material->bIsSky;
		MaterialRelevance.bUsesSingleLayerWaterMaterial = bUsesSingleLayerWaterMaterial;
		MaterialRelevance.bUsesAnisotropy = bUsesAnisotropy;
		MaterialRelevance.SubstrateClosureCountMask = SubstrateBSDFCountMask;
		MaterialRelevance.SubstrateUintPerPixel = SubstrateUintPerPixel;
		MaterialRelevance.bUsesComplexSpecialRenderPath = MaterialResource->MaterialGetSubstrateUsesComplexSpecialRenderPath_GameThread();

		return MaterialRelevance;
	}
	else
	{
		return FMaterialRelevance();
	}
}

FMaterialParameterInfo UMaterialInterface::GetParameterInfo(EMaterialParameterAssociation Association, FName ParameterName, UMaterialFunctionInterface* LayerFunction) const
{
	int32 Index = INDEX_NONE;
	if (Association != GlobalParameter)
	{
		if (LayerFunction)
		{
			FMaterialLayersFunctions MaterialLayers;
			if (GetMaterialLayers(MaterialLayers))
			{
				if (Association == BlendParameter) Index = MaterialLayers.Blends.Find(LayerFunction);
				else if (Association == LayerParameter) Index = MaterialLayers.Layers.Find(LayerFunction);
			}
		}
		if (Index == INDEX_NONE)
		{
			return FMaterialParameterInfo();
		}
	}

	return FMaterialParameterInfo(ParameterName, Association, Index);
}

FMaterialRelevance UMaterialInterface::GetRelevance(ERHIFeatureLevel::Type InFeatureLevel) const
{
	// Find the interface's concrete material.
	const UMaterial* Material = GetMaterial();
	return GetRelevance_Internal(Material, InFeatureLevel);
}

FMaterialRelevance UMaterialInterface::GetRelevance_Concurrent(ERHIFeatureLevel::Type InFeatureLevel) const
{
	// Find the interface's concrete material.
	const UMaterial* Material = GetMaterial_Concurrent();
	return GetRelevance_Internal(Material, InFeatureLevel);
}

int32 UMaterialInterface::GetWidth() const
{
	return ME_PREV_THUMBNAIL_SZ+(ME_STD_BORDER*2);
}

int32 UMaterialInterface::GetHeight() const
{
	return ME_PREV_THUMBNAIL_SZ+ME_CAPTION_HEIGHT+(ME_STD_BORDER*2);
}


void UMaterialInterface::SetForceMipLevelsToBeResident( bool OverrideForceMiplevelsToBeResident, bool bForceMiplevelsToBeResidentValue, float ForceDuration, int32 CinematicTextureGroups, bool bFastResponse )
{
	TArray<UTexture*> Textures;
	
	GetUsedTextures(Textures, EMaterialQualityLevel::Num, false, ERHIFeatureLevel::Num, true);
	
#if WITH_EDITOR
	FTextureCompilingManager::Get().FinishCompilation(Textures);
#endif

	for ( int32 TextureIndex=0; TextureIndex < Textures.Num(); ++TextureIndex )
	{
		UTexture2D* Texture = Cast<UTexture2D>(Textures[TextureIndex]);
		if ( Texture )
		{
			Texture->SetForceMipLevelsToBeResident( ForceDuration, CinematicTextureGroups );
			if (OverrideForceMiplevelsToBeResident)
			{
				Texture->bForceMiplevelsToBeResident = bForceMiplevelsToBeResidentValue;
			}

			if (bFastResponse && (ForceDuration > 0.f || Texture->bForceMiplevelsToBeResident))
			{
				static IConsoleVariable* CVarAllowFastForceResident = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Streaming.AllowFastForceResident"));

				Texture->bIgnoreStreamingMipBias = CVarAllowFastForceResident && CVarAllowFastForceResident->GetInt();
				if (Texture->IsStreamable())
				{
					IStreamingManager::Get().GetRenderAssetStreamingManager().FastForceFullyResident(Texture);
				}
			}
		}
	}
}

void UMaterialInterface::RecacheAllMaterialUniformExpressions(bool bRecreateUniformBuffer)
{
	// For each interface, reacache its uniform parameters
	for( TObjectIterator<UMaterialInterface> MaterialIt; MaterialIt; ++MaterialIt )
	{
		MaterialIt->RecacheUniformExpressions(bRecreateUniformBuffer);
	}
}

void UMaterialInterface::SubmitRemainingJobsForWorld(UWorld* World, EMaterialShaderPrecompileMode CompileMode)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UMaterialInterface::SubmitRemainingJobsForWorld);

	TSet<UMaterialInterface*> MaterialsToCache;
	FObjectCacheContextScope ObjectCacheScope;

	for (IPrimitiveComponent* PrimitiveComponentInterface : ObjectCacheScope.GetContext().GetPrimitiveComponents())
	{
		if (World && PrimitiveComponentInterface->GetWorld() == World)
		{
			continue;
		}

		if (PrimitiveComponentInterface->IsRenderStateCreated())
		{
			TObjectCacheIterator<UMaterialInterface> UsedMaterials = ObjectCacheScope.GetContext().GetUsedMaterials(PrimitiveComponentInterface);
			for (UMaterialInterface* MaterialInterface : UsedMaterials)
			{
				if (MaterialInterface && !MaterialInterface->IsComplete())
				{
					MaterialsToCache.Add(MaterialInterface);
				}
			}
		}
	}

	// Add UI and PP Materials.
	for (TObjectIterator<UMaterialInterface> It; It; ++It)
	{
		UMaterial* Material = It->GetMaterial();
		if (Material &&
			(Material->IsUIMaterial() || Material->IsPostProcessMaterial()) &&
			!Material->IsComplete())
		{
			MaterialsToCache.Add(*It);
		}
	}

	// Add Decal Component Materials.
	for (TObjectIterator<UDecalComponent> It; It; ++It)
	{
		if (It)
		{
			TArray<UMaterialInterface*> OutMaterials;
			It->GetUsedMaterials(OutMaterials);

			for (UMaterialInterface* MaterialInterface : OutMaterials)
			{
				if (MaterialInterface && !MaterialInterface->IsComplete())
				{
					MaterialsToCache.Add(MaterialInterface);
				}
			}
		}
	}

	if (MaterialsToCache.Num())
	{
		FMaterialUpdateContext UpdateContext(FMaterialUpdateContext::EOptions::SyncWithRenderingThread);
		for (UMaterialInterface* Material : MaterialsToCache)
		{
			// This is needed because CacheShaders blindly recreates uniform buffers
			// which can only be done if the draw command is going to be re-cached.
			UpdateContext.AddMaterialInterface(Material);
			Material->CacheShaders(CompileMode);
		}
	}
}

bool UMaterialInterface::IsReadyForFinishDestroy()
{
	bool bIsReady = Super::IsReadyForFinishDestroy();
	bIsReady = bIsReady && ParentRefFence.IsFenceComplete(); 
	return bIsReady;
}

void UMaterialInterface::BeginDestroy()
{
	ParentRefFence.BeginFence();
	Super::BeginDestroy();

#if WITH_EDITOR
	// The object cache needs to be notified when we're getting destroyed
	FObjectCacheEventSink::NotifyMaterialDestroyed_Concurrent(this);
#endif
}

void UMaterialInterface::FinishDestroy()
{
	CachedExpressionData.Reset();
	Super::FinishDestroy();
}

void UMaterialInterface::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UMaterialInterface* This = CastChecked<UMaterialInterface>(InThis);
	if (This->CachedExpressionData)
	{
		This->CachedExpressionData->AddReferencedObjects(Collector);
	}
	Super::AddReferencedObjects(This, Collector);
}

void UMaterialInterface::PostInitProperties()
{
#if WITH_EDITORONLY_DATA
	EditorOnlyData = CreateEditorOnlyData();
#endif
	Super::PostInitProperties();
}

void UMaterialInterface::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	SetLightingGuid();

#if WITH_EDITORONLY_DATA
	// Duplication will create a new editor only data but this MI already links a populated data. This makes sure that the
	// duplicate instance EOD has the correct name (necessary for running the editor on cooked data).
	FString EditorOnlyDataName = MaterialInterface::GetEditorOnlyDataName(*GetName());
	DisposeInvalidEditorOnlyData(this, EditorOnlyDataName);
	if (GetEditorOnlyData())
	{
		GetEditorOnlyData()->Rename(*EditorOnlyDataName, nullptr, REN_NonTransactional | REN_DontCreateRedirectors);
	}
#endif // WITH_EDITORONLY_DATA
}

#if WITH_EDITOR
void UMaterialInterface::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// flush the lighting guid on all changes
	SetLightingGuid();

	LightmassSettings.EmissiveBoost = FMath::Max(LightmassSettings.EmissiveBoost, 0.0f);
	LightmassSettings.DiffuseBoost = FMath::Max(LightmassSettings.DiffuseBoost, 0.0f);
	LightmassSettings.ExportResolutionScale = FMath::Clamp(LightmassSettings.ExportResolutionScale, 0.0f, 16.0f);

	for (UAssetUserData* Datum : AssetUserData)
	{
		if (Datum != nullptr)
		{
			Datum->PostEditChangeOwner();
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UMaterialInterface::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS;
	Super::GetAssetRegistryTags(OutTags);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS;
}

void UMaterialInterface::GetAssetRegistryTags(FAssetRegistryTagsContext Context) const
{
	if (AssetImportData)
	{
		Context.AddTag( FAssetRegistryTag(SourceFileTagName(), AssetImportData->GetSourceData().ToJson(), FAssetRegistryTag::TT_Hidden) );
	}

	{
		const FMaterialCachedExpressionData& CachedData = GetCachedExpressionData();
		Context.AddTag(FAssetRegistryTag("HasSceneColor", CachedData.bHasSceneColor ? TEXT("True") : TEXT("False"), FAssetRegistryTag::TT_Alphabetical));
		Context.AddTag(FAssetRegistryTag("HasPerInstanceRandom", CachedData.bHasPerInstanceRandom ? TEXT("True") : TEXT("False"), FAssetRegistryTag::TT_Alphabetical));
		Context.AddTag(FAssetRegistryTag("HasPerInstanceCustomData", CachedData.bHasPerInstanceCustomData ? TEXT("True") : TEXT("False"), FAssetRegistryTag::TT_Alphabetical));
		Context.AddTag(FAssetRegistryTag("HasVertexInterpolator", CachedData.bHasVertexInterpolator ? TEXT("True") : TEXT("False"), FAssetRegistryTag::TT_Alphabetical));
	}

	Super::GetAssetRegistryTags(Context);
}
#endif // WITH_EDITOR

void UMaterialInterface::GetLightingGuidChain(bool bIncludeTextures, TArray<FGuid>& OutGuids) const
{
	const FMaterialCachedExpressionData& CachedData = GetCachedExpressionData();
	CachedData.AppendReferencedFunctionIdsTo(OutGuids);
	CachedData.AppendReferencedParameterCollectionIdsTo(OutGuids);

#if WITH_EDITORONLY_DATA
	OutGuids.Add(LightingGuid);
#endif // WITH_EDITORONLY_DATA
}

#if WITH_EDITOR
uint32 UMaterialInterface::ComputeAllStateCRC() const
{
	uint32 CRC = 0xffffffff;

	const FMaterialCachedExpressionData& CachedData = GetCachedExpressionData();

	// use the precalculated CRC for the function info state ids (faster, as there can be thousands of these)
	CRC = FCrc::TypeCrc32(CachedData.FunctionInfosStateCRC, CRC);

	// mix in the parameter collection info state ids
	for (const FMaterialParameterCollectionInfo& CollectionInfo : CachedData.ParameterCollectionInfos)
	{
		CRC = FCrc::TypeCrc32(CollectionInfo.StateId, CRC);
	}

	return CRC;
}
#endif // WITH_EDITOR

bool UMaterialInterface::GetVectorParameterValue(const FHashedMaterialParameterInfo& ParameterInfo, FLinearColor& OutValue, bool bOveriddenOnly) const
{
	FMaterialParameterMetadata Result;
	if (GetParameterValue(EMaterialParameterType::Vector, ParameterInfo, Result, MakeParameterValueFlags(bOveriddenOnly)))
	{
		OutValue = Result.Value.AsLinearColor();
		return true;
	}
	return false;
}

bool UMaterialInterface::GetDoubleVectorParameterValue(const FHashedMaterialParameterInfo& ParameterInfo, FVector4d& OutValue, bool bOveriddenOnly) const
{
	FMaterialParameterMetadata Result;
	if (GetParameterValue(EMaterialParameterType::DoubleVector, ParameterInfo, Result, MakeParameterValueFlags(bOveriddenOnly)))
	{
		OutValue = Result.Value.AsVector4d();
		return true;
	}
	return false;
}

#if WITH_EDITOR
bool UMaterialInterface::IsVectorParameterUsedAsChannelMask(const FHashedMaterialParameterInfo& ParameterInfo, bool& OutValue) const
{
	FMaterialParameterMetadata Result;
	if (GetParameterValue(EMaterialParameterType::Vector, ParameterInfo, Result, EMaterialGetParameterValueFlags::CheckNonOverrides))
	{
		OutValue = Result.bUsedAsChannelMask;
		return true;
	}
	return false;
}

bool UMaterialInterface::GetVectorParameterChannelNames(const FHashedMaterialParameterInfo& ParameterInfo, FParameterChannelNames& OutValue) const
{
	FMaterialParameterMetadata Result;
	if (GetParameterValue(EMaterialParameterType::Vector, ParameterInfo, Result, EMaterialGetParameterValueFlags::CheckNonOverrides))
	{
		OutValue = Result.ChannelNames;
		return true;
	}
	return false;
}

bool UMaterialInterface::IsDoubleVectorParameterUsedAsChannelMask(const FHashedMaterialParameterInfo& ParameterInfo, bool& OutValue) const
{
	FMaterialParameterMetadata Result;
	if (GetParameterValue(EMaterialParameterType::DoubleVector, ParameterInfo, Result, EMaterialGetParameterValueFlags::CheckNonOverrides))
	{
		OutValue = Result.bUsedAsChannelMask;
		return true;
	}
	return false;
}

bool UMaterialInterface::GetDoubleVectorParameterChannelNames(const FHashedMaterialParameterInfo& ParameterInfo, FParameterChannelNames& OutValue) const
{
	FMaterialParameterMetadata Result;
	if (GetParameterValue(EMaterialParameterType::DoubleVector, ParameterInfo, Result, EMaterialGetParameterValueFlags::CheckNonOverrides))
	{
		OutValue = Result.ChannelNames;
		return true;
	}
	return false;
}

bool UMaterialInterface::GetScalarParameterSliderMinMax(const FHashedMaterialParameterInfo& ParameterInfo, float& OutSliderMin, float& OutSliderMax) const
{
	FMaterialParameterMetadata Result;
	if (GetParameterValue(EMaterialParameterType::Scalar, ParameterInfo, Result, EMaterialGetParameterValueFlags::CheckNonOverrides))
	{
		OutSliderMin = Result.ScalarMin;
		OutSliderMax = Result.ScalarMax;
		return true;
	}
	return false;
}
#endif // WITH_EDITOR

bool UMaterialInterface::GetScalarParameterValue(const FHashedMaterialParameterInfo& ParameterInfo, float& OutValue, bool bOveriddenOnly) const
{
	FMaterialParameterMetadata Result;
	if (GetParameterValue(EMaterialParameterType::Scalar, ParameterInfo, Result, MakeParameterValueFlags(bOveriddenOnly)))
	{
		OutValue = Result.Value.AsScalar();
		return true;
	}
	return false;
}

bool UMaterialInterface::GetParameterValue(EMaterialParameterType Type, const FMemoryImageMaterialParameterInfo& ParameterInfo, FMaterialParameterMetadata& OutValue, EMaterialGetParameterValueFlags Flags) const
{
	return false;
}

#if WITH_EDITOR
bool UMaterialInterface::IsScalarParameterUsedAsAtlasPosition(const FHashedMaterialParameterInfo& ParameterInfo, bool& OutValue, TSoftObjectPtr<class UCurveLinearColor>& Curve, TSoftObjectPtr<class UCurveLinearColorAtlas>& Atlas) const
{
	FMaterialParameterMetadata Result;
	if (GetParameterValue(EMaterialParameterType::Scalar, ParameterInfo, Result, EMaterialGetParameterValueFlags::CheckNonOverrides))
	{
		OutValue = Result.bUsedAsAtlasPosition;
		Curve = Result.ScalarCurve;
		Atlas = Result.ScalarAtlas;
		return true;
	}
	return false;
}
#endif // WITH_EDITOR

bool UMaterialInterface::GetTextureParameterValue(const FHashedMaterialParameterInfo& ParameterInfo, UTexture*& OutValue, bool bOveriddenOnly) const
{
	FMaterialParameterMetadata Result;
	if (GetParameterValue(EMaterialParameterType::Texture, ParameterInfo, Result, MakeParameterValueFlags(bOveriddenOnly)))
	{
		OutValue = Result.Value.Texture;
		return true;
	}
	return false;
}

bool UMaterialInterface::GetRuntimeVirtualTextureParameterValue(const FHashedMaterialParameterInfo& ParameterInfo, URuntimeVirtualTexture*& OutValue, bool bOveriddenOnly) const
{
	FMaterialParameterMetadata Result;
	if (GetParameterValue(EMaterialParameterType::RuntimeVirtualTexture, ParameterInfo, Result, MakeParameterValueFlags(bOveriddenOnly)))
	{
		OutValue = Result.Value.RuntimeVirtualTexture;
		return true;
	}
	return false;
}

bool UMaterialInterface::GetSparseVolumeTextureParameterValue(const FHashedMaterialParameterInfo& ParameterInfo, USparseVolumeTexture*& OutValue, bool bOveriddenOnly) const
{
	FMaterialParameterMetadata Result;
	if (GetParameterValue(EMaterialParameterType::SparseVolumeTexture, ParameterInfo, Result, MakeParameterValueFlags(bOveriddenOnly)))
	{
		OutValue = Result.Value.SparseVolumeTexture;
		return true;
	}
	return false;
}

#if WITH_EDITOR
bool UMaterialInterface::GetTextureParameterChannelNames(const FHashedMaterialParameterInfo& ParameterInfo, FParameterChannelNames& OutValue) const
{
	FMaterialParameterMetadata Result;
	if (GetParameterValue(EMaterialParameterType::Texture, ParameterInfo, Result, EMaterialGetParameterValueFlags::CheckNonOverrides))
	{
		OutValue = Result.ChannelNames;
		return true;
	}
	return false;
}
#endif

bool UMaterialInterface::GetFontParameterValue(const FHashedMaterialParameterInfo& ParameterInfo, class UFont*& OutFontValue, int32& OutFontPage, bool bOveriddenOnly) const
{
	FMaterialParameterMetadata Result;
	if (GetParameterValue(EMaterialParameterType::Font, ParameterInfo, Result, MakeParameterValueFlags(bOveriddenOnly)))
	{
		OutFontValue = Result.Value.Font.Value;
		OutFontPage = Result.Value.Font.Page;
		return true;
	}
	return false;
}

bool UMaterialInterface::GetParameterDefaultValue(EMaterialParameterType Type, const FMemoryImageMaterialParameterInfo& ParameterInfo, FMaterialParameterMetadata& OutValue) const
{
	return GetParameterValue(Type, ParameterInfo, OutValue, EMaterialGetParameterValueFlags::CheckNonOverrides);
}


bool UMaterialInterface::GetScalarParameterDefaultValue(const FHashedMaterialParameterInfo& ParameterInfo, float& OutValue) const
{
	FMaterialParameterMetadata Result;
	if (GetParameterDefaultValue(EMaterialParameterType::Scalar, ParameterInfo, Result))
	{
		OutValue = Result.Value.AsScalar();
		return true;
	}
	return false;
}

bool UMaterialInterface::GetVectorParameterDefaultValue(const FHashedMaterialParameterInfo& ParameterInfo, FLinearColor& OutValue) const
{
	FMaterialParameterMetadata Result;
	if (GetParameterDefaultValue(EMaterialParameterType::Vector, ParameterInfo, Result))
	{
		OutValue = Result.Value.AsLinearColor();
		return true;
	}
	return false;
}

bool UMaterialInterface::GetDoubleVectorParameterDefaultValue(const FHashedMaterialParameterInfo& ParameterInfo, FVector4& OutValue) const
{
	FMaterialParameterMetadata Result;
	if (GetParameterDefaultValue(EMaterialParameterType::DoubleVector, ParameterInfo, Result))
	{
		OutValue = Result.Value.AsVector4d();
		return true;
	}
	return false;
}

bool UMaterialInterface::GetTextureParameterDefaultValue(const FHashedMaterialParameterInfo& ParameterInfo, class UTexture*& OutValue) const
{
	FMaterialParameterMetadata Result;
	if (GetParameterDefaultValue(EMaterialParameterType::Texture, ParameterInfo, Result))
	{
		OutValue = Result.Value.Texture;
		return true;
	}
	return false;
}

bool UMaterialInterface::GetRuntimeVirtualTextureParameterDefaultValue(const FHashedMaterialParameterInfo& ParameterInfo, class URuntimeVirtualTexture*& OutValue) const
{
	FMaterialParameterMetadata Result;
	if (GetParameterDefaultValue(EMaterialParameterType::RuntimeVirtualTexture, ParameterInfo, Result))
	{
		OutValue = Result.Value.RuntimeVirtualTexture;
		return true;
	}
	return false;
}

bool UMaterialInterface::GetSparseVolumeTextureParameterDefaultValue(const FHashedMaterialParameterInfo& ParameterInfo, class USparseVolumeTexture*& OutValue) const
{
	FMaterialParameterMetadata Result;
	if (GetParameterDefaultValue(EMaterialParameterType::SparseVolumeTexture, ParameterInfo, Result))
	{
		OutValue = Result.Value.SparseVolumeTexture;
		return true;
	}
	return false;
}

bool UMaterialInterface::GetFontParameterDefaultValue(const FHashedMaterialParameterInfo& ParameterInfo, class UFont*& OutFontValue, int32& OutFontPage) const
{
	FMaterialParameterMetadata Result;
	if (GetParameterDefaultValue(EMaterialParameterType::Font, ParameterInfo, Result))
	{
		OutFontValue = Result.Value.Font.Value;
		OutFontPage = Result.Value.Font.Page;
		return true;
	}
	return false;
}


#if WITH_EDITOR
bool UMaterialInterface::GetStaticSwitchParameterDefaultValue(const FHashedMaterialParameterInfo& ParameterInfo, bool& OutValue, FGuid& OutExpressionGuid) const
{
	FMaterialParameterMetadata Result;
	if (GetParameterDefaultValue(EMaterialParameterType::StaticSwitch, ParameterInfo, Result))
	{
		OutExpressionGuid = Result.ExpressionGuid;
		OutValue = Result.Value.AsStaticSwitch();
		return true;
	}
	return false;
}

bool UMaterialInterface::GetStaticComponentMaskParameterDefaultValue(const FHashedMaterialParameterInfo& ParameterInfo, bool& OutR, bool& OutG, bool& OutB, bool& OutA, FGuid& OutExpressionGuid) const
{
	FMaterialParameterMetadata Result;
	if (GetParameterDefaultValue(EMaterialParameterType::StaticComponentMask, ParameterInfo, Result))
	{
		OutExpressionGuid = Result.ExpressionGuid;
		OutR = Result.Value.Bool[0];
		OutG = Result.Value.Bool[1];
		OutB = Result.Value.Bool[2];
		OutA = Result.Value.Bool[3];
		return true;
	}
	return false;
}

#endif // WITH_EDITOR

void UMaterialInterface::GetAllParametersOfType(EMaterialParameterType Type, TMap<FMaterialParameterInfo, FMaterialParameterMetadata>& OutParameters) const
{
	OutParameters.Reset();
	GetCachedExpressionData().GetAllParametersOfType(Type, OutParameters);
}

void UMaterialInterface::GetAllParameterInfoOfType(EMaterialParameterType Type, TArray<FMaterialParameterInfo>& OutParameterInfo, TArray<FGuid>& OutParameterIds) const
{
	OutParameterInfo.Reset();
	OutParameterIds.Reset();
	GetCachedExpressionData().GetAllParameterInfoOfType(Type, OutParameterInfo, OutParameterIds);
}

void UMaterialInterface::GetAllScalarParameterInfo(TArray<FMaterialParameterInfo>& OutParameterInfo, TArray<FGuid>& OutParameterIds) const
{
	GetAllParameterInfoOfType(EMaterialParameterType::Scalar, OutParameterInfo, OutParameterIds);
}

void UMaterialInterface::GetAllVectorParameterInfo(TArray<FMaterialParameterInfo>& OutParameterInfo, TArray<FGuid>& OutParameterIds) const
{
	GetAllParameterInfoOfType(EMaterialParameterType::Vector, OutParameterInfo, OutParameterIds);
}

void UMaterialInterface::GetAllDoubleVectorParameterInfo(TArray<FMaterialParameterInfo>& OutParameterInfo, TArray<FGuid>& OutParameterIds) const
{
	GetAllParameterInfoOfType(EMaterialParameterType::DoubleVector, OutParameterInfo, OutParameterIds);
}

void UMaterialInterface::GetAllTextureParameterInfo(TArray<FMaterialParameterInfo>& OutParameterInfo, TArray<FGuid>& OutParameterIds) const
{
	GetAllParameterInfoOfType(EMaterialParameterType::Texture, OutParameterInfo, OutParameterIds);
}

void UMaterialInterface::GetAllRuntimeVirtualTextureParameterInfo(TArray<FMaterialParameterInfo>& OutParameterInfo, TArray<FGuid>& OutParameterIds) const
{
	GetAllParameterInfoOfType(EMaterialParameterType::RuntimeVirtualTexture, OutParameterInfo, OutParameterIds);
}

void UMaterialInterface::GetAllSparseVolumeTextureParameterInfo(TArray<FMaterialParameterInfo>& OutParameterInfo, TArray<FGuid>& OutParameterIds) const
{
	GetAllParameterInfoOfType(EMaterialParameterType::SparseVolumeTexture, OutParameterInfo, OutParameterIds);
}

void UMaterialInterface::GetAllFontParameterInfo(TArray<FMaterialParameterInfo>& OutParameterInfo, TArray<FGuid>& OutParameterIds) const
{
	GetAllParameterInfoOfType(EMaterialParameterType::Font, OutParameterInfo, OutParameterIds);
}

#if WITH_EDITORONLY_DATA
void UMaterialInterface::GetAllStaticSwitchParameterInfo(TArray<FMaterialParameterInfo>&OutParameterInfo, TArray<FGuid>&OutParameterIds) const
{
	GetAllParameterInfoOfType(EMaterialParameterType::StaticSwitch, OutParameterInfo, OutParameterIds);
}

void UMaterialInterface::GetAllStaticComponentMaskParameterInfo(TArray<FMaterialParameterInfo>&OutParameterInfo, TArray<FGuid>&OutParameterIds) const
{
	GetAllParameterInfoOfType(EMaterialParameterType::StaticComponentMask, OutParameterInfo, OutParameterIds);
}
#endif // WITH_EDITOR

bool UMaterialInterface::GetRefractionSettings(float& OutBiasValue) const
{
	return false;
}

#if WITH_EDITOR
bool UMaterialInterface::GetParameterDesc(const FHashedMaterialParameterInfo& ParameterInfo, FString& OutDesc) const
{
	for (int32 TypeIndex = 0; TypeIndex < NumMaterialParameterTypes; ++TypeIndex)
	{
		FMaterialParameterMetadata Meta;
		if (GetParameterValue((EMaterialParameterType)TypeIndex, ParameterInfo, Meta, EMaterialGetParameterValueFlags::CheckNonOverrides))
		{
			OutDesc = Meta.Description;
			return true;
		}
	}
	return false;
}

bool UMaterialInterface::GetGroupName(const FHashedMaterialParameterInfo& ParameterInfo, FName& OutDesc) const
{
	for (int32 TypeIndex = 0; TypeIndex < NumMaterialParameterTypes; ++TypeIndex)
	{
		FMaterialParameterMetadata Meta;
		if (GetParameterValue((EMaterialParameterType)TypeIndex, ParameterInfo, Meta, EMaterialGetParameterValueFlags::CheckNonOverrides))
		{
			OutDesc = Meta.Group;
			return true;
		}
	}
	return false;
}

bool UMaterialInterface::GetParameterSortPriority(const FHashedMaterialParameterInfo& ParameterInfo, int32& OutSortPriority) const
{
	for (int32 TypeIndex = 0; TypeIndex < NumMaterialParameterTypes; ++TypeIndex)
	{
		FMaterialParameterMetadata Meta;
		if (GetParameterValue((EMaterialParameterType)TypeIndex, ParameterInfo, Meta, EMaterialGetParameterValueFlags::CheckNonOverrides))
		{
			OutSortPriority = Meta.SortPriority;
			return true;
		}
	}
	return false;
}
#endif // WITH_EDITOR

UMaterial* UMaterialInterface::GetBaseMaterial()
{
	return GetMaterial();
}

void UMaterialInterface::OnAssignedAsOverride(const UObject* Owner)
{

}

void UMaterialInterface::OnRemovedAsOverride(const UObject* Owner)
{

}

bool DoesMaterialUseTexture(const UMaterialInterface* Material,const UTexture* CheckTexture)
{
	//Do not care if we're running dedicated server
	if (FPlatformProperties::IsServerOnly())
	{
		return false;
	}

	TArray<UTexture*> Textures;
	Material->GetUsedTextures(Textures, EMaterialQualityLevel::Num, true, GMaxRHIFeatureLevel, true);
	for (int32 i = 0; i < Textures.Num(); i++)
	{
		if (Textures[i] == CheckTexture)
		{
			return true;
		}
	}
	return false;
}

float UMaterialInterface::GetOpacityMaskClipValue() const
{
	return 0.0f;
}

EBlendMode UMaterialInterface::GetBlendMode() const
{
	return BLEND_Opaque;
}

bool UMaterialInterface::IsTwoSided() const
{
	return false;
}

bool UMaterialInterface::IsThinSurface() const
{
	return false;
}

bool UMaterialInterface::IsDitheredLODTransition() const
{
	return false;
}

bool UMaterialInterface::IsTranslucencyWritingCustomDepth() const
{
	return false;
}

bool UMaterialInterface::IsTranslucencyWritingVelocity() const
{
	return false;
}

bool UMaterialInterface::IsTranslucencyWritingFrontLayerTransparency() const
{
	return false;
}

bool UMaterialInterface::IsMasked() const
{
	return false;
}

FDisplacementScaling UMaterialInterface::GetDisplacementScaling() const
{
	return FDisplacementScaling();
}

float UMaterialInterface::GetMaxWorldPositionOffsetDisplacement() const
{
	return 0.0f;
}

bool UMaterialInterface::ShouldAlwaysEvaluateWorldPositionOffset() const
{
	return false;
}

bool UMaterialInterface::HasPixelAnimation() const
{
	return false;
}

bool UMaterialInterface::IsDeferredDecal() const
{
	return false;
}

bool UMaterialInterface::GetCastDynamicShadowAsMasked() const
{
	return false;
}

bool UMaterialInterface::WritesToRuntimeVirtualTexture() const
{
	return false;
}

FMaterialShadingModelField UMaterialInterface::GetShadingModels() const
{
	return MSM_DefaultLit;
}

bool UMaterialInterface::IsShadingModelFromMaterialExpression() const
{
	return false;
}

USubsurfaceProfile* UMaterialInterface::GetSubsurfaceProfile_Internal() const
{
	return NULL;
}

USpecularProfile* UMaterialInterface::GetSpecularProfile_Internal(uint32 Index) const
{
	return nullptr;
}

UNeuralProfile* UMaterialInterface::GetNeuralProfile_Internal() const
{
	return nullptr;
}


uint32 UMaterialInterface::NumSpecularProfile_Internal() const
{
	return 0u;
}

bool UMaterialInterface::CastsRayTracedShadows() const
{
	return true;
}

bool UMaterialInterface::IsTessellationEnabled() const
{
	return false;
}

void UMaterialInterface::SetFeatureLevelToCompile(ERHIFeatureLevel::Type FeatureLevel, bool bShouldCompile)
{
	uint32 FeatureLevelBit = (1 << FeatureLevel);
	if (bShouldCompile)
	{
		FeatureLevelsToForceCompile |= FeatureLevelBit;
	}
	else
	{
		FeatureLevelsToForceCompile &= (~FeatureLevelBit);
	}
}

uint32 UMaterialInterface::FeatureLevelsForAllMaterials = 0;

void UMaterialInterface::SetGlobalRequiredFeatureLevel(ERHIFeatureLevel::Type FeatureLevel, bool bShouldCompile)
{
	uint32 FeatureLevelBit = (1 << FeatureLevel);
	if (bShouldCompile)
	{
		FeatureLevelsForAllMaterials |= FeatureLevelBit;
	}
	else
	{
		FeatureLevelsForAllMaterials &= (~FeatureLevelBit);
	}
}


uint32 UMaterialInterface::GetFeatureLevelsToCompileForRendering() const
{
	return FeatureLevelsToForceCompile | GetFeatureLevelsToCompileForAllMaterials();
}


void UMaterialInterface::UpdateMaterialRenderProxy(FMaterialRenderProxy& Proxy)
{
	// no 0 pointer
	check(&Proxy);

	FMaterialShadingModelField MaterialShadingModels = GetShadingModels();

	// for better performance we only update SubsurfaceProfileRT if the feature is used
	if (UseSubsurfaceProfile(MaterialShadingModels))
	{
		USubsurfaceProfile* LocalSubsurfaceProfile = GetSubsurfaceProfile_Internal();
		
		FSubsurfaceProfileStruct Settings;
		if (LocalSubsurfaceProfile)
		{
			Settings = LocalSubsurfaceProfile->Settings;
		}

		FMaterialRenderProxy* InProxy = &Proxy;
		ENQUEUE_RENDER_COMMAND(UpdateMaterialRenderProxySubsurface)(
		[Settings, LocalSubsurfaceProfile, InProxy](FRHICommandListImmediate& RHICmdList)
		{
			if (LocalSubsurfaceProfile)
			{
				const uint32 AllocationId = GSubsurfaceProfileTextureObject.AddOrUpdateProfile(Settings, LocalSubsurfaceProfile);
				check(AllocationId >= 0 && AllocationId < MAX_SUBSURFACE_PROFILE_COUNT);
			}
			InProxy->SetSubsurfaceProfileRT(LocalSubsurfaceProfile/*, ParameterName */); // how to have a unique identifier?
		});
	}

	UMaterial* Material = GetMaterial();
	if (Material && Material->IsPostProcessMaterial())
	{
		struct FEntry
		{
			UNeuralProfile* Profile = nullptr;
			FNeuralProfileStruct Setting;
			FGuid Guid;
		};
		
		UNeuralProfile* LocalNeuralProfile = GetNeuralProfile_Internal();
		
		if (LocalNeuralProfile)
		{
			FEntry Entry;
			Entry.Profile = LocalNeuralProfile;
			Entry.Setting = LocalNeuralProfile->Settings;
			Entry.Guid = LocalNeuralProfile->Guid;

			FMaterialRenderProxy* InProxy = &Proxy;
			ENQUEUE_RENDER_COMMAND(UpdateMaterialRenderProxyNNEModelData)(
				[LocalNeuralProfile, InProxy, Entry, Material](FRHICommandListImmediate& RHICmdList)
				{
					
					const uint32 AllocationId = NeuralProfile::AddOrUpdateProfile(Entry.Profile, Entry.Guid, Entry.Setting);
					check(AllocationId >= 0 && AllocationId < MAX_NEURAL_PROFILE_COUNT);

					Material->NeuralProfileId = AllocationId;
					
					InProxy->SetNeuralProfileRT(LocalNeuralProfile);
				});
		}
	}

	if (Substrate::IsSubstrateEnabled())
	{
		struct FEntry
		{
			USpecularProfile* Profile = nullptr;
			FSpecularProfileStruct Settings;
			const FTextureReference* Texture = nullptr;
			FGuid Guid;
		};
		TArray<FEntry> Entries;
		for (int32 It = 0, Count = NumSpecularProfile_Internal(); It<Count; ++It)
		{
			FEntry& Entry = Entries.AddDefaulted_GetRef();
			Entry.Profile = GetSpecularProfile_Internal(It);
			if (Entry.Profile)
			{
				Entry.Settings 	= Entry.Profile->Settings;
				Entry.Guid 		= Entry.Profile->Guid;	
				if (!Entry.Settings.IsProcedural())
				{
					Entry.Texture = &Entry.Profile->Settings.Texture->TextureReference;
				}
			}
		}

		FMaterialRenderProxy* InProxy = &Proxy;
		ENQUEUE_RENDER_COMMAND(UpdateMaterialRenderProxySpecular)(
		[InProxy, Entries](FRHICommandListImmediate& RHICmdList)
		{
			for (const FEntry& Entry : Entries)
			{
				if (Entry.Profile)
				{
					const uint32 AllocationId = SpecularProfileAtlas::AddOrUpdateProfile(Entry.Profile, Entry.Guid, Entry.Settings, Entry.Texture);
					check(AllocationId >= 0 && AllocationId < MAX_SPECULAR_PROFILE_COUNT);
				}
				InProxy->AddSpecularProfileRT(Entry.Profile);
			}
		});
	}
}

bool FMaterialTextureInfo::IsValid(bool bCheckTextureIndex) const
{ 
#if WITH_EDITORONLY_DATA
	if (bCheckTextureIndex && (TextureIndex < 0 || TextureIndex >= TEXSTREAM_MAX_NUM_TEXTURES_PER_MATERIAL))
	{
		return false;
	}
#endif
	return TextureName != NAME_None && SamplingScale > UE_SMALL_NUMBER && UVChannelIndex >= 0 && UVChannelIndex < TEXSTREAM_MAX_NUM_UVCHANNELS;
}

void UMaterialInterface::SortTextureStreamingData(bool bForceSort, bool bFinalSort)
{
#if WITH_EDITOR
	// In cook that was already done in the save.
	if (!bTextureStreamingDataSorted || bForceSort)
	{
		TSet<const UTexture*> UsedTextures;
		if (bFinalSort)
		{
			TSet<const UTexture*> UnfilteredUsedTextures;
			GetReferencedTexturesAndOverrides(UnfilteredUsedTextures);

			// Sort some of the conditions that could make the texture unstreamable, to make the data leaner.
			// Note that because we are cooking, UStreamableRenderAsset::bIsStreamable is not reliable here.
			for (const UTexture* UnfilteredTexture : UnfilteredUsedTextures)
			{
				if (UnfilteredTexture && UnfilteredTexture->IsPossibleToStream() )
				{
					UsedTextures.Add(UnfilteredTexture);
				}
			}
		}

		for (int32 Index = 0; Index < TextureStreamingData.Num(); ++Index)
		{
			FMaterialTextureInfo& TextureData = TextureStreamingData[Index];
			UTexture* Texture = Cast<UTexture>(TextureData.TextureReference.ResolveObject());

			// Also, when cooking, only keep textures that are directly referenced by this material to prevent non-deterministic cooking.
			// This would happen if a texture reference resolves to a texture not used anymore by this material. The resolved object could then be valid or not.
			if (Texture && (!bFinalSort || UsedTextures.Contains(Texture)))
			{
				TextureData.TextureName = Texture->GetFName();
			}
			else if (bFinalSort) // In the final sort we remove null names as they will never match.
			{
				TextureStreamingData.RemoveAtSwap(Index);
				--Index;
			}
			else
			{
				TextureData.TextureName = NAME_None;
			}
		}

		// Sort by name to be compatible with FindTextureStreamingDataIndexRange
		TextureStreamingData.Sort([](const FMaterialTextureInfo& Lhs, const FMaterialTextureInfo& Rhs) 
		{ 
			// Sort by register indices when the name are the same, as when initially added in the streaming data.
			if (Lhs.TextureName == Rhs.TextureName)
			{
				return Lhs.TextureIndex < Rhs.TextureIndex;

			}
			return Lhs.TextureName.LexicalLess(Rhs.TextureName); 
		});
		bTextureStreamingDataSorted = true;
	}
#endif // WITH_EDITOR
}

extern 	TAutoConsoleVariable<int32> CVarStreamingUseMaterialData;

bool UMaterialInterface::FindTextureStreamingDataIndexRange(FName TextureName, int32& LowerIndex, int32& HigherIndex) const
{
#if WITH_EDITORONLY_DATA
	// Because of redirectors (when textures are renammed), the texture names might be invalid and we need to udpate the data at every load.
	// Normally we would do that in the post load, but since the process needs to resolve the SoftObjectPaths, this is forbidden at that place.
	// As a workaround, we do it on demand. Note that this is not required in cooked build as it is done in the presave.
	const_cast<UMaterialInterface*>(this)->SortTextureStreamingData(false, false);
#endif

	if (CVarStreamingUseMaterialData.GetValueOnGameThread() == 0 || CVarStreamingUseNewMetrics.GetValueOnGameThread() == 0)
	{
		return false;
	}

	const int32 MatchingIndex = Algo::BinarySearchBy(TextureStreamingData, TextureName, &FMaterialTextureInfo::TextureName, FNameLexicalLess());
	if (MatchingIndex != INDEX_NONE)
	{
		// Find the range of entries for this texture. 
		// This is possible because the same texture could be bound to several register and also be used with different sampling UV.
		LowerIndex = MatchingIndex;
		HigherIndex = MatchingIndex;
		while (HigherIndex + 1 < TextureStreamingData.Num() && TextureStreamingData[HigherIndex + 1].TextureName == TextureName)
		{
			++HigherIndex;
		}
		return true;
	}
	return false;
}

void UMaterialInterface::SetTextureStreamingData(const TArray<FMaterialTextureInfo>& InTextureStreamingData)
{
	TextureStreamingData = InTextureStreamingData;
#if WITH_EDITORONLY_DATA
	bTextureStreamingDataSorted = false;
	TextureStreamingDataVersion = InTextureStreamingData.Num() ? MATERIAL_TEXTURE_STREAMING_DATA_VERSION : 0;
	TextureStreamingDataMissingEntries.Empty();
#endif
	SortTextureStreamingData(true, false);
}

float UMaterialInterface::GetTextureDensity(FName TextureName, const FMeshUVChannelInfo& UVChannelData) const
{
	ensure(UVChannelData.bInitialized);

	int32 LowerIndex = INDEX_NONE;
	int32 HigherIndex = INDEX_NONE;
	if (FindTextureStreamingDataIndexRange(TextureName, LowerIndex, HigherIndex))
	{
		// Compute the max, at least one entry will be valid. 
		float MaxDensity = 0;
		for (int32 Index = LowerIndex; Index <= HigherIndex; ++Index)
		{
			const FMaterialTextureInfo& MatchingData = TextureStreamingData[Index];
			ensure(MatchingData.IsValid() && MatchingData.TextureName == TextureName);
			MaxDensity = FMath::Max<float>(UVChannelData.LocalUVDensities[MatchingData.UVChannelIndex] / MatchingData.SamplingScale, MaxDensity);
		}
		return MaxDensity;
	}

	// Otherwise return 0 to indicate the data is not found.
	return 0;
}

uint32 UMaterialInterface::GetFeatureLevelsToCompileForAllMaterials()
{
	return FeatureLevelsForAllMaterials | (1 << GMaxRHIFeatureLevel);
}

bool UMaterialInterface::UseAnyStreamingTexture() const
{
	TArray<UTexture*> Textures;
	GetUsedTextures(Textures, EMaterialQualityLevel::Num, true, ERHIFeatureLevel::Num, true);

	for (UTexture* Texture : Textures)
	{
		if (Texture && Texture->IsStreamable())
		{
			return true;
		}
	}
	return false;
}

void UMaterialInterface::PreSave(const class ITargetPlatform* TargetPlatform)
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS;
	Super::PreSave(TargetPlatform);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS;
}

void UMaterialInterface::PreSave(FObjectPreSaveContext ObjectSaveContext)
{
	Super::PreSave(ObjectSaveContext);
	const ITargetPlatform* TargetPlatform = ObjectSaveContext.GetTargetPlatform();
	if (TargetPlatform && TargetPlatform->RequiresCookedData())
	{
		SortTextureStreamingData(true, true);
	}

#if WITH_EDITORONLY_DATA
	// Make sure the EditorOnlyData is named correctly. Some MI assets have a differently named editor only data that can cause problems in
	// the editor running on cooked data.
	UMaterialInterfaceEditorOnlyData* EditorOnly = GetEditorOnlyData();
	FString EditorOnlyDataName = MaterialInterface::GetEditorOnlyDataName(*GetName());
	if (!ObjectSaveContext.IsProceduralSave() && EditorOnly && EditorOnly->GetName() != EditorOnlyDataName)
	{
		UE_LOG(LogMaterial, Display, TEXT("MaterialInterface %s has a incorrectly name EditorOnlyData '%s'. This may cause issues when running the editor on cooked data. Trying to rename it to the correct name '%s'."), *GetName(), *EditorOnly->GetName(), *EditorOnlyDataName);
		DisposeInvalidEditorOnlyData(this, EditorOnlyDataName);
		if (EditorOnly)
		{
			EditorOnly->Rename(*EditorOnlyDataName, nullptr, REN_NonTransactional | REN_DontCreateRedirectors);
		}
	}
#endif // WITH_EDITORONLY_DATA

#if WITH_EDITOR
	if (ObjectSaveContext.IsCooking())
	{
		GShaderCompilerStats->IncrementMaterialCook();
	}
#endif // WITH_EDITOR
}

void UMaterialInterface::AddAssetUserData(UAssetUserData* InUserData)
{
	if (InUserData != nullptr)
	{
		UAssetUserData* ExistingData = GetAssetUserDataOfClass(InUserData->GetClass());
		if (ExistingData != nullptr)
		{
			AssetUserData.Remove(ExistingData);
		}
		AssetUserData.Add(InUserData);
	}
}

UAssetUserData* UMaterialInterface::GetAssetUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass)
{
	for (int32 DataIdx = 0; DataIdx < AssetUserData.Num(); DataIdx++)
	{
		UAssetUserData* Datum = AssetUserData[DataIdx];
		if (Datum != nullptr && Datum->IsA(InUserDataClass))
		{
			return Datum;
		}
	}
	return nullptr;
}

void UMaterialInterface::RemoveUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass)
{
	for (int32 DataIdx = 0; DataIdx < AssetUserData.Num(); DataIdx++)
	{
		UAssetUserData* Datum = AssetUserData[DataIdx];
		if (Datum != nullptr && Datum->IsA(InUserDataClass))
		{
			AssetUserData.RemoveAt(DataIdx);
			return;
		}
	}
}

void UMaterialInterface::EnsureIsComplete()
{
	// TODO:
	// The commented out code is roughly what we want to do to make the shaders ready. However this currently breaks for 
	// material instances, where IsComplete isn't checking for readiness of the parent, and seems to not work even when
	// CacheShaders is called on the parent. For now, to avoid issues with user blueprints failing on their first
	// draw with on demand shader compilation, we do the more direct requests below, which seem to be more reliable.
	
	//if (!IsComplete())
	//{
	//	FScopedSlowTask SlowTask(0.0f, LOCTEXT("CacheShaders", "Caching Shaders..."));
	//	SlowTask.MakeDialog();
	//	CacheShaders(EMaterialShaderPrecompileMode::Synchronous);
	//}

#if WITH_EDITOR
	uint32 FeatureLevelsToCompile = GetFeatureLevelsToCompileForRendering();
	while (FeatureLevelsToCompile != 0)
	{
		const ERHIFeatureLevel::Type FeatureLevel = (ERHIFeatureLevel::Type)FBitSet::GetAndClearNextBit(FeatureLevelsToCompile);
		FMaterialResource* MaterialResource = GetMaterialResource(FeatureLevel);
		if (MaterialResource && !MaterialResource->IsGameThreadShaderMapComplete())
		{
			MaterialResource->SubmitCompileJobs_GameThread(EShaderCompileJobPriority::ForceLocal);
			MaterialResource->FinishCompilation(); // this is WITH_EDITOR only
		}
	}
#endif
}

void UMaterialInterface::FilterOutPlatformShadingModels(const EShaderPlatform InPlatform, FMaterialShadingModelField& ShadingModels)
{
	const FStaticShaderPlatform Platform(InPlatform);
	if (ShadingModels.CountShadingModels() > 1 && !AllowPerPixelShadingModels(Platform))
	{
		ShadingModels = FMaterialShadingModelField(ShadingModels.GetFirstShadingModel());
	}

	uint32 ShadingModelsMask = GetPlatformShadingModelsMask(Platform);
	if (ShadingModelsMask != 0xFFFFFFFF)
	{
		uint16 FilteredShadingModels = (ShadingModels.GetShadingModelField() & ShadingModelsMask);
		ShadingModels.SetShadingModelField(FilteredShadingModels);
		if (!ShadingModels.IsValid())
		{
			ShadingModels.AddShadingModel(MSM_DefaultLit);
		}
	}
}

#if WITH_EDITORONLY_DATA


const UClass* UMaterialInterface::GetEditorOnlyDataClass() const
{
	return UMaterialInterfaceEditorOnlyData::StaticClass();
}


UMaterialInterfaceEditorOnlyData* UMaterialInterface::CreateEditorOnlyData()
{
	const UClass* EditorOnlyClass = GetEditorOnlyDataClass();
	check(EditorOnlyClass);
	check(EditorOnlyClass->HasAllClassFlags(CLASS_Optional));

	const FString EditorOnlyName = MaterialInterface::GetEditorOnlyDataName(*GetName());
	const EObjectFlags EditorOnlyFlags = GetMaskedFlags(RF_PropagateToSubObjects);
	return NewObject<UMaterialInterfaceEditorOnlyData>(this, EditorOnlyClass, *EditorOnlyName, EditorOnlyFlags);
}
#endif // WITH_EDITORONLY_DATA

bool UMaterialInterface::Rename(const TCHAR* NewName, UObject* NewOuter, ERenameFlags Flags)
{
	bool bRenamed = Super::Rename(NewName, NewOuter, Flags);
#if WITH_EDITORONLY_DATA
	// if we have EditorOnlyData, also rename it if we are changing the material's name
	if (bRenamed && NewName && EditorOnlyData)
	{
		FString EditorOnlyDataName = MaterialInterface::GetEditorOnlyDataName(NewName);
		DisposeInvalidEditorOnlyData(this, EditorOnlyDataName);
		bRenamed = EditorOnlyData->Rename(*EditorOnlyDataName, nullptr, Flags);
	}
#endif
	return bRenamed;
}

UMaterialInterfaceEditorOnlyData::UMaterialInterfaceEditorOnlyData()
{
}

UMaterialInterfaceEditorOnlyData::UMaterialInterfaceEditorOnlyData(FVTableHelper& Helper)
{
}

UMaterialInterfaceEditorOnlyData::~UMaterialInterfaceEditorOnlyData()
{
}

void UMaterialInterfaceEditorOnlyData::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	bool bSavedCachedExpressionData = false;
	if ((Ar.IsCooking() || (FPlatformProperties::RequiresCookedData() && Ar.IsSaving() && (Ar.GetPortFlags() & PPF_Duplicate))) && (bool)CachedExpressionData)
	{
		bSavedCachedExpressionData = true;
	}

	Ar << bSavedCachedExpressionData;

	if (bSavedCachedExpressionData)
	{
		// if we do not have our CachedExpressionData set at this point, 
		// it means this object's name doesn't match the default created object name and we need to fix our pointer into the material interface
		if (CachedExpressionData == nullptr)
		{
#if WITH_EDITORONLY_DATA
			UMaterialInterface* Owner = Cast<UMaterialInterface>(GetOuter());
			Owner->EditorOnlyData = this;
			CachedExpressionData = Owner->CachedExpressionData->EditorOnlyData;
#endif
		}

		check(CachedExpressionData);
		UScriptStruct* Struct = FMaterialCachedExpressionEditorOnlyData::StaticStruct();
		Struct->SerializeTaggedProperties(Ar, (uint8*)CachedExpressionData.Get(), Struct, nullptr);
		bLoadedCachedExpressionData = true;
	}
}

#undef LOCTEXT_NAMESPACE 
