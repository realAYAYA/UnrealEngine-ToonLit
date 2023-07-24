// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneManagement.h"
#include "DeviceProfiles/DeviceProfile.h"
#include "DeviceProfiles/DeviceProfileManager.h"
#include "PrimitiveSceneProxy.h"
#include "StaticMeshResources.h"
#include "RHIStaticStates.h"
#include "SceneRendering.h"
#include "SceneCore.h"
#include "SceneView.h"
#include "Async/ParallelFor.h"
#include "LightMap.h"
#include "LightSceneProxy.h"
#include "ShadowMap.h"
#include "Materials/MaterialRenderProxy.h"
#include "TextureResource.h"
#include "VT/LightmapVirtualTexture.h"
#include "UnrealEngine.h"
#include "ColorSpace.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "StaticMeshBatch.h"

static TAutoConsoleVariable<float> CVarLODTemporalLag(
	TEXT("lod.TemporalLag"),
	0.5f,
	TEXT("This controls the the time lag for temporal LOD, in seconds."),
	ECVF_Scalability | ECVF_Default);

bool AreCompressedTransformsSupported()
{
	return FDataDrivenShaderPlatformInfo::GetSupportSceneDataCompressedTransforms(GMaxRHIShaderPlatform);
}

bool DoesPlatformSupportDistanceFields(const FStaticShaderPlatform Platform)
{
	return FDataDrivenShaderPlatformInfo::GetSupportsDistanceFields(Platform);
}

bool DoesPlatformSupportDistanceFieldShadowing(EShaderPlatform Platform)
{
	return DoesPlatformSupportDistanceFields(Platform);
}

bool DoesPlatformSupportDistanceFieldAO(EShaderPlatform Platform)
{
	return DoesPlatformSupportDistanceFields(Platform);
}

bool DoesProjectSupportDistanceFields()
{
	static const auto CVarGenerateDF = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.GenerateMeshDistanceFields"));
	static const auto CVarDFIfNoHWRT = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DistanceFields.SupportEvenIfHardwareRayTracingSupported"));

	return DoesPlatformSupportDistanceFields(GMaxRHIShaderPlatform)
		&& CVarGenerateDF->GetValueOnAnyThread() != 0
		&& (CVarDFIfNoHWRT->GetValueOnAnyThread() != 0 || !IsRayTracingAllowed());
}

bool ShouldAllPrimitivesHaveDistanceField(EShaderPlatform ShaderPlatform)
{
	return (DoesPlatformSupportDistanceFieldAO(ShaderPlatform) || DoesPlatformSupportDistanceFieldShadowing(ShaderPlatform))
		&& IsUsingDistanceFields(ShaderPlatform)
		&& DoesProjectSupportDistanceFields();
}

bool ShouldCompileDistanceFieldShaders(EShaderPlatform ShaderPlatform)
{
	return IsFeatureLevelSupported(ShaderPlatform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(ShaderPlatform) && IsUsingDistanceFields(ShaderPlatform);
}


void FTemporalLODState::UpdateTemporalLODTransition(const FViewInfo& View, float LastRenderTime)
{
	bool bOk = false;
	if (!View.bDisableDistanceBasedFadeTransitions)
	{
		bOk = true;
		TemporalLODLag = CVarLODTemporalLag.GetValueOnRenderThread();
		if (TemporalLODTime[1] < LastRenderTime - TemporalLODLag)
		{
			if (TemporalLODTime[0] < TemporalLODTime[1])
			{
				TemporalLODViewOrigin[0] = TemporalLODViewOrigin[1];
				TemporalLODTime[0] = TemporalLODTime[1];
			}
			TemporalLODViewOrigin[1] = View.ViewMatrices.GetViewOrigin();
			TemporalLODTime[1] = LastRenderTime;
			if (TemporalLODTime[1] <= TemporalLODTime[0])
			{
				bOk = false; // we are paused or something or otherwise didn't get a good sample
			}
		}
	}
	if (!bOk)
	{
		TemporalLODViewOrigin[0] = View.ViewMatrices.GetViewOrigin();
		TemporalLODViewOrigin[1] = View.ViewMatrices.GetViewOrigin();
		TemporalLODTime[0] = LastRenderTime;
		TemporalLODTime[1] = LastRenderTime;
		TemporalLODLag = 0.0f;
	}
}

FFrozenSceneViewMatricesGuard::FFrozenSceneViewMatricesGuard(FSceneView& SV)
	: SceneView(SV)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (SceneView.State)
	{
		SceneView.State->ActivateFrozenViewMatrices(SceneView);
	}
#endif
}

FFrozenSceneViewMatricesGuard::~FFrozenSceneViewMatricesGuard()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (SceneView.State)
	{
		SceneView.State->RestoreUnfrozenViewMatrices(SceneView);
	}
#endif
}


IMPLEMENT_STATIC_UNIFORM_BUFFER_SLOT(WorkingColorSpace);
IMPLEMENT_STATIC_UNIFORM_BUFFER_STRUCT(FWorkingColorSpaceShaderParameters, "WorkingColorSpace", WorkingColorSpace);

void FDefaultWorkingColorSpaceUniformBuffer::Update(const UE::Color::FColorSpace& InColorSpace)
{
	using namespace UE::Color;

	const FVector2d& White = InColorSpace.GetWhiteChromaticity();
	const FVector2d ACES_D60 = GetWhitePoint(EWhitePoint::ACES_D60);

	FWorkingColorSpaceShaderParameters Parameters;
	Parameters.ToXYZ = Transpose<float>(InColorSpace.GetRgbToXYZ());
	Parameters.FromXYZ = Transpose<float>(InColorSpace.GetXYZToRgb());

	Parameters.ToAP1 = Transpose<float>(FColorSpaceTransform(InColorSpace, FColorSpace(EColorSpace::ACESAP1)));
	Parameters.FromAP1 = Parameters.ToAP1.Inverse();
	
	Parameters.ToAP0 = Transpose<float>(FColorSpaceTransform(InColorSpace, FColorSpace(EColorSpace::ACESAP0)));

	Parameters.bIsSRGB = InColorSpace.IsSRGB();

	SetContents(Parameters);
}

TGlobalResource<FDefaultWorkingColorSpaceUniformBuffer> GDefaultWorkingColorSpaceUniformBuffer;


FSimpleElementCollector::FSimpleElementCollector() :
	FPrimitiveDrawInterface(nullptr)
{
	static auto* MobileHDRCvar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileHDR"));
	bIsMobileHDR = (MobileHDRCvar->GetValueOnAnyThread() == 1);
}

FSimpleElementCollector::~FSimpleElementCollector()
{
	// Cleanup the dynamic resources.
	for(int32 ResourceIndex = 0;ResourceIndex < DynamicResources.Num();ResourceIndex++)
	{
		//release the resources before deleting, they will delete themselves
		DynamicResources[ResourceIndex]->ReleasePrimitiveResource();
	}
}

void FSimpleElementCollector::SetHitProxy(HHitProxy* HitProxy)
{
	if (HitProxy)
	{
		HitProxyId = HitProxy->Id;
	}
	else
	{
		HitProxyId = FHitProxyId();
	}
}

void FSimpleElementCollector::DrawSprite(
	const FVector& Position,
	float SizeX,
	float SizeY,
	const FTexture* Sprite,
	const FLinearColor& Color,
	uint8 DepthPriorityGroup,
	float U,
	float UL,
	float V,
	float VL,
	uint8 BlendMode,
	float OpacityMaskRefVal
	)
{
	FBatchedElements& Elements = DepthPriorityGroup == SDPG_World ? BatchedElements : TopBatchedElements;

	Elements.AddSprite(
		Position,
		SizeX,
		SizeY,
		Sprite,
		Color,
		HitProxyId,
		U,
		UL,
		V,
		VL,
		BlendMode,
		OpacityMaskRefVal
		);
}

void FSimpleElementCollector::DrawLine(
	const FVector& Start,
	const FVector& End,
	const FLinearColor& Color,
	uint8 DepthPriorityGroup,
	float Thickness/* = 0.0f*/,
	float DepthBias/* = 0.0f*/,
	bool bScreenSpace/* = false*/
	)
{
	FBatchedElements& Elements = DepthPriorityGroup == SDPG_World ? BatchedElements : TopBatchedElements;

	Elements.AddLine(
		Start,
		End,
		Color,
		HitProxyId,
		Thickness,
		DepthBias,
		bScreenSpace
		);
}


void FSimpleElementCollector::DrawTranslucentLine(
	const FVector& Start,
	const FVector& End,
	const FLinearColor& Color,
	uint8 DepthPriorityGroup,
	float Thickness/* = 0.0f*/,
	float DepthBias/* = 0.0f*/,
	bool bScreenSpace/* = false*/
)
{
	FBatchedElements& Elements = DepthPriorityGroup == SDPG_World ? BatchedElements : TopBatchedElements;

	Elements.AddTranslucentLine(
		Start,
		End,
		Color,
		HitProxyId,
		Thickness,
		DepthBias,
		bScreenSpace
	);
}


