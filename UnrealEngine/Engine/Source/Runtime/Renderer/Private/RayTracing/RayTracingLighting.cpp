// Copyright Epic Games, Inc. All Rights Reserved.

#include "RayTracingLighting.h"
#include "RHIDefinitions.h"
#include "RendererPrivate.h"

#if RHI_RAYTRACING

#include "SceneRendering.h"

static TAutoConsoleVariable<int32> CVarRayTracingLightingCells(
	TEXT("r.RayTracing.LightCulling.Cells"),
	16,
	TEXT("Number of cells in each dimension for lighting grid (default 16)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarRayTracingLightingCellSize(
	TEXT("r.RayTracing.LightCulling.CellSize"),
	200.0f,
	TEXT("Minimum size of light cell (default 200 units)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarRayTracingLightFunction(
	TEXT("r.RayTracing.LightFunction"),
	1,
	TEXT("Whether to support light material functions in ray tracing effects. (default = 1)"),
	ECVF_RenderThreadSafe);
IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FRaytracingLightDataPacked, "RaytracingLightsDataPacked");

class FSetupRayTracingLightCullData : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSetupRayTracingLightCullData);
	SHADER_USE_PARAMETER_STRUCT(FSetupRayTracingLightCullData, FGlobalShader)

	static int32 GetGroupSize()
	{
		return 32;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		// Allow this shader to be compiled if either inline or full pipeline ray tracing mode is supported by the platform
		return IsRayTracingEnabledForProject(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, RankedLights)
		SHADER_PARAMETER(FVector3f, TranslatedWorldPos)
		SHADER_PARAMETER(uint32, NumLightsToUse)
		SHADER_PARAMETER(uint32, CellCount)
		SHADER_PARAMETER(float, CellScale)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, LightIndices)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint4>, LightCullingVolume)
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FSetupRayTracingLightCullData, "/Engine/Private/RayTracing/GenerateCulledLightListCS.usf", "GenerateCulledLightListCS", SF_Compute);
DECLARE_GPU_STAT_NAMED(LightCullingVolumeCompute, TEXT("RT Light Culling Volume Compute"));


static void SelectRaytracingLights(
	const FScene::FLightSceneInfoCompactSparseArray& Lights,
	TArray<int32>& OutSelectedLights,
	uint32& NumOfSkippedRayTracingLights
)
{
	OutSelectedLights.Empty();
	NumOfSkippedRayTracingLights = 0;

	for (auto Light : Lights)
	{
		const bool bHasStaticLighting = Light.LightSceneInfo->Proxy->HasStaticLighting() && Light.LightSceneInfo->IsPrecomputedLightingValid();
		const bool bAffectReflection = Light.LightSceneInfo->Proxy->AffectReflection();
		if (bHasStaticLighting || !bAffectReflection) continue;

		if (OutSelectedLights.Num() < RAY_TRACING_LIGHT_COUNT_MAXIMUM)
		{
			OutSelectedLights.Add(Light.LightSceneInfo->Id);
		}
		else
		{
			NumOfSkippedRayTracingLights++;
		};
	}
}

static int32 GetCellsPerDim()
{
	// round to next even as the structure relies on symmetry for address computations
	return (FMath::Max(2, CVarRayTracingLightingCells.GetValueOnRenderThread()) + 1) & (~1); 
}

