// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
LightGridInjection.cpp
=============================================================================*/

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "HAL/IConsoleManager.h"
#include "RHI.h"
#include "UniformBuffer.h"
#include "ShaderParameters.h"
#include "RendererInterface.h"
#include "EngineDefines.h"
#include "PrimitiveSceneProxy.h"
#include "Shader.h"
#include "SceneUtils.h"
#include "PostProcess/SceneRenderTargets.h"
#include "LightSceneInfo.h"
#include "GlobalShader.h"
#include "SceneRendering.h"
#include "DeferredShadingRenderer.h"
#include "BasePassRendering.h"
#include "RendererModule.h"
#include "ScenePrivate.h"
#include "ClearQuad.h"
#include "VolumetricFog.h"
#include "VolumetricCloudRendering.h"
#include "Components/LightComponent.h"
#include "Engine/MapBuildDataRegistry.h"
#include "PixelShaderUtils.h"
#include "ShaderPrint.h"
#include "ShaderPrintParameters.h"
#include "RenderUtils.h"
#include "ManyLights/ManyLights.h"

int32 GLightGridPixelSize = 64;
FAutoConsoleVariableRef CVarLightGridPixelSize(
	TEXT("r.Forward.LightGridPixelSize"),
	GLightGridPixelSize,
	TEXT("Size of a cell in the light grid, in pixels."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLightGridSizeZ = 32;
FAutoConsoleVariableRef CVarLightGridSizeZ(
	TEXT("r.Forward.LightGridSizeZ"),
	GLightGridSizeZ,
	TEXT("Number of Z slices in the light grid."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GForwardLightGridDebug = 0;
FAutoConsoleVariableRef CVarLightGridDebug(
	TEXT("r.Forward.LightGridDebug"),
	GForwardLightGridDebug,
	TEXT("Whether to display on screen culledlight per tile.\n")
	TEXT(" 0: off (default)\n")
	TEXT(" 1: on - showing light count onto the depth buffer\n")
	TEXT(" 2: on - showing max light count per tile accoung for each slice but the last one (culling there is too conservative)\n")
	TEXT(" 3: on - showing max light count per tile accoung for each slice and the last one \n"),
	ECVF_RenderThreadSafe
);

int32 GMaxCulledLightsPerCell = 32;
FAutoConsoleVariableRef CVarMaxCulledLightsPerCell(
	TEXT("r.Forward.MaxCulledLightsPerCell"),
	GMaxCulledLightsPerCell,
	TEXT("Controls how much memory is allocated for each cell for light culling.  When r.Forward.LightLinkedListCulling is enabled, this is used to compute a global max instead of a per-cell limit on culled lights."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLightLinkedListCulling = 1;
FAutoConsoleVariableRef CVarLightLinkedListCulling(
	TEXT("r.Forward.LightLinkedListCulling"),
	GLightLinkedListCulling,
	TEXT("Uses a reverse linked list to store culled lights, removing the fixed limit on how many lights can affect a cell - it becomes a global limit instead."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLightCullingQuality = 1;
FAutoConsoleVariableRef CVarLightCullingQuality(
	TEXT("r.LightCulling.Quality"),
	GLightCullingQuality,
	TEXT("Whether to run compute light culling pass.\n")
	TEXT(" 0: off \n")
	TEXT(" 1: on (default)\n"),
	ECVF_RenderThreadSafe
);

float GLightCullingMaxDistanceOverrideKilometers = -1.0f;
FAutoConsoleVariableRef CVarLightCullingMaxDistanceOverride(
	TEXT("r.LightCulling.MaxDistanceOverrideKilometers"),
	GLightCullingMaxDistanceOverrideKilometers,
	TEXT("Used to override the maximum far distance at which we can store data in the light grid.\n If this is increase, you might want to update r.Forward.LightGridSizeZ to a reasonable value according to your use case light count and distribution.")
	TEXT(" <=0: off \n")
	TEXT(" >0: the far distance in kilometers.\n"),
	ECVF_RenderThreadSafe
);

extern TAutoConsoleVariable<int32> CVarVirtualShadowOnePassProjection;

bool ShouldVisualizeLightGrid()
{
	return GForwardLightGridDebug > 0;
}

// If this is changed, the LIGHT_GRID_USES_16BIT_BUFFERS define from LightGridCommon.ush should also be updated.
bool LightGridUses16BitBuffers(EShaderPlatform Platform)
{
	// CulledLightDataGrid, is typically 16bit elements to save on memory and bandwidth. So to not introduce any regressions it will stay as texel buffer on all platforms, except mobile and Metal (which does not support type conversions).
	return RHISupportsBufferLoadTypeConversion(Platform) && !IsMobilePlatform(Platform);
}

void SetupDummyForwardLightUniformParameters(FRDGBuilder& GraphBuilder, FForwardLightData& ForwardLightData, EShaderPlatform ShaderPlatform)
{
	const FRDGSystemTextures& SystemTextures = FRDGSystemTextures::Get(GraphBuilder);
	
	ForwardLightData.DirectionalLightShadowmapAtlas = SystemTextures.Black;
	ForwardLightData.DirectionalLightStaticShadowmap = GBlackTexture->TextureRHI;

	FRDGBufferRef ForwardLocalLightBuffer = GSystemTextures.GetDefaultStructuredBuffer(GraphBuilder, sizeof(FVector4f));
	ForwardLightData.ForwardLocalLightBuffer = GraphBuilder.CreateSRV(ForwardLocalLightBuffer);

	FRDGBufferRef NumCulledLightsGrid = GSystemTextures.GetDefaultStructuredBuffer(GraphBuilder, sizeof(uint32));
	ForwardLightData.NumCulledLightsGrid = GraphBuilder.CreateSRV(NumCulledLightsGrid);

	const bool bLightGridUses16BitBuffers = LightGridUses16BitBuffers(ShaderPlatform);
	FRDGBufferSRVRef CulledLightDataGridSRV = nullptr;
	if (bLightGridUses16BitBuffers)
	{
		FRDGBufferRef CulledLightDataGrid = GSystemTextures.GetDefaultBuffer(GraphBuilder, sizeof(uint16));
		CulledLightDataGridSRV = GraphBuilder.CreateSRV(CulledLightDataGrid, PF_R16_UINT);
	}
	else
	{
		FRDGBufferRef CulledLightDataGrid = GSystemTextures.GetDefaultStructuredBuffer(GraphBuilder, sizeof(uint32));
		CulledLightDataGridSRV = GraphBuilder.CreateSRV(CulledLightDataGrid);
	}
	ForwardLightData.CulledLightDataGrid32Bit = CulledLightDataGridSRV;
	ForwardLightData.CulledLightDataGrid16Bit = CulledLightDataGridSRV;

	ForwardLightData.LightFunctionAtlasLightIndex = 0;
}

TRDGUniformBufferRef<FForwardLightData> CreateDummyForwardLightUniformBuffer(FRDGBuilder& GraphBuilder, EShaderPlatform ShaderPlatform)
{
	FForwardLightData* ForwardLightData = GraphBuilder.AllocParameters<FForwardLightData>();
	SetupDummyForwardLightUniformParameters(GraphBuilder, *ForwardLightData, ShaderPlatform);
	return GraphBuilder.CreateUniformBuffer(ForwardLightData);
}

void SetDummyForwardLightUniformBufferOnViews(FRDGBuilder& GraphBuilder, EShaderPlatform ShaderPlatform, TArray<FViewInfo>& Views)
{
	TRDGUniformBufferRef<FForwardLightData> ForwardLightUniformBuffer = CreateDummyForwardLightUniformBuffer(GraphBuilder, ShaderPlatform);
	for (auto& View : Views)
	{
		View.ForwardLightingResources.SetUniformBuffer(ForwardLightUniformBuffer);
	}
}

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FForwardLightData, "ForwardLightData");

FForwardLightData::FForwardLightData()
{
	FMemory::Memzero(*this);
	ShadowmapSampler = TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();
	DirectionalLightStaticShadowmap = GBlackTexture->TextureRHI;
	StaticShadowmapSampler = TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();
}

int32 NumCulledLightsGridStride = 2;
int32 NumCulledGridPrimitiveTypes = 2;
int32 LightLinkStride = 2;

// 65k indexable light limit
typedef uint16 FLightIndexType;
// UINT_MAX indexable light limit
typedef uint32 FLightIndexType32;

uint32 LightGridInjectionGroupSize = 4;


class FLightGridInjectionCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLightGridInjectionCS);
	SHADER_USE_PARAMETER_STRUCT(FLightGridInjectionCS, FGlobalShader)
public:
	class FUseLinkedListDim : SHADER_PERMUTATION_BOOL("USE_LINKED_CULL_LIST");
	using FPermutationDomain = TShaderPermutationDomain<FUseLinkedListDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FReflectionCaptureShaderData, ReflectionCapture)
		SHADER_PARAMETER_STRUCT_REF(FMobileReflectionCaptureShaderData, MobileReflectionCaptureData)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWNumCulledLightsGrid)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWCulledLightDataGrid32Bit)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWCulledLightDataGrid16Bit)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWNextCulledLightLink)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWStartOffsetGrid)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWCulledLightLinks)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, LightViewSpacePositionAndRadius)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, LightViewSpaceDirAndPreprocAngle)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, ForwardLocalLightBuffer)

		SHADER_PARAMETER(FIntVector, CulledGridSize)
		SHADER_PARAMETER(uint32, NumReflectionCaptures)
		SHADER_PARAMETER(FVector3f, LightGridZParams)
		SHADER_PARAMETER(uint32, NumLocalLights)
		SHADER_PARAMETER(uint32, NumGridCells)
		SHADER_PARAMETER(uint32, MaxCulledLightsPerCell)
		SHADER_PARAMETER(uint32, LightGridPixelSizeShift)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_LIGHT_GRID_INJECTION_CS"), 1);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), LightGridInjectionGroupSize);
		FForwardLightingParameters::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("LIGHT_LINK_STRIDE"), LightLinkStride);
		OutEnvironment.SetDefine(TEXT("ENABLE_LIGHT_CULLING_VIEW_SPACE_BUILD_DATA"), ENABLE_LIGHT_CULLING_VIEW_SPACE_BUILD_DATA);
	}
};