void FSimpleElementCollector::DrawPoint(
	const FVector& Position,
	const FLinearColor& Color,
	float PointSize,
	uint8 DepthPriorityGroup
	)
{
	FBatchedElements& Elements = DepthPriorityGroup == SDPG_World ? BatchedElements : TopBatchedElements;

	Elements.AddPoint(
		Position,
		PointSize,
		Color,
		HitProxyId
		);
}

void FSimpleElementCollector::RegisterDynamicResource(FDynamicPrimitiveResource* DynamicResource)
{
	// Add the dynamic resource to the list of resources to cleanup on destruction.
	DynamicResources.Add(DynamicResource);

	// Initialize the dynamic resource immediately.
	DynamicResource->InitPrimitiveResource();
}

void FSimpleElementCollector::DrawBatchedElements(FRHICommandList& RHICmdList, const FMeshPassProcessorRenderState& DrawRenderState, const FSceneView& InView, EBlendModeFilter::Type Filter, ESceneDepthPriorityGroup DepthPriorityGroup) const
{
	const FBatchedElements& Elements = DepthPriorityGroup == SDPG_World ? BatchedElements : TopBatchedElements;

	// Draw the batched elements.
	Elements.Draw(
		RHICmdList,
		DrawRenderState,
		InView.GetFeatureLevel(),
		InView,
		InView.Family->EngineShowFlags.HitProxies,
		1.0f,
		Filter
		);
}

FMeshBatchAndRelevance::FMeshBatchAndRelevance(const FMeshBatch& InMesh, const FPrimitiveSceneProxy* InPrimitiveSceneProxy, ERHIFeatureLevel::Type FeatureLevel) :
	Mesh(&InMesh),
	PrimitiveSceneProxy(InPrimitiveSceneProxy)
{
	const FMaterial& Material = InMesh.MaterialRenderProxy->GetIncompleteMaterialWithFallback(FeatureLevel);
	bHasOpaqueMaterial = IsOpaqueBlendMode(Material);
	bHasMaskedMaterial = IsMaskedBlendMode(Material);
	bRenderInMainPass = PrimitiveSceneProxy->ShouldRenderInMainPass();
}

static TAutoConsoleVariable<int32> CVarUseParallelGetDynamicMeshElementsTasks(
	TEXT("r.UseParallelGetDynamicMeshElementsTasks"),
	0,
	TEXT("If > 0, and if FApp::ShouldUseThreadingForPerformance(), then parts of GetDynamicMeshElements will be done in parallel."));

FMeshElementCollector::FMeshElementCollector(ERHIFeatureLevel::Type InFeatureLevel, FSceneRenderingBulkObjectAllocator& InBulkAllocator) :
	OneFrameResources(InBulkAllocator),
	PrimitiveSceneProxy(NULL),
	DynamicIndexBuffer(nullptr),
	DynamicVertexBuffer(nullptr),
	DynamicReadBuffer(nullptr),
	FeatureLevel(InFeatureLevel),
	bUseAsyncTasks(FApp::ShouldUseThreadingForPerformance() && CVarUseParallelGetDynamicMeshElementsTasks.GetValueOnAnyThread() > 0)
{	
}

FMeshElementCollector::~FMeshElementCollector()
{
	DeleteTemporaryProxies();
}

void FMeshElementCollector::DeleteTemporaryProxies()
{
	check(!ParallelTasks.Num()); // We should have blocked on this already
	for (int32 ProxyIndex = 0; ProxyIndex < TemporaryProxies.Num(); ProxyIndex++)
	{
		delete TemporaryProxies[ProxyIndex];
	}

	TemporaryProxies.Empty();
}

void FMeshElementCollector::SetPrimitive(const FPrimitiveSceneProxy* InPrimitiveSceneProxy, FHitProxyId DefaultHitProxyId)
{
	check(InPrimitiveSceneProxy);
	PrimitiveSceneProxy = InPrimitiveSceneProxy;

	for (int32 ViewIndex = 0; ViewIndex < SimpleElementCollectors.Num(); ViewIndex++)
	{
		SimpleElementCollectors[ViewIndex]->HitProxyId = DefaultHitProxyId;
		SimpleElementCollectors[ViewIndex]->PrimitiveMeshId = 0;
	}

	for (int32 ViewIndex = 0; ViewIndex < MeshIdInPrimitivePerView.Num(); ++ViewIndex)
	{
		MeshIdInPrimitivePerView[ViewIndex] = 0;
	}
}

void FMeshElementCollector::ClearViewMeshArrays()
{
	Views.Empty();
	MeshBatches.Empty();
	SimpleElementCollectors.Empty();
	MeshIdInPrimitivePerView.Empty();
	DynamicPrimitiveCollectorPerView.Empty();
	NumMeshBatchElementsPerView.Empty();
	DynamicIndexBuffer = nullptr;
	DynamicVertexBuffer = nullptr;
	DynamicReadBuffer = nullptr;
}

void FMeshElementCollector::AddViewMeshArrays(
	FSceneView* InView,
	TArray<FMeshBatchAndRelevance, SceneRenderingAllocator>* ViewMeshes,
	FSimpleElementCollector* ViewSimpleElementCollector,
	FGPUScenePrimitiveCollector* InDynamicPrimitiveCollector,
	ERHIFeatureLevel::Type InFeatureLevel,
	FGlobalDynamicIndexBuffer* InDynamicIndexBuffer,
	FGlobalDynamicVertexBuffer* InDynamicVertexBuffer,
	FGlobalDynamicReadBuffer* InDynamicReadBuffer)
{
	Views.Add(InView);
	MeshIdInPrimitivePerView.Add(0);
	MeshBatches.Add(ViewMeshes);
	NumMeshBatchElementsPerView.Add(0);
	SimpleElementCollectors.Add(ViewSimpleElementCollector);
	DynamicPrimitiveCollectorPerView.Add(InDynamicPrimitiveCollector);

	check(InDynamicIndexBuffer && InDynamicVertexBuffer && InDynamicReadBuffer);
	DynamicIndexBuffer = InDynamicIndexBuffer;
	DynamicVertexBuffer = InDynamicVertexBuffer;
	DynamicReadBuffer = InDynamicReadBuffer;
}

void FMeshElementCollector::ProcessTasks()
{
	check(IsInRenderingThread());
	check(!ParallelTasks.Num() || bUseAsyncTasks);

	if (ParallelTasks.Num())
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FMeshElementCollector_ProcessTasks);
		TArray<TFunction<void()>, SceneRenderingAllocator>& LocalParallelTasks(ParallelTasks);
		ParallelFor(ParallelTasks.Num(), 
			[&LocalParallelTasks](int32 Index)
			{
				LocalParallelTasks[Index]();
				LocalParallelTasks[Index] = {};
			});
		ParallelTasks.Empty();
	}
}

void FMeshElementCollector::AddMesh(int32 ViewIndex, FMeshBatch& MeshBatch)
{
	DEFINE_LOG_CATEGORY_STATIC(FMeshElementCollector_AddMesh, Warning, All);

	if (MeshBatch.bCanApplyViewModeOverrides)
	{
		FSceneView* View = Views[ViewIndex];

		ApplyViewModeOverrides(
			ViewIndex,
			View->Family->EngineShowFlags,
			View->GetFeatureLevel(),
			PrimitiveSceneProxy,
			MeshBatch.bUseWireframeSelectionColoring,
			MeshBatch,
			*this);
	}

	if (!MeshBatch.Validate(PrimitiveSceneProxy, FeatureLevel))
	{
		return;
	}

	MeshBatch.PreparePrimitiveUniformBuffer(PrimitiveSceneProxy, FeatureLevel);

	// If we are maintaining primitive scene data on the GPU, copy the primitive uniform buffer data to a unified array so it can be uploaded later
	if (UseGPUScene(GMaxRHIShaderPlatform, FeatureLevel) && MeshBatch.VertexFactory->GetPrimitiveIdStreamIndex(FeatureLevel, EVertexInputStreamType::Default) >= 0)
	{
		for (int32 Index = 0; Index < MeshBatch.Elements.Num(); ++Index)
		{
			const TUniformBuffer<FPrimitiveUniformShaderParameters>* PrimitiveUniformBufferResource = MeshBatch.Elements[Index].PrimitiveUniformBufferResource;
			if (PrimitiveUniformBufferResource)
			{
				FMeshBatchElement& Element = MeshBatch.Elements[Index];

				Element.PrimitiveIdMode = PrimID_DynamicPrimitiveShaderData;
				DynamicPrimitiveCollectorPerView[ViewIndex]->Add(
					Element.DynamicPrimitiveData,
					*reinterpret_cast<const FPrimitiveUniformShaderParameters*>(PrimitiveUniformBufferResource->GetContents()),
					Element.NumInstances,
					Element.DynamicPrimitiveIndex,
					Element.DynamicPrimitiveInstanceSceneDataOffset);
			}
		}
	}

	MeshBatch.MaterialRenderProxy->UpdateUniformExpressionCacheIfNeeded(Views[ViewIndex]->GetFeatureLevel());

	MeshBatch.MeshIdInPrimitive = MeshIdInPrimitivePerView[ViewIndex];
	++MeshIdInPrimitivePerView[ViewIndex];

	NumMeshBatchElementsPerView[ViewIndex] += MeshBatch.Elements.Num();

	TArray<FMeshBatchAndRelevance,SceneRenderingAllocator>& ViewMeshBatches = *MeshBatches[ViewIndex];
	new (ViewMeshBatches) FMeshBatchAndRelevance(MeshBatch, PrimitiveSceneProxy, FeatureLevel);	
}