static void CreateRaytracingLightCullingStructure(
	FRDGBuilder& GraphBuilder,
	const FScene::FLightSceneInfoCompactSparseArray& Lights,
	const FSceneView& View,
	FGlobalShaderMap* ShaderMap,
	const TArray<int32>& LightIndices,
	FRDGBufferRef& LightCullVolume,
	FRDGBufferRef& LightIndicesBuffer)
{
	const int32 NumLightsToUse = LightIndices.Num();

	struct FCullRecord
	{
		uint32 NumLights;
		uint32 Offset;
		uint32 Unused1;
		uint32 Unused2;
	};

	const int32 CellsPerDim = GetCellsPerDim();

	FRDGUploadData<FVector4f> RankedLights(GraphBuilder, FMath::Max(NumLightsToUse, 1));

	// setup light vector array sorted by rank
	for (int32 LightIndex = 0; LightIndex < NumLightsToUse; LightIndex++)
	{
		VectorRegister BoundingSphereRegister = Lights[LightIndices[LightIndex]].BoundingSphereVector;
		FVector4 BoundingSphere = FVector4(
			VectorGetComponentImpl<0>(BoundingSphereRegister),
			VectorGetComponentImpl<1>(BoundingSphereRegister),
			VectorGetComponentImpl<2>(BoundingSphereRegister),
			VectorGetComponentImpl<3>(BoundingSphereRegister)
		);
		FVector4f TranslatedBoundingSphere = FVector4f(BoundingSphere + View.ViewMatrices.GetPreViewTranslation());
		RankedLights[LightIndex] = TranslatedBoundingSphere;
	}

	FRDGBufferRef RayTracingCullLights = CreateStructuredBuffer(GraphBuilder, TEXT("RayTracingCullLights"), RankedLights);

	LightCullVolume = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(FUintVector4), CellsPerDim * CellsPerDim * CellsPerDim), TEXT("RayTracingLightCullVolume"));

	{
		FRDGBufferDesc BufferDesc;
		BufferDesc.Usage = EBufferUsageFlags::Static | EBufferUsageFlags::UnorderedAccess | EBufferUsageFlags::ShaderResource | EBufferUsageFlags::VertexBuffer;
		BufferDesc.BytesPerElement = sizeof(uint16);
		BufferDesc.NumElements = FMath::Max(NumLightsToUse, 1) * CellsPerDim * CellsPerDim * CellsPerDim;

		LightIndicesBuffer = GraphBuilder.CreateBuffer(BufferDesc, TEXT("RayTracingLightIndices"));
	}

	{
		auto Parameters = GraphBuilder.AllocParameters<FSetupRayTracingLightCullData::FParameters>();

		Parameters->RankedLights = GraphBuilder.CreateSRV(RayTracingCullLights);
		Parameters->LightCullingVolume = GraphBuilder.CreateUAV(LightCullVolume);
		Parameters->LightIndices = GraphBuilder.CreateUAV(LightIndicesBuffer, EPixelFormat::PF_R16_UINT);

		Parameters->TranslatedWorldPos = (FVector3f)(View.ViewMatrices.GetViewOrigin() + View.ViewMatrices.GetPreViewTranslation());
		Parameters->NumLightsToUse = NumLightsToUse;
		Parameters->CellCount = CellsPerDim;
		Parameters->CellScale = CVarRayTracingLightingCellSize.GetValueOnRenderThread() / 2.0f; // cells are based on pow2, and initial cell is 2^1, so scale is half min cell size

		auto Shader = ShaderMap->GetShader<FSetupRayTracingLightCullData>();
		FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("LightCullingVolumeCompute"), Shader, Parameters, FIntVector(CellsPerDim, CellsPerDim, CellsPerDim));
	}
}