IMPLEMENT_GLOBAL_SHADER(FLightGridInjectionCS, "/Engine/Private/LightGridInjection.usf", "LightGridInjectionCS", SF_Compute);


class FLightGridCompactCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLightGridCompactCS)
	SHADER_USE_PARAMETER_STRUCT(FLightGridCompactCS, FGlobalShader)
public:
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWNumCulledLightsGrid)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWCulledLightDataGrid32Bit)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWCulledLightDataGrid16Bit)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWNextCulledLightData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, StartOffsetGrid)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, CulledLightLinks)

		SHADER_PARAMETER(FIntVector, CulledGridSize)
		SHADER_PARAMETER(uint32, NumReflectionCaptures)
		SHADER_PARAMETER(uint32, NumLocalLights)
		SHADER_PARAMETER(uint32, NumGridCells)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_LIGHT_GRID_COMPACT_CS"), 1);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), LightGridInjectionGroupSize);
		FForwardLightingParameters::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("LIGHT_LINK_STRIDE"), LightLinkStride);
		OutEnvironment.SetDefine(TEXT("MAX_CAPTURES"), GetMaxNumReflectionCaptures(Parameters.Platform));
		OutEnvironment.SetDefine(TEXT("ENABLE_LIGHT_CULLING_VIEW_SPACE_BUILD_DATA"), ENABLE_LIGHT_CULLING_VIEW_SPACE_BUILD_DATA);
	}
};

IMPLEMENT_GLOBAL_SHADER(FLightGridCompactCS, "/Engine/Private/LightGridInjection.usf", "LightGridCompactCS", SF_Compute);

/**
 */
FORCEINLINE float GetTanRadAngleOrZero(float coneAngle)
{
	if (coneAngle < PI / 2.001f)
	{
		return FMath::Tan(coneAngle);
	}

	return 0.0f;
}


FVector GetLightGridZParams(float NearPlane, float FarPlane)
{
	// S = distribution scale
	// B, O are solved for given the z distances of the first+last slice, and the # of slices.
	//
	// slice = log2(z*B + O) * S

	// Don't spend lots of resolution right in front of the near plane
	double NearOffset = .095 * 100;
	// Space out the slices so they aren't all clustered at the near plane
	double S = 4.05;

	double N = NearPlane + NearOffset;
	double F = FarPlane;

	double O = (F - N * exp2((GLightGridSizeZ - 1) / S)) / (F - N);
	double B = (1 - O) / N;

	return FVector(B, O, S);
}

static uint32 PackRG16(float In0, float In1)
{
	return uint32(FFloat16(In0).Encoded) | (uint32(FFloat16(In1).Encoded) << 16);
}

static FVector2f PackLightColor(const FVector3f& LightColor)
{
	FVector3f LightColorDir;
	float LightColorLength;
	LightColor.ToDirectionAndLength(LightColorDir, LightColorLength);

	FVector2f LightColorPacked;
	uint32 LightColorDirPacked = 
		((static_cast<uint32>(LightColorDir.X * 0x3FF) & 0x3FF) <<  0) |
		((static_cast<uint32>(LightColorDir.Y * 0x3FF) & 0x3FF) << 10) |
		((static_cast<uint32>(LightColorDir.Z * 0x3FF) & 0x3FF) << 20);

	LightColorPacked.X = LightColorLength / 0x3FF;
	*(uint32*)(&LightColorPacked.Y) = LightColorDirPacked;

	return LightColorPacked;
}

static void PackLocalLightData(
	FForwardLocalLightData& Out,
	const FViewInfo& View,
	const FSimpleLightEntry& SimpleLight,
	const FSimpleLightPerViewEntry& SimpleLightPerViewData)
{
	// Put simple lights in all lighting channels
	FLightingChannels SimpleLightLightingChannels;
	SimpleLightLightingChannels.bChannel0 = SimpleLightLightingChannels.bChannel1 = SimpleLightLightingChannels.bChannel2 = true;

	const uint32 SimpleLightLightingChannelMask = GetLightingChannelMaskForStruct(SimpleLightLightingChannels);
	const FVector3f LightTranslatedWorldPosition(View.ViewMatrices.GetPreViewTranslation() + SimpleLightPerViewData.Position);

	// No shadowmap channels for simple lights
	uint32 ShadowMapChannelMask = 0;
	ShadowMapChannelMask |= SimpleLightLightingChannelMask << 8;

	// Pack both values into a single float to keep float4 alignment
	const float SimpleLightSourceLength = 0;
	const uint32 PackedW = PackRG16(SimpleLightSourceLength, SimpleLight.VolumetricScatteringIntensity);

	// Pack both values into a single float to keep float4 alignment
	const float SourceRadius = 0;
	const float SourceSoftRadius = 0;
	const uint32 PackedZ = PackRG16(SourceRadius, SourceSoftRadius);

	// Pack both rect light data (barn door length is initialized to -2 
	const uint32 RectPackedX = 0;
	const uint32 RectPackedY = 0;
	const uint32 RectPackedZ = FFloat16(-2.f).Encoded;

	// Pack specular scale and IES profile index
	const float SpecularScale = 1.f;
	const float IESAtlasIndex = INDEX_NONE;
	const uint32 SpecularScaleAndIESData = PackRG16(SpecularScale, IESAtlasIndex);

	const FVector3f LightColor = (FVector3f)SimpleLight.Color * FLightRenderParameters::GetLightExposureScale(View.GetLastEyeAdaptationExposure(), SimpleLight.InverseExposureBlend);
	const FVector2f LightColorPacked = PackLightColor(LightColor);

	Out.LightPositionAndInvRadius							= FVector4f(LightTranslatedWorldPosition, 1.0f / FMath::Max(SimpleLight.Radius, KINDA_SMALL_NUMBER));
	Out.LightColorAndIdAndFalloffExponent					= FVector4f(LightColorPacked.X, LightColorPacked.Y, INDEX_NONE, SimpleLight.Exponent);
	Out.LightDirectionAndShadowMapChannelMask				= FVector4f(FVector3f(1, 0, 0), FMath::AsFloat(ShadowMapChannelMask));
	Out.SpotAnglesAndSourceRadiusPacked						= FVector4f(-2, 1, FMath::AsFloat(PackedZ), FMath::AsFloat(PackedW));
	Out.LightTangentAndIESDataAndSpecularScale				= FVector4f(1.0f, 0.0f, 0.0f, FMath::AsFloat(SpecularScaleAndIESData));
	Out.RectDataAndVirtualShadowMapIdOrPrevLocalLightIndex	= FVector4f(FMath::AsFloat(RectPackedX), FMath::AsFloat(RectPackedY), FMath::AsFloat(RectPackedZ), -1);
}