FDynamicPrimitiveUniformBuffer::FDynamicPrimitiveUniformBuffer() = default;
FDynamicPrimitiveUniformBuffer::~FDynamicPrimitiveUniformBuffer()
{
	UniformBuffer.ReleaseResource();
}

void FDynamicPrimitiveUniformBuffer::Set(
	const FMatrix& LocalToWorld,
	const FMatrix& PreviousLocalToWorld,
	const FVector& ActorPositionWS,
	const FBoxSphereBounds& WorldBounds,
	const FBoxSphereBounds& LocalBounds,
	const FBoxSphereBounds& PreSkinnedLocalBounds,
	bool bReceivesDecals,
	bool bHasPrecomputedVolumetricLightmap,
	bool bOutputVelocity,
	const FCustomPrimitiveData* CustomPrimitiveData)
{
	check(IsInRenderingThread());
	UniformBuffer.SetContents(
		FPrimitiveUniformShaderParametersBuilder{}
		.Defaults()
			.LocalToWorld(LocalToWorld)
			.PreviousLocalToWorld(PreviousLocalToWorld)
			.ActorWorldPosition(ActorPositionWS)
			.WorldBounds(WorldBounds)
			.LocalBounds(LocalBounds)
			.PreSkinnedLocalBounds(PreSkinnedLocalBounds)
			.ReceivesDecals(bReceivesDecals)
			.OutputVelocity(bOutputVelocity)
			.UseVolumetricLightmap(bHasPrecomputedVolumetricLightmap)
			.CustomPrimitiveData(CustomPrimitiveData)
		.Build()
	);
	UniformBuffer.InitResource();
}

void FDynamicPrimitiveUniformBuffer::Set(
	const FMatrix& LocalToWorld,
	const FMatrix& PreviousLocalToWorld,
	const FBoxSphereBounds& WorldBounds,
	const FBoxSphereBounds& LocalBounds,
	const FBoxSphereBounds& PreSkinnedLocalBounds,
	bool bReceivesDecals,
	bool bHasPrecomputedVolumetricLightmap,
	bool bOutputVelocity,
	const FCustomPrimitiveData* CustomPrimitiveData)
{
	Set(LocalToWorld, PreviousLocalToWorld, WorldBounds.Origin, WorldBounds, LocalBounds, PreSkinnedLocalBounds, bReceivesDecals, bHasPrecomputedVolumetricLightmap, bOutputVelocity, CustomPrimitiveData);
}

void FDynamicPrimitiveUniformBuffer::Set(
	const FMatrix& LocalToWorld,
	const FMatrix& PreviousLocalToWorld,
	const FBoxSphereBounds& WorldBounds,
	const FBoxSphereBounds& LocalBounds,
	const FBoxSphereBounds& PreSkinnedLocalBounds,
	bool bReceivesDecals,
	bool bHasPrecomputedVolumetricLightmap,
	bool bOutputVelocity)
{
	Set(LocalToWorld, PreviousLocalToWorld, WorldBounds, LocalBounds, PreSkinnedLocalBounds, bReceivesDecals, bHasPrecomputedVolumetricLightmap, bOutputVelocity, nullptr);
}

void FDynamicPrimitiveUniformBuffer::Set(
	const FMatrix& LocalToWorld,
	const FMatrix& PreviousLocalToWorld,
	const FBoxSphereBounds& WorldBounds,
	const FBoxSphereBounds& LocalBounds,
	bool bReceivesDecals,
	bool bHasPrecomputedVolumetricLightmap,
	bool bOutputVelocity)
{
	Set(LocalToWorld, PreviousLocalToWorld, WorldBounds, LocalBounds, LocalBounds, bReceivesDecals, bHasPrecomputedVolumetricLightmap, bOutputVelocity, nullptr);
}

FLightMapInteraction FLightMapInteraction::Texture(
	const class ULightMapTexture2D* const* InTextures,
	const ULightMapTexture2D* InSkyOcclusionTexture,
	const ULightMapTexture2D* InAOMaterialMaskTexture,
	const FVector4f* InCoefficientScales,
	const FVector4f* InCoefficientAdds,
	const FVector2D& InCoordinateScale,
	const FVector2D& InCoordinateBias,
	bool bUseHighQualityLightMaps)
{
	FLightMapInteraction Result;
	Result.Type = LMIT_Texture;

#if ALLOW_LQ_LIGHTMAPS && ALLOW_HQ_LIGHTMAPS
	// however, if simple and directional are allowed, then we must use the value passed in,
	// and then cache the number as well
	Result.bAllowHighQualityLightMaps = bUseHighQualityLightMaps;
	if (bUseHighQualityLightMaps)
	{
		Result.NumLightmapCoefficients = NUM_HQ_LIGHTMAP_COEF;
	}
	else
	{
		Result.NumLightmapCoefficients = NUM_LQ_LIGHTMAP_COEF;
	}
#endif

	//copy over the appropriate textures and scales
	if (bUseHighQualityLightMaps)
	{
#if ALLOW_HQ_LIGHTMAPS
		Result.HighQualityTexture = InTextures[0];
		Result.SkyOcclusionTexture = InSkyOcclusionTexture;
		Result.AOMaterialMaskTexture = InAOMaterialMaskTexture;
		for(uint32 CoefficientIndex = 0;CoefficientIndex < NUM_HQ_LIGHTMAP_COEF;CoefficientIndex++)
		{
			Result.HighQualityCoefficientScales[CoefficientIndex] = InCoefficientScales[CoefficientIndex];
			Result.HighQualityCoefficientAdds[CoefficientIndex] = InCoefficientAdds[CoefficientIndex];
		}
#endif
	}

	// NOTE: In PC editor we cache both Simple and Directional textures as we may need to dynamically switch between them
	if( GIsEditor || !bUseHighQualityLightMaps )
	{
#if ALLOW_LQ_LIGHTMAPS
		Result.LowQualityTexture = InTextures[1];
		for(uint32 CoefficientIndex = 0;CoefficientIndex < NUM_LQ_LIGHTMAP_COEF;CoefficientIndex++)
		{
			Result.LowQualityCoefficientScales[CoefficientIndex] = InCoefficientScales[ LQ_LIGHTMAP_COEF_INDEX + CoefficientIndex ];
			Result.LowQualityCoefficientAdds[CoefficientIndex] = InCoefficientAdds[ LQ_LIGHTMAP_COEF_INDEX + CoefficientIndex ];
		}
#endif
	}

	Result.CoordinateScale = InCoordinateScale;
	Result.CoordinateBias = InCoordinateBias;
	return Result;
}

FLightMapInteraction FLightMapInteraction::InitVirtualTexture(
	const ULightMapVirtualTexture2D* VirtualTexture,
	const FVector4f* InCoefficientScales,
	const FVector4f* InCoefficientAdds,
	const FVector2D& InCoordinateScale,
	const FVector2D& InCoordinateBias,
	bool bAllowHighQualityLightMaps)
{
	FLightMapInteraction Result;
	Result.Type = LMIT_Texture;

#if ALLOW_LQ_LIGHTMAPS && ALLOW_HQ_LIGHTMAPS
	// however, if simple and directional are allowed, then we must use the value passed in,
	// and then cache the number as well
	Result.bAllowHighQualityLightMaps = bAllowHighQualityLightMaps;
	if (bAllowHighQualityLightMaps)
	{
		Result.NumLightmapCoefficients = NUM_HQ_LIGHTMAP_COEF;
	}
	else
	{
		Result.NumLightmapCoefficients = NUM_LQ_LIGHTMAP_COEF;
	}
#endif

	//copy over the appropriate textures and scales
	if (bAllowHighQualityLightMaps)
	{
#if ALLOW_HQ_LIGHTMAPS
		Result.VirtualTexture = VirtualTexture;
		for (uint32 CoefficientIndex = 0; CoefficientIndex < NUM_HQ_LIGHTMAP_COEF; CoefficientIndex++)
		{
			Result.HighQualityCoefficientScales[CoefficientIndex] = InCoefficientScales[CoefficientIndex];
			Result.HighQualityCoefficientAdds[CoefficientIndex] = InCoefficientAdds[CoefficientIndex];
		}
#endif
	}

	// NOTE: In PC editor we cache both Simple and Directional textures as we may need to dynamically switch between them
	if (GIsEditor || !bAllowHighQualityLightMaps)
	{
#if ALLOW_LQ_LIGHTMAPS
		Result.VirtualTexture = VirtualTexture;
		for (uint32 CoefficientIndex = 0; CoefficientIndex < NUM_LQ_LIGHTMAP_COEF; CoefficientIndex++)
		{
			Result.LowQualityCoefficientScales[CoefficientIndex] = InCoefficientScales[LQ_LIGHTMAP_COEF_INDEX + CoefficientIndex];
			Result.LowQualityCoefficientAdds[CoefficientIndex] = InCoefficientAdds[LQ_LIGHTMAP_COEF_INDEX + CoefficientIndex];
		}
#endif
	}

	Result.CoordinateScale = InCoordinateScale;
	Result.CoordinateBias = InCoordinateBias;
	return Result;
}