static void SetupRaytracingLightDataPacked(
	FRDGBuilder& GraphBuilder,
	const FScene* Scene,
	const TArray<int32>& LightIndices,
	const FSceneView& View,
	FRaytracingLightDataPacked& OutLightData,
	TArrayView<FRTLightingData>& OutLightDataArray)
{
	const FScene::FLightSceneInfoCompactSparseArray& Lights = Scene->Lights;
	TMap<UTextureLightProfile*, int32> IESLightProfilesMap;
	TMap<FRHITexture*, uint32> RectTextureMap;

	OutLightData.Count = 0;
	{
		// IES profiles
		FRHITexture* IESTextureRHI = nullptr;
		float IESInvProfileCount = 1.0f;

		if (View.IESLightProfileResource && View.IESLightProfileResource->GetIESLightProfilesCount())
		{
			OutLightData.IESLightProfileTexture = View.IESLightProfileResource->GetTexture();

			uint32 ProfileCount = View.IESLightProfileResource->GetIESLightProfilesCount();
			IESInvProfileCount = ProfileCount ? 1.f / static_cast<float>(ProfileCount) : 0.f;
		}
		else
		{
			OutLightData.IESLightProfileTexture = GWhiteTexture->TextureRHI;
		}

		OutLightData.IESLightProfileInvCount = IESInvProfileCount;
		OutLightData.IESLightProfileTextureSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	}

	const FRayTracingLightFunctionMap* RayTracingLightFunctionMap = GraphBuilder.Blackboard.Get<FRayTracingLightFunctionMap>();
	for (auto LightIndex : LightIndices)
	{
		auto Light = Lights[LightIndex];
		const bool bHasStaticLighting = Light.LightSceneInfo->Proxy->HasStaticLighting() && Light.LightSceneInfo->IsPrecomputedLightingValid();
		const bool bAffectReflection = Light.LightSceneInfo->Proxy->AffectReflection();
		checkf(!bHasStaticLighting && bAffectReflection, TEXT("Lights need to be prefiltered by SelectRaytracingLights()."));

		FLightRenderParameters LightParameters;
		Light.LightSceneInfo->Proxy->GetLightShaderParameters(LightParameters);

		if (Light.LightSceneInfo->Proxy->IsInverseSquared())
		{
			LightParameters.FalloffExponent = 0;
		}

		int32 IESLightProfileIndex = INDEX_NONE;
		if (View.Family->EngineShowFlags.TexturedLightProfiles)
		{
			UTextureLightProfile* IESLightProfileTexture = Light.LightSceneInfo->Proxy->GetIESTexture();
			if (IESLightProfileTexture)
			{
				int32* IndexFound = IESLightProfilesMap.Find(IESLightProfileTexture);
				if (!IndexFound)
				{
					IESLightProfileIndex = IESLightProfilesMap.Add(IESLightProfileTexture, IESLightProfilesMap.Num());
				}
				else
				{
					IESLightProfileIndex = *IndexFound;
				}
			}
		}

		FRTLightingData& LightDataElement = OutLightDataArray[OutLightData.Count];

		LightDataElement.Type = Light.LightType;
		LightDataElement.LightProfileIndex = IESLightProfileIndex;

		LightDataElement.Direction = LightParameters.Direction;
		LightDataElement.TranslatedLightPosition = FVector3f(LightParameters.WorldPosition + View.ViewMatrices.GetPreViewTranslation());
		LightDataElement.LightColor = FVector3f(LightParameters.Color) * LightParameters.GetLightExposureScale(View.GetLastEyeAdaptationExposure());
		LightDataElement.Tangent = LightParameters.Tangent;

		// Ray tracing should compute fade parameters ignoring lightmaps
		const FVector2D FadeParams = Light.LightSceneInfo->Proxy->GetDirectionalLightDistanceFadeParameters(View.GetFeatureLevel(), false, View.MaxShadowCascades);
		const FVector2D DistanceFadeMAD = { FadeParams.Y, -FadeParams.X * FadeParams.Y };

		for (int32 Element = 0; Element < 2; Element++)
		{
			LightDataElement.SpotAngles[Element] = LightParameters.SpotAngles[Element];
			LightDataElement.DistanceFadeMAD[Element] = DistanceFadeMAD[Element];
		}

		LightDataElement.InvRadius = LightParameters.InvRadius;
		LightDataElement.SpecularScale = LightParameters.SpecularScale;
		LightDataElement.FalloffExponent = LightParameters.FalloffExponent;
		LightDataElement.SourceRadius = LightParameters.SourceRadius;
		LightDataElement.SourceLength = LightParameters.SourceLength;
		LightDataElement.SoftSourceRadius = LightParameters.SoftSourceRadius;
		LightDataElement.RectLightBarnCosAngle = LightParameters.RectLightBarnCosAngle;
		LightDataElement.RectLightBarnLength = LightParameters.RectLightBarnLength;

		LightDataElement.RectLightAtlasUVOffset[0] = LightParameters.RectLightAtlasUVOffset.X;
		LightDataElement.RectLightAtlasUVOffset[1] = LightParameters.RectLightAtlasUVOffset.Y;
		LightDataElement.RectLightAtlasUVScale[0] = LightParameters.RectLightAtlasUVScale.X;
		LightDataElement.RectLightAtlasUVScale[1] = LightParameters.RectLightAtlasUVScale.Y;
		LightDataElement.RectLightAtlasMaxLevel = LightParameters.RectLightAtlasMaxLevel;
		LightDataElement.LightMissShaderIndex = RAY_TRACING_MISS_SHADER_SLOT_LIGHTING;

		// Stuff directional light's shadow angle factor into a RectLight parameter
		if (Light.LightType == LightType_Directional)
		{
			LightDataElement.RectLightBarnCosAngle = Light.LightSceneInfo->Proxy->GetShadowSourceAngleFactor();
		}

		// NOTE: This map will be empty if the light functions are disabled for some reason
		if (RayTracingLightFunctionMap)
		{
			const int32* LightFunctionIndex = RayTracingLightFunctionMap->Find(Light.LightSceneInfo);
			if (LightFunctionIndex)
			{
				check(uint32(*LightFunctionIndex) > RAY_TRACING_MISS_SHADER_SLOT_LIGHTING);
				check(uint32(*LightFunctionIndex) < Scene->RayTracingScene.NumMissShaderSlots);
				LightDataElement.LightMissShaderIndex = *LightFunctionIndex;
			}
		}

		OutLightData.Count++;
	}

	check(OutLightData.Count <= RAY_TRACING_LIGHT_COUNT_MAXIMUM);

	// Update IES light profiles texture 
	// TODO (Move to a shared place)
	if (View.IESLightProfileResource != nullptr && IESLightProfilesMap.Num() > 0)
	{
		TArray<UTextureLightProfile*, SceneRenderingAllocator> IESProfilesArray;
		IESProfilesArray.AddUninitialized(IESLightProfilesMap.Num());
		for (auto It = IESLightProfilesMap.CreateIterator(); It; ++It)
		{
			IESProfilesArray[It->Value] = It->Key;
		}

		View.IESLightProfileResource->BuildIESLightProfilesTexture(GraphBuilder.RHICmdList, IESProfilesArray);
	}
}