static void PackLocalLightData(
	FForwardLocalLightData& Out, 
	const FViewInfo& View,
	const FLightRenderParameters& LightParameters,
	const uint32 LightTypeAndShadowMapChannelMaskAndLightFunctionIndexPacked,
	const int32 LightSceneId,
	const int32 VirtualShadowMapId,
	const int32 PrevLocalLightIndex,
	const bool bHandledByManyLights,
	const float VolumetricScatteringIntensity)
{
	const FVector3f LightTranslatedWorldPosition(View.ViewMatrices.GetPreViewTranslation() + LightParameters.WorldPosition);

	// Pack both values into a single float to keep float4 alignment
	const uint32 PackedW = PackRG16(LightParameters.SourceLength, VolumetricScatteringIntensity);

	// Pack both SourceRadius and SoftSourceRadius
	const uint32 PackedZ = PackRG16(LightParameters.SourceRadius, LightParameters.SoftSourceRadius);
	
	// Pack rect light data
	uint32 RectPackedX = PackRG16(LightParameters.RectLightAtlasUVOffset.X, LightParameters.RectLightAtlasUVOffset.Y);
	uint32 RectPackedY = PackRG16(LightParameters.RectLightAtlasUVScale.X, LightParameters.RectLightAtlasUVScale.Y);
	uint32 RectPackedZ = 0;
	RectPackedZ |= FFloat16(LightParameters.RectLightBarnLength).Encoded;									// 16 bits
	RectPackedZ |= uint32(FMath::Clamp(LightParameters.RectLightBarnCosAngle,  0.f, 1.0f) * 0x3FF) << 16;	// 10 bits
	RectPackedZ |= uint32(FMath::Clamp(LightParameters.RectLightAtlasMaxLevel, 0.f, 63.f)) << 26;			//  6 bits

	// Pack specular scale and IES profile index
	const uint32 SpecularScaleAndIESData = PackRG16(LightParameters.SpecularScale, LightParameters.IESAtlasIndex); // pack atlas id here? 16bit specular 8bit IES and 8 bit LightFunction

	const FVector2f LightColorPacked = PackLightColor(FVector3f(LightParameters.Color));

	// Since lights don't use VSM and Many Lights simultaneously and
	// currently PrevLocalLightIndex is only accessed by Many Lights shaders
	// we can store only one of the values to avoid increasing the size of the struct
	// TODO: Improve packing to avoid this so that PrevLocalLightIndex can be accessed in other shaders as well
	const int32 VirtualShadowMapIdOrPrevLocalLightIndex = bHandledByManyLights ? PrevLocalLightIndex : VirtualShadowMapId;

	// NOTE: This cast of VirtualShadowMapIdOrPrevLocalLightIndex to float is not ideal, but bitcast has issues here with INDEX_NONE -> NaN
	// and 32-bit floats have enough mantissa to cover all reasonable numbers here for now.
	// NOTE: SpotAngles needs full-precision for VSM one pass projection
	Out.LightPositionAndInvRadius							= FVector4f(LightTranslatedWorldPosition, LightParameters.InvRadius);
	Out.LightColorAndIdAndFalloffExponent					= FVector4f(LightColorPacked.X, LightColorPacked.Y, LightSceneId, LightParameters.FalloffExponent);
	Out.LightDirectionAndShadowMapChannelMask				= FVector4f(LightParameters.Direction, FMath::AsFloat(LightTypeAndShadowMapChannelMaskAndLightFunctionIndexPacked));
	Out.SpotAnglesAndSourceRadiusPacked						= FVector4f(LightParameters.SpotAngles.X, LightParameters.SpotAngles.Y, FMath::AsFloat(PackedZ), FMath::AsFloat(PackedW));
	Out.LightTangentAndIESDataAndSpecularScale				= FVector4f(LightParameters.Tangent, FMath::AsFloat(SpecularScaleAndIESData));
	Out.RectDataAndVirtualShadowMapIdOrPrevLocalLightIndex	= FVector4f(FMath::AsFloat(RectPackedX), FMath::AsFloat(RectPackedY), FMath::AsFloat(RectPackedZ), float(VirtualShadowMapIdOrPrevLocalLightIndex));

	checkSlow(int32(Out.RectDataAndVirtualShadowMapIdOrPrevLocalLightIndex.W) == VirtualShadowMapIdOrPrevLocalLightIndex);
}