float ComputeBoundsScreenRadiusSquared(const FVector4& BoundsOrigin, const float SphereRadius, const FVector4& ViewOrigin, const FMatrix& ProjMatrix)
{
	// ignore perspective foreshortening for orthographic projections
	const float DistSqr = FVector::DistSquared(BoundsOrigin, ViewOrigin) * ProjMatrix.M[2][3];

	// Get projection multiple accounting for view scaling.
	const float ScreenMultiple = FMath::Max(0.5f * ProjMatrix.M[0][0], 0.5f * ProjMatrix.M[1][1]);

	// Calculate screen-space projected radius
	return FMath::Square(ScreenMultiple * SphereRadius) / FMath::Max(1.0f, DistSqr);
}

/** Runtime comparison version of ComputeTemporalLODBoundsScreenSize that avoids a square root */
static float ComputeTemporalLODBoundsScreenRadiusSquared(const FVector& Origin, const float SphereRadius, const FSceneView& View, int32 SampleIndex)
{
	return ComputeBoundsScreenRadiusSquared(Origin, SphereRadius, View.GetTemporalLODOrigin(SampleIndex), View.ViewMatrices.GetProjectionMatrix());
}

float ComputeBoundsScreenRadiusSquared(const FVector4& Origin, const float SphereRadius, const FSceneView& View)
{
	return ComputeBoundsScreenRadiusSquared(Origin, SphereRadius, View.ViewMatrices.GetViewOrigin(), View.ViewMatrices.GetProjectionMatrix());
}

float ComputeBoundsScreenSize( const FVector4& Origin, const float SphereRadius, const FSceneView& View )
{
	return ComputeBoundsScreenSize(Origin, SphereRadius, View.ViewMatrices.GetViewOrigin(), View.ViewMatrices.GetProjectionMatrix());
}

float ComputeTemporalLODBoundsScreenSize( const FVector& Origin, const float SphereRadius, const FSceneView& View, int32 SampleIndex )
{
	return ComputeBoundsScreenSize(Origin, SphereRadius, View.GetTemporalLODOrigin(SampleIndex), View.ViewMatrices.GetProjectionMatrix());
}

float ComputeBoundsScreenSize(const FVector4& BoundsOrigin, const float SphereRadius, const FVector4& ViewOrigin, const FMatrix& ProjMatrix)
{
	const float Dist = FVector::Dist(BoundsOrigin, ViewOrigin);

	// Get projection multiple accounting for view scaling.
	const float ScreenMultiple = FMath::Max(0.5f * ProjMatrix.M[0][0], 0.5f * ProjMatrix.M[1][1]);

	// Calculate screen-space projected radius
	const float ScreenRadius = ScreenMultiple * SphereRadius / FMath::Max(1.0f, Dist);

	// For clarity, we end up comparing the diameter
	return ScreenRadius * 2.0f;
}

float ComputeBoundsDrawDistance(const float ScreenSize, const float SphereRadius, const FMatrix& ProjMatrix)
{
	// Get projection multiple accounting for view scaling.
	const float ScreenMultiple = FMath::Max(0.5f * ProjMatrix.M[0][0], 0.5f * ProjMatrix.M[1][1]);

	// ScreenSize is the projected diameter, so halve it
	const float ScreenRadius = FMath::Max(UE_SMALL_NUMBER, ScreenSize * 0.5f);

	// Invert the calcs in ComputeBoundsScreenSize
	return (ScreenMultiple * SphereRadius) / ScreenRadius;
}

int8 ComputeTemporalStaticMeshLOD( const FStaticMeshRenderData* RenderData, const FVector4& Origin, const float SphereRadius, const FSceneView& View, int32 MinLOD, float FactorScale, int32 SampleIndex )
{
	const int32 NumLODs = MAX_STATIC_MESH_LODS;

	const float ScreenRadiusSquared = ComputeTemporalLODBoundsScreenRadiusSquared(Origin, SphereRadius, View, SampleIndex);
	const float ScreenSizeScale = FactorScale * View.LODDistanceFactor;

	// Walk backwards and return the first matching LOD
	for(int32 LODIndex = NumLODs - 1 ; LODIndex >= 0 ; --LODIndex)
	{
		const float MeshScreenSize = RenderData->ScreenSize[LODIndex].GetValue() * ScreenSizeScale;
		
		if(FMath::Square(MeshScreenSize * 0.5f) > ScreenRadiusSquared)
		{
			return FMath::Max(LODIndex, MinLOD);
		}
	}

	return MinLOD;
}

// Ensure we always use the left eye when selecting lods to avoid divergent selections in stereo
const FSceneView& GetLODView(const FSceneView& InView)
{
	if (IStereoRendering::IsStereoEyeView(InView) && GEngine->StereoRenderingDevice.IsValid())
	{
		uint32 LODViewIndex = GEngine->StereoRenderingDevice->GetLODViewIndex();
		if (InView.Family && InView.Family->Views.IsValidIndex(LODViewIndex))
		{
			return *InView.Family->Views[LODViewIndex];
		}
	}

	return InView;
}

int8 ComputeStaticMeshLOD( const FStaticMeshRenderData* RenderData, const FVector4& Origin, const float SphereRadius, const FSceneView& View, int32 MinLOD, float FactorScale )
{
	if (RenderData)
	{
		const int32 NumLODs = MAX_STATIC_MESH_LODS;
		const FSceneView& LODView = GetLODView(View);
		const float ScreenRadiusSquared = ComputeBoundsScreenRadiusSquared(Origin, SphereRadius, LODView);
		const float ScreenSizeScale = FactorScale * LODView.LODDistanceFactor;

		// Walk backwards and return the first matching LOD
		for (int32 LODIndex = NumLODs - 1; LODIndex >= 0; --LODIndex)
		{
			float MeshScreenSize = RenderData->ScreenSize[LODIndex].GetValue() * ScreenSizeScale;

			if (FMath::Square(MeshScreenSize * 0.5f) > ScreenRadiusSquared)
			{
				return FMath::Max(LODIndex, MinLOD);
			}
		}
	}

	return MinLOD;
}

FLODMask ComputeLODForMeshes(const TArray<class FStaticMeshBatchRelevance>& StaticMeshRelevances, const FSceneView& View, const FVector4& Origin, float SphereRadius, int32 ForcedLODLevel, float& OutScreenRadiusSquared, int8 CurFirstLODIdx, float ScreenSizeScale, bool bDitheredLODTransition)
{
	FLODMask LODToRender;
	const FSceneView& LODView = GetLODView(View);

	const int32 NumMeshes = StaticMeshRelevances.Num();

	// Handle forced LOD level first
	if (ForcedLODLevel >= 0)
	{
		OutScreenRadiusSquared = 0.0f;

		int32 MinLOD = 127, MaxLOD = 0;
		for (int32 MeshIndex = 0; MeshIndex < StaticMeshRelevances.Num(); ++MeshIndex)
		{
			const FStaticMeshBatchRelevance& Mesh = StaticMeshRelevances[MeshIndex];
			if (Mesh.ScreenSize > 0.0f)
			{
				MinLOD = FMath::Min(MinLOD, (int32)Mesh.LODIndex);
				MaxLOD = FMath::Max(MaxLOD, (int32)Mesh.LODIndex);
			}
		}
		MinLOD = FMath::Max(MinLOD, (int32)CurFirstLODIdx);
		LODToRender.SetLOD(FMath::Clamp(ForcedLODLevel, MinLOD, MaxLOD));
	}
	else if (LODView.Family->EngineShowFlags.LOD && NumMeshes)
	{
		if (bDitheredLODTransition && StaticMeshRelevances[0].bDitheredLODTransition)
		{
			for (int32 SampleIndex = 0; SampleIndex < 2; SampleIndex++)
			{
				int32 MinLODFound = INT_MAX;
				bool bFoundLOD = false;
				OutScreenRadiusSquared = ComputeTemporalLODBoundsScreenRadiusSquared(Origin, SphereRadius, LODView, SampleIndex);

				for (int32 MeshIndex = NumMeshes - 1; MeshIndex >= 0; --MeshIndex)
				{
					const FStaticMeshBatchRelevance& Mesh = StaticMeshRelevances[MeshIndex];
					if (Mesh.ScreenSize > 0.0f)
					{
						float MeshScreenSize = Mesh.ScreenSize * ScreenSizeScale;

						if (FMath::Square(MeshScreenSize * 0.5f) >= OutScreenRadiusSquared)
						{
							LODToRender.SetLODSample(Mesh.LODIndex, SampleIndex);
							bFoundLOD = true;
							break;
						}

						MinLODFound = FMath::Min<int32>(MinLODFound, Mesh.LODIndex);
					}
				}
				// If no LOD was found matching the screen size, use the lowest in the array instead of LOD 0, to handle non-zero MinLOD
				if (!bFoundLOD)
				{
					LODToRender.SetLODSample(MinLODFound, SampleIndex);
				}
			}
		}
		else
		{
			int32 MinLODFound = INT_MAX;
			bool bFoundLOD = false;
			OutScreenRadiusSquared = ComputeBoundsScreenRadiusSquared(Origin, SphereRadius, LODView);

			for (int32 MeshIndex = NumMeshes - 1; MeshIndex >= 0; --MeshIndex)
			{
				const FStaticMeshBatchRelevance& Mesh = StaticMeshRelevances[MeshIndex];

				float MeshScreenSize = Mesh.ScreenSize * ScreenSizeScale;

				if (FMath::Square(MeshScreenSize * 0.5f) >= OutScreenRadiusSquared)
				{
					LODToRender.SetLOD(Mesh.LODIndex);
					bFoundLOD = true;
					break;
				}

				MinLODFound = FMath::Min<int32>(MinLODFound, Mesh.LODIndex);
			}
			// If no LOD was found matching the screen size, use the lowest in the array instead of LOD 0, to handle non-zero MinLOD
			if (!bFoundLOD)
			{
				LODToRender.SetLOD(MinLODFound);
			}
		}
		LODToRender.ClampToFirstLOD(CurFirstLODIdx);
	}
	return LODToRender;
}