TRDGUniformBufferRef<FRaytracingLightDataPacked> CreateRayTracingLightData(
	FRDGBuilder& GraphBuilder,
	const FScene* Scene,
	const FSceneView& View,
	FGlobalShaderMap* ShaderMap,
	uint32& NumOfSkippedRayTracingLights)
{
	auto* LightData = GraphBuilder.AllocParameters<FRaytracingLightDataPacked>();
	LightData->CellCount = GetCellsPerDim();
	LightData->CellScale = CVarRayTracingLightingCellSize.GetValueOnRenderThread() / 2.0f;

	TArray<int32> LightIndices;
	SelectRaytracingLights(Scene->Lights, LightIndices, NumOfSkippedRayTracingLights);

	// Create light culling volume
	FRDGBufferRef LightCullVolume;
	FRDGBufferRef LightIndicesBuffer;
	CreateRaytracingLightCullingStructure(GraphBuilder, Scene->Lights, View, ShaderMap, LightIndices, LightCullVolume, LightIndicesBuffer);

	FRDGUploadData<FRTLightingData> LightDataArray(GraphBuilder, FMath::Max(LightIndices.Num(), 1));
	SetupRaytracingLightDataPacked(GraphBuilder, Scene, LightIndices, View, *LightData, LightDataArray);

	check(LightData->Count == LightIndices.Num());

	static_assert(sizeof(FRTLightingData) % sizeof(FUintVector4) == 0, "sizeof(FRTLightingData) must be a multiple of sizeof(FUintVector4)");
	const uint32 NumUintVector4Elements = LightDataArray.GetTotalSize() / sizeof(FUintVector4);
	FRDGBufferRef LightBuffer = CreateStructuredBuffer(GraphBuilder, TEXT("LightBuffer"), sizeof(FUintVector4), NumUintVector4Elements, LightDataArray.GetData(), LightDataArray.GetTotalSize(), ERDGInitialDataFlags::NoCopy);

	LightData->LightDataBuffer = GraphBuilder.CreateSRV(LightBuffer);
	LightData->LightIndices = GraphBuilder.CreateSRV(LightIndicesBuffer, EPixelFormat::PF_R16_UINT);
	LightData->LightCullingVolume = GraphBuilder.CreateSRV(LightCullVolume);

	return GraphBuilder.CreateUniformBuffer(LightData);
}