FComputeLightGridOutput FSceneRenderer::ComputeLightGrid(FRDGBuilder& GraphBuilder, bool bCullLightsToGrid, FSortedLightSetSceneInfo& SortedLightSet)
{
	FComputeLightGridOutput Result = {};

	RDG_CSV_STAT_EXCLUSIVE_SCOPE(GraphBuilder, ComputeLightGrid);
	QUICK_SCOPE_CYCLE_COUNTER(STAT_ComputeLightGrid);
	RDG_EVENT_SCOPE(GraphBuilder, "ComputeLightGrid");

	const bool bAllowStaticLighting = IsStaticLightingAllowed();
	const bool bLightGridUses16BitBuffers = LightGridUses16BitBuffers(ShaderPlatform);

	const FRDGSystemTextures& SystemTextures = FRDGSystemTextures::Get(GraphBuilder);

	TArray<FForwardLightData*, TInlineAllocator<4>> ForwardLightDataPerView;
#if WITH_EDITOR
	bool bMultipleDirLightsConflictForForwardShading = false;
#endif

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		FViewInfo& View = Views[ViewIndex];

		View.ForwardLightingResources.SelectedForwardDirectionalLightProxy = nullptr;

		FForwardLightData* ForwardLightData = GraphBuilder.AllocParameters<FForwardLightData>();
		ForwardLightData->DirectionalLightShadowmapAtlas = SystemTextures.Black;
		ForwardLightData->DirectionalLightStaticShadowmap = GBlackTexture->TextureRHI;

		TArray<FForwardLocalLightData, SceneRenderingAllocator> ForwardLocalLightData;
		TArray<int32, SceneRenderingAllocator>  LocalLightVisibleLightInfosIndex;
#if ENABLE_LIGHT_CULLING_VIEW_SPACE_BUILD_DATA
		TArray<FVector4f, SceneRenderingAllocator> ViewSpacePosAndRadiusData;
		TArray<FVector4f, SceneRenderingAllocator> ViewSpaceDirAndPreprocAngleData;
#endif // ENABLE_LIGHT_CULLING_VIEW_SPACE_BUILD_DATA

		float FurthestLight = 1000;

		int32 ConflictingLightCountForForwardShading = 0;

		// Track the end markers for different types
		int32 SimpleLightsEnd = 0;
		int32 ClusteredSupportedEnd = 0;
		int32 ManyLightsSupportedStart = 0;

		const float Exposure = View.GetLastEyeAdaptationExposure();

		if (bCullLightsToGrid)
		{
			// Simple lights are copied without view dependent checks, so same in and out
			SimpleLightsEnd = SortedLightSet.SimpleLightsEnd;
			// 1. insert simple lights
			if (SimpleLightsEnd > 0)
			{
				ForwardLocalLightData.Reserve(SimpleLightsEnd);
				LocalLightVisibleLightInfosIndex.Reserve(SimpleLightsEnd);
#if ENABLE_LIGHT_CULLING_VIEW_SPACE_BUILD_DATA
				ViewSpacePosAndRadiusData.Reserve(SimpleLightsEnd);
				ViewSpaceDirAndPreprocAngleData.Reserve(SimpleLightsEnd);
#endif // ENABLE_LIGHT_CULLING_VIEW_SPACE_BUILD_DATA

				const FSimpleLightArray& SimpleLights = SortedLightSet.SimpleLights;

				// Pack both values into a single float to keep float4 alignment
				const FFloat16 SimpleLightSourceLength16f = FFloat16(0);
				FLightingChannels SimpleLightLightingChannels;

				// Put simple lights in all lighting channels
				SimpleLightLightingChannels.bChannel0 = SimpleLightLightingChannels.bChannel1 = SimpleLightLightingChannels.bChannel2 = true;
				const uint32 SimpleLightLightingChannelMask = GetLightingChannelMaskForStruct(SimpleLightLightingChannels);

				// Now using the sorted lights, and keep track of ranges as we go.
				for (int32 SortedIndex = 0; SortedIndex < SimpleLightsEnd; ++SortedIndex)
				{
					check(SortedLightSet.SortedLights[SortedIndex].LightSceneInfo == nullptr);
					check(!SortedLightSet.SortedLights[SortedIndex].SortKey.Fields.bIsNotSimpleLight);


					int32 SimpleLightIndex = SortedLightSet.SortedLights[SortedIndex].SimpleLightIndex;

					ForwardLocalLightData.AddUninitialized(1);
					FForwardLocalLightData& LightData = ForwardLocalLightData.Last();

					// Simple lights have no 'VisibleLight' info
					LocalLightVisibleLightInfosIndex.Add(INDEX_NONE);

					const FSimpleLightEntry& SimpleLight = SimpleLights.InstanceData[SimpleLightIndex];
					const FSimpleLightPerViewEntry& SimpleLightPerViewData = SimpleLights.GetViewDependentData(SimpleLightIndex, ViewIndex, Views.Num());
					PackLocalLightData(LightData, View, SimpleLight, SimpleLightPerViewData);

				#if ENABLE_LIGHT_CULLING_VIEW_SPACE_BUILD_DATA
					FVector4f ViewSpacePosAndRadius(FVector4f(View.ViewMatrices.GetViewMatrix().TransformPosition(SimpleLightPerViewData.Position)), SimpleLight.Radius);
					ViewSpacePosAndRadiusData.Add(ViewSpacePosAndRadius);
					ViewSpaceDirAndPreprocAngleData.AddZeroed();
				#endif // ENABLE_LIGHT_CULLING_VIEW_SPACE_BUILD_DATA
				}
			}

			float SelectedForwardDirectionalLightIntensitySq = 0.0f;
			int32 SelectedForwardDirectionalLightPriority = -1;
			const TArray<FSortedLightSceneInfo, SceneRenderingAllocator>& SortedLights = SortedLightSet.SortedLights;
			ClusteredSupportedEnd = SimpleLightsEnd;
			ManyLightsSupportedStart = MAX_int32;
			// Next add all the other lights, track the end index for clustered supporting lights
			for (int32 SortedIndex = SimpleLightsEnd; SortedIndex < SortedLights.Num(); ++SortedIndex)
			{
				const FSortedLightSceneInfo& SortedLightInfo = SortedLights[SortedIndex];
				const FLightSceneInfo* const LightSceneInfo = SortedLightInfo.LightSceneInfo;
				const FLightSceneProxy* LightProxy = LightSceneInfo->Proxy;

				if (LightSceneInfo->ShouldRenderLight(View))
				{
					FLightRenderParameters LightParameters;
					LightProxy->GetLightShaderParameters(LightParameters);

					if (LightProxy->IsInverseSquared())
					{
						LightParameters.FalloffExponent = 0;
					}

					// When rendering reflection captures, the direct lighting of the light is actually the indirect specular from the main view
					if (View.bIsReflectionCapture)
					{
						LightParameters.Color *= LightProxy->GetIndirectLightingScale();
					}

					uint32 LightTypeAndShadowMapChannelMaskPacked = LightSceneInfo->PackLightTypeAndShadowMapChannelMask(bAllowStaticLighting, SortedLightInfo.SortKey.Fields.bLightFunction);

					const bool bDynamicShadows = ViewFamily.EngineShowFlags.DynamicShadows && VisibleLightInfos.IsValidIndex(LightSceneInfo->Id);
					const int32 VirtualShadowMapId = bDynamicShadows ? VisibleLightInfos[LightSceneInfo->Id].GetVirtualShadowMapId( &View ) : INDEX_NONE;

					if ((SortedLightInfo.SortKey.Fields.LightType == LightType_Point && ViewFamily.EngineShowFlags.PointLights) ||
						(SortedLightInfo.SortKey.Fields.LightType == LightType_Spot && ViewFamily.EngineShowFlags.SpotLights) ||
						(SortedLightInfo.SortKey.Fields.LightType == LightType_Rect && ViewFamily.EngineShowFlags.RectLights))
					{
						int32 PrevLocalLightIndex = INDEX_NONE;
						if (View.ViewState)
						{
							PrevLocalLightIndex = View.ViewState->LightSceneIdToLocalLightIndex.FindOrAdd(LightSceneInfo->Id, INDEX_NONE);
							View.ViewState->LightSceneIdToLocalLightIndex[LightSceneInfo->Id] = ForwardLocalLightData.Num();
						}

						ForwardLocalLightData.AddUninitialized(1);
						FForwardLocalLightData& LightData = ForwardLocalLightData.Last();
						LocalLightVisibleLightInfosIndex.Add(LightSceneInfo->Id);

						// Track the last one to support clustered deferred
						if (!SortedLightInfo.SortKey.Fields.bClusteredDeferredNotSupported)
						{
							ClusteredSupportedEnd = FMath::Max(ClusteredSupportedEnd, ForwardLocalLightData.Num());
						}

						if (SortedLightInfo.SortKey.Fields.bHandledByManyLights && ManyLightsSupportedStart == MAX_int32)
						{
							ManyLightsSupportedStart = ForwardLocalLightData.Num() - 1;
						}
						const float LightFade = GetLightFadeFactor(View, LightProxy);
						LightParameters.Color *= LightFade;
						LightParameters.Color *= LightParameters.GetLightExposureScale(Exposure);

						float VolumetricScatteringIntensity = LightProxy->GetVolumetricScatteringIntensity();
						if (LightNeedsSeparateInjectionIntoVolumetricFogForOpaqueShadow(View, LightSceneInfo, VisibleLightInfos[LightSceneInfo->Id]))
						{
							// Disable this lights forward shading volumetric scattering contribution
							VolumetricScatteringIntensity = 0;
						}

						PackLocalLightData(
							LightData,
							View,
							LightParameters,
							LightTypeAndShadowMapChannelMaskPacked,
							LightSceneInfo->Id,
							VirtualShadowMapId,
							PrevLocalLightIndex,
							SortedLightInfo.SortKey.Fields.bHandledByManyLights,
							VolumetricScatteringIntensity);

						const FSphere BoundingSphere = LightProxy->GetBoundingSphere();
						const float Distance = View.ViewMatrices.GetViewMatrix().TransformPosition(BoundingSphere.Center).Z + BoundingSphere.W;
						FurthestLight = FMath::Max(FurthestLight, Distance);

					#if ENABLE_LIGHT_CULLING_VIEW_SPACE_BUILD_DATA
						// Note: inverting radius twice seems stupid (but done in shader anyway otherwise)
						const FVector3f LightViewPosition = FVector4f(View.ViewMatrices.GetViewMatrix().TransformPosition(LightParameters.WorldPosition)); // LWC_TODO: precision loss
						FVector4f ViewSpacePosAndRadius(LightViewPosition, 1.0f / LightParameters.InvRadius);
						ViewSpacePosAndRadiusData.Add(ViewSpacePosAndRadius);

						const float PreProcAngle = SortedLightInfo.SortKey.Fields.LightType == LightType_Spot ? GetTanRadAngleOrZero(LightSceneInfo->Proxy->GetOuterConeAngle()) : 0.0f;
						FVector4f ViewSpaceDirAndPreprocAngle(FVector4f(View.ViewMatrices.GetViewMatrix().TransformVector((FVector)LightParameters.Direction)), PreProcAngle); // LWC_TODO: precision loss
						ViewSpaceDirAndPreprocAngleData.Add(ViewSpaceDirAndPreprocAngle);
					#endif // ENABLE_LIGHT_CULLING_VIEW_SPACE_BUILD_DATA
					}
					// On mobile there is a separate FMobileDirectionalLightShaderParameters UB which holds all directional light data.
					else if (SortedLightInfo.SortKey.Fields.LightType == LightType_Directional && ViewFamily.EngineShowFlags.DirectionalLights && !IsMobilePlatform(View.GetShaderPlatform()))
					{
						// The selected forward directional light is also used for volumetric lighting using ForwardLightData UB.
						// Also some people noticed that depending on the order a two directional lights are made visible in a level, the selected light for volumetric fog lighting will be different.
						// So to be clear and avoid such issue, we select the most intense directional light for forward shading and volumetric lighting.
						const float LightIntensitySq = FVector3f(LightParameters.Color).SizeSquared();
						const int32 LightForwardShadingPriority = LightProxy->GetDirectionalLightForwardShadingPriority();
#if WITH_EDITOR
						if (LightForwardShadingPriority > SelectedForwardDirectionalLightPriority)
						{
							// Reset the count if the new light has a higher priority than the previous one.
							ConflictingLightCountForForwardShading = 1;
						}
						else if (LightForwardShadingPriority == SelectedForwardDirectionalLightPriority)
						{
							// Accumulate new light if also has the highest priority value.
							ConflictingLightCountForForwardShading++;
						}
#endif
						if (LightForwardShadingPriority > SelectedForwardDirectionalLightPriority
							|| (LightForwardShadingPriority == SelectedForwardDirectionalLightPriority && LightIntensitySq > SelectedForwardDirectionalLightIntensitySq))
						{

							SelectedForwardDirectionalLightPriority = LightForwardShadingPriority;
							SelectedForwardDirectionalLightIntensitySq = LightIntensitySq;
							View.ForwardLightingResources.SelectedForwardDirectionalLightProxy = LightProxy;

							ForwardLightData->HasDirectionalLight = 1;
							ForwardLightData->DirectionalLightColor = FVector3f(LightParameters.Color);
							ForwardLightData->DirectionalLightVolumetricScatteringIntensity = LightProxy->GetVolumetricScatteringIntensity();
							ForwardLightData->DirectionalLightSpecularScale = LightProxy->GetSpecularScale();
							ForwardLightData->DirectionalLightDirection = LightParameters.Direction;
							ForwardLightData->DirectionalLightSourceRadius = LightParameters.SourceRadius;
							ForwardLightData->DirectionalLightSoftSourceRadius = LightParameters.SoftSourceRadius;
							ForwardLightData->DirectionalLightShadowMapChannelMask = LightTypeAndShadowMapChannelMaskPacked;
							ForwardLightData->DirectionalLightVSM = INDEX_NONE;
							ForwardLightData->DirectionalLightSMRTSettings = GetVirtualShadowMapSMRTSettings(true);
							ForwardLightData->LightFunctionAtlasLightIndex = LightParameters.LightFunctionAtlasLightIndex;

							const FVector2D FadeParams = LightProxy->GetDirectionalLightDistanceFadeParameters(View.GetFeatureLevel(), LightSceneInfo->IsPrecomputedLightingValid(), View.MaxShadowCascades);

							ForwardLightData->DirectionalLightDistanceFadeMAD = FVector2f(FadeParams.Y, -FadeParams.X * FadeParams.Y);	// LWC_TODO: Precision loss

							const FMatrix TranslatedWorldToWorld = FTranslationMatrix(-View.ViewMatrices.GetPreViewTranslation());

							if (bDynamicShadows)
							{
								const TArray<FProjectedShadowInfo*, SceneRenderingAllocator>& DirectionalLightShadowInfos = VisibleLightInfos[LightSceneInfo->Id].AllProjectedShadows;

								ForwardLightData->DirectionalLightVSM = VirtualShadowMapId;

								ForwardLightData->NumDirectionalLightCascades = 0;
								// Unused cascades should compare > all scene depths
								ForwardLightData->CascadeEndDepths = FVector4f(MAX_FLT, MAX_FLT, MAX_FLT, MAX_FLT);

								for (const FProjectedShadowInfo* ShadowInfo : DirectionalLightShadowInfos)
								{
									if (ShadowInfo->DependentView)
									{
										// when rendering stereo views, allow using the shadows rendered for the primary view as 'close enough'
										if (ShadowInfo->DependentView != &View && ShadowInfo->DependentView != View.GetPrimaryView())
										{
											continue;
										}
									}

									const int32 CascadeIndex = ShadowInfo->CascadeSettings.ShadowSplitIndex;

									if (ShadowInfo->IsWholeSceneDirectionalShadow() && !ShadowInfo->HasVirtualShadowMap() && ShadowInfo->bAllocated && CascadeIndex < GMaxForwardShadowCascades)
									{
										const FMatrix WorldToShadow = ShadowInfo->GetWorldToShadowMatrix(ForwardLightData->DirectionalLightShadowmapMinMax[CascadeIndex]);
										const FMatrix44f TranslatedWorldToShadow = FMatrix44f(TranslatedWorldToWorld * WorldToShadow);

										ForwardLightData->NumDirectionalLightCascades++;
										ForwardLightData->DirectionalLightTranslatedWorldToShadowMatrix[CascadeIndex] = TranslatedWorldToShadow;
										ForwardLightData->CascadeEndDepths[CascadeIndex] = ShadowInfo->CascadeSettings.SplitFar;

										if (CascadeIndex == 0)
										{
											ForwardLightData->DirectionalLightShadowmapAtlas = GraphBuilder.RegisterExternalTexture(ShadowInfo->RenderTargets.DepthTarget);
											ForwardLightData->DirectionalLightDepthBias = ShadowInfo->GetShaderDepthBias();
											FVector2D AtlasSize = ForwardLightData->DirectionalLightShadowmapAtlas->Desc.Extent;
											ForwardLightData->DirectionalLightShadowmapAtlasBufferSize = FVector4f(AtlasSize.X, AtlasSize.Y, 1.0f / AtlasSize.X, 1.0f / AtlasSize.Y);
										}
									}
								}
							}

							const FStaticShadowDepthMap* StaticShadowDepthMap = LightSceneInfo->Proxy->GetStaticShadowDepthMap();
							const uint32 bStaticallyShadowedValue = LightSceneInfo->IsPrecomputedLightingValid() 
																	&& StaticShadowDepthMap 
																	&& StaticShadowDepthMap->Data 
																	&& !StaticShadowDepthMap->Data->WorldToLight.ContainsNaN()
																	&& StaticShadowDepthMap->TextureRHI ? 1 : 0;
							ForwardLightData->DirectionalLightUseStaticShadowing = bStaticallyShadowedValue;
							if (bStaticallyShadowedValue)
							{
								const FMatrix44f TranslatedWorldToShadow = FMatrix44f(TranslatedWorldToWorld * StaticShadowDepthMap->Data->WorldToLight);
								ForwardLightData->DirectionalLightStaticShadowBufferSize = FVector4f(StaticShadowDepthMap->Data->ShadowMapSizeX, StaticShadowDepthMap->Data->ShadowMapSizeY, 1.0f / StaticShadowDepthMap->Data->ShadowMapSizeX, 1.0f / StaticShadowDepthMap->Data->ShadowMapSizeY);
								ForwardLightData->DirectionalLightTranslatedWorldToStaticShadow = TranslatedWorldToShadow;
								ForwardLightData->DirectionalLightStaticShadowmap = StaticShadowDepthMap->TextureRHI;
							}
							else
							{
								ForwardLightData->DirectionalLightStaticShadowBufferSize = FVector4f(0, 0, 0, 0);
								ForwardLightData->DirectionalLightTranslatedWorldToStaticShadow = FMatrix44f::Identity;
								ForwardLightData->DirectionalLightStaticShadowmap = GWhiteTexture->TextureRHI;
							}
						}
					}
				}
			}
		}

#if WITH_EDITOR
		// For any views, if there are more than two light that compete for the forward shaded light, we report it.
		bMultipleDirLightsConflictForForwardShading |= ConflictingLightCountForForwardShading >= 2;
#endif

		// Store off the number of lights before we add a fake entry
		const int32 NumLocalLightsFinal = ForwardLocalLightData.Num();

		// Some platforms index the StructuredBuffer in the shader based on the stride specified at buffer creation time, not from the stride specified in the shader.
		// ForwardLocalLightBuffer is a StructuredBuffer<float4> in the shader, so create the buffer with a stride of sizeof(float4)
		static_assert(sizeof(FForwardLocalLightData) % sizeof(FVector4f) == 0, "ForwardLocalLightBuffer is used as a StructuredBuffer<float4> in the shader");
		const uint32 ForwardLocalLightDataSizeNumFloat4 = (NumLocalLightsFinal * sizeof(FForwardLocalLightData)) / sizeof(FVector4f);

		FRDGBufferRef ForwardLocalLightBuffer = CreateStructuredBuffer(GraphBuilder, TEXT("ForwardLocalLightBuffer"),
			TConstArrayView<FVector4f>(reinterpret_cast<const FVector4f*>(ForwardLocalLightData.GetData()), ForwardLocalLightDataSizeNumFloat4));

		View.ForwardLightingResources.LocalLightVisibleLightInfosIndex = LocalLightVisibleLightInfosIndex;

		const FIntPoint LightGridSizeXY = FIntPoint::DivideAndRoundUp(View.ViewRect.Size(), GLightGridPixelSize);
		ForwardLightData->ForwardLocalLightBuffer = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(ForwardLocalLightBuffer));
		ForwardLightData->NumLocalLights = NumLocalLightsFinal;
		ForwardLightData->NumReflectionCaptures = View.NumBoxReflectionCaptures + View.NumSphereReflectionCaptures;
		ForwardLightData->NumGridCells = LightGridSizeXY.X * LightGridSizeXY.Y * GLightGridSizeZ;
		ForwardLightData->CulledGridSize = FIntVector(LightGridSizeXY.X, LightGridSizeXY.Y, GLightGridSizeZ);
		ForwardLightData->MaxCulledLightsPerCell = GLightLinkedListCulling ? NumLocalLightsFinal: GMaxCulledLightsPerCell;
		ForwardLightData->LightGridPixelSizeShift = FMath::FloorLog2(GLightGridPixelSize);
		ForwardLightData->SimpleLightsEndIndex = SimpleLightsEnd;
		ForwardLightData->ClusteredDeferredSupportedEndIndex = ClusteredSupportedEnd;
		ForwardLightData->ManyLightsSupportedStartIndex = FMath::Min<int32>(ManyLightsSupportedStart, NumLocalLightsFinal);
		ForwardLightData->DirectLightingShowFlag = ViewFamily.EngineShowFlags.DirectLighting ? 1 : 0;

		// Clamp far plane to something reasonable
		const float KilometersToCentimeters = 100000.0f;
		const float LightCullingMaxDistance = GLightCullingMaxDistanceOverrideKilometers <= 0.0f ? (float)UE_OLD_HALF_WORLD_MAX / 5.0f : GLightCullingMaxDistanceOverrideKilometers * KilometersToCentimeters;
		float FarPlane = FMath::Min(FMath::Max(FurthestLight, View.FurthestReflectionCaptureDistance), LightCullingMaxDistance);
		FVector ZParams = GetLightGridZParams(View.NearClippingDistance, FarPlane + 10.f);
		ForwardLightData->LightGridZParams = (FVector3f)ZParams;

		const uint64 NumIndexableLights = !bLightGridUses16BitBuffers ? (1llu << (sizeof(FLightIndexType32) * 8llu)) : (1llu << (sizeof(FLightIndexType) * 8llu));

		if ((uint64)ForwardLocalLightData.Num() > NumIndexableLights)
		{
			static bool bWarned = false;

			if (!bWarned)
			{
				UE_LOG(LogRenderer, Warning, TEXT("Exceeded indexable light count, glitches will be visible (%u / %llu)"), ForwardLocalLightData.Num(), NumIndexableLights);
				bWarned = true;
			}
		}