FMobileDirectionalLightShaderParameters::FMobileDirectionalLightShaderParameters()
{
	FMemory::Memzero(*this);

	// light, default to black
	DirectionalLightColor = FLinearColor::Black;
	DirectionalLightDirectionAndShadowTransition = FVector4f(EForceInit::ForceInitToZero);
	DirectionalLightShadowMapChannelMask = 0xFF;

	// white texture should act like a shadowmap cleared to the farplane.
	DirectionalLightShadowTexture = GWhiteTexture->TextureRHI;
	DirectionalLightShadowSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	DirectionalLightShadowSize = FVector4f(EForceInit::ForceInitToZero);
	DirectionalLightDistanceFadeMADAndSpecularScale = FVector4f(EForceInit::ForceInitToZero);
	for (int32 i = 0; i < MAX_MOBILE_SHADOWCASCADES; ++i)
	{
		DirectionalLightScreenToShadow[i].SetIdentity();
		DirectionalLightShadowDistances[i] = 0.0f;
	}
}

FViewUniformShaderParameters::FViewUniformShaderParameters()
{
	FMemory::Memzero(*this);

	FRHITexture* BlackVolume = (GBlackVolumeTexture &&  GBlackVolumeTexture->TextureRHI) ? GBlackVolumeTexture->TextureRHI : GBlackTexture->TextureRHI;
	FRHITexture* BlackUintVolume = (GBlackUintVolumeTexture &&  GBlackUintVolumeTexture->TextureRHI) ? GBlackUintVolumeTexture->TextureRHI : GBlackTexture->TextureRHI;
	check(GBlackVolumeTexture);

	MaterialTextureBilinearClampedSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	MaterialTextureBilinearWrapedSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();

	VolumetricLightmapIndirectionTexture = BlackUintVolume;
	VolumetricLightmapBrickAmbientVector = BlackVolume;
	VolumetricLightmapBrickSHCoefficients0 = BlackVolume;
	VolumetricLightmapBrickSHCoefficients1 = BlackVolume;
	VolumetricLightmapBrickSHCoefficients2 = BlackVolume;
	VolumetricLightmapBrickSHCoefficients3 = BlackVolume;
	VolumetricLightmapBrickSHCoefficients4 = BlackVolume;
	VolumetricLightmapBrickSHCoefficients5 = BlackVolume;
	SkyBentNormalBrickTexture = BlackVolume;
	DirectionalLightShadowingBrickTexture = BlackVolume;

	VolumetricLightmapBrickAmbientVectorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	VolumetricLightmapTextureSampler0 = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	VolumetricLightmapTextureSampler1 = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	VolumetricLightmapTextureSampler2 = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	VolumetricLightmapTextureSampler3 = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	VolumetricLightmapTextureSampler4 = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	VolumetricLightmapTextureSampler5 = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	SkyBentNormalTextureSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	DirectionalLightShadowingTextureSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	AtmosphereTransmittanceTexture = GWhiteTexture->TextureRHI;
	AtmosphereTransmittanceTextureSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
	AtmosphereIrradianceTexture = GWhiteTexture->TextureRHI;
	AtmosphereIrradianceTextureSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
	AtmosphereInscatterTexture = BlackVolume;
	AtmosphereInscatterTextureSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();

	PerlinNoiseGradientTexture = GWhiteTexture->TextureRHI;
	PerlinNoiseGradientTextureSampler = TStaticSamplerState<SF_Point, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();

	PerlinNoise3DTexture = BlackVolume;
	PerlinNoise3DTextureSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();

	SobolSamplingTexture = GWhiteTexture->TextureRHI;

	GlobalDistanceFieldPageAtlasTexture = BlackVolume;
	GlobalDistanceFieldCoverageAtlasTexture = BlackVolume;
	GlobalDistanceFieldPageTableTexture = BlackVolume;
	GlobalDistanceFieldMipTexture = BlackVolume;

	SharedPointWrappedSampler = TStaticSamplerState<SF_Point, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
	SharedPointClampedSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	SharedBilinearWrappedSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
	SharedBilinearClampedSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	SharedBilinearAnisoClampedSampler = TStaticSamplerState<SF_AnisotropicLinear, AM_Clamp, AM_Clamp, AM_Clamp, 0, 0>::GetRHI();
	SharedTrilinearWrappedSampler = TStaticSamplerState<SF_Trilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
	SharedTrilinearClampedSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	PreIntegratedBRDF = GWhiteTexture->TextureRHI;
	PreIntegratedBRDFSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	TransmittanceLutTexture = GWhiteTexture->TextureRHI;
	TransmittanceLutTextureSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();

	SkyViewLutTexture = GBlackTexture->TextureRHI;
	SkyViewLutTextureSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();

	DistantSkyLightLutTexture = GBlackTexture->TextureRHI;
	DistantSkyLightLutTextureSampler = TStaticSamplerState<SF_Point, AM_Wrap, AM_Wrap>::GetRHI();

	CameraAerialPerspectiveVolume = GBlackAlpha1VolumeTexture->TextureRHI;
	CameraAerialPerspectiveVolumeSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
	CameraAerialPerspectiveVolumeMieOnly = GBlackAlpha1VolumeTexture->TextureRHI;
	CameraAerialPerspectiveVolumeMieOnlySampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
	CameraAerialPerspectiveVolumeRayOnly = GBlackAlpha1VolumeTexture->TextureRHI;
	CameraAerialPerspectiveVolumeRayOnlySampler = TStaticSamplerState<SF_Bilinear>::GetRHI();

	PrimitiveSceneData = GIdentityPrimitiveBuffer.PrimitiveSceneDataBufferSRV;
	InstanceSceneData = GIdentityPrimitiveBuffer.InstanceSceneDataBufferSRV;
	InstancePayloadData = GIdentityPrimitiveBuffer.InstancePayloadDataBufferSRV;
	LightmapSceneData = GIdentityPrimitiveBuffer.LightmapSceneDataBufferSRV;

	SkyIrradianceEnvironmentMap = GIdentityPrimitiveBuffer.SkyIrradianceEnvironmentMapSRV;

	PhysicsFieldClipmapBuffer = GWhiteVertexBufferWithSRV->ShaderResourceViewRHI;

	// Water
	WaterIndirection = GWhiteVertexBufferWithSRV->ShaderResourceViewRHI;
	WaterData = GWhiteVertexBufferWithSRV->ShaderResourceViewRHI;

	// Landscape
	LandscapeWeightmapSampler = TStaticSamplerState<SF_AnisotropicPoint, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	LandscapeIndirection = GWhiteVertexBufferWithSRV->ShaderResourceViewRHI;
	LandscapePerComponentData = GWhiteVertexBufferWithSRV->ShaderResourceViewRHI;

	// Hair
	HairScatteringLUTTexture = BlackVolume;
	HairScatteringLUTSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();

	// Rect area light
	LTCMatTexture = GBlackTextureWithSRV->TextureRHI;
	LTCMatSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	LTCAmpTexture = GBlackTextureWithSRV->TextureRHI;
	LTCAmpSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	// Shading energy conservation
	bShadingEnergyConservation = 0u;
	bShadingEnergyPreservation = 0u;
	ShadingEnergySampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	ShadingEnergyGGXSpecTexture = GBlackTextureWithSRV->TextureRHI;
	ShadingEnergyGGXGlassTexture = BlackVolume;
	ShadingEnergyClothSpecTexture = GBlackTextureWithSRV->TextureRHI;
	ShadingEnergyDiffuseTexture = GBlackTextureWithSRV->TextureRHI;

	// Rect light atlas
	RectLightAtlasMaxMipLevel = 1;
	RectLightAtlasSizeAndInvSize = FVector4f(1, 1, 1, 1);
	RectLightAtlasTexture = GBlackTextureWithSRV->TextureRHI;
	RectLightAtlasSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	// IES atlas
	IESAtlasSizeAndInvSize = FVector4f(1, 1, 1, 1);
	IESAtlasTexture = GBlackTextureWithSRV->TextureRHI;
	IESAtlasSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	// Subsurface profiles/pre-intregrated
	SSProfilesTextureSizeAndInvSize = FVector4f(1.f,1.f,1.f,1.f);
	SSProfilesTexture = GBlackTextureWithSRV->TextureRHI;
	SSProfilesSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();;
	SSProfilesTransmissionSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	SSProfilesPreIntegratedTextureSizeAndInvSize = FVector4f(1.f,1.f,1.f,1.f);
	SSProfilesPreIntegratedTexture = GBlackArrayTexture->TextureRHI;
	SSProfilesPreIntegratedSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	//this can be deleted once sm4 support is removed.
	if (!PrimitiveSceneData)
	{
		PrimitiveSceneData = GBlackTextureWithSRV->ShaderResourceViewRHI;
	}
	if (!InstanceSceneData)
	{
		InstanceSceneData = GBlackTextureWithSRV->ShaderResourceViewRHI;
	}
	if (!InstancePayloadData)
	{
		InstancePayloadData = GBlackTextureWithSRV->ShaderResourceViewRHI;
	}
	if (!LightmapSceneData)
	{
		LightmapSceneData = GBlackTextureWithSRV->ShaderResourceViewRHI;
	}
	VTFeedbackBuffer = GEmptyVertexBufferWithUAV->UnorderedAccessViewRHI;
//#if WITH_EDITOR
	EditorVisualizeLevelInstanceIds = GIdentityPrimitiveBuffer.EditorVisualizeLevelInstanceDataBufferSRV;
	EditorSelectedHitProxyIds = GIdentityPrimitiveBuffer.EditorSelectedDataBufferSRV;
//#endif
}