class FRayTracingLightingMS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRayTracingLightingMS);
	SHADER_USE_ROOT_PARAMETER_STRUCT(FRayTracingLightingMS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FRaytracingLightDataPacked, LightDataPacked)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}
};

IMPLEMENT_GLOBAL_SHADER(FRayTracingLightingMS, "/Engine/Private/RayTracing/RayTracingLightingMS.usf", "RayTracingLightingMS", SF_RayMiss);

/**
 * FLightFunctionParametersRayTracing
 * Global constant buffer derived from loose parameters of standard light function materials. Note that it lacks
 * the screen to world transform, as the RT version never have screen as a reference frame
 * This is nearly identical to the one found in Lumen and ultimately should be converted to a shared solution
 *
 * function to create the constant buffer is deirved from the LightFunctionMaterial SetParameters code
 */
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FLightFunctionParametersRayTracing, )
	SHADER_PARAMETER(FMatrix44f, LightFunctionTranslatedWorldToLight)
	SHADER_PARAMETER(FVector4f, LightFunctionParameters)
	SHADER_PARAMETER(FVector3f, LightFunctionParameters2)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FLightFunctionParametersRayTracing, "RaytracingLightFunctionParameters");

static TUniformBufferRef<FLightFunctionParametersRayTracing> CreateLightFunctionParametersBufferRT(
	const FLightSceneInfo* LightSceneInfo,
	const FSceneView& View,
	EUniformBufferUsage Usage)
{
	FLightFunctionParametersRayTracing LightFunctionParameters;

	const FVector Scale = LightSceneInfo->Proxy->GetLightFunctionScale();
	// Switch x and z so that z of the user specified scale affects the distance along the light direction
	const FVector InverseScale = FVector(1.f / Scale.Z, 1.f / Scale.Y, 1.f / Scale.X);
	const FMatrix WorldToLight = LightSceneInfo->Proxy->GetWorldToLight() * FScaleMatrix(FVector(InverseScale));

	LightFunctionParameters.LightFunctionTranslatedWorldToLight = FMatrix44f(FTranslationMatrix(-View.ViewMatrices.GetPreViewTranslation()) * WorldToLight);

	const bool bIsSpotLight = LightSceneInfo->Proxy->GetLightType() == LightType_Spot;
	const bool bIsPointLight = LightSceneInfo->Proxy->GetLightType() == LightType_Point;
	const float TanOuterAngle = bIsSpotLight ? FMath::Tan(LightSceneInfo->Proxy->GetOuterConeAngle()) : 1.0f;

	// should this match raster?
	const float ShadowFadeFraction = 1.0f;

	LightFunctionParameters.LightFunctionParameters = FVector4f(TanOuterAngle, ShadowFadeFraction, bIsSpotLight ? 1.0f : 0.0f, bIsPointLight ? 1.0f : 0.0f);

	// do we need this?
	const bool bRenderingPreviewShadowIndicator = false;

	LightFunctionParameters.LightFunctionParameters2 = FVector3f(
		LightSceneInfo->Proxy->GetLightFunctionFadeDistance(),
		LightSceneInfo->Proxy->GetLightFunctionDisabledBrightness(),
		bRenderingPreviewShadowIndicator ? 1.0f : 0.0f);

	return CreateUniformBufferImmediate(LightFunctionParameters, Usage);
}

/**
 * Generic light function for ray tracing compilable as miss shader with lighting
 */
class FLightFunctionRayTracingShader : public FMaterialShader
{
	DECLARE_SHADER_TYPE(FLightFunctionRayTracingShader, Material);
public:

	/**
	  * Makes sure only shaders for materials that are explicitly flagged
	  * as 'UsedAsLightFunction' in the Material Editor gets compiled into
	  * the shader cache.
	  */
	static bool ShouldCompilePermutation(const FMaterialShaderPermutationParameters& Parameters)
	{
		return Parameters.MaterialParameters.MaterialDomain == MD_LightFunction && ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	FLightFunctionRayTracingShader() {}
	FLightFunctionRayTracingShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMaterialShader(Initializer)
	{
		LightMaterialsParameter.Bind(Initializer.ParameterMap, TEXT("RaytracingLightFunctionParameters"));
		LightDataPacked.Bind(Initializer.ParameterMap, TEXT("RaytracingLightsDataPacked"));
	}

	void GetShaderBindings(
		const FScene* Scene,
		ERHIFeatureLevel::Type FeatureLevel,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material,
		const FViewInfo& View,
		const TUniformBufferRef<FDeferredLightUniformStruct>& DeferredLightBuffer,
		const TUniformBufferRef<FLightFunctionParametersRayTracing>& LightFunctionParameters,
		const TUniformBufferRef<FRaytracingLightDataPacked>& LightDataPackedBuffer,
		FMeshDrawSingleShaderBindings& ShaderBindings) const
	{
		FMaterialShader::GetShaderBindings(Scene, FeatureLevel, MaterialRenderProxy, Material, ShaderBindings);

		// Bind view
		ShaderBindings.Add(GetUniformBufferParameter<FViewUniformShaderParameters>(), View.ViewUniformBuffer);

		// Bind light parameters
		ShaderBindings.Add(GetUniformBufferParameter<FDeferredLightUniformStruct>(), DeferredLightBuffer);

		//bind Lightfunction parameters
		ShaderBindings.Add(LightMaterialsParameter, LightFunctionParameters);

		//bind light data
		ShaderBindings.Add(LightDataPacked, LightDataPackedBuffer);
	}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("SUPPORT_LIGHT_FUNCTION"), 1);
		FMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

private:

	LAYOUT_FIELD(FShaderUniformBufferParameter, LightMaterialsParameter);
	LAYOUT_FIELD(FShaderUniformBufferParameter, LightDataPacked);
};

IMPLEMENT_MATERIAL_SHADER_TYPE(, FLightFunctionRayTracingShader, TEXT("/Engine/Private/RayTracing/RayTracingLightingMS.usf"), TEXT("RayTracingLightingMS"), SF_RayMiss);

FRayTracingLightFunctionMap GatherLightFunctionLights(FScene* Scene, const FEngineShowFlags EngineShowFlags, ERHIFeatureLevel::Type InFeatureLevel)
{
	checkf(EngineShowFlags.LightFunctions, TEXT("This function should not be called if light functions are disabled"));

	// gives control over lighting functions in raytraced effects, independently of the show flag (for performance testing / debug)
	if (CVarRayTracingLightFunction.GetValueOnRenderThread() == 0)
	{
		return {};
	}

	FRayTracingLightFunctionMap RayTracingLightFunctionMap;
	for (const FLightSceneInfoCompact& Light : Scene->Lights)
	{
		FLightSceneInfo* LightSceneInfo = Light.LightSceneInfo;
		auto MaterialProxy = LightSceneInfo->Proxy->GetLightFunctionMaterial();
		if (MaterialProxy)
		{
			const FMaterialRenderProxy* FallbackMaterialRenderProxyPtr = nullptr;
			const FMaterial& Material = MaterialProxy->GetMaterialWithFallback(InFeatureLevel, FallbackMaterialRenderProxyPtr);
			if (Material.IsLightFunction())
			{
				const FMaterialShaderMap* MaterialShaderMap = Material.GetRenderingThreadShaderMap();
				// Getting the shader here has the side-effect of populating the raytracing miss shader library which is used when building the raytracing pipeline
				MaterialShaderMap->GetShader<FLightFunctionRayTracingShader>().GetRayTracingShader();

				int32 Index = Scene->RayTracingScene.NumMissShaderSlots;
				Scene->RayTracingScene.NumMissShaderSlots++;
				RayTracingLightFunctionMap.Add(LightSceneInfo, Index);
			}
		}
	}
	return RayTracingLightFunctionMap;
}

