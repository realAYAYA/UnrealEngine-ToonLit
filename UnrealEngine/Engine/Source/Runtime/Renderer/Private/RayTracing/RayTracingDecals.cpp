// Copyright Epic Games, Inc. All Rights Reserved.

#include "RayTracingDecals.h"

#include "RHIDefinitions.h"

#if RHI_RAYTRACING

#include "RendererPrivate.h"
#include "RayTracingMaterialHitShaders.h"
#include "DecalRenderingShared.h"
#include "RayTracingTypes.h"
#include "RayTracingDefinitions.h"

TAutoConsoleVariable<int32> CVarDecalGridResolution(
	TEXT("r.RayTracing.DecalGrid.Resolution"),
	256,
	TEXT("Controls the resolution of the 2D decal grid used to cull irrelevant decal from calculations (default = 256)\n"),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarDecalGridMaxCount(
	TEXT("r.RayTracing.DecalGrid.MaxCount"),
	128,
	TEXT("Controls the maximum number of decals per cell in the 2D decal grid. The minimum of this value and the number of decal in the scene is used. (default = 128)\n"),
	ECVF_RenderThreadSafe
);

enum class EDecalsWriteFlags : uint32
{
	None						= 0,
	BaseColor					= (1 << 0),
	Normal						= (1 << 1),
	RoughnessSpecularMetallic	= (1 << 2),
	Emissive					= (1 << 3),
	AmbientOcclusion			= (1 << 4),
};

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FRayTracingDecals, "RayTracingDecals");

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FDecalParametersRayTracing, )
	SHADER_PARAMETER(FMatrix44f, WorldToDecal)
	SHADER_PARAMETER(FMatrix44f, DecalToWorld)
	SHADER_PARAMETER(FVector3f, DecalTilePosition)
	SHADER_PARAMETER(FVector3f, DecalToWorldInvScale)
	SHADER_PARAMETER(FVector3f, DecalOrientation)
	SHADER_PARAMETER(FVector2f, DecalParams)
	SHADER_PARAMETER(uint32, DecalWriteFlags)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FDecalParametersRayTracing, "RayTracingDecalParameters");

static TUniformBufferRef<FDecalParametersRayTracing> CreateDecalParametersBuffer(
	const FViewInfo& View,
	EShaderPlatform Platform,
	const FTransientDecalRenderData& DecalData,
	EUniformBufferUsage Usage)
{
	FDecalParametersRayTracing Parameters;

	const FLargeWorldRenderPosition AbsoluteOrigin(View.ViewMatrices.GetInvViewMatrix().GetOrigin());
	const FVector3f TilePosition = AbsoluteOrigin.GetTile();
	const FMatrix WorldToDecalMatrix = DecalData.Proxy.ComponentTrans.ToInverseMatrixWithScale();
	const FMatrix DecalToWorldMatrix = DecalData.Proxy.ComponentTrans.ToMatrixWithScale();
	const FMatrix44f RelativeDecalToWorldMatrix = FLargeWorldRenderScalar::MakeToRelativeWorldMatrix(AbsoluteOrigin.GetTileOffset(), DecalToWorldMatrix);
	const FVector3f OrientationVector = (FVector3f)DecalData.Proxy.ComponentTrans.GetUnitAxis(EAxis::X);

	Parameters.DecalTilePosition = TilePosition;
	Parameters.WorldToDecal = FMatrix44f(FTranslationMatrix(-View.ViewMatrices.GetPreViewTranslation()) * WorldToDecalMatrix);
	Parameters.DecalToWorld = RelativeDecalToWorldMatrix;
	Parameters.DecalToWorldInvScale = RelativeDecalToWorldMatrix.GetScaleVector().Reciprocal();
	Parameters.DecalOrientation = OrientationVector;

	float LifetimeAlpha = 1.0f;

	// Certain engine captures (e.g. environment reflection) don't have a tick. Default to fully opaque.
	if (View.Family->Time.GetWorldTimeSeconds())
	{
		LifetimeAlpha = FMath::Clamp(FMath::Min(View.Family->Time.GetWorldTimeSeconds() * -DecalData.Proxy.InvFadeDuration + DecalData.Proxy.FadeStartDelayNormalized, View.Family->Time.GetWorldTimeSeconds() * DecalData.Proxy.InvFadeInDuration + DecalData.Proxy.FadeInStartDelayNormalized), 0.0f, 1.0f);
	}

	Parameters.DecalParams = FVector2f(DecalData.FadeAlpha, LifetimeAlpha);

	Parameters.DecalWriteFlags = (uint32)EDecalsWriteFlags::None;
#define SET_FLAG(BOOL_VAL, WRITE_FLAG) Parameters.DecalWriteFlags |= uint32(DecalData.BlendDesc.BOOL_VAL ? EDecalsWriteFlags::WRITE_FLAG : EDecalsWriteFlags::None)
	SET_FLAG(bWriteBaseColor, BaseColor);
	SET_FLAG(bWriteNormal, Normal);
	SET_FLAG(bWriteRoughnessSpecularMetallic, RoughnessSpecularMetallic);
	SET_FLAG(bWriteEmissive, Emissive);
	SET_FLAG(bWriteAmbientOcclusion, AmbientOcclusion);
#undef SET_FLAG

	return CreateUniformBufferImmediate(Parameters, Usage);
}

class FRayTracingBuildDecalGridCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRayTracingBuildDecalGridCS)
	SHADER_USE_PARAMETER_STRUCT(FRayTracingBuildDecalGridCS, FGlobalShader)

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.CompilerFlags.Add(CFLAG_WarningsAsErrors);
		OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_X"), FComputeShaderUtils::kGolden2DGroupSize);
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_Y"), FComputeShaderUtils::kGolden2DGroupSize);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FRayTracingDecal>, SceneDecals)
		SHADER_PARAMETER(uint32, SceneDecalCount)
		SHADER_PARAMETER(FVector3f, SceneDecalsTranslatedBoundMin)
		SHADER_PARAMETER(FVector3f, SceneDecalsTranslatedBoundMax)
		SHADER_PARAMETER(unsigned, DecalGridResolution)
		SHADER_PARAMETER(unsigned, DecalGridMaxCount)
		SHADER_PARAMETER(int, DecalGridAxis)

		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, RWDecalGrid)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWDecalGridData)
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_SHADER_TYPE(, FRayTracingBuildDecalGridCS, TEXT("/Engine/Private/RayTracing/RayTracingBuildDecalGrid.usf"), TEXT("BuildDecalGridCS"), SF_Compute);

class FRayTracingDecalMaterialShader : public FMaterialShader
{
	DECLARE_SHADER_TYPE(FRayTracingDecalMaterialShader, Material);
public:
	FRayTracingDecalMaterialShader() = default;
	FRayTracingDecalMaterialShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMaterialShader(Initializer)
	{
		DecalParameter.Bind(Initializer.ParameterMap, TEXT("RaytracingDecalParameters"));
	}

	static bool ShouldCompilePermutation(const FMaterialShaderPermutationParameters& Parameters)
	{
		if (!ShouldCompileRayTracingCallableShadersForProject(Parameters.Platform))
		{
			// is raytracing enabled at all?
			return false;
		}
		if (Parameters.MaterialParameters.MaterialDomain != MD_DeferredDecal)
		{
			// only compile callable shader permutation of deferred decal material
			return false;
		}

		return true;
	}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

	static bool ValidateCompiledResult(EShaderPlatform Platform, const FShaderParameterMap& ParameterMap, TArray<FString>& OutError)
	{
		if (ParameterMap.ContainsParameterAllocation(FSceneTextureUniformParameters::StaticStructMetadata.GetShaderVariableName()))
		{
			OutError.Add(TEXT("Ray tracing callable shaders cannot read from the SceneTexturesStruct."));
			return false;
		}

		for (const auto& It : ParameterMap.GetParameterMap())
		{
			const FParameterAllocation& ParamAllocation = It.Value;
			if (ParamAllocation.Type != EShaderParameterType::UniformBuffer)
			{
				OutError.Add(FString::Printf(TEXT("Invalid ray tracing callable shader parameter '%s'. Only uniform buffers and loose data parameters are supported."), *(It.Key)));
				return false;
			}
		}

		return true;
	}