FInstancedViewUniformShaderParameters::FInstancedViewUniformShaderParameters()
{
	FMemory::Memzero(*this);
}

void FSharedSamplerState::InitRHI()
{
	const float MipMapBias = UTexture2D::GetGlobalMipMapLODBias();

	FSamplerStateInitializerRHI SamplerStateInitializer
	(
	(ESamplerFilter)UDeviceProfileManager::Get().GetActiveProfile()->GetTextureLODSettings()->GetSamplerFilter(TEXTUREGROUP_World),
		bWrap ? AM_Wrap : AM_Clamp,
		bWrap ? AM_Wrap : AM_Clamp,
		bWrap ? AM_Wrap : AM_Clamp,
		MipMapBias
	);
	SamplerStateRHI = RHICreateSamplerState(SamplerStateInitializer);
}

FSharedSamplerState* Wrap_WorldGroupSettings = NULL;
FSharedSamplerState* Clamp_WorldGroupSettings = NULL;

void InitializeSharedSamplerStates()
{
	if (!Wrap_WorldGroupSettings)
	{
		Wrap_WorldGroupSettings = new FSharedSamplerState(true);
		Clamp_WorldGroupSettings = new FSharedSamplerState(false);
		BeginInitResource(Wrap_WorldGroupSettings);
		BeginInitResource(Clamp_WorldGroupSettings);
	}
}

void FLightCacheInterface::CreatePrecomputedLightingUniformBuffer_RenderingThread(ERHIFeatureLevel::Type FeatureLevel)
{
	const bool bPrecomputedLightingParametersFromGPUScene = UseGPUScene(GMaxRHIShaderPlatform, FeatureLevel) && bCanUsePrecomputedLightingParametersFromGPUScene;

	// Only create UB when GPUScene isn't available
	if (!bPrecomputedLightingParametersFromGPUScene && (LightMap || ShadowMap))
	{
		FPrecomputedLightingUniformParameters Parameters;
		GetPrecomputedLightingParameters(FeatureLevel, Parameters, this);
		if (PrecomputedLightingUniformBuffer)
		{
			// Don't recreate the buffer if it already exists
			RHIUpdateUniformBuffer(PrecomputedLightingUniformBuffer, &Parameters);
		}
		else
		{
			PrecomputedLightingUniformBuffer = FPrecomputedLightingUniformParameters::CreateUniformBuffer(Parameters, UniformBuffer_MultiFrame);
		}
	}
}

bool FLightCacheInterface::GetVirtualTextureLightmapProducer(ERHIFeatureLevel::Type FeatureLevel, FVirtualTextureProducerHandle& OutProducerHandle)
{
	const FLightMapInteraction LightMapInteraction = GetLightMapInteraction(FeatureLevel);
	if (LightMapInteraction.GetType() == LMIT_Texture)
	{
		const ULightMapVirtualTexture2D* VirtualTexture = LightMapInteraction.GetVirtualTexture();
		// Preview lightmaps don't stream from disk, thus no FVirtualTexture2DResource
		if (VirtualTexture && !VirtualTexture->bPreviewLightmap)
		{
			FVirtualTexture2DResource* Resource = (FVirtualTexture2DResource*)VirtualTexture->GetResource();
			OutProducerHandle = Resource->GetProducerHandle();
			return true;
		}
	}
	return false;
}

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FLightmapResourceClusterShaderParameters, "LightmapResourceCluster");

static FRHISamplerState* GetTextureSamplerState(const UTexture* Texture, FRHISamplerState* Default)
{
	FRHISamplerState* Result = nullptr;
	if (Texture && Texture->GetResource())
	{
		Result = Texture->GetResource()->SamplerStateRHI;
	}
	return Result ? Result : Default;
}

void GetLightmapClusterResourceParameters(
	ERHIFeatureLevel::Type FeatureLevel, 
	const FLightmapClusterResourceInput& Input,
	const IAllocatedVirtualTexture* AllocatedVT,
	FLightmapResourceClusterShaderParameters& Parameters)
{
	const bool bAllowHighQualityLightMaps = AllowHighQualityLightmaps(FeatureLevel);

	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.VirtualTexturedLightmaps"));
	const bool bUseVirtualTextures = (CVar->GetValueOnRenderThread() != 0) && UseVirtualTexturing(FeatureLevel);

	Parameters.LightMapTexture = GBlackTexture->TextureRHI;
	Parameters.SkyOcclusionTexture = GWhiteTexture->TextureRHI;
	Parameters.AOMaterialMaskTexture = GBlackTexture->TextureRHI;
	Parameters.StaticShadowTexture = GWhiteTexture->TextureRHI;
	Parameters.VTLightMapTexture = GBlackTextureWithSRV->ShaderResourceViewRHI;
	Parameters.VTLightMapTexture_1 = GBlackTextureWithSRV->ShaderResourceViewRHI;
	Parameters.VTSkyOcclusionTexture = GWhiteTextureWithSRV->ShaderResourceViewRHI;
	Parameters.VTAOMaterialMaskTexture = GBlackTextureWithSRV->ShaderResourceViewRHI;
	Parameters.VTStaticShadowTexture = GWhiteTextureWithSRV->ShaderResourceViewRHI;
	Parameters.LightmapVirtualTexturePageTable0 = GBlackUintTexture->TextureRHI;
	Parameters.LightmapVirtualTexturePageTable1 = GBlackUintTexture->TextureRHI;
	Parameters.LightMapSampler = GBlackTexture->SamplerStateRHI;
	Parameters.LightMapSampler_1 = GBlackTexture->SamplerStateRHI;
	Parameters.SkyOcclusionSampler = GWhiteTexture->SamplerStateRHI;
	Parameters.AOMaterialMaskSampler = GBlackTexture->SamplerStateRHI;
	Parameters.StaticShadowTextureSampler = GWhiteTexture->SamplerStateRHI;

	if (bUseVirtualTextures)
	{
		// this is sometimes called with NULL input to initialize default buffer
		const ULightMapVirtualTexture2D* VirtualTexture = Input.LightMapVirtualTextures[bAllowHighQualityLightMaps ? 0 : 1];
		if (VirtualTexture && AllocatedVT)
		{
			// Bind VT here
			Parameters.VTLightMapTexture = AllocatedVT->GetPhysicalTextureSRV((uint32)ELightMapVirtualTextureType::LightmapLayer0, false);
			Parameters.VTLightMapTexture_1 = AllocatedVT->GetPhysicalTextureSRV((uint32)ELightMapVirtualTextureType::LightmapLayer1, false);

			if (VirtualTexture->HasLayerForType(ELightMapVirtualTextureType::SkyOcclusion))
			{
				Parameters.VTSkyOcclusionTexture = AllocatedVT->GetPhysicalTextureSRV((uint32)ELightMapVirtualTextureType::SkyOcclusion, false);
			}
			else
			{
				Parameters.VTSkyOcclusionTexture = GWhiteTextureWithSRV->ShaderResourceViewRHI;
			}

			if (VirtualTexture->HasLayerForType(ELightMapVirtualTextureType::AOMaterialMask))
			{
				Parameters.VTAOMaterialMaskTexture = AllocatedVT->GetPhysicalTextureSRV((uint32)ELightMapVirtualTextureType::AOMaterialMask, false);
			}
			else
			{
				Parameters.VTAOMaterialMaskTexture = GBlackTextureWithSRV->ShaderResourceViewRHI;
			}

			if (VirtualTexture->HasLayerForType(ELightMapVirtualTextureType::ShadowMask))
			{
				Parameters.VTStaticShadowTexture = AllocatedVT->GetPhysicalTextureSRV((uint32)ELightMapVirtualTextureType::ShadowMask, false);
			}
			else
			{
				Parameters.VTStaticShadowTexture = GWhiteTextureWithSRV->ShaderResourceViewRHI;
			}

			FRHITexture* PageTable0 = AllocatedVT->GetPageTableTexture(0u);
			Parameters.LightmapVirtualTexturePageTable0 = PageTable0;
			if (AllocatedVT->GetNumPageTableTextures() > 1u)
			{
				check(AllocatedVT->GetNumPageTableTextures() == 2u);
				Parameters.LightmapVirtualTexturePageTable1 = AllocatedVT->GetPageTableTexture(1u);
			}
			else
			{
				Parameters.LightmapVirtualTexturePageTable1 = PageTable0;
			}

			const uint32 MaxAniso = 4;
			Parameters.LightMapSampler = TStaticSamplerState<SF_AnisotropicLinear, AM_Clamp, AM_Clamp, AM_Clamp, 0, MaxAniso>::GetRHI();
			Parameters.LightMapSampler_1 = TStaticSamplerState<SF_AnisotropicLinear, AM_Clamp, AM_Clamp, AM_Clamp, 0, MaxAniso>::GetRHI();
			Parameters.SkyOcclusionSampler = TStaticSamplerState<SF_AnisotropicLinear, AM_Clamp, AM_Clamp, AM_Clamp, 0, MaxAniso>::GetRHI();
			Parameters.AOMaterialMaskSampler = TStaticSamplerState<SF_AnisotropicLinear, AM_Clamp, AM_Clamp, AM_Clamp, 0, MaxAniso>::GetRHI();
			Parameters.StaticShadowTextureSampler = TStaticSamplerState<SF_AnisotropicLinear, AM_Clamp, AM_Clamp, AM_Clamp, 0, MaxAniso>::GetRHI();
		}
	}
	else
	{
		const UTexture2D* LightMapTexture = Input.LightMapTextures[bAllowHighQualityLightMaps ? 0 : 1];

		Parameters.LightMapTexture = LightMapTexture ? LightMapTexture->TextureReference.TextureReferenceRHI.GetReference() : GBlackTexture->TextureRHI;
		Parameters.SkyOcclusionTexture = Input.SkyOcclusionTexture ? Input.SkyOcclusionTexture->TextureReference.TextureReferenceRHI.GetReference() : GWhiteTexture->TextureRHI;
		Parameters.AOMaterialMaskTexture = Input.AOMaterialMaskTexture ? Input.AOMaterialMaskTexture->TextureReference.TextureReferenceRHI.GetReference() : GBlackTexture->TextureRHI;

		Parameters.LightMapSampler = GetTextureSamplerState(LightMapTexture, GBlackTexture->SamplerStateRHI);
		Parameters.LightMapSampler_1 = GetTextureSamplerState(LightMapTexture, GBlackTexture->SamplerStateRHI);
		Parameters.SkyOcclusionSampler = GetTextureSamplerState(Input.SkyOcclusionTexture, GWhiteTexture->SamplerStateRHI);
		Parameters.AOMaterialMaskSampler = GetTextureSamplerState(Input.AOMaterialMaskTexture, GBlackTexture->SamplerStateRHI);

		Parameters.StaticShadowTexture = Input.ShadowMapTexture ? Input.ShadowMapTexture->TextureReference.TextureReferenceRHI.GetReference() : GWhiteTexture->TextureRHI;
		Parameters.StaticShadowTextureSampler = GetTextureSamplerState(Input.ShadowMapTexture, GWhiteTexture->SamplerStateRHI);

		Parameters.LightmapVirtualTexturePageTable0 = GBlackUintTexture->TextureRHI;
		Parameters.LightmapVirtualTexturePageTable1 = GBlackUintTexture->TextureRHI;
	}
}