#if ENABLE_LIGHT_CULLING_VIEW_SPACE_BUILD_DATA
		// Fuse these loops as I see no reason why not and we build some temporary data that is needed in the build pass and is 
		// not needed to be stored permanently.
#else // !ENABLE_LIGHT_CULLING_VIEW_SPACE_BUILD_DATA

		ForwardLightDataPerView.Emplace(ForwardLightData);
	}

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FViewInfo& View = Views[ViewIndex];
		FForwardLightData* ForwardLightData = ForwardLightDataPerView[ViewIndex];

		const FIntPoint LightGridSizeXY = FIntPoint::DivideAndRoundUp(View.ViewRect.Size(), GLightGridPixelSize);

#endif // ENABLE_LIGHT_CULLING_VIEW_SPACE_BUILD_DATA

		// Allocate buffers using the scene render targets size so we won't reallocate every frame with dynamic resolution
		const FIntPoint MaxLightGridSizeXY = FIntPoint::DivideAndRoundUp(View.GetSceneTexturesConfig().Extent, GLightGridPixelSize);

		const int32 MaxNumCells = MaxLightGridSizeXY.X * MaxLightGridSizeXY.Y * GLightGridSizeZ * NumCulledGridPrimitiveTypes;

		const FIntVector NumGroups = FIntVector::DivideAndRoundUp(FIntVector(LightGridSizeXY.X, LightGridSizeXY.Y, GLightGridSizeZ), LightGridInjectionGroupSize);

		{
			RDG_EVENT_SCOPE(GraphBuilder, "CullLights %ux%ux%u NumLights %u NumCaptures %u",
				ForwardLightData->CulledGridSize.X,
				ForwardLightData->CulledGridSize.Y,
				ForwardLightData->CulledGridSize.Z,
				ForwardLightData->NumLocalLights,
				ForwardLightData->NumReflectionCaptures);

			const uint32 CulledLightLinksElements = MaxNumCells * GMaxCulledLightsPerCell * LightLinkStride;


			FRDGBufferRef CulledLightLinksBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), CulledLightLinksElements), TEXT("CulledLightLinks"));
			FRDGBufferRef StartOffsetGridBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), MaxNumCells), TEXT("StartOffsetGrid"));
			FRDGBufferRef NextCulledLightLinkBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 1), TEXT("NextCulledLightLink"));
			FRDGBufferRef NextCulledLightDataBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 1), TEXT("NextCulledLightData"));
			FRDGBufferUAVRef NextCulledLightDataUAV = GraphBuilder.CreateUAV(NextCulledLightDataBuffer);
			FRDGBufferSRVRef CulledLightDataGridSRV = nullptr;
			FRDGBufferUAVRef CulledLightDataGridUAV = nullptr;
			FRDGBufferRef NumCulledLightsGrid       = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), MaxNumCells * NumCulledLightsGridStride), TEXT("NumCulledLightsGrid"));
			FRDGBufferUAVRef NumCulledLightsGridUAV = GraphBuilder.CreateUAV(NumCulledLightsGrid);

			if (bLightGridUses16BitBuffers)
			{
				const SIZE_T LightIndexTypeSize = sizeof(FLightIndexType);
				const EPixelFormat CulledLightDataGridFormat = PF_R16_UINT;
				FRDGBufferRef CulledLightDataGrid = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(LightIndexTypeSize, MaxNumCells * GMaxCulledLightsPerCell), TEXT("CulledLightDataGrid"));
				CulledLightDataGridSRV = GraphBuilder.CreateSRV(CulledLightDataGrid, CulledLightDataGridFormat);
				CulledLightDataGridUAV = GraphBuilder.CreateUAV(CulledLightDataGrid, CulledLightDataGridFormat);
			}
			else
			{
				const SIZE_T LightIndexTypeSize = sizeof(FLightIndexType32);
				const EPixelFormat CulledLightDataGridFormat = PF_R32_UINT;
				FRDGBufferRef CulledLightDataGrid = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(LightIndexTypeSize, MaxNumCells * GMaxCulledLightsPerCell), TEXT("CulledLightDataGrid"));
				CulledLightDataGridSRV = GraphBuilder.CreateSRV(CulledLightDataGrid);
				CulledLightDataGridUAV = GraphBuilder.CreateUAV(CulledLightDataGrid);
			}

			FLightGridInjectionCS::FParameters *PassParameters = GraphBuilder.AllocParameters<FLightGridInjectionCS::FParameters>();

			PassParameters->View                    = View.ViewUniformBuffer;
			
			if (IsMobilePlatform(View.GetShaderPlatform()))
			{
				PassParameters->MobileReflectionCaptureData = View.MobileReflectionCaptureUniformBuffer;
			}
			else
			{
				PassParameters->ReflectionCapture = View.ReflectionCaptureUniformBuffer;
			}

			PassParameters->RWNumCulledLightsGrid   = NumCulledLightsGridUAV;
			PassParameters->RWCulledLightDataGrid32Bit   = CulledLightDataGridUAV;
			PassParameters->RWCulledLightDataGrid16Bit = CulledLightDataGridUAV;
			PassParameters->RWNextCulledLightLink   = GraphBuilder.CreateUAV(NextCulledLightLinkBuffer);
			PassParameters->RWStartOffsetGrid       = GraphBuilder.CreateUAV(StartOffsetGridBuffer);
			PassParameters->RWCulledLightLinks      = GraphBuilder.CreateUAV(CulledLightLinksBuffer);
			PassParameters->ForwardLocalLightBuffer = ForwardLightData->ForwardLocalLightBuffer;
			PassParameters->CulledGridSize          = ForwardLightData->CulledGridSize;
			PassParameters->LightGridZParams        = ForwardLightData->LightGridZParams;
			PassParameters->NumReflectionCaptures   = ForwardLightData->NumReflectionCaptures;
			PassParameters->NumLocalLights          = ForwardLightData->NumLocalLights;
			PassParameters->MaxCulledLightsPerCell  = GMaxCulledLightsPerCell;
			PassParameters->NumGridCells            = ForwardLightData->NumGridCells;
			PassParameters->LightGridPixelSizeShift = ForwardLightData->LightGridPixelSizeShift;