static void BindLightFunction(
	FRHICommandList& RHICmdList,
	const FScene* Scene,
	const FViewInfo& View,
	const FMaterial& Material,
	const FMaterialRenderProxy& MaterialRenderProxy,
	const TUniformBufferRef<FDeferredLightUniformStruct>& DeferredLightBuffer,
	const TUniformBufferRef<FLightFunctionParametersRayTracing>& LightFunctionParameters,
	int32 Index
	)
{
	FRHIRayTracingScene* RTScene = View.GetRayTracingSceneChecked();
	FRayTracingPipelineState* Pipeline = View.RayTracingMaterialPipeline;
	const FMaterialShaderMap* MaterialShaderMap = Material.GetRenderingThreadShaderMap();


	auto Shader = MaterialShaderMap->GetShader<FLightFunctionRayTracingShader>();

	TMeshProcessorShaders<
		FMaterialShader,
		FMaterialShader,
		FMaterialShader,
		FLightFunctionRayTracingShader,
		FMaterialShader> RayTracingShaders;

	RayTracingShaders.RayTracingShader = Shader;

	FMeshDrawShaderBindings ShaderBindings;
	ShaderBindings.Initialize(RayTracingShaders.GetUntypedShaders());

	int32 DataOffset = 0;
	FMeshDrawSingleShaderBindings SingleShaderBindings = ShaderBindings.GetSingleShaderBindings( SF_RayMiss, DataOffset);

	RayTracingShaders.RayTracingShader->GetShaderBindings(Scene, Scene->GetFeatureLevel(), MaterialRenderProxy, Material, View, DeferredLightBuffer, LightFunctionParameters, View.RayTracingLightDataUniformBuffer->GetRHIRef(), SingleShaderBindings);

	int32 MissShaderPipelineIndex = FindRayTracingMissShaderIndex(View.RayTracingMaterialPipeline, Shader.GetRayTracingShader(), true);

	ShaderBindings.SetRayTracingShaderBindingsForMissShader(RHICmdList, RTScene, Pipeline, MissShaderPipelineIndex, Index);
}

FRHIRayTracingShader* FDeferredShadingSceneRenderer::GetRayTracingLightingMissShader(const FViewInfo& View)
{
	return View.ShaderMap->GetShader<FRayTracingLightingMS>().GetRayTracingShader();
}

void BindLightFunctionShaders(
	FRHICommandList& RHICmdList,
	const FScene* Scene,
	const FRayTracingLightFunctionMap* RayTracingLightFunctionMap,
	const class FViewInfo& View)
{
	if (RayTracingLightFunctionMap == nullptr)
	{
		return;
	}

	for (const FRayTracingLightFunctionMap::ElementType& LightAndIndex : *RayTracingLightFunctionMap)
	{
		const FLightSceneInfo* LightSceneInfo = LightAndIndex.Key;

		const FMaterialRenderProxy* MaterialProxy = LightSceneInfo->Proxy->GetLightFunctionMaterial();
		check(MaterialProxy != nullptr);
		// Catch the fallback material case
		const FMaterialRenderProxy* FallbackMaterialRenderProxyPtr = nullptr;
		const FMaterial& Material = MaterialProxy->GetMaterialWithFallback(Scene->GetFeatureLevel(), FallbackMaterialRenderProxyPtr);

		check(Material.IsLightFunction());

		const FMaterialRenderProxy& MaterialRenderProxy = FallbackMaterialRenderProxyPtr ? *FallbackMaterialRenderProxyPtr : *MaterialProxy;

		//create the uniform buffers we need
		TUniformBufferRef<FDeferredLightUniformStruct> DeferredLightBuffer = CreateUniformBufferImmediate(GetDeferredLightParameters(View, *LightSceneInfo), EUniformBufferUsage::UniformBuffer_SingleFrame);
		TUniformBufferRef<FLightFunctionParametersRayTracing> LightFunctionParameters = CreateLightFunctionParametersBufferRT(LightSceneInfo, View, EUniformBufferUsage::UniformBuffer_SingleFrame);

		int32 MissIndex = LightAndIndex.Value;
		BindLightFunction(RHICmdList, Scene, View, Material, MaterialRenderProxy, DeferredLightBuffer, LightFunctionParameters, MissIndex);
	}
}