void FDefaultLightmapResourceClusterUniformBuffer::InitDynamicRHI()
{
	FLightmapResourceClusterShaderParameters Parameters;
	GetLightmapClusterResourceParameters(GMaxRHIFeatureLevel, FLightmapClusterResourceInput(), nullptr, Parameters);
	SetContents(Parameters);
	Super::InitDynamicRHI();
}

/** Global uniform buffer containing the default precomputed lighting data. */
TGlobalResource< FDefaultLightmapResourceClusterUniformBuffer > GDefaultLightmapResourceClusterUniformBuffer;

FLightMapInteraction FLightCacheInterface::GetLightMapInteraction(ERHIFeatureLevel::Type InFeatureLevel) const
{
	if (bGlobalVolumeLightmap)
	{
		return FLightMapInteraction::GlobalVolume();
	}

	return LightMap ? LightMap->GetInteraction(InFeatureLevel) : FLightMapInteraction();
}

FShadowMapInteraction FLightCacheInterface::GetShadowMapInteraction(ERHIFeatureLevel::Type InFeatureLevel) const
{
	if (bGlobalVolumeLightmap)
	{
		return FShadowMapInteraction::GlobalVolume();
	}

	FShadowMapInteraction Interaction;
	if (LightMap)
	{
		// Lightmap gets the first chance to provide shadow interaction,
		// this is used if VT lightmaps are enabled, and shadowmap is packed into the same VT stack as other lightmap textures
		Interaction = LightMap->GetShadowInteraction(InFeatureLevel);
	}
	if (Interaction.GetType() == SMIT_None && ShadowMap)
	{
		Interaction = ShadowMap->GetInteraction();
	}

	return Interaction;
}

ELightInteractionType FLightCacheInterface::GetStaticInteraction(const FLightSceneProxy* LightSceneProxy, const TArray<FGuid>& IrrelevantLights) const
{
	if (bGlobalVolumeLightmap)
	{
		if (LightSceneProxy->HasStaticLighting())
		{
			return LIT_CachedLightMap;
		}
		else if (LightSceneProxy->HasStaticShadowing())
		{
			return LIT_CachedSignedDistanceFieldShadowMap2D;
		}
		else
		{
			return LIT_MAX;
		}
	}

	ELightInteractionType Ret = LIT_MAX;

	// Check if the light has static lighting or shadowing.
	if(LightSceneProxy->HasStaticShadowing())
	{
		const FGuid LightGuid = LightSceneProxy->GetLightGuid();

		if(IrrelevantLights.Contains(LightGuid))
		{
			Ret = LIT_CachedIrrelevant;
		}
		else if(LightMap && LightMap->ContainsLight(LightGuid))
		{
			Ret = LIT_CachedLightMap;
		}
		else if(ShadowMap && ShadowMap->ContainsLight(LightGuid))
		{
			Ret = LIT_CachedSignedDistanceFieldShadowMap2D;
		}
	}

	return Ret;
}

FReadOnlyCVARCache GReadOnlyCVARCache;

const FReadOnlyCVARCache& FReadOnlyCVARCache::Get()
{
	checkSlow(GReadOnlyCVARCache.bInitialized);
	return GReadOnlyCVARCache;
}

void FReadOnlyCVARCache::Init()
{
	UE_LOG(LogInit, Log, TEXT("Initializing FReadOnlyCVARCache"));
	
	static const auto CVarSupportStationarySkylight = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SupportStationarySkylight"));
	static const auto CVarSupportLowQualityLightmaps = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SupportLowQualityLightmaps"));
	static const auto CVarSupportPointLightWholeSceneShadows = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SupportPointLightWholeSceneShadows"));
	static const auto CVarSupportAllShaderPermutations = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SupportAllShaderPermutations"));	
	static const auto CVarVertexFoggingForOpaque = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.VertexFoggingForOpaque"));	
	static const auto CVarAllowStaticLighting = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllowStaticLighting"));
	static const auto CVarSupportSkyAtmosphere = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SupportSkyAtmosphere"));

	static const auto CVarMobileAllowMovableDirectionalLights = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.AllowMovableDirectionalLights"));
	static const auto CVarMobileEnableStaticAndCSMShadowReceivers = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.EnableStaticAndCSMShadowReceivers"));
	static const auto CVarMobileAllowDistanceFieldShadows = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.AllowDistanceFieldShadows"));
	static const auto CVarMobileSkyLightPermutation = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.SkyLightPermutation"));
	static const auto CVarMobileEnableNoPrecomputedLightingCSMShader = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.EnableNoPrecomputedLightingCSMShader"));

	const bool bForceAllPermutations = CVarSupportAllShaderPermutations && CVarSupportAllShaderPermutations->GetValueOnAnyThread() != 0;

	bEnableStationarySkylight = !CVarSupportStationarySkylight || CVarSupportStationarySkylight->GetValueOnAnyThread() != 0 || bForceAllPermutations;
	bEnablePointLightShadows = !CVarSupportPointLightWholeSceneShadows || CVarSupportPointLightWholeSceneShadows->GetValueOnAnyThread() != 0 || bForceAllPermutations;
	bEnableLowQualityLightmaps = !CVarSupportLowQualityLightmaps || CVarSupportLowQualityLightmaps->GetValueOnAnyThread() != 0 || bForceAllPermutations;
	bAllowStaticLighting = CVarAllowStaticLighting->GetValueOnAnyThread() != 0;
	bSupportSkyAtmosphere = !CVarSupportSkyAtmosphere || CVarSupportSkyAtmosphere->GetValueOnAnyThread() != 0 || bForceAllPermutations;;

	// mobile
	bMobileAllowMovableDirectionalLights = CVarMobileAllowMovableDirectionalLights->GetValueOnAnyThread() != 0;
	bMobileAllowDistanceFieldShadows = CVarMobileAllowDistanceFieldShadows->GetValueOnAnyThread() != 0;
	bMobileEnableStaticAndCSMShadowReceivers = CVarMobileEnableStaticAndCSMShadowReceivers->GetValueOnAnyThread() != 0;
	MobileSkyLightPermutation = CVarMobileSkyLightPermutation->GetValueOnAnyThread();
	bMobileEnableNoPrecomputedLightingCSMShader = CVarMobileEnableNoPrecomputedLightingCSMShader->GetValueOnAnyThread() != 0;

	const bool bShowMissmatchedLowQualityLightmapsWarning = (!bEnableLowQualityLightmaps) && (GEngine->bShouldGenerateLowQualityLightmaps_DEPRECATED);
	if ( bShowMissmatchedLowQualityLightmapsWarning )
	{
		UE_LOG(LogInit, Warning, TEXT("Mismatch between bShouldGenerateLowQualityLightmaps(%d) and r.SupportLowQualityLightmaps(%d), UEngine::bShouldGenerateLowQualityLightmaps has been deprecated please use r.SupportLowQualityLightmaps instead"), GEngine->bShouldGenerateLowQualityLightmaps_DEPRECATED, bEnableLowQualityLightmaps);
	}

	bInitialized = true;
}