	void GetShaderBindings(
		const FScene* Scene,
		ERHIFeatureLevel::Type FeatureLevel,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material,
		const FViewInfo& View,
		const TUniformBufferRef<FDecalParametersRayTracing>& DecalParameters,
		FMeshDrawSingleShaderBindings& ShaderBindings) const
	{
		FMaterialShader::GetShaderBindings(Scene, FeatureLevel, MaterialRenderProxy, Material, ShaderBindings);

		ShaderBindings.Add(GetUniformBufferParameter<FViewUniformShaderParameters>(), View.ViewUniformBuffer);
		ShaderBindings.Add(DecalParameter, DecalParameters);
	}

private:
	LAYOUT_FIELD(FShaderUniformBufferParameter, DecalParameter);
};

IMPLEMENT_MATERIAL_SHADER_TYPE(, FRayTracingDecalMaterialShader, TEXT("/Engine/Private/RayTracing/RayTracingDecalMaterialShader.usf"), TEXT("RayTracingDecalMaterialShader"), SF_RayCallable);

void BuildDecalGrid(FRDGBuilder& GraphBuilder, uint32 NumDecals, FRDGBufferSRVRef DecalsSRV, FRayTracingDecals& OutParameters)
{
	// if we have some decals -- build a decal grid
	if (NumDecals > 0)
	{
		const uint32 Resolution = FMath::RoundUpToPowerOfTwo(CVarDecalGridResolution.GetValueOnRenderThread());
		const uint32 MaxCount = FMath::Clamp(CVarDecalGridMaxCount.GetValueOnRenderThread(), 1, FMath::Min((int32)NumDecals, RAY_TRACING_DECAL_COUNT_MAXIMUM));
		OutParameters.GridResolution = Resolution;
		OutParameters.GridMaxCount = MaxCount;

		// pick the shortest axis
		FVector3f Diag = OutParameters.TranslatedBoundMax - OutParameters.TranslatedBoundMin;
		if (Diag.X < Diag.Y && Diag.X < Diag.Z)
		{
			OutParameters.GridAxis = 0;
		}
		else if (Diag.Y < Diag.Z)
		{
			OutParameters.GridAxis = 1;
		}
		else
		{
			OutParameters.GridAxis = 2;
		}

		FRDGTextureDesc DecalGridDesc = FRDGTextureDesc::Create2D(
			FIntPoint(Resolution, Resolution),
			PF_R32_UINT,
			FClearValueBinding::None,
			TexCreate_ShaderResource | TexCreate_UAV);
		FRDGTexture* DecalGridTexture = GraphBuilder.CreateTexture(DecalGridDesc, TEXT("PathTracer.DecalGrid"), ERDGTextureFlags::None);

		EPixelFormat DecalGridDataFormat = PF_R32_UINT;
		size_t DecalGridDataNumBytes = sizeof(uint32);
		if (NumDecals <= (MAX_uint8 + 1))
		{
			DecalGridDataFormat = PF_R8_UINT;
			DecalGridDataNumBytes = sizeof(uint8);
		}
		else if (NumDecals <= (MAX_uint16 + 1))
		{
			DecalGridDataFormat = PF_R16_UINT;
			DecalGridDataNumBytes = sizeof(uint16);
		}
		FRDGBufferDesc DecalGridDataDesc = FRDGBufferDesc::CreateBufferDesc(DecalGridDataNumBytes, MaxCount * Resolution * Resolution);
		FRDGBuffer* DecalGridData = GraphBuilder.CreateBuffer(DecalGridDataDesc, TEXT("PathTracer.DecalGridData"));

		auto* PassParameters = GraphBuilder.AllocParameters<FRayTracingBuildDecalGridCS::FParameters>();
		PassParameters->RWDecalGrid = GraphBuilder.CreateUAV(DecalGridTexture);
		PassParameters->RWDecalGridData = GraphBuilder.CreateUAV(DecalGridData, DecalGridDataFormat);
		
		PassParameters->SceneDecals = DecalsSRV;
		PassParameters->SceneDecalCount = OutParameters.Count;
		PassParameters->SceneDecalsTranslatedBoundMin = OutParameters.TranslatedBoundMin;
		PassParameters->SceneDecalsTranslatedBoundMax = OutParameters.TranslatedBoundMax;
		PassParameters->DecalGridResolution = OutParameters.GridResolution;
		PassParameters->DecalGridMaxCount = OutParameters.GridMaxCount;
		PassParameters->DecalGridAxis = OutParameters.GridAxis;

		TShaderMapRef<FRayTracingBuildDecalGridCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("Decal Grid Create (%u decals)", NumDecals),
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(FIntPoint(Resolution, Resolution), FComputeShaderUtils::kGolden2DGroupSize));

		// hookup to the actual rendering pass
		OutParameters.Grid = DecalGridTexture;
		OutParameters.GridData = GraphBuilder.CreateSRV(DecalGridData, DecalGridDataFormat);
	}
	else
	{
		// light grid is not needed - just hookup dummy data

		FRDGBufferDesc DecalGridDataDesc = FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), 1);
		FRDGBuffer* DecalGridData = GraphBuilder.CreateBuffer(DecalGridDataDesc, TEXT("PathTracer.DecalGridData"));
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(DecalGridData, PF_R32_UINT), 0);

		OutParameters.GridResolution = 0;
		OutParameters.GridMaxCount = 0;
		OutParameters.GridAxis = 0;
		OutParameters.Grid = GSystemTextures.GetBlackDummy(GraphBuilder);
		OutParameters.GridData = GraphBuilder.CreateSRV(DecalGridData, PF_R32_UINT);
	}
}