#if ENABLE_LIGHT_CULLING_VIEW_SPACE_BUILD_DATA
			check(ViewSpacePosAndRadiusData.Num() == ForwardLocalLightData.Num());
			check(ViewSpaceDirAndPreprocAngleData.Num() == ForwardLocalLightData.Num());

			FRDGBufferRef LightViewSpacePositionAndRadius  = CreateStructuredBuffer(GraphBuilder, TEXT("ViewSpacePosAndRadiusData"), TConstArrayView<FVector4f>(ViewSpacePosAndRadiusData));
			FRDGBufferRef LightViewSpaceDirAndPreprocAngle = CreateStructuredBuffer(GraphBuilder, TEXT("ViewSpacePosAndRadiusData"), TConstArrayView<FVector4f>(ViewSpaceDirAndPreprocAngleData));

			PassParameters->LightViewSpacePositionAndRadius  = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(LightViewSpacePositionAndRadius));
			PassParameters->LightViewSpaceDirAndPreprocAngle = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(LightViewSpaceDirAndPreprocAngle));
#endif // ENABLE_LIGHT_CULLING_VIEW_SPACE_BUILD_DATA

			FLightGridInjectionCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FLightGridInjectionCS::FUseLinkedListDim>(GLightLinkedListCulling != 0);
			TShaderMapRef<FLightGridInjectionCS> ComputeShader(View.ShaderMap, PermutationVector);

			if (GLightLinkedListCulling != 0)
			{
				AddClearUAVPass(GraphBuilder, PassParameters->RWStartOffsetGrid, 0xFFFFFFFF);
				AddClearUAVPass(GraphBuilder, PassParameters->RWNextCulledLightLink, 0);
				AddClearUAVPass(GraphBuilder, NextCulledLightDataUAV, 0);
				AddClearUAVPass(GraphBuilder, NumCulledLightsGridUAV, 0);
				FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("LightGridInject:LinkedList"), ComputeShader, PassParameters, NumGroups);

				{
					TShaderMapRef<FLightGridCompactCS> ComputeShaderCompact(View.ShaderMap);
					FLightGridCompactCS::FParameters *PassParametersCompact = GraphBuilder.AllocParameters<FLightGridCompactCS::FParameters>();
					PassParametersCompact->View = View.ViewUniformBuffer;

					PassParametersCompact->CulledLightLinks = GraphBuilder.CreateSRV(CulledLightLinksBuffer);
					PassParametersCompact->RWNumCulledLightsGrid = NumCulledLightsGridUAV;
					PassParametersCompact->RWCulledLightDataGrid32Bit = CulledLightDataGridUAV;
					PassParametersCompact->RWCulledLightDataGrid16Bit = CulledLightDataGridUAV;
					PassParametersCompact->RWNextCulledLightData = NextCulledLightDataUAV;
					PassParametersCompact->StartOffsetGrid = GraphBuilder.CreateSRV(StartOffsetGridBuffer);

					PassParametersCompact->CulledGridSize = ForwardLightData->CulledGridSize;
					PassParametersCompact->NumReflectionCaptures = ForwardLightData->NumReflectionCaptures;
					PassParametersCompact->NumLocalLights = ForwardLightData->NumLocalLights;
					PassParametersCompact->NumGridCells = ForwardLightData->NumGridCells;

					Result.CompactLinksPass = FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("CompactLinks"), ComputeShaderCompact, PassParametersCompact, NumGroups);
				}
			}
			else
			{
				AddClearUAVPass(GraphBuilder, NumCulledLightsGridUAV, 0);
				FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("LightGridInject:NotLinkedList"), ComputeShader, PassParameters, NumGroups);
			}
			ForwardLightData->CulledLightDataGrid32Bit = CulledLightDataGridSRV;
			ForwardLightData->CulledLightDataGrid16Bit = CulledLightDataGridSRV;
			ForwardLightData->NumCulledLightsGrid = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(NumCulledLightsGrid));
			View.ForwardLightingResources.SetUniformBuffer(GraphBuilder.CreateUniformBuffer(ForwardLightData));
		}
	}