void FMeshBatch::PreparePrimitiveUniformBuffer(const FPrimitiveSceneProxy* PrimitiveSceneProxy, ERHIFeatureLevel::Type FeatureLevel)
{
	// Fallback to using the primitive uniform buffer if GPU scene is disabled.
	// Vertex shaders on mobile may still use PrimitiveUB with GPUScene enabled
	if (!UseGPUScene(GMaxRHIShaderPlatform, FeatureLevel) || FeatureLevel == ERHIFeatureLevel::ES3_1)
	{
		for (int32 ElementIndex = 0; ElementIndex < Elements.Num(); ElementIndex++)
		{
			FMeshBatchElement& MeshElement = Elements[ElementIndex];

			if (!MeshElement.PrimitiveUniformBuffer && !MeshElement.PrimitiveUniformBufferResource)
			{
				MeshElement.PrimitiveUniformBuffer = PrimitiveSceneProxy->GetUniformBuffer();
			}
		}
	}
}

#if USE_MESH_BATCH_VALIDATION
bool FMeshBatch::Validate(const FPrimitiveSceneProxy* PrimitiveSceneProxy, ERHIFeatureLevel::Type FeatureLevel) const
		{
	check(PrimitiveSceneProxy);

	const auto LogMeshError = [&](const FString& Error) -> bool
	{
		const FString VertexFactoryName = VertexFactory ? VertexFactory->GetType()->GetFName().ToString() : TEXT("nullptr");
		const uint32 bVertexFactoryInitialized = (VertexFactory && VertexFactory->IsInitialized()) ? 1 : 0;

		ensureMsgf(false,
			TEXT("FMeshBatch was not properly setup. %s.\n\tVertexFactory[Name: %s, Initialized: %u]\n\tPrimitiveSceneProxy[Level: %s, Owner: %s, Resource: %s]"),
			*Error,
			*VertexFactoryName,
			bVertexFactoryInitialized,
			*PrimitiveSceneProxy->GetLevelName().ToString(),
			*PrimitiveSceneProxy->GetOwnerName().ToString(),
			*PrimitiveSceneProxy->GetResourceName().ToString());

		return false;
	};

	if (!MaterialRenderProxy)
	{
		return LogMeshError(TEXT("Mesh has a null material render proxy!"));
	}

	if (!PrimitiveSceneProxy->VerifyUsedMaterial(MaterialRenderProxy))
	{
		return LogMeshError(TEXT("Mesh material is not marked as used by the primitive scene proxy."));
	}

	if (!VertexFactory)
	{
		return LogMeshError(TEXT("Mesh has a null vertex factory!"));
}

	if (!VertexFactory->IsInitialized())
	{
		return LogMeshError(TEXT("Mesh has an uninitialized vertex factory!"));
}

	for (int32 Index = 0; Index < Elements.Num(); ++Index)
	{
		const FMeshBatchElement& MeshBatchElement = Elements[Index];

		if (MeshBatchElement.IndexBuffer)
		{
			if (const FRHIBuffer* IndexBufferRHI = MeshBatchElement.IndexBuffer->IndexBufferRHI)
			{
				const uint32 IndexCount = GetVertexCountForPrimitiveCount(MeshBatchElement.NumPrimitives, Type);
				const uint32 IndexBufferSize = IndexBufferRHI->GetSize();

				// A zero-sized index buffer is valid for streaming.
				if (IndexBufferSize != 0 && (MeshBatchElement.FirstIndex + IndexCount) * IndexBufferRHI->GetStride() > IndexBufferSize)
				{
					return LogMeshError(FString::Printf(
						TEXT("MeshBatchElement %d, Material '%s', index range extends past index buffer bounds: Start %u, Count %u, Buffer Size %u, Buffer stride %u"),
						Index, MaterialRenderProxy ? *MaterialRenderProxy->GetFriendlyName() : TEXT("nullptr"),
						MeshBatchElement.FirstIndex, IndexCount, IndexBufferRHI->GetSize(), IndexBufferRHI->GetStride()));
				}
			}
			else
			{
				return LogMeshError(FString::Printf(
					TEXT("FMeshElementCollector::AddMesh - On MeshBatchElement %d, Material '%s', index buffer object has null RHI resource"),
					Index, MaterialRenderProxy ? *MaterialRenderProxy->GetFriendlyName() : TEXT("nullptr")));
			}
		}
	}

	const bool bVFSupportsPrimitiveIdStream = VertexFactory->GetType()->SupportsPrimitiveIdStream();

	if (!PrimitiveSceneProxy->DoesVFRequirePrimitiveUniformBuffer() && !bVFSupportsPrimitiveIdStream)
	{
		return LogMeshError(TEXT("PrimitiveSceneProxy has bVFRequiresPrimitiveUniformBuffer disabled yet tried to draw with a vertex factory that did not support PrimitiveIdStream"));
	}

	if (PrimitiveSceneProxy->SupportsGPUScene() && !VertexFactory->SupportsGPUScene(FeatureLevel))
	{
		return LogMeshError(TEXT("PrimitiveSceneProxy has SupportsGPUScene() does not match VertexFactory->SupportsGPUScene()"));
	}
	const bool bUseGPUScene = UseGPUScene(GMaxRHIShaderPlatform, FeatureLevel);
	
	const bool bPrimitiveShaderDataComesFromSceneBuffer = bUseGPUScene && VertexFactory->GetPrimitiveIdStreamIndex(FeatureLevel, EVertexInputStreamType::Default) >= 0;

	const bool bPrimitiveHasUniformBuffer = PrimitiveSceneProxy->GetUniformBuffer() != nullptr;

	for (int32 ElementIndex = 0; ElementIndex < Elements.Num(); ElementIndex++)
	{
		const FMeshBatchElement& MeshElement = Elements[ElementIndex];

		if (bPrimitiveShaderDataComesFromSceneBuffer && Elements[ElementIndex].PrimitiveUniformBuffer)
		{
			// on mobile VS has access to PrimitiveUniformBuffer
			if (FeatureLevel > ERHIFeatureLevel::ES3_1)
			{
				// This is a non-fatal error.
				LogMeshError(
					TEXT("FMeshBatch was assigned a PrimitiveUniformBuffer even though the vertex factory fetches primitive shader data through the GPUScene buffer. ")
					TEXT("The assigned PrimitiveUniformBuffer cannot be respected. Use PrimitiveUniformBufferResource instead for dynamic primitive data, or leave ")
					TEXT("both null to get FPrimitiveSceneProxy->UniformBuffer"));
			}
		}

		const bool bValidPrimitiveData =
			   bPrimitiveShaderDataComesFromSceneBuffer
			|| bPrimitiveHasUniformBuffer
			|| Elements[ElementIndex].PrimitiveUniformBuffer
			|| Elements[ElementIndex].PrimitiveUniformBufferResource;

		if (!bValidPrimitiveData)
		{
			return LogMeshError(TEXT("No primitive uniform buffer was specified and the vertex factory does not have a valid primitive id stream"));
		}
	}

	return true;
}
#endif

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FMobileReflectionCaptureShaderParameters, "MobileReflectionCapture");

void FDefaultMobileReflectionCaptureUniformBuffer::InitDynamicRHI()
{
	FMobileReflectionCaptureShaderParameters Parameters;
	Parameters.Params = FVector4f(1.f, 0.f, 0.f, 0.f);
	Parameters.Texture = GBlackTextureCube->TextureRHI;
	Parameters.TextureSampler = GBlackTextureCube->SamplerStateRHI;
	SetContents(Parameters);
	Super::InitDynamicRHI();
}

/** Global uniform buffer containing the default reflection data used in mobile renderer. */
TGlobalResource<FDefaultMobileReflectionCaptureUniformBuffer> GDefaultMobileReflectionCaptureUniformBuffer;