FTransientDecalRenderDataList GetSortedDecals(const TSparseArray<FDeferredDecalProxy*>& Decals, FScene& Scene, const FViewInfo& View)
{
	const float FadeMultiplier = DecalRendering::GetDecalFadeScreenSizeMultiplier();
	const bool bIsPerspectiveProjection = View.IsPerspectiveProjection();

	FTransientDecalRenderDataList SortedDecals;
	SortedDecals.Reserve(Decals.Num());

	for (auto DecalProxy : Decals)
	{
		if (!DecalProxy->DecalMaterial || !DecalProxy->DecalMaterial->IsValidLowLevelFast())
		{
			continue;
		}

		bool bIsShown = true;

		if (!DecalProxy->IsShown(&View))
		{
			bIsShown = false;
		}

		if (bIsShown)
		{
			FTransientDecalRenderData Data(Scene, *DecalProxy, 0.0f);

			if (bIsPerspectiveProjection && Data.Proxy.FadeScreenSize != 0.0f)
			{
				const FMatrix ComponentToWorldMatrix = Data.Proxy.ComponentTrans.ToMatrixWithScale();
				Data.FadeAlpha = DecalRendering::CalculateDecalFadeAlpha(Data.Proxy.FadeScreenSize, ComponentToWorldMatrix, View, FadeMultiplier);
			}

			const bool bShouldRender = Data.FadeAlpha > 0.0f;

			if (bShouldRender)
			{
				SortedDecals.Add(Data);
			}
		}
	}

	DecalRendering::SortDecalList(SortedDecals);

	return SortedDecals;
}

TRDGUniformBufferRef<FRayTracingDecals> CreateNullRayTracingDecalsUniformBuffer(FRDGBuilder& GraphBuilder)
{
	auto* OutParameters = GraphBuilder.AllocParameters<FRayTracingDecals>();
	OutParameters->Count = 0;
	OutParameters->TranslatedBoundMin = FVector3f::ZeroVector;
	OutParameters->TranslatedBoundMax = FVector3f::ZeroVector;
	
	BuildDecalGrid(GraphBuilder, 0, nullptr, *OutParameters);
	
	return GraphBuilder.CreateUniformBuffer(OutParameters);
}