#if WITH_EDITOR
	if (bMultipleDirLightsConflictForForwardShading)
	{
		OnGetOnScreenMessages.AddLambda([](FScreenMessageWriter& ScreenMessageWriter)->void
		{
			static const FText Message = NSLOCTEXT("Renderer", "MultipleDirLightsConflictForForwardShading", "Multiple directional lights are competing to be the single one used for forward shading, translucent, water or volumetric fog. Please adjust their ForwardShadingPriority.\nAs a fallback, the main directional light will be selected based on overall brightness.");
			ScreenMessageWriter.DrawLine(Message, 10, FColor::Orange);
		});
	}
#endif

	return Result;
}

FComputeLightGridOutput FDeferredShadingSceneRenderer::GatherLightsAndComputeLightGrid(FRDGBuilder& GraphBuilder, bool bNeedLightGrid, FSortedLightSetSceneInfo& SortedLightSet)
{
	SCOPED_NAMED_EVENT(GatherLightsAndComputeLightGrid, FColor::Emerald);
	FComputeLightGridOutput Result = {};

	bool bShadowedLightsInClustered = ShouldUseClusteredDeferredShading()
		&& CVarVirtualShadowOnePassProjection.GetValueOnRenderThread()
		&& VirtualShadowMapArray.IsEnabled();

	GatherAndSortLights(SortedLightSet, bShadowedLightsInClustered);
	
	if (!bNeedLightGrid)
	{
		SetDummyForwardLightUniformBufferOnViews(GraphBuilder, ShaderPlatform, Views);
		return Result;
	}

	bool bAnyViewUsesForwardLighting = false;
	bool bAnyViewUsesLumen = false;
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FViewInfo& View = Views[ViewIndex];
		bAnyViewUsesForwardLighting |= View.bTranslucentSurfaceLighting || ShouldRenderVolumetricFog() || View.bHasSingleLayerWaterMaterial 
			|| VolumetricCloudWantsToSampleLocalLights(Scene, ViewFamily.EngineShowFlags) || ShouldVisualizeLightGrid() || ShouldRenderLocalFogVolume(Scene, ViewFamily);
		bAnyViewUsesLumen |= GetViewPipelineState(View).DiffuseIndirectMethod == EDiffuseIndirectMethod::Lumen || GetViewPipelineState(View).ReflectionsMethod == EReflectionsMethod::Lumen;
	}
	
	const bool bCullLightsToGrid = GLightCullingQuality 
		&& (IsForwardShadingEnabled(ShaderPlatform) || bAnyViewUsesForwardLighting || IsRayTracingEnabled() || ShouldUseClusteredDeferredShading() ||
			bAnyViewUsesLumen || ViewFamily.EngineShowFlags.VisualizeMeshDistanceFields || VirtualShadowMapArray.IsEnabled() || ManyLights::IsEnabled());

	// Store this flag if lights are injected in the grids, check with 'AreLightsInLightGrid()'
	bAreLightsInLightGrid = bCullLightsToGrid;
	
	Result = ComputeLightGrid(GraphBuilder, bCullLightsToGrid, SortedLightSet);

	return Result;
}