template< typename ShaderClass>
static int32 BindParameters(const TShaderRef<ShaderClass>& Shader, typename ShaderClass::FParameters & Parameters, int32 MaxParams, const FRHIUniformBuffer **OutUniformBuffers)
{
	FRayTracingShaderBindingsWriter ResourceBinder;

	auto &ParameterMap = Shader->ParameterMapInfo;

	// all parameters should be in uniform buffers
	check(ParameterMap.LooseParameterBuffers.Num() == 0);
	check(ParameterMap.SRVs.Num() == 0);
	check(ParameterMap.TextureSamplers.Num() == 0);

	SetShaderParameters(ResourceBinder, Shader, Parameters);

	FMemory::Memzero(OutUniformBuffers, sizeof(FRHIUniformBuffer *)*MaxParams);

	const int32 NumUniformBuffers = ParameterMap.UniformBuffers.Num();

	int32 MaxUniformBufferUsed = -1;
	for (int32 UniformBufferIndex = 0; UniformBufferIndex < NumUniformBuffers; UniformBufferIndex++)
	{
		const FShaderUniformBufferParameterInfo Parameter = ParameterMap.UniformBuffers[UniformBufferIndex];
		checkSlow(Parameter.BaseIndex < MaxParams);
		const FRHIUniformBuffer* UniformBuffer = ResourceBinder.UniformBuffers[UniformBufferIndex];
		if (Parameter.BaseIndex < MaxParams)
		{
			OutUniformBuffers[Parameter.BaseIndex] = UniformBuffer;
			MaxUniformBufferUsed = FMath::Max((int32)Parameter.BaseIndex, MaxUniformBufferUsed);
		}
	}

	return MaxUniformBufferUsed + 1;
}

void FDeferredShadingSceneRenderer::SetupRayTracingDefaultMissShader(FRHICommandListImmediate& RHICmdList, const FViewInfo& View)
{
	int32 MissShaderPipelineIndex = FindRayTracingMissShaderIndex(View.RayTracingMaterialPipeline, GetRayTracingDefaultMissShader(View), true);

	RHICmdList.SetRayTracingMissShader(View.GetRayTracingSceneChecked(),
		RAY_TRACING_MISS_SHADER_SLOT_DEFAULT,
		View.RayTracingMaterialPipeline,
		MissShaderPipelineIndex,
		0, nullptr, 0);
}

void FDeferredShadingSceneRenderer::SetupRayTracingLightingMissShader(FRHICommandListImmediate& RHICmdList, const FViewInfo& View)
{
	FRayTracingLightingMS::FParameters MissParameters;
	MissParameters.ViewUniformBuffer = View.ViewUniformBuffer;
	MissParameters.LightDataPacked = View.RayTracingLightDataUniformBuffer;

	static constexpr uint32 MaxUniformBuffers = UE_ARRAY_COUNT(FRayTracingShaderBindings::UniformBuffers);
	const FRHIUniformBuffer* MissData[MaxUniformBuffers] = {};
	auto MissShader = View.ShaderMap->GetShader<FRayTracingLightingMS>();

	int32 ParameterSlots = BindParameters(MissShader, MissParameters, MaxUniformBuffers, MissData);

	int32 MissShaderPipelineIndex = FindRayTracingMissShaderIndex(View.RayTracingMaterialPipeline, MissShader.GetRayTracingShader(), true);

	RHICmdList.SetRayTracingMissShader(View.GetRayTracingSceneChecked(),
		RAY_TRACING_MISS_SHADER_SLOT_LIGHTING,
		View.RayTracingMaterialPipeline,
		MissShaderPipelineIndex,
		ParameterSlots, (FRHIUniformBuffer**)MissData, 0);
}

#endif // RHI_RAYTRACING