TRDGUniformBufferRef<FRayTracingDecals> CreateRayTracingDecalData(FRDGBuilder& GraphBuilder, FScene& Scene, const FViewInfo& View, uint32 BaseCallableSlotIndex)
{
	const EShaderPlatform ShaderPlatform = View.Family->GetShaderPlatform();

	FTransientDecalRenderDataList SortedDecals = GetSortedDecals(Scene.Decals, Scene, View);

	// Allocate from the graph builder so that we don't need to copy the data again when queuing the upload
	auto& RayTracingDecals = GraphBuilder.AllocArray<FRayTracingDecal>();
	RayTracingDecals.Reserve(SortedDecals.Num());

	const float Inf = std::numeric_limits<float>::infinity();
	FVector3f TranslatedBoundMin(+Inf, +Inf, +Inf);
	FVector3f TranslatedBoundMax(-Inf, -Inf, -Inf);

	for (int32 Index = 0; Index < SortedDecals.Num(); ++Index)
	{
		const auto& DecalData = SortedDecals[Index];

		const FMaterialRenderProxy* MaterialProxy = DecalData.MaterialProxy;
		const FMaterial* MaterialResource = &MaterialProxy->GetMaterialWithFallback(View.GetFeatureLevel(), MaterialProxy);
		const FMaterialShaderMap* MaterialShaderMap = MaterialResource->GetRenderingThreadShaderMap();

		auto CallableShader = MaterialShaderMap->GetShader<FRayTracingDecalMaterialShader>();

		if (!CallableShader.IsValid())
		{
			continue;
		}

		TUniformBufferRef<FDecalParametersRayTracing> DecalParameters = CreateDecalParametersBuffer(View, ShaderPlatform, DecalData, EUniformBufferUsage::UniformBuffer_SingleFrame);

		Scene.RayTracingScene.UniformBuffers.Add(DecalParameters); // Hold uniform buffer ref in RayTracingScene since FMeshDrawSingleShaderBindings doesn't

		FRayTracingShaderCommand& Command = Scene.RayTracingScene.CallableCommands.AddDefaulted_GetRef();
		Command.SetShader(CallableShader);
		Command.SlotInScene = BaseCallableSlotIndex + Index;

		int32 DataOffset = 0;
		FMeshDrawSingleShaderBindings SingleShaderBindings = Command.ShaderBindings.GetSingleShaderBindings(SF_RayCallable, DataOffset);
		CallableShader->GetShaderBindings(&Scene, Scene.GetFeatureLevel(), *MaterialProxy, *MaterialResource, View, DecalParameters, SingleShaderBindings);

		const FBox BoxBounds = DecalData.Proxy.GetBounds().GetBox();

		FRayTracingDecal& DestDecal = RayTracingDecals.AddDefaulted_GetRef();
		DestDecal.TranslatedBoundMin = FVector3f(BoxBounds.Min + View.ViewMatrices.GetPreViewTranslation());
		DestDecal.TranslatedBoundMax = FVector3f(BoxBounds.Max + View.ViewMatrices.GetPreViewTranslation());
		DestDecal.CallableSlotIndex = BaseCallableSlotIndex + Index;

		TranslatedBoundMin = FVector3f::Min(TranslatedBoundMin, DestDecal.TranslatedBoundMin);
		TranslatedBoundMax = FVector3f::Max(TranslatedBoundMax, DestDecal.TranslatedBoundMax);
	}

	// Upload the buffer of decals to the GPU
	const uint32 NumCopyDecals = FMath::Max(1u, (uint32)RayTracingDecals.Num()); // need at least one since zero-sized buffers are not allowed
	const size_t DataSize = sizeof(FRayTracingDecal) * RayTracingDecals.Num();
	FRDGBufferSRVRef DecalsSRV = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(CreateStructuredBuffer(GraphBuilder, TEXT("PathTracer.DecalsBuffer"), sizeof(FRayTracingDecal), NumCopyDecals, RayTracingDecals.GetData(), DataSize, ERDGInitialDataFlags::NoCopy)));

	auto* OutParameters = GraphBuilder.AllocParameters<FRayTracingDecals>();
	OutParameters->Count = RayTracingDecals.Num();
	OutParameters->TranslatedBoundMin = TranslatedBoundMin;
	OutParameters->TranslatedBoundMax = TranslatedBoundMax;

	BuildDecalGrid(GraphBuilder, RayTracingDecals.Num(), DecalsSRV, *OutParameters);
	
	return GraphBuilder.CreateUniformBuffer(OutParameters);
}

#endif // RHI_RAYTRACING