void FDeferredShadingSceneRenderer::RenderForwardShadowProjections(
	FRDGBuilder& GraphBuilder,
	const FMinimalSceneTextures& SceneTextures,
	FRDGTextureRef& OutForwardScreenSpaceShadowMask,
	FRDGTextureRef& OutForwardScreenSpaceShadowMaskSubPixel)
{
	CheckShadowDepthRenderCompleted();

	const bool bIsHairEnable = HairStrands::HasViewHairStrandsData(Views);
	bool bScreenShadowMaskNeeded = false;

	FRDGTextureRef SceneDepthTexture = SceneTextures.Depth.Target;

	for (auto LightIt = Scene->Lights.CreateConstIterator(); LightIt; ++LightIt)
	{
		const FLightSceneInfoCompact& LightSceneInfoCompact = *LightIt;
		const FLightSceneInfo* const LightSceneInfo = LightSceneInfoCompact.LightSceneInfo;
		const FVisibleLightInfo& VisibleLightInfo = VisibleLightInfos[LightSceneInfo->Id];

		bScreenShadowMaskNeeded |= VisibleLightInfo.ShadowsToProject.Num() > 0 || VisibleLightInfo.CapsuleShadowsToProject.Num() > 0 || LightSceneInfo->Proxy->GetLightFunctionMaterial() != nullptr;
	}

	if (bScreenShadowMaskNeeded)
	{
		RDG_CSV_STAT_EXCLUSIVE_SCOPE(GraphBuilder, RenderForwardShadingShadowProjections);

		FRDGTextureMSAA ForwardScreenSpaceShadowMask;
		FRDGTextureMSAA ForwardScreenSpaceShadowMaskSubPixel;

		{
			FRDGTextureDesc Desc(FRDGTextureDesc::Create2D(SceneTextures.Config.Extent, PF_B8G8R8A8, FClearValueBinding::White, TexCreate_RenderTargetable | TexCreate_ShaderResource));
			Desc.NumSamples = SceneDepthTexture->Desc.NumSamples;
			ForwardScreenSpaceShadowMask = CreateTextureMSAA(GraphBuilder, Desc, TEXT("ShadowMaskTextureMS"), TEXT("ShadowMaskTexture"), GFastVRamConfig.ScreenSpaceShadowMask);
			if (bIsHairEnable)
			{
				Desc.NumSamples = 1;
				ForwardScreenSpaceShadowMaskSubPixel = CreateTextureMSAA(GraphBuilder, Desc, TEXT("ShadowMaskSubPixelTextureMS"), TEXT("ShadowMaskSubPixelTexture"), GFastVRamConfig.ScreenSpaceShadowMask);
			}
		}

		RDG_EVENT_SCOPE(GraphBuilder, "ShadowProjectionOnOpaque");
		RDG_GPU_STAT_SCOPE(GraphBuilder, ShadowProjection);

		// All shadows render with min blending
		AddClearRenderTargetPass(GraphBuilder, ForwardScreenSpaceShadowMask.Target);
		if (bIsHairEnable)
		{
			AddClearRenderTargetPass(GraphBuilder, ForwardScreenSpaceShadowMaskSubPixel.Target);
		}

		const bool bProjectingForForwardShading = true;

		for (auto LightIt = Scene->Lights.CreateConstIterator(); LightIt; ++LightIt)
		{
			const FLightSceneInfoCompact& LightSceneInfoCompact = *LightIt;
			const FLightSceneInfo* const LightSceneInfo = LightSceneInfoCompact.LightSceneInfo;
			FVisibleLightInfo& VisibleLightInfo = VisibleLightInfos[LightSceneInfo->Id];

			const bool bIssueLightDrawEvent = VisibleLightInfo.ShadowsToProject.Num() > 0 || VisibleLightInfo.CapsuleShadowsToProject.Num() > 0;

			FString LightNameWithLevel;
			GetLightNameForDrawEvent(LightSceneInfo->Proxy, LightNameWithLevel);
			RDG_EVENT_SCOPE_CONDITIONAL(GraphBuilder, bIssueLightDrawEvent, "%s", *LightNameWithLevel);

			if (VisibleLightInfo.ShadowsToProject.Num() > 0)
			{
				RenderShadowProjections(
					GraphBuilder,
					SceneTextures,
					ForwardScreenSpaceShadowMask.Target,
					ForwardScreenSpaceShadowMaskSubPixel.Target,
					LightSceneInfo,
					bProjectingForForwardShading);

				if (bIsHairEnable)
				{
					RenderHairStrandsShadowMask(GraphBuilder, Views, LightSceneInfo, VisibleLightInfos, bProjectingForForwardShading, ForwardScreenSpaceShadowMask.Target);
				}
			}

			RenderCapsuleDirectShadows(GraphBuilder, SceneTextures.UniformBuffer, *LightSceneInfo, ForwardScreenSpaceShadowMask.Target, VisibleLightInfo.CapsuleShadowsToProject, bProjectingForForwardShading);

			if (LightSceneInfo->GetDynamicShadowMapChannel() >= 0 && LightSceneInfo->GetDynamicShadowMapChannel() < 4)
			{
				RenderLightFunction(
					GraphBuilder,
					SceneTextures,
					LightSceneInfo,
					ForwardScreenSpaceShadowMask.Target,
					true, true, false);
			}
		}

		auto* PassParameters = GraphBuilder.AllocParameters<FRenderTargetParameters>();
		PassParameters->RenderTargets[0] = FRenderTargetBinding(ForwardScreenSpaceShadowMask.Target, ForwardScreenSpaceShadowMask.Resolve, ERenderTargetLoadAction::ELoad);
		OutForwardScreenSpaceShadowMask = ForwardScreenSpaceShadowMask.Resolve;

		if (bIsHairEnable)
		{
			OutForwardScreenSpaceShadowMaskSubPixel = ForwardScreenSpaceShadowMaskSubPixel.Target;
		}

		GraphBuilder.AddPass(RDG_EVENT_NAME("ResolveScreenSpaceShadowMask"), PassParameters, ERDGPassFlags::Raster, [](FRHICommandList&) {});
	}
}

class FDebugLightGridPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FDebugLightGridPS);
	SHADER_USE_PARAMETER_STRUCT(FDebugLightGridPS, FGlobalShader);

	using FPermutationDomain = TShaderPermutationDomain<>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FForwardLightData, Forward)
		SHADER_PARAMETER_STRUCT_INCLUDE(ShaderPrint::FShaderParameters, ShaderPrintParameters)
		SHADER_PARAMETER_TEXTURE(Texture2D, MiniFontTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DepthTexture)
		SHADER_PARAMETER(uint32, DebugMode)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static FPermutationDomain RemapPermutation(FPermutationDomain PermutationVector)
	{
		return PermutationVector;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return EnumHasAllFlags(Parameters.Flags, EShaderPermutationFlags::HasEditorOnlyData) && ShaderPrint::IsSupported(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		ShaderPrint::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		// Stay debug and skip optimizations to reduce compilation time on this long shader.
		OutEnvironment.CompilerFlags.Add(CFLAG_Debug);
		OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
		OutEnvironment.SetDefine(TEXT("SHADER_DEBUG_LIGHT_GRID_PS"), 1);
		FForwardLightingParameters::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
	}
};
IMPLEMENT_GLOBAL_SHADER(FDebugLightGridPS, "/Engine/Private/LightGridInjection.usf", "DebugLightGridPS", SF_Pixel);

FScreenPassTexture AddVisualizeLightGridPass(FRDGBuilder& GraphBuilder, const FViewInfo& View, FScreenPassTexture& ScreenPassSceneColor, FRDGTextureRef SceneDepthTexture)
{
	if (ShaderPrint::IsSupported(View.Family->GetShaderPlatform()))
	{
		RDG_EVENT_SCOPE(GraphBuilder, "VisualizeLightGrid");

		// Force ShaderPrint on.
		ShaderPrint::SetEnabled(true);

		ShaderPrint::RequestSpaceForLines(128);
		ShaderPrint::RequestSpaceForCharacters(128);

		FDebugLightGridPS::FPermutationDomain PermutationVector;
		TShaderMapRef<FDebugLightGridPS> PixelShader(View.ShaderMap, PermutationVector);
		FDebugLightGridPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FDebugLightGridPS::FParameters>();
		PassParameters->ViewUniformBuffer = View.ViewUniformBuffer;
		PassParameters->Forward = View.ForwardLightingResources.ForwardLightUniformBuffer;
		ShaderPrint::SetParameters(GraphBuilder, View.ShaderPrintData, PassParameters->ShaderPrintParameters);
		PassParameters->DepthTexture = SceneDepthTexture ? SceneDepthTexture : GSystemTextures.GetMaxFP16Depth(GraphBuilder);
		PassParameters->MiniFontTexture = GetMiniFontTexture();
		PassParameters->RenderTargets[0] = FRenderTargetBinding(ScreenPassSceneColor.Texture, ERenderTargetLoadAction::ELoad);
		PassParameters->DebugMode = GForwardLightGridDebug;

		FRHIBlendState* PreMultipliedColorTransmittanceBlend = TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_SourceAlpha, BO_Add, BF_Zero, BF_One>::GetRHI();

		FPixelShaderUtils::AddFullscreenPass<FDebugLightGridPS>(GraphBuilder, View.ShaderMap, RDG_EVENT_NAME("DebugLightGridCS"), PixelShader, PassParameters,
			ScreenPassSceneColor.ViewRect, PreMultipliedColorTransmittanceBlend);
	}

	return MoveTemp(ScreenPassSceneColor);
}
