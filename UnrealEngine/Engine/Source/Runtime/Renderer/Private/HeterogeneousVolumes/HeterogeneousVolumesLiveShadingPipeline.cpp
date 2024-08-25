// Copyright Epic Games, Inc. All Rights Reserved.

#include "HeterogeneousVolumes.h"
#include "HeterogeneousVolumeInterface.h"

#include "LightRendering.h"
#include "LocalVertexFactory.h"
#include "MeshPassUtils.h"
#include "PixelShaderUtils.h"
#include "RayTracingDefinitions.h"
#include "RayTracingInstance.h"
#include "RayTracingInstanceBufferUtil.h"
#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneManagement.h"
#include "VolumeLighting.h"
#include "VolumetricFog.h"
#include "BlueNoise.h"

static TAutoConsoleVariable<int32> CVarHeterogeneousLightingCacheBoundsCulling(
	TEXT("r.HeterogeneousVolumes.LightingCache.BoundsCulling"),
	1,
	TEXT("Enables bounds culling when populating the lighting cache (Default = 1)"),
	ECVF_RenderThreadSafe
);

namespace HeterogeneousVolumes
{
	bool ShouldBoundsCull()
	{
		return CVarHeterogeneousLightingCacheBoundsCulling.GetValueOnRenderThread() != 0;
	}
}

//-OPT: Remove duplicate bindings
// At the moment we need to bind the mesh draw parameters as they will be applied and on some RHIs this will crash if the texture is nullptr
// We have the same parameters in the loose FParameters shader structure that are applied after the mesh draw.
class FRenderLightingCacheLooseBindings
{
	DECLARE_TYPE_LAYOUT(FRenderLightingCacheLooseBindings, NonVirtual);

public:
	void Bind(const FShaderParameterMap& ParameterMap)
	{
		SceneDepthTextureBinding.Bind(ParameterMap, TEXT("SceneDepthTexture"));
		ShadowDepthTextureBinding.Bind(ParameterMap, TEXT("ShadowDepthTexture"));
		ShadowDepthTextureSamplerBinding.Bind(ParameterMap, TEXT("ShadowDepthTextureSampler"));
		StaticShadowDepthTextureBinding.Bind(ParameterMap, TEXT("StaticShadowDepthTexture"));
		StaticShadowDepthTextureSamplerBinding.Bind(ParameterMap, TEXT("StaticShadowDepthTextureSampler"));
		ShadowDepthCubeTextureBinding.Bind(ParameterMap, TEXT("ShadowDepthCubeTexture"));
		ShadowDepthCubeTexture2Binding.Bind(ParameterMap, TEXT("ShadowDepthCubeTexture2"));
		ShadowDepthCubeTextureSamplerBinding.Bind(ParameterMap, TEXT("ShadowDepthCubeTextureSampler"));
		LightingCacheTextureBinding.Bind(ParameterMap, TEXT("LightingCacheTexture"));
	}

	template<typename TPassParameters>	
	void SetParameters(FMeshDrawSingleShaderBindings& ShaderBindings, const TPassParameters* PassParameters)
	{
		ShaderBindings.AddTexture(
			SceneDepthTextureBinding,
			FShaderResourceParameter(),
			TStaticSamplerState<SF_Point>::GetRHI(),
			PassParameters->SceneTextures.SceneDepthTexture->GetRHI()
		);
		ShaderBindings.AddTexture(
			ShadowDepthTextureBinding,
			ShadowDepthTextureSamplerBinding,
			PassParameters->VolumeShadowingShaderParameters.ShadowDepthTextureSampler,
			PassParameters->VolumeShadowingShaderParameters.ShadowDepthTexture->GetRHI()
		);
		ShaderBindings.AddTexture(
			StaticShadowDepthTextureBinding,
			StaticShadowDepthTextureSamplerBinding,
			PassParameters->VolumeShadowingShaderParameters.StaticShadowDepthTextureSampler,
			PassParameters->VolumeShadowingShaderParameters.StaticShadowDepthTexture
		);
		ShaderBindings.AddTexture(
			ShadowDepthCubeTextureBinding,
			ShadowDepthCubeTextureSamplerBinding,
			PassParameters->VolumeShadowingShaderParameters.OnePassPointShadowProjection.ShadowDepthCubeTextureSampler,
			PassParameters->VolumeShadowingShaderParameters.OnePassPointShadowProjection.ShadowDepthCubeTexture->GetRHI()
		);
		ShaderBindings.AddTexture(
			ShadowDepthCubeTexture2Binding,
			ShadowDepthCubeTextureSamplerBinding,
			PassParameters->VolumeShadowingShaderParameters.OnePassPointShadowProjection.ShadowDepthCubeTextureSampler,
			PassParameters->VolumeShadowingShaderParameters.OnePassPointShadowProjection.ShadowDepthCubeTexture->GetRHI()
		);
		ShaderBindings.AddTexture(
			LightingCacheTextureBinding,
			FShaderResourceParameter(),
			TStaticSamplerState<SF_Point>::GetRHI(),
			PassParameters->LightingCache.LightingCacheTexture->GetRHI()
		);
	}

	LAYOUT_FIELD(FShaderResourceParameter, SceneDepthTextureBinding);
	LAYOUT_FIELD(FShaderResourceParameter, ShadowDepthTextureBinding);
	LAYOUT_FIELD(FShaderResourceParameter, ShadowDepthTextureSamplerBinding);
	LAYOUT_FIELD(FShaderResourceParameter, StaticShadowDepthTextureBinding);
	LAYOUT_FIELD(FShaderResourceParameter, StaticShadowDepthTextureSamplerBinding);
	LAYOUT_FIELD(FShaderResourceParameter, ShadowDepthCubeTextureBinding);
	LAYOUT_FIELD(FShaderResourceParameter, ShadowDepthCubeTexture2Binding);
	LAYOUT_FIELD(FShaderResourceParameter, ShadowDepthCubeTextureSamplerBinding);
	LAYOUT_FIELD(FShaderResourceParameter, LightingCacheTextureBinding);
};
IMPLEMENT_TYPE_LAYOUT(FRenderLightingCacheLooseBindings);

class FRenderLightingCacheWithLiveShadingCS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FRenderLightingCacheWithLiveShadingCS, MeshMaterial);

	class FLightingCacheMode : SHADER_PERMUTATION_INT("DIM_LIGHTING_CACHE_MODE", 2);
	class FUseAdaptiveVolumetricShadowMap : SHADER_PERMUTATION_BOOL("DIM_USE_ADAPTIVE_VOLUMETRIC_SHADOW_MAP");
	using FPermutationDomain = TShaderPermutationDomain<FLightingCacheMode, FUseAdaptiveVolumetricShadowMap>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		// Scene data
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureParameters, SceneTextures)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneUniformParameters, Scene)

		// Light data
		SHADER_PARAMETER(int, bApplyEmissionAndTransmittance)
		SHADER_PARAMETER(int, bApplyDirectLighting)
		SHADER_PARAMETER(int, bApplyShadowTransmittance)
		SHADER_PARAMETER(int, LightType)
		SHADER_PARAMETER_STRUCT_REF(FDeferredLightUniformStruct, DeferredLight)
		SHADER_PARAMETER(float, VolumetricScatteringIntensity)

		// Shadow data
		SHADER_PARAMETER(float, ShadowStepSize)
		SHADER_PARAMETER(float, ShadowStepFactor)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FForwardLightData, ForwardLightData)
		SHADER_PARAMETER_STRUCT_INCLUDE(FVolumeShadowingShaderParameters, VolumeShadowingShaderParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FVirtualShadowMapSamplingParameters, VirtualShadowMapSamplingParameters)
		SHADER_PARAMETER(int32, VirtualShadowMapId)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FAdaptiveVolumetricShadowMapUniformBufferParameters, AVSM)

		// Object data
		SHADER_PARAMETER(FMatrix44f, LocalToWorld)
		SHADER_PARAMETER(FMatrix44f, WorldToLocal)
		SHADER_PARAMETER(FVector3f, LocalBoundsOrigin)
		SHADER_PARAMETER(FVector3f, LocalBoundsExtent)
		SHADER_PARAMETER(int32, PrimitiveId)

		// Ray data
		SHADER_PARAMETER(float, MaxTraceDistance)
		SHADER_PARAMETER(float, MaxShadowTraceDistance)
		SHADER_PARAMETER(float, StepSize)
		SHADER_PARAMETER(float, StepFactor)
		SHADER_PARAMETER(int, MaxStepCount)
		SHADER_PARAMETER(int, bJitter)

		// Volume data
		SHADER_PARAMETER(FIntVector, VoxelResolution)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLightingCacheParameters, LightingCache)
		SHADER_PARAMETER(FIntVector, VoxelMin)
		SHADER_PARAMETER(FIntVector, VoxelMax)

		// Output
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float>, RWLightingCacheTexture)
	END_SHADER_PARAMETER_STRUCT()

	FRenderLightingCacheWithLiveShadingCS() = default;

	FRenderLightingCacheWithLiveShadingCS(
		const FMeshMaterialShaderType::CompiledShaderInitializerType & Initializer
	)
		: FMeshMaterialShader(Initializer)
	{
		Bindings.BindForLegacyShaderParameters(
			this,
			Initializer.PermutationId,
			Initializer.ParameterMap,
			*FParameters::FTypeInfo::GetStructMetadata(),
			// Don't require full bindings, we use FMaterialShader::SetParameters
			false
		);
		ShaderLooseBindings.Bind(Initializer.ParameterMap);
	}

	static bool ShouldCompilePermutation(
		const FMaterialShaderPermutationParameters & Parameters
	)
	{
		return DoesPlatformSupportHeterogeneousVolumes(Parameters.Platform)
			&& DoesMaterialShaderSupportHeterogeneousVolumes(Parameters.MaterialParameters);
	}

	static FPermutationDomain RemapPermutation(FPermutationDomain PermutationVector)
	{
		return PermutationVector;
	}

	static void ModifyCompilationEnvironment(
		const FMaterialShaderPermutationParameters & Parameters,
		FShaderCompilerEnvironment & OutEnvironment
	)
	{
		FMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_1D"), GetThreadGroupSize1D());
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_2D"), GetThreadGroupSize2D());
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_3D"), GetThreadGroupSize3D());

		bool bSupportVirtualShadowMap = IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
		if (bSupportVirtualShadowMap)
		{
			OutEnvironment.SetDefine(TEXT("VIRTUAL_SHADOW_MAP"), 1);
			FVirtualShadowMapArray::SetShaderDefines(OutEnvironment);
		}

		// This shader takes a very long time to compile with FXC, so we pre-compile it with DXC first and then forward the optimized HLSL to FXC.
		//OutEnvironment.CompilerFlags.Add(CFLAG_PrecompileWithDXC); // @lh-todo - Disabled to workaround SPIRV-Cross bug: StructuredBuffer<uint> is translated to ByteAddressBuffer in HLSL backend
		OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);

		OutEnvironment.SetDefine(TEXT("GET_PRIMITIVE_DATA_OVERRIDE"), 1);
	}

	static int32 GetThreadGroupSize1D() { return GetThreadGroupSize2D() * GetThreadGroupSize2D(); }
	static int32 GetThreadGroupSize2D() { return 8; }
	static int32 GetThreadGroupSize3D() { return 4; }

	LAYOUT_FIELD(FRenderLightingCacheLooseBindings, ShaderLooseBindings);
};

IMPLEMENT_MATERIAL_SHADER_TYPE(, FRenderLightingCacheWithLiveShadingCS, TEXT("/Engine/Private/HeterogeneousVolumes/HeterogeneousVolumesLiveShadingPipeline.usf"), TEXT("RenderLightingCacheWithLiveShadingCS"), SF_Compute);

class FRenderSingleScatteringWithLiveShadingCS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FRenderSingleScatteringWithLiveShadingCS, MeshMaterial);

	class FUseTransmittanceVolume : SHADER_PERMUTATION_BOOL("DIM_USE_TRANSMITTANCE_VOLUME");
	class FUseInscatteringVolume : SHADER_PERMUTATION_BOOL("DIM_USE_INSCATTERING_VOLUME");
	class FUseLumenGI : SHADER_PERMUTATION_BOOL("DIM_USE_LUMEN_GI");
	class FWriteVelocity : SHADER_PERMUTATION_BOOL("DIM_WRITE_VELOCITY");
	class FUseAdaptiveVolumetricShadowMap : SHADER_PERMUTATION_BOOL("DIM_USE_ADAPTIVE_VOLUMETRIC_SHADOW_MAP");
	class FApplyFogInscattering : SHADER_PERMUTATION_INT("APPLY_FOG_INSCATTERING", 3);
	using FPermutationDomain = TShaderPermutationDomain<FUseTransmittanceVolume, FUseInscatteringVolume, FUseLumenGI, FWriteVelocity, FUseAdaptiveVolumetricShadowMap, FApplyFogInscattering>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		// Scene data
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureParameters, SceneTextures)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneUniformParameters, Scene)
		SHADER_PARAMETER_STRUCT_REF(FBlueNoise, BlueNoise)

		// Light data
		SHADER_PARAMETER(int, bApplyEmissionAndTransmittance)
		SHADER_PARAMETER(int, bApplyDirectLighting)
		SHADER_PARAMETER(int, bApplyShadowTransmittance)
		SHADER_PARAMETER(int, LightType)
		SHADER_PARAMETER_STRUCT_REF(FDeferredLightUniformStruct, DeferredLight)
		SHADER_PARAMETER(float, VolumetricScatteringIntensity)

		// Shadow data
		SHADER_PARAMETER(float, ShadowStepSize)
		SHADER_PARAMETER(float, ShadowStepFactor)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FForwardLightData, ForwardLightData)
		SHADER_PARAMETER_STRUCT_INCLUDE(FVolumeShadowingShaderParameters, VolumeShadowingShaderParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FVirtualShadowMapSamplingParameters, VirtualShadowMapSamplingParameters)
		SHADER_PARAMETER(int32, VirtualShadowMapId)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FAdaptiveVolumetricShadowMapUniformBufferParameters, AVSM)

		// Atmosphere
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FFogUniformParameters, FogStruct)
		SHADER_PARAMETER(int, bApplyHeightFog)
		SHADER_PARAMETER(int, bApplyVolumetricFog)

		// Indirect Lighting
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FLumenTranslucencyLightingUniforms, LumenGIVolumeStruct)

		// Object data
		SHADER_PARAMETER(FMatrix44f, LocalToWorld)
		SHADER_PARAMETER(FMatrix44f, WorldToLocal)
		SHADER_PARAMETER(FVector3f, LocalBoundsOrigin)
		SHADER_PARAMETER(FVector3f, LocalBoundsExtent)
		SHADER_PARAMETER(int32, PrimitiveId)

		// Volume data
		SHADER_PARAMETER(FIntVector, VoxelResolution)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLightingCacheParameters, LightingCache)

		// Ray data
		SHADER_PARAMETER(float, MaxTraceDistance)
		SHADER_PARAMETER(float, StepSize)
		SHADER_PARAMETER(float, StepFactor)
		SHADER_PARAMETER(int, MaxStepCount)
		SHADER_PARAMETER(int, bJitter)

		// Dispatch data
		SHADER_PARAMETER(FIntVector, GroupCount)

		// Output
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RWLightingTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RWVelocityTexture)
		//SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<Volumes::FDebugOutput>, RWDebugOutputBuffer)
	END_SHADER_PARAMETER_STRUCT()

	FRenderSingleScatteringWithLiveShadingCS() = default;

	FRenderSingleScatteringWithLiveShadingCS(
		const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer
	)
		: FMeshMaterialShader(Initializer)
	{
		Bindings.BindForLegacyShaderParameters(
			this,
			Initializer.PermutationId,
			Initializer.ParameterMap,
			*FParameters::FTypeInfo::GetStructMetadata(),
			// Don't require full bindings, we use FMaterialShader::SetParameters
			false
		);
		ShaderLooseBindings.Bind(Initializer.ParameterMap);
	}

	static bool ShouldCompilePermutation(
		const FMaterialShaderPermutationParameters& Parameters
	)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		if (PermutationVector.Get<FApplyFogInscattering>() == 0)
		{
			return false;
		}

		return DoesPlatformSupportHeterogeneousVolumes(Parameters.Platform)
			&& DoesMaterialShaderSupportHeterogeneousVolumes(Parameters.MaterialParameters);
	}

	static FPermutationDomain RemapPermutation(FPermutationDomain PermutationVector)
	{
		// Remap Off to Stochastic and turn off individual fog modes
		if (PermutationVector.Get<FApplyFogInscattering>() == 0)
		{
			PermutationVector.Set<FApplyFogInscattering>(2);
		}

		return PermutationVector;
	}

	static void ModifyCompilationEnvironment(
		const FMaterialShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment
	)
	{
		FMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_1D"), GetThreadGroupSize1D());
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_2D"), GetThreadGroupSize2D());
		OutEnvironment.SetDefine(TEXT("FOG_MATERIALBLENDING_OVERRIDE"), 1);

		bool bSupportVirtualShadowMap = IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
		if (bSupportVirtualShadowMap)
		{
			OutEnvironment.SetDefine(TEXT("VIRTUAL_SHADOW_MAP"), 1);
			FVirtualShadowMapArray::SetShaderDefines(OutEnvironment);
		}

		// This shader takes a very long time to compile with FXC, so we pre-compile it with DXC first and then forward the optimized HLSL to FXC.
		OutEnvironment.CompilerFlags.Add(CFLAG_PrecompileWithDXC);
		OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);

		OutEnvironment.SetDefine(TEXT("GET_PRIMITIVE_DATA_OVERRIDE"), 1);
	}

	static int32 GetThreadGroupSize1D() { return GetThreadGroupSize2D() * GetThreadGroupSize2D(); }
	static int32 GetThreadGroupSize2D() { return 8; }

	LAYOUT_FIELD(FRenderLightingCacheLooseBindings, ShaderLooseBindings);
};

IMPLEMENT_MATERIAL_SHADER_TYPE(, FRenderSingleScatteringWithLiveShadingCS, TEXT("/Engine/Private/HeterogeneousVolumes/HeterogeneousVolumesLiveShadingPipeline.usf"), TEXT("RenderSingleScatteringWithLiveShadingCS"), SF_Compute);

template<bool bWithLumen, typename ComputeShaderType>
void AddComputePass(
	FRDGBuilder& GraphBuilder,
	TShaderRef<ComputeShaderType>& ComputeShader,
	typename ComputeShaderType::FParameters* PassParameters,
	const FScene* Scene,
	const FMaterialRenderProxy* MaterialRenderProxy,
	const FMaterial& Material,
	const FString& PassName,
	FIntVector GroupCount
)
{
	//ClearUnusedGraphResources(ComputeShader, PassParameters);

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("%s", *PassName),
		PassParameters,
		ERDGPassFlags::Compute,
		[ComputeShader, PassParameters, Scene, MaterialRenderProxy, &Material, GroupCount](FRHIComputeCommandList& RHICmdList)
		{
			FMeshMaterialShaderElementData ShaderElementData;
			ShaderElementData.InitializeMeshMaterialData();

			FMeshProcessorShaders PassShaders;
			PassShaders.ComputeShader = ComputeShader;

			FMeshDrawShaderBindings ShaderBindings;
			ShaderBindings.Initialize(PassShaders);
			{
				FMeshDrawSingleShaderBindings SingleShaderBindings = ShaderBindings.GetSingleShaderBindings(SF_Compute);
				ComputeShader->GetShaderBindings(Scene, Scene->GetFeatureLevel(), nullptr, *MaterialRenderProxy, Material, ShaderElementData, SingleShaderBindings);
				SingleShaderBindings.Add(ComputeShader->template GetUniformBufferParameter<FDeferredLightUniformStruct>(), PassParameters->DeferredLight.GetUniformBuffer());
				SingleShaderBindings.Add(ComputeShader->template GetUniformBufferParameter<FForwardLightData>(), PassParameters->ForwardLightData.GetUniformBuffer()->GetRHIRef());
				SingleShaderBindings.Add(ComputeShader->template GetUniformBufferParameter<FVirtualShadowMapUniformParameters>(), PassParameters->VirtualShadowMapSamplingParameters.VirtualShadowMap.GetUniformBuffer()->GetRHIRef());
				if constexpr (bWithLumen)
				{
					SingleShaderBindings.Add(ComputeShader->template GetUniformBufferParameter<FLumenTranslucencyLightingUniforms>(), PassParameters->LumenGIVolumeStruct.GetUniformBuffer()->GetRHIRef());
				}
				ComputeShader->ShaderLooseBindings.SetParameters(SingleShaderBindings, PassParameters);
				ShaderBindings.Finalize(&PassShaders);
			}

			UE::MeshPassUtils::Dispatch(RHICmdList, ComputeShader, ShaderBindings, *PassParameters, GroupCount);
		}
	);
}

static void RenderLightingCacheWithLiveShading(
	FRDGBuilder& GraphBuilder,
	// Scene data
	const FScene* Scene,
	const FViewInfo& View,
	const FSceneTextures& SceneTextures,
	// Light data
	bool bApplyEmissionAndTransmittance,
	bool bApplyDirectLighting,
	bool bApplyShadowTransmittance,
	uint32 LightType,
	const FLightSceneInfo* LightSceneInfo,
	// Shadow data
	const FVisibleLightInfo* VisibleLightInfo,
	const FVirtualShadowMapArray& VirtualShadowMapArray,
	// Object data
	const IHeterogeneousVolumeInterface* HeterogeneousVolumeInterface,
	const FMaterialRenderProxy* DefaultMaterialRenderProxy,
	FPersistentPrimitiveIndex PersistentPrimitiveIndex,
	const FBoxSphereBounds LocalBoxSphereBounds,
	// Output
	FRDGTextureRef LightingCacheTexture
)
{
	const FMaterialRenderProxy* MaterialRenderProxy = nullptr;
	const FMaterial& Material = DefaultMaterialRenderProxy->GetMaterialWithFallback(View.GetFeatureLevel(), MaterialRenderProxy);
	MaterialRenderProxy = MaterialRenderProxy ? MaterialRenderProxy : DefaultMaterialRenderProxy;

	check(Material.GetMaterialDomain() == MD_Volume);

	// Note must be done in the same scope as we add the pass otherwise the UB lifetime will not be guaranteed
	FDeferredLightUniformStruct DeferredLightUniform = GetDeferredLightParameters(View, *LightSceneInfo);
	TUniformBufferRef<FDeferredLightUniformStruct> DeferredLightUB = CreateUniformBufferImmediate(DeferredLightUniform, UniformBuffer_SingleDraw);
	float LODFactor = HeterogeneousVolumes::CalcLODFactor(View, HeterogeneousVolumeInterface);

	FRenderLightingCacheWithLiveShadingCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FRenderLightingCacheWithLiveShadingCS::FParameters>();
	{
		// Scene data
		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->SceneTextures = GetSceneTextureParameters(GraphBuilder, SceneTextures);
		PassParameters->Scene = View.GetSceneUniforms().GetBuffer(GraphBuilder);

		// Light data
		check(LightSceneInfo != nullptr)
		PassParameters->bApplyEmissionAndTransmittance = bApplyEmissionAndTransmittance;
		PassParameters->bApplyDirectLighting = bApplyDirectLighting;
		PassParameters->bApplyShadowTransmittance = bApplyShadowTransmittance;
		PassParameters->DeferredLight = DeferredLightUB;
		PassParameters->LightType = LightType;
		PassParameters->VolumetricScatteringIntensity = LightSceneInfo->Proxy->GetVolumetricScatteringIntensity();

		// Object data
		// TODO: Convert to relative-local space
		//FVector3f ViewOriginHigh = FDFVector3(View.ViewMatrices.GetViewOrigin()).High;
		//FMatrix44f RelativeLocalToWorld = FDFMatrix::MakeToRelativeWorldMatrix(ViewOriginHigh, HeterogeneousVolumeInterface->GetLocalToWorld()).M;
		FMatrix InstanceToLocal = HeterogeneousVolumeInterface->GetInstanceToLocal();
		FMatrix LocalToWorld = HeterogeneousVolumeInterface->GetLocalToWorld();
		PassParameters->LocalToWorld = FMatrix44f(InstanceToLocal * LocalToWorld);
		PassParameters->WorldToLocal = PassParameters->LocalToWorld.Inverse();

		FMatrix LocalToInstance = InstanceToLocal.Inverse();
		FBoxSphereBounds InstanceBoxSphereBounds = LocalBoxSphereBounds.TransformBy(LocalToInstance);
		PassParameters->LocalBoundsOrigin = FVector3f(InstanceBoxSphereBounds.Origin);
		PassParameters->LocalBoundsExtent = FVector3f(InstanceBoxSphereBounds.BoxExtent);
		PassParameters->PrimitiveId = PersistentPrimitiveIndex.Index;

		// Transmittance volume
		PassParameters->VoxelResolution = HeterogeneousVolumeInterface->GetVoxelResolution();
		PassParameters->LightingCache.LightingCacheResolution = HeterogeneousVolumes::GetLightingCacheResolution(HeterogeneousVolumeInterface, LODFactor);
		PassParameters->LightingCache.LightingCacheVoxelBias = HeterogeneousVolumeInterface->GetShadowBiasFactor();
		//PassParameters->LightingCache.LightingCacheTexture = GraphBuilder.CreateSRV(LightingCacheTexture);
		PassParameters->LightingCache.LightingCacheTexture = FRDGSystemTextures::Get(GraphBuilder).VolumetricBlack;

		// Ray data
		PassParameters->MaxTraceDistance = HeterogeneousVolumes::GetMaxTraceDistance();
		PassParameters->MaxShadowTraceDistance = HeterogeneousVolumes::GetMaxShadowTraceDistance();
		PassParameters->StepSize = HeterogeneousVolumes::GetStepSize();
		PassParameters->StepFactor = HeterogeneousVolumeInterface->GetStepFactor() * LODFactor;
		PassParameters->ShadowStepSize = HeterogeneousVolumes::GetShadowStepSize();
		PassParameters->ShadowStepFactor = HeterogeneousVolumeInterface->GetShadowStepFactor() * LODFactor;
		PassParameters->MaxStepCount = HeterogeneousVolumes::GetMaxStepCount();
		PassParameters->bJitter = HeterogeneousVolumes::ShouldJitter();

		// Shadow data
		PassParameters->ForwardLightData = View.ForwardLightingResources.ForwardLightUniformBuffer;
		if (VisibleLightInfo != nullptr)
		{
			const FProjectedShadowInfo* ProjectedShadowInfo = GetShadowForInjectionIntoVolumetricFog(*VisibleLightInfo);
			bool bDynamicallyShadowed = ProjectedShadowInfo != NULL;
			if (bDynamicallyShadowed)
			{
				GetVolumeShadowingShaderParameters(GraphBuilder, View, LightSceneInfo, ProjectedShadowInfo, PassParameters->VolumeShadowingShaderParameters);
			}
			else
			{
				SetVolumeShadowingDefaultShaderParametersGlobal(GraphBuilder, PassParameters->VolumeShadowingShaderParameters);
			}
			PassParameters->VirtualShadowMapId = VisibleLightInfo->GetVirtualShadowMapId(&View);
		}
		else
		{
			SetVolumeShadowingDefaultShaderParametersGlobal(GraphBuilder, PassParameters->VolumeShadowingShaderParameters);
			PassParameters->VirtualShadowMapId = -1;
		}
		PassParameters->VirtualShadowMapSamplingParameters = VirtualShadowMapArray.GetSamplingParameters(GraphBuilder);
		PassParameters->AVSM = HeterogeneousVolumes::GetAdaptiveVolumetricShadowMapUniformBuffer(GraphBuilder, View.ViewState, LightSceneInfo);

		// Output
		PassParameters->RWLightingCacheTexture = GraphBuilder.CreateUAV(LightingCacheTexture);
	}

	FString PassName;
#if WANTS_DRAW_MESH_EVENTS
	if (GetEmitDrawEvents())
	{
		FString LightName = TEXT("none");
		if (LightSceneInfo != nullptr)
		{
			FSceneRenderer::GetLightNameForDrawEvent(LightSceneInfo->Proxy, LightName);
		}
		FString ModeName = HeterogeneousVolumes::UseLightingCacheForInscattering() ? TEXT("In-Scattering") : TEXT("Transmittance");
		PassName = FString::Printf(TEXT("RenderLightingCacheWithLiveShadingCS [%s] (Light = %s)"), *ModeName, *LightName);
	}
#endif // WANTS_DRAW_MESH_EVENTS

	PassParameters->VoxelMin = FIntVector::ZeroValue;
	PassParameters->VoxelMax = HeterogeneousVolumes::GetLightingCacheResolution(HeterogeneousVolumeInterface, LODFactor) - FIntVector(1);
	
	bool bShouldBoundsCull = HeterogeneousVolumes::ShouldBoundsCull();
	if (LightType != LightType_Directional && bShouldBoundsCull)
	{
		auto ToFVector3f = [](FVector4 V4f) {
			return FVector3f(V4f.X, V4f.Y, V4f.Z);
		};

		auto FloorVector = [](const FVector& V) {
			return FVector(
				FMath::FloorToFloat(V.X),
				FMath::FloorToFloat(V.Y),
				FMath::FloorToFloat(V.Z)
			);
		};

		auto CeilVector = [](const FVector& V) {
			return FVector(
				FMath::CeilToFloat(V.X),
				FMath::CeilToFloat(V.Y),
				FMath::CeilToFloat(V.Z)
			);
		};

		auto ClampVector = [](const FVector& V, const FIntVector& Min, const FIntVector& Max) {
			FIntVector IntV;
			IntV.X = FMath::Clamp(V.X, Min.X, Max.X);
			IntV.Y = FMath::Clamp(V.Y, Min.Y, Max.Y);
			IntV.Z = FMath::Clamp(V.Z, Min.Z, Max.Z);
			return IntV;
		};

		FSphere WorldLightBoundingSphere = LightSceneInfo->Proxy->GetBoundingSphere();
		FVector LocalLightCenter = FVector(PassParameters->WorldToLocal.TransformPosition(ToFVector3f(WorldLightBoundingSphere.Center)));
		FVector3f ScalingTerm = PassParameters->WorldToLocal.GetScaleVector();
		FVector LocalLightExtent = FVector(ScalingTerm) * WorldLightBoundingSphere.W;
		FVector LocalLightMin = LocalLightCenter - LocalLightExtent;
		FVector LocalLightMax = LocalLightCenter + LocalLightExtent;

		FVector LightingCacheMin = LocalBoxSphereBounds.Origin - LocalBoxSphereBounds.BoxExtent;
		FVector LightingCacheMax = LocalBoxSphereBounds.Origin + LocalBoxSphereBounds.BoxExtent;

		FVector LocalLightMinUV = (LocalLightMin - LightingCacheMin) / (LightingCacheMax - LightingCacheMin);
		FVector LocalLightMaxUV = (LocalLightMax - LightingCacheMin) / (LightingCacheMax - LightingCacheMin);
		FVector LightingCacheResolution = FVector(PassParameters->LightingCache.LightingCacheResolution);
		PassParameters->VoxelMin = ClampVector(FloorVector(LocalLightMinUV * LightingCacheResolution), FIntVector::ZeroValue, PassParameters->VoxelMax);
		PassParameters->VoxelMax = ClampVector(CeilVector(LocalLightMaxUV * LightingCacheResolution), FIntVector::ZeroValue, PassParameters->VoxelMax);
	}

	FIntVector GroupCount = PassParameters->VoxelMax - PassParameters->VoxelMin + FIntVector(1);
	check(GroupCount.X > 0 && GroupCount.Y > 0 && GroupCount.Z > 0);
	GroupCount.X = FMath::DivideAndRoundUp(GroupCount.X, FRenderLightingCacheWithLiveShadingCS::GetThreadGroupSize3D());
	GroupCount.Y = FMath::DivideAndRoundUp(GroupCount.Y, FRenderLightingCacheWithLiveShadingCS::GetThreadGroupSize3D());
	GroupCount.Z = FMath::DivideAndRoundUp(GroupCount.Z, FRenderLightingCacheWithLiveShadingCS::GetThreadGroupSize3D());

	bool bUseAVSM = HeterogeneousVolumes::UseAdaptiveVolumetricShadowMapForSelfShadowing(HeterogeneousVolumeInterface->GetPrimitiveSceneProxy());

	FRenderLightingCacheWithLiveShadingCS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FRenderLightingCacheWithLiveShadingCS::FLightingCacheMode>(HeterogeneousVolumes::GetLightingCacheMode() - 1);
	PermutationVector.Set<FRenderLightingCacheWithLiveShadingCS::FUseAdaptiveVolumetricShadowMap>(bUseAVSM);
	TShaderRef<FRenderLightingCacheWithLiveShadingCS> ComputeShader = Material.GetShader<FRenderLightingCacheWithLiveShadingCS>(&FLocalVertexFactory::StaticType, PermutationVector, false);
	if (!ComputeShader.IsNull())
	{
		AddComputePass<false>(GraphBuilder, ComputeShader, PassParameters, Scene, MaterialRenderProxy, Material, PassName, GroupCount);
	}
}

static void RenderSingleScatteringWithLiveShading(
	FRDGBuilder& GraphBuilder,
	// Scene data
	const FScene* Scene,
	const FViewInfo& View,
	const FSceneTextures& SceneTextures,
	// Light data
	bool bApplyEmissionAndTransmittance,
	bool bApplyDirectLighting,
	bool bApplyShadowTransmittance,
	uint32 LightType,
	const FLightSceneInfo* LightSceneInfo,
	// Shadow data
	const FVisibleLightInfo* VisibleLightInfo,
	const FVirtualShadowMapArray& VirtualShadowMapArray,
	// Object data
	const IHeterogeneousVolumeInterface* HeterogeneousVolumeInterface,
	const FMaterialRenderProxy* DefaultMaterialRenderProxy,
	FPersistentPrimitiveIndex PersistentPrimitiveIndex,
	const FBoxSphereBounds LocalBoxSphereBounds,
	// Transmittance acceleration
	FRDGTextureRef LightingCacheTexture,
	// Output
	FRDGTextureRef& HeterogeneousVolumeTexture
)
{
	const FMaterialRenderProxy* MaterialRenderProxy = nullptr;
	const FMaterial& Material = DefaultMaterialRenderProxy->GetMaterialWithFallback(View.GetFeatureLevel(), MaterialRenderProxy);
	MaterialRenderProxy = MaterialRenderProxy ? MaterialRenderProxy : DefaultMaterialRenderProxy;
	check(Material.GetMaterialDomain() == MD_Volume);

	uint32 GroupCountX = FMath::DivideAndRoundUp(View.ViewRect.Size().X, FRenderSingleScatteringWithLiveShadingCS::GetThreadGroupSize2D());
	uint32 GroupCountY = FMath::DivideAndRoundUp(View.ViewRect.Size().Y, FRenderSingleScatteringWithLiveShadingCS::GetThreadGroupSize2D());
	FIntVector GroupCount = FIntVector(GroupCountX, GroupCountY, 1);

	// Note must be done in the same scope as we add the pass otherwise the UB lifetime will not be guaranteed
	FDeferredLightUniformStruct DeferredLightUniform;
	if (bApplyDirectLighting && (LightSceneInfo != nullptr))
	{
		DeferredLightUniform = GetDeferredLightParameters(View, *LightSceneInfo);
	}
	TUniformBufferRef<FDeferredLightUniformStruct> DeferredLightUB = CreateUniformBufferImmediate(DeferredLightUniform, UniformBuffer_SingleDraw);

	bool bWriteVelocity = HeterogeneousVolumes::ShouldWriteVelocity() && HasBeenProduced(SceneTextures.Velocity);
	FRenderSingleScatteringWithLiveShadingCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FRenderSingleScatteringWithLiveShadingCS::FParameters>();
	{
		// Scene data
		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->SceneTextures = GetSceneTextureParameters(GraphBuilder, SceneTextures);
		PassParameters->Scene = View.GetSceneUniforms().GetBuffer(GraphBuilder);
		FBlueNoise BlueNoise = GetBlueNoiseGlobalParameters();
		PassParameters->BlueNoise = CreateUniformBufferImmediate(BlueNoise, EUniformBufferUsage::UniformBuffer_SingleDraw);

		// Light data
		float LODFactor = HeterogeneousVolumes::CalcLODFactor(View, HeterogeneousVolumeInterface);
		PassParameters->bApplyEmissionAndTransmittance = bApplyEmissionAndTransmittance;
		PassParameters->bApplyDirectLighting = bApplyDirectLighting;
		PassParameters->bApplyShadowTransmittance = bApplyShadowTransmittance;
		if (bApplyDirectLighting && (LightSceneInfo != nullptr))
		{
			PassParameters->VolumetricScatteringIntensity = LightSceneInfo->Proxy->GetVolumetricScatteringIntensity();
		}
		else
		{
			PassParameters->VolumetricScatteringIntensity = 1.0f;
		}
		PassParameters->DeferredLight = DeferredLightUB;
		PassParameters->LightType = LightType;
		PassParameters->ShadowStepSize = HeterogeneousVolumes::GetShadowStepSize();
		PassParameters->ShadowStepFactor = HeterogeneousVolumeInterface->GetShadowStepFactor() * LODFactor;

		// Object data
		// TODO: Convert to relative-local space
		//FVector3f ViewOriginHigh = FDFVector3(View.ViewMatrices.GetViewOrigin()).High;
		//FMatrix44f RelativeLocalToWorld = FDFMatrix::MakeToRelativeWorldMatrix(ViewOriginHigh, HeterogeneousVolumeInterface->GetLocalToWorld()).M;
		FMatrix InstanceToLocal = HeterogeneousVolumeInterface->GetInstanceToLocal();
		FMatrix LocalToWorld = HeterogeneousVolumeInterface->GetLocalToWorld();
		PassParameters->LocalToWorld = FMatrix44f(InstanceToLocal * LocalToWorld);
		PassParameters->WorldToLocal = PassParameters->LocalToWorld.Inverse();

		FMatrix LocalToInstance = InstanceToLocal.Inverse();
		FBoxSphereBounds InstanceBoxSphereBounds = LocalBoxSphereBounds.TransformBy(LocalToInstance);
		PassParameters->LocalBoundsOrigin = FVector3f(InstanceBoxSphereBounds.Origin);
		PassParameters->LocalBoundsExtent = FVector3f(InstanceBoxSphereBounds.BoxExtent);
		PassParameters->PrimitiveId = PersistentPrimitiveIndex.Index;

		// Volume data
		PassParameters->VoxelResolution = HeterogeneousVolumeInterface->GetVoxelResolution();

		// Ray data
		PassParameters->MaxTraceDistance = HeterogeneousVolumes::GetMaxTraceDistance();
		PassParameters->StepSize = HeterogeneousVolumes::GetStepSize();
		PassParameters->StepFactor = HeterogeneousVolumeInterface->GetStepFactor() * LODFactor;
		PassParameters->MaxStepCount = HeterogeneousVolumes::GetMaxStepCount();
		PassParameters->bJitter = HeterogeneousVolumes::ShouldJitter();

		// Shadow data
		PassParameters->ForwardLightData = View.ForwardLightingResources.ForwardLightUniformBuffer;
		if (VisibleLightInfo != nullptr)
		{
			const FProjectedShadowInfo* ProjectedShadowInfo = GetShadowForInjectionIntoVolumetricFog(*VisibleLightInfo);
			bool bDynamicallyShadowed = ProjectedShadowInfo != NULL;
			if (bDynamicallyShadowed)
			{
				GetVolumeShadowingShaderParameters(GraphBuilder, View, LightSceneInfo, ProjectedShadowInfo, PassParameters->VolumeShadowingShaderParameters);
			}
			else
			{
				SetVolumeShadowingDefaultShaderParametersGlobal(GraphBuilder, PassParameters->VolumeShadowingShaderParameters);
			}
			PassParameters->VirtualShadowMapId = VisibleLightInfo->GetVirtualShadowMapId(&View);
		}
		else
		{
			SetVolumeShadowingDefaultShaderParametersGlobal(GraphBuilder, PassParameters->VolumeShadowingShaderParameters);
		}
		PassParameters->VirtualShadowMapSamplingParameters = VirtualShadowMapArray.GetSamplingParameters(GraphBuilder);
		PassParameters->AVSM = HeterogeneousVolumes::GetAdaptiveVolumetricShadowMapUniformBuffer(GraphBuilder, View.ViewState, LightSceneInfo);

		TRDGUniformBufferRef<FFogUniformParameters> FogBuffer = CreateFogUniformBuffer(GraphBuilder, View);
		PassParameters->FogStruct = FogBuffer;
		PassParameters->bApplyHeightFog = 0;
		PassParameters->bApplyVolumetricFog = 0;
		if (bApplyEmissionAndTransmittance &&
			(HeterogeneousVolumes::GetApplyFogInscattering() != HeterogeneousVolumes::EFogMode::Off))
		{
			PassParameters->bApplyHeightFog = HeterogeneousVolumes::ShouldApplyHeightFog();
			PassParameters->bApplyVolumetricFog = HeterogeneousVolumes::ShouldApplyVolumetricFog();
		}

		// Indirect lighting data
		auto* LumenUniforms = GraphBuilder.AllocParameters<FLumenTranslucencyLightingUniforms>();
		LumenUniforms->Parameters = GetLumenTranslucencyLightingParameters(GraphBuilder, View.GetLumenTranslucencyGIVolume(), View.LumenFrontLayerTranslucency);
		PassParameters->LumenGIVolumeStruct = GraphBuilder.CreateUniformBuffer(LumenUniforms);

		// Volume data
		if ((HeterogeneousVolumes::UseLightingCacheForTransmittance() && bApplyShadowTransmittance) || HeterogeneousVolumes::UseLightingCacheForInscattering())
		{
			PassParameters->LightingCache.LightingCacheResolution = HeterogeneousVolumes::GetLightingCacheResolution(HeterogeneousVolumeInterface, LODFactor);
			PassParameters->LightingCache.LightingCacheVoxelBias = HeterogeneousVolumeInterface->GetShadowBiasFactor();
			PassParameters->LightingCache.LightingCacheTexture = LightingCacheTexture;
		}
		else
		{
			PassParameters->LightingCache.LightingCacheResolution = FIntVector::ZeroValue;
			PassParameters->LightingCache.LightingCacheVoxelBias = 0.0f;
			PassParameters->LightingCache.LightingCacheTexture = FRDGSystemTextures::Get(GraphBuilder).VolumetricBlack;
		}

		// Dispatch data
		PassParameters->GroupCount = GroupCount;

		// Output
		PassParameters->RWLightingTexture = GraphBuilder.CreateUAV(HeterogeneousVolumeTexture);
		if (bWriteVelocity)
		{
			PassParameters->RWVelocityTexture = GraphBuilder.CreateUAV(SceneTextures.Velocity);
		}
		//PassParameters->RWDebugOutputBuffer = GraphBuilder.CreateUAV(DebugOutputBuffer);
	}

	FString PassName;
#if WANTS_DRAW_MESH_EVENTS
	if (GetEmitDrawEvents())
	{
		FString LightName = TEXT("none");
		if (LightSceneInfo != nullptr)
		{
			FSceneRenderer::GetLightNameForDrawEvent(LightSceneInfo->Proxy, LightName);
		}
		PassName = FString::Printf(TEXT("RenderSingleScatteringWithLiveShadingCS (Light = %s)"), *LightName);
	}
#endif // WANTS_DRAW_MESH_EVENTS

	bool bUseAVSM = HeterogeneousVolumes::UseAdaptiveVolumetricShadowMapForSelfShadowing(HeterogeneousVolumeInterface->GetPrimitiveSceneProxy());

	FRenderSingleScatteringWithLiveShadingCS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FRenderSingleScatteringWithLiveShadingCS::FUseTransmittanceVolume>(HeterogeneousVolumes::UseLightingCacheForTransmittance() && PassParameters->bApplyShadowTransmittance);
	PermutationVector.Set<FRenderSingleScatteringWithLiveShadingCS::FUseInscatteringVolume>(HeterogeneousVolumes::UseLightingCacheForInscattering());
	PermutationVector.Set<FRenderSingleScatteringWithLiveShadingCS::FUseLumenGI>(HeterogeneousVolumes::UseIndirectLighting() && View.GetLumenTranslucencyGIVolume().Texture0 != nullptr);
	PermutationVector.Set<FRenderSingleScatteringWithLiveShadingCS::FWriteVelocity>(bWriteVelocity);
	PermutationVector.Set<FRenderSingleScatteringWithLiveShadingCS::FUseAdaptiveVolumetricShadowMap>(bUseAVSM);
	PermutationVector.Set<FRenderSingleScatteringWithLiveShadingCS::FApplyFogInscattering>(static_cast<int32>(HeterogeneousVolumes::GetApplyFogInscattering()));
	PermutationVector = FRenderSingleScatteringWithLiveShadingCS::RemapPermutation(PermutationVector);
	TShaderRef<FRenderSingleScatteringWithLiveShadingCS> ComputeShader = Material.GetShader<FRenderSingleScatteringWithLiveShadingCS>(&FLocalVertexFactory::StaticType, PermutationVector, false);
	if (!ComputeShader.IsNull())
	{
		AddComputePass<true>(GraphBuilder, ComputeShader, PassParameters, Scene, MaterialRenderProxy, Material, PassName, GroupCount);
	}
}

static void RenderWithTransmittanceVolumePipeline(
	FRDGBuilder& GraphBuilder,
	const FSceneTextures& SceneTextures,
	const FScene* Scene,
	const FViewInfo& View,
	// Shadow data
	TArray<FVisibleLightInfo, SceneRenderingAllocator>& VisibleLightInfos,
	const FVirtualShadowMapArray& VirtualShadowMapArray,
	// Object data
	const IHeterogeneousVolumeInterface* HeterogeneousVolumeInterface,
	const FMaterialRenderProxy* MaterialRenderProxy,
	FPersistentPrimitiveIndex PersistentPrimitiveIndex,
	const FBoxSphereBounds LocalBoxSphereBounds,
	// Transmittance acceleration
	FRDGTextureRef LightingCacheTexture,
	// Output
	FRDGTextureRef& HeterogeneousVolumeRadiance
)
{
	// Light culling
	TArray<FLightSceneInfoCompact, TInlineAllocator<64>> LightSceneInfoCompact;
	for (auto LightIt = Scene->Lights.CreateConstIterator(); LightIt; ++LightIt)
	{
		if (LightIt->AffectsPrimitive(HeterogeneousVolumeInterface->GetBounds(), HeterogeneousVolumeInterface->GetPrimitiveSceneProxy()))
		{
			LightSceneInfoCompact.Add(*LightIt);
		}
	}

	// Light loop:
	int32 NumPasses = FMath::Max(LightSceneInfoCompact.Num(), 1);
	for (int32 PassIndex = 0; PassIndex < NumPasses; ++PassIndex)
	{
		bool bIsLastPass = (PassIndex == (NumPasses - 1));
		bool bApplyEmissionAndTransmittance = bIsLastPass;
		bool bApplyDirectLighting = !LightSceneInfoCompact.IsEmpty();
		bool bApplyShadowTransmittance = false;

		uint32 LightType = 0;
		FLightSceneInfo* LightSceneInfo = nullptr;
		const FVisibleLightInfo* VisibleLightInfo = nullptr;
		if (bApplyDirectLighting)
		{
			LightType = LightSceneInfoCompact[PassIndex].LightType;
			LightSceneInfo = LightSceneInfoCompact[PassIndex].LightSceneInfo;
			check(LightSceneInfo != nullptr);

			bApplyDirectLighting = (LightSceneInfo != nullptr);
			if (LightSceneInfo)
			{
				VisibleLightInfo = &VisibleLightInfos[LightSceneInfo->Id];
				bApplyShadowTransmittance = LightSceneInfo->Proxy->CastsVolumetricShadow();
			}
		}

		if (HeterogeneousVolumes::UseLightingCacheForTransmittance() && bApplyShadowTransmittance)
		{
			RenderLightingCacheWithLiveShading(
				GraphBuilder,
				// Scene data
				Scene,
				View,
				SceneTextures,
				// Light data
				bApplyEmissionAndTransmittance,
				bApplyDirectLighting,
				bApplyShadowTransmittance,
				LightType,
				LightSceneInfo,
				// Shadow data
				VisibleLightInfo,
				VirtualShadowMapArray,
				// Object data
				HeterogeneousVolumeInterface,
				MaterialRenderProxy,
				PersistentPrimitiveIndex,
				LocalBoxSphereBounds,
				// Output
				LightingCacheTexture
			);
		}

		RenderSingleScatteringWithLiveShading(
			GraphBuilder,
			// Scene data
			Scene,
			View,
			SceneTextures,
			// Light data
			bApplyEmissionAndTransmittance,
			bApplyDirectLighting,
			bApplyShadowTransmittance,
			LightType,
			LightSceneInfo,
			// Shadow data
			VisibleLightInfo,
			VirtualShadowMapArray,
			// Object data
			HeterogeneousVolumeInterface,
			MaterialRenderProxy,
			PersistentPrimitiveIndex,
			LocalBoxSphereBounds,
			// Transmittance acceleration
			LightingCacheTexture,
			// Output
			HeterogeneousVolumeRadiance
		);
	}
}

static void RenderWithInscatteringVolumePipeline(
	FRDGBuilder& GraphBuilder,
	const FSceneTextures& SceneTextures,
	const FScene* Scene,
	const FViewInfo& View,
	// Shadow data
	TArray<FVisibleLightInfo, SceneRenderingAllocator>& VisibleLightInfos,
	const FVirtualShadowMapArray& VirtualShadowMapArray,
	// Object data
	const IHeterogeneousVolumeInterface* HeterogeneousVolumeInterface,
	const FMaterialRenderProxy* MaterialRenderProxy,
	FPersistentPrimitiveIndex PersistentPrimitiveIndex,
	const FBoxSphereBounds LocalBoxSphereBounds,
	// Transmittance acceleration
	FRDGTextureRef LightingCacheTexture,
	// Output
	FRDGTextureRef& HeterogeneousVolumeRadiance
)
{
	// Light culling
	TArray<FLightSceneInfoCompact, TInlineAllocator<64>> LightSceneInfoCompact;
	for (auto LightIt = Scene->Lights.CreateConstIterator(); LightIt; ++LightIt)
	{
		if (LightIt->AffectsPrimitive(HeterogeneousVolumeInterface->GetBounds(), HeterogeneousVolumeInterface->GetPrimitiveSceneProxy()))
		{
			LightSceneInfoCompact.Add(*LightIt);
		}
	}

	// Light loop:
	int32 NumPasses = LightSceneInfoCompact.Num();
	for (int32 PassIndex = 0; PassIndex < NumPasses; ++PassIndex)
	{
		bool bApplyEmissionAndTransmittance = (PassIndex == (NumPasses - 1));
		bool bApplyDirectLighting = !LightSceneInfoCompact.IsEmpty();
		bool bApplyShadowTransmittance = false;

		uint32 LightType = 0;
		FLightSceneInfo* LightSceneInfo = nullptr;
		const FVisibleLightInfo* VisibleLightInfo = nullptr;
		if (bApplyDirectLighting)
		{
			LightType = LightSceneInfoCompact[PassIndex].LightType;
			LightSceneInfo = LightSceneInfoCompact[PassIndex].LightSceneInfo;
			check(LightSceneInfo != nullptr);

			bApplyDirectLighting = (LightSceneInfo != nullptr);
			if (LightSceneInfo)
			{
				VisibleLightInfo = &VisibleLightInfos[LightSceneInfo->Id];
				bApplyShadowTransmittance = LightSceneInfo->Proxy->CastsVolumetricShadow();
			}
		}

		RenderLightingCacheWithLiveShading(
			GraphBuilder,
			// Scene data
			Scene,
			View,
			SceneTextures,
			// Light data
			bApplyEmissionAndTransmittance,
			bApplyDirectLighting,
			bApplyShadowTransmittance,
			LightType,
			LightSceneInfo,
			// Shadow data
			VisibleLightInfo,
			VirtualShadowMapArray,
			// Object data
			HeterogeneousVolumeInterface,
			MaterialRenderProxy,
			PersistentPrimitiveIndex,
			LocalBoxSphereBounds,
			// Output
			LightingCacheTexture
		);
	}

	// Direct volume integrator
	{
		bool bApplyEmissionAndTransmittance = true;
		bool bApplyDirectLighting = true;
		bool bApplyShadowTransmittance = true;

		uint32 LightType = 0;
		FLightSceneInfo* LightSceneInfo = nullptr;
		const FVisibleLightInfo* VisibleLightInfo = nullptr;

		RenderSingleScatteringWithLiveShading(
			GraphBuilder,
			// Scene data
			Scene,
			View,
			SceneTextures,
			// Light data
			bApplyEmissionAndTransmittance,
			bApplyDirectLighting,
			bApplyShadowTransmittance,
			LightType,
			LightSceneInfo,
			// Shadow data
			VisibleLightInfo,
			VirtualShadowMapArray,
			// Object data
			HeterogeneousVolumeInterface,
			MaterialRenderProxy,
			PersistentPrimitiveIndex,
			LocalBoxSphereBounds,
			// Transmittance acceleration
			LightingCacheTexture,
			// Output
			HeterogeneousVolumeRadiance
		);
	}
}

void RenderWithLiveShading(
	FRDGBuilder& GraphBuilder,
	const FSceneTextures& SceneTextures,
	const FScene* Scene,
	const FViewInfo& View,
	// Shadow data
	TArray<FVisibleLightInfo, SceneRenderingAllocator>& VisibleLightInfos,
	const FVirtualShadowMapArray& VirtualShadowMapArray,
	// Object data
	const IHeterogeneousVolumeInterface* HeterogeneousVolumeInterface,
	const FMaterialRenderProxy* MaterialRenderProxy,
	const FPersistentPrimitiveIndex &PersistentPrimitiveIndex,
	const FBoxSphereBounds LocalBoxSphereBounds,
	// Transmittance acceleration
	FRDGTextureRef LightingCacheTexture,
	// Output
	FRDGTextureRef& HeterogeneousVolumeRadiance
)
{
	if (HeterogeneousVolumes::UseLightingCacheForInscattering())
	{
		RenderWithInscatteringVolumePipeline(
			GraphBuilder,
			SceneTextures,
			Scene,
			View,
			// Shadow data
			VisibleLightInfos,
			VirtualShadowMapArray,
			// Object data
			HeterogeneousVolumeInterface,
			MaterialRenderProxy,
			PersistentPrimitiveIndex,
			LocalBoxSphereBounds,
			// Transmittance acceleration
			LightingCacheTexture,
			// Output
			HeterogeneousVolumeRadiance
		);
	}
	else
	{
		RenderWithTransmittanceVolumePipeline(
			GraphBuilder,
			SceneTextures,
			Scene,
			View,
			// Shadow data
			VisibleLightInfos,
			VirtualShadowMapArray,
			// Object data
			HeterogeneousVolumeInterface,
			MaterialRenderProxy,
			PersistentPrimitiveIndex,
			LocalBoxSphereBounds,
			// Transmittance acceleration
			LightingCacheTexture,
			// Output
			HeterogeneousVolumeRadiance
		);
	}
}

class FRenderVolumetricShadowMapForLightWithLiveShadingCS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FRenderVolumetricShadowMapForLightWithLiveShadingCS, MeshMaterial);

	class FUseAVSMCompression : SHADER_PERMUTATION_BOOL("USE_AVSM_COMPRESSION");
	using FPermutationDomain = TShaderPermutationDomain<FUseAVSMCompression>;


	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		// Scene data
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureParameters, SceneTextures)
		SHADER_PARAMETER_STRUCT_REF(FBlueNoise, BlueNoise)

		// Volumetric Shadow Map data
		SHADER_PARAMETER(FVector3f, TranslatedWorldOrigin)
		SHADER_PARAMETER(FIntPoint, ShadowResolution)
		SHADER_PARAMETER(int, MaxSampleCount)
		SHADER_PARAMETER(float, AbsoluteErrorThreshold)
		SHADER_PARAMETER(float, RelativeErrorThreshold)

		SHADER_PARAMETER(int, NumShadowMatrices)
		SHADER_PARAMETER_ARRAY(FMatrix44f, TranslatedWorldToShadow, [6])
		SHADER_PARAMETER_ARRAY(FMatrix44f, ShadowToTranslatedWorld, [6])

		// Object data
		SHADER_PARAMETER(FMatrix44f, LocalToWorld)
		SHADER_PARAMETER(FMatrix44f, WorldToLocal)
		SHADER_PARAMETER(FVector3f, LocalBoundsOrigin)
		SHADER_PARAMETER(FVector3f, LocalBoundsExtent)
		SHADER_PARAMETER(int32, PrimitiveId)

		SHADER_PARAMETER(FIntVector, VoxelResolution)

		// Ray data
		SHADER_PARAMETER(float, ShadowStepSize)
		SHADER_PARAMETER(float, ShadowStepFactor)
		SHADER_PARAMETER(float, MaxTraceDistance)
		SHADER_PARAMETER(int, MaxStepCount)
		SHADER_PARAMETER(int, bJitter)

		// Dispatch data
		SHADER_PARAMETER(FIntVector, GroupCount)
		SHADER_PARAMETER(int, ShadowDebugTweak)

		// Output
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<int>, RWVolumetricShadowLinkedListAllocatorBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<int2>, RWVolumetricShadowLinkedListBuffer)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RWBeerShadowMapTexture)

		// Debug
		//SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FVolumetricShadowMapDebugData>, RWDebugBuffer)
	END_SHADER_PARAMETER_STRUCT()

	FRenderVolumetricShadowMapForLightWithLiveShadingCS() = default;

	FRenderVolumetricShadowMapForLightWithLiveShadingCS(
		const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer
	)
		: FMeshMaterialShader(Initializer)
	{
		Bindings.BindForLegacyShaderParameters(
			this,
			Initializer.PermutationId,
			Initializer.ParameterMap,
			*FParameters::FTypeInfo::GetStructMetadata(),
			// Don't require full bindings, we use FMaterialShader::SetParameters
			false
		);
		ShaderLooseBindings.Bind(Initializer.ParameterMap);
	}

	static bool ShouldCompilePermutation(
		const FMaterialShaderPermutationParameters& Parameters
	)
	{
		return DoesPlatformSupportHeterogeneousVolumes(Parameters.Platform)
			&& DoesMaterialShaderSupportHeterogeneousVolumes(Parameters.MaterialParameters);
	}

	static void ModifyCompilationEnvironment(
		const FMaterialShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment
	)
	{
		FMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_1D"), GetThreadGroupSize1D());
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_2D"), GetThreadGroupSize2D());
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_3D"), GetThreadGroupSize3D());

		// Disable in-scattering features
		OutEnvironment.SetDefine(TEXT("DIM_USE_TRANSMITTANCE_VOLUME"), 0);
		OutEnvironment.SetDefine(TEXT("DIM_USE_INSCATTERING_VOLUME"), 0);
		OutEnvironment.SetDefine(TEXT("DIM_USE_LUMEN_GI"), 0);

		// This shader takes a very long time to compile with FXC, so we pre-compile it with DXC first and then forward the optimized HLSL to FXC.
		OutEnvironment.CompilerFlags.Add(CFLAG_PrecompileWithDXC); // @lh-todo - Disabled to workaround SPIRV-Cross bug: StructuredBuffer<uint> is translated to ByteAddressBuffer in HLSL backend
		OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);

		OutEnvironment.SetDefine(TEXT("GET_PRIMITIVE_DATA_OVERRIDE"), 1);
	}

	static int32 GetThreadGroupSize1D() { return GetThreadGroupSize3D() * GetThreadGroupSize3D() * GetThreadGroupSize3D(); }
	static int32 GetThreadGroupSize2D() { return 8; }
	static int32 GetThreadGroupSize3D() { return 4; }

	LAYOUT_FIELD(FRenderLightingCacheLooseBindings, ShaderLooseBindings);
};

IMPLEMENT_MATERIAL_SHADER_TYPE(, FRenderVolumetricShadowMapForLightWithLiveShadingCS, TEXT("/Engine/Private/HeterogeneousVolumes/HeterogeneousVolumesLiveShadingShadows.usf"), TEXT("RenderVolumetricShadowMapForLightWithLiveShadingCS"), SF_Compute);

void CollectHeterogeneousVolumeMeshBatchesForView(
	const FViewInfo& View,
	bool bCollectForShadowCasting,
	TSet<FVolumetricMeshBatch>& HeterogeneousVolumesMeshBatches,
	FBoxSphereBounds& WorldBounds
)
{
	for (int32 MeshBatchIndex = 0; MeshBatchIndex < View.HeterogeneousVolumesMeshBatches.Num(); ++MeshBatchIndex)
	{
		const FVolumetricMeshBatch& MeshBatch = View.HeterogeneousVolumesMeshBatches[MeshBatchIndex];

		// TODO: Is material determiniation too expensive?
		const FMaterialRenderProxy* MaterialRenderProxy = nullptr;
		const FMaterialRenderProxy* DefaultMaterialRenderProxy = MeshBatch.Mesh->MaterialRenderProxy;
		const FMaterial& Material = DefaultMaterialRenderProxy->GetMaterialWithFallback(View.GetFeatureLevel(), MaterialRenderProxy);
		MaterialRenderProxy = MaterialRenderProxy ? MaterialRenderProxy : DefaultMaterialRenderProxy;
		bool bIsVolumeMaterial = Material.GetMaterialDomain() == MD_Volume;

		bool bCollectMeshBatch = bIsVolumeMaterial;
		if (bCollectForShadowCasting)
		{
			bool bIsShadowCast = MeshBatch.Proxy->IsShadowCast(&View);
			bCollectMeshBatch = bCollectMeshBatch && bIsShadowCast;
		}

		if (bCollectMeshBatch)
		{
			HeterogeneousVolumesMeshBatches.FindOrAdd(FVolumetricMeshBatch(MeshBatch.Mesh, MeshBatch.Proxy));
			WorldBounds = WorldBounds + MeshBatch.Proxy->GetBounds();
		}
	}
}

void CollectHeterogeneousVolumeMeshBatchesForLight(
	const FLightSceneInfo* LightSceneInfo,
	const FVisibleLightInfo* VisibleLightInfo,
	const FViewInfo& View,
	TSet<FVolumetricMeshBatch>& HeterogeneousVolumesMeshBatches,
	FBoxSphereBounds& WorldBounds
)
{
	check(LightSceneInfo);
	check(VisibleLightInfo);

	if (LightSceneInfo->Proxy->CastsVolumetricShadow())
	{
		//for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			//const FViewInfo& View = Views[ViewIndex];
			bool bCollectForShadowCasting = true;
			CollectHeterogeneousVolumeMeshBatchesForView(View, bCollectForShadowCasting, HeterogeneousVolumesMeshBatches, WorldBounds);
		}

		for (int32 ShadowIndex = 0; ShadowIndex < VisibleLightInfo->ShadowsToProject.Num(); ++ShadowIndex)
		{
			const FProjectedShadowInfo* ProjectedShadowInfo = HeterogeneousVolumes::GetProjectedShadowInfo(VisibleLightInfo, ShadowIndex);
			if (ProjectedShadowInfo != nullptr)
			{
				const TArray<FMeshBatchAndRelevance, SceneRenderingAllocator>& MeshBatches = ProjectedShadowInfo->GetDynamicSubjectHeterogeneousVolumeMeshElements();
				for (int32 MeshBatchIndex = 0; MeshBatchIndex < MeshBatches.Num(); ++MeshBatchIndex)
				{
					const FMeshBatchAndRelevance& MeshBatch = MeshBatches[MeshBatchIndex];
					bool bIsShadowCast = MeshBatch.PrimitiveSceneProxy->IsShadowCast(ProjectedShadowInfo->ShadowDepthView);

					// TODO: Is material determiniation too expensive?
					const FMaterialRenderProxy* MaterialRenderProxy = nullptr;
					const FMaterialRenderProxy* DefaultMaterialRenderProxy = MeshBatch.Mesh->MaterialRenderProxy;
					const FMaterial& Material = DefaultMaterialRenderProxy->GetMaterialWithFallback(View.GetFeatureLevel(), MaterialRenderProxy);
					MaterialRenderProxy = MaterialRenderProxy ? MaterialRenderProxy : DefaultMaterialRenderProxy;
					bool bIsVolumeMaterial = Material.GetMaterialDomain() == MD_Volume;

					if (bIsShadowCast && bIsVolumeMaterial)
					{
						HeterogeneousVolumesMeshBatches.FindOrAdd(FVolumetricMeshBatch(MeshBatch.Mesh, MeshBatch.PrimitiveSceneProxy));
						WorldBounds = WorldBounds + MeshBatch.PrimitiveSceneProxy->GetBounds();
					}
				}
			}
		}
	}
}

bool RenderVolumetricShadowMapForLightForHeterogeneousVolumeWithLiveShading(
	FRDGBuilder& GraphBuilder,
	// Scene data
	const FSceneTextures& SceneTextures,
	FScene* Scene,
	const FSceneViewFamily& ViewFamily,
	FViewInfo& View,
	// Light data
	uint32 LightType,
	const FLightSceneInfo* LightSceneInfo,
	const FVisibleLightInfo* VisibleLightInfo,
	// Shadow data
	const FVector3f& TranslatedWorldOrigin,
	int32 NumShadowMatrices,
	FMatrix44f* TranslatedWorldToShadow,
	FMatrix44f* ShadowToTranslatedWorld,
	FIntPoint ShadowMapResolution,
	uint32 MaxSampleCount,
	// Volume
	const FVolumetricMeshBatch& VolumetricMeshBatch,
	// Dispatch
	FIntVector& GroupCount,
	// Output
	FRDGTextureRef& BeerShadowMapTexture,
	FRDGBufferRef& VolumetricShadowLinkedListBuffer
)
{
	// TODO: Understand how the default world material can be triggered here during a recompilation, but not elsewhere..
	const FMaterialRenderProxy* MaterialRenderProxy = nullptr;
	const FMaterialRenderProxy* DefaultMaterialRenderProxy = VolumetricMeshBatch.Mesh->MaterialRenderProxy;
	const FMaterial& Material = DefaultMaterialRenderProxy->GetMaterialWithFallback(View.GetFeatureLevel(), MaterialRenderProxy);
	MaterialRenderProxy = MaterialRenderProxy ? MaterialRenderProxy : DefaultMaterialRenderProxy;
	if (Material.GetMaterialDomain() != MD_Volume)
	{
		return false;
	}

	FRDGBufferRef VolumetricShadowLinkedListAllocatorBuffer = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), 1),
		TEXT("HeterogeneousVolume.VolumetricShadowLinkedListAllocatorBuffer")
	);
	// Initialize allocator to contain 1-spp
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(VolumetricShadowLinkedListAllocatorBuffer, PF_R32_UINT), ShadowMapResolution.X * ShadowMapResolution.Y);

	FRenderVolumetricShadowMapForLightWithLiveShadingCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FRenderVolumetricShadowMapForLightWithLiveShadingCS::FParameters>();
	{
		// Scene data
		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->SceneTextures = GetSceneTextureParameters(GraphBuilder, SceneTextures);
		FBlueNoise BlueNoise = GetBlueNoiseGlobalParameters();
		PassParameters->BlueNoise = CreateUniformBufferImmediate(BlueNoise, EUniformBufferUsage::UniformBuffer_SingleDraw);

		// Shadow map data
		PassParameters->TranslatedWorldOrigin = TranslatedWorldOrigin;
		PassParameters->ShadowResolution = ShadowMapResolution;
		PassParameters->MaxSampleCount = MaxSampleCount;
		PassParameters->AbsoluteErrorThreshold = HeterogeneousVolumes::GetShadowAbsoluteErrorThreshold();
		PassParameters->RelativeErrorThreshold = HeterogeneousVolumes::GetShadowRelativeErrorThreshold();

		PassParameters->NumShadowMatrices = NumShadowMatrices;
		for (int32 i = 0; i < PassParameters->NumShadowMatrices; ++i)
		{
			PassParameters->TranslatedWorldToShadow[i] = TranslatedWorldToShadow[i];
			PassParameters->ShadowToTranslatedWorld[i] = ShadowToTranslatedWorld[i];
		}

		// TODO: Instancing support
		int32 VolumeIndex = 0;

		// Object data
		const IHeterogeneousVolumeInterface* HeterogeneousVolumeInterface = (IHeterogeneousVolumeInterface*)VolumetricMeshBatch.Mesh->Elements[VolumeIndex].UserData;
		FMatrix InstanceToLocal = HeterogeneousVolumeInterface->GetInstanceToLocal();
		FMatrix LocalToWorld = HeterogeneousVolumeInterface->GetLocalToWorld();
		PassParameters->LocalToWorld = FMatrix44f(InstanceToLocal * LocalToWorld);
		PassParameters->WorldToLocal = PassParameters->LocalToWorld.Inverse();

		FMatrix LocalToInstance = InstanceToLocal.Inverse();
		FBoxSphereBounds InstanceBoxSphereBounds = HeterogeneousVolumeInterface->GetLocalBounds().TransformBy(LocalToInstance);
		PassParameters->LocalBoundsOrigin = FVector3f(InstanceBoxSphereBounds.Origin);
		PassParameters->LocalBoundsExtent = FVector3f(InstanceBoxSphereBounds.BoxExtent);
		const FPrimitiveSceneInfo* PrimitiveSceneInfo = VolumetricMeshBatch.Proxy->GetPrimitiveSceneInfo();
		check(PrimitiveSceneInfo);
		PassParameters->PrimitiveId = PrimitiveSceneInfo->GetPersistentIndex().Index;

		PassParameters->VoxelResolution = HeterogeneousVolumeInterface->GetVoxelResolution();

		// Ray Data
		float LODFactor = HeterogeneousVolumes::CalcLODFactor(View, HeterogeneousVolumeInterface);
		PassParameters->ShadowStepSize = HeterogeneousVolumes::GetShadowStepSize();
		PassParameters->ShadowStepFactor = HeterogeneousVolumeInterface->GetShadowStepFactor() * LODFactor;
		PassParameters->MaxTraceDistance = HeterogeneousVolumes::GetMaxTraceDistance();
		PassParameters->MaxStepCount = HeterogeneousVolumes::GetMaxStepCount();
		PassParameters->bJitter = HeterogeneousVolumes::ShouldJitter();

		// Dispatch data
		PassParameters->GroupCount = GroupCount;
		//PassParameters->ShadowDebugTweak = CVarHeterogeneousVolumesShadowDebugTweak.GetValueOnRenderThread();
		PassParameters->ShadowDebugTweak = 0;

		// Output
		PassParameters->RWVolumetricShadowLinkedListAllocatorBuffer = GraphBuilder.CreateUAV(VolumetricShadowLinkedListAllocatorBuffer, PF_R32_UINT);
		PassParameters->RWVolumetricShadowLinkedListBuffer = GraphBuilder.CreateUAV(VolumetricShadowLinkedListBuffer);
		PassParameters->RWBeerShadowMapTexture = GraphBuilder.CreateUAV(BeerShadowMapTexture);
		//PassParameters->RWDebugBuffer = GraphBuilder.CreateUAV(DebugBuffer);
	}

	FString PassName;
#if WANTS_DRAW_MESH_EVENTS
	if (GetEmitDrawEvents())
	{
		FString LightName = TEXT("none");
		if (LightSceneInfo != nullptr)
		{
			FSceneRenderer::GetLightNameForDrawEvent(LightSceneInfo->Proxy, LightName);
		}
		PassName = FString::Printf(TEXT("RenderVolumetricShadowMapForLightWithLiveShadingCS (Light = %s)"), *LightName);
	}
#endif // WANTS_DRAW_MESH_EVENTS

	FRenderVolumetricShadowMapForLightWithLiveShadingCS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FRenderVolumetricShadowMapForLightWithLiveShadingCS::FUseAVSMCompression>(HeterogeneousVolumes::UseAVSMCompression());
	TShaderRef<FRenderVolumetricShadowMapForLightWithLiveShadingCS> ComputeShader = Material.GetShader<FRenderVolumetricShadowMapForLightWithLiveShadingCS>(&FLocalVertexFactory::StaticType, PermutationVector, false);
	if (!ComputeShader.IsNull())
	{
		GraphBuilder.AddPass(
			RDG_EVENT_NAME("%s", *PassName),
			PassParameters,
			ERDGPassFlags::Compute,
			[ComputeShader, PassParameters, Scene, MaterialRenderProxy, &Material, GroupCount](FRHIComputeCommandList& RHICmdList)
			{
#if 1
				FMeshDrawShaderBindings ShaderBindings;
		UE::MeshPassUtils::SetupComputeBindings(ComputeShader, Scene, Scene->GetFeatureLevel(), nullptr, *MaterialRenderProxy, Material, ShaderBindings);
#else
				FMeshMaterialShaderElementData ShaderElementData;
		ShaderElementData.InitializeMeshMaterialData();

		FMeshProcessorShaders PassShaders;
		PassShaders.ComputeShader = ComputeShader;

		FMeshDrawShaderBindings ShaderBindings;
		ShaderBindings.Initialize(PassShaders);
		{
			FMeshDrawSingleShaderBindings SingleShaderBindings = ShaderBindings.GetSingleShaderBindings(SF_Compute);
			ComputeShader->GetShaderBindings(Scene, Scene->GetFeatureLevel(), nullptr, *MaterialRenderProxy, Material, ShaderElementData, SingleShaderBindings);
			//ComputeShader->ShaderLooseBindings.SetParameters(SingleShaderBindings, PassParameters);
			SingleShaderBindings.AddTexture(
				ComputeShader->ShaderLooseBindings.SceneDepthTextureBinding,
				FShaderResourceParameter(),
				TStaticSamplerState<SF_Point>::GetRHI(),
				PassParameters->SceneTextures.SceneDepthTexture->GetRHI()
			);
			ShaderBindings.Finalize(&PassShaders);
		}
#endif

		UE::MeshPassUtils::Dispatch(RHICmdList, ComputeShader, ShaderBindings, *PassParameters, GroupCount);
			}
		);
	}

	return true;
}

bool RenderVolumetricShadowMapForLightWithLiveShading(
	FRDGBuilder& GraphBuilder,
	// Scene data
	const FSceneTextures& SceneTextures,
	FScene* Scene,
	const FSceneViewFamily& ViewFamily,
	FViewInfo& View,
	// Light data
	uint32 LightType,
	const FLightSceneInfo* LightSceneInfo,
	const FVisibleLightInfo* VisibleLightInfo,
	// Output
	bool& bIsDirectionalLight,
	FVector3f& TranslatedWorldOrigin,
	FVector4f& TranslatedWorldPlane,
	FMatrix44f* TranslatedWorldToShadow,
	FIntVector& GroupCount,
	int32& NumShadowMatrices,
	FIntPoint& ShadowMapResolution,
	uint32& MaxSampleCount,
	FRDGTextureRef& BeerShadowMapTexture,
	FRDGBufferRef& VolumetricShadowLinkedListBuffer
)
{
	check(LightSceneInfo);
	check(VisibleLightInfo);

	const FProjectedShadowInfo* ProjectedShadowInfo = HeterogeneousVolumes::GetProjectedShadowInfo(VisibleLightInfo, 0);
	check(ProjectedShadowInfo != NULL)

	ShadowMapResolution = HeterogeneousVolumes::GetShadowMapResolution();

	bool bIsMultiProjection = (LightType == LightType_Point) || (LightType == LightType_Rect);
	GroupCount = FIntVector(1);
	GroupCount.X = FMath::DivideAndRoundUp(ShadowMapResolution.X, FRenderVolumetricShadowMapForLightWithLiveShadingCS::GetThreadGroupSize2D());
	GroupCount.Y = FMath::DivideAndRoundUp(ShadowMapResolution.Y, FRenderVolumetricShadowMapForLightWithLiveShadingCS::GetThreadGroupSize2D());
	GroupCount.Z = bIsMultiProjection ? 6 : 1;

	// Collect shadow-casting volumes
	TSet<FVolumetricMeshBatch> HeterogeneousVolumesMeshBatches;
	FBoxSphereBounds WorldVolumeBounds(ForceInit);
	CollectHeterogeneousVolumeMeshBatchesForLight(LightSceneInfo, VisibleLightInfo, View, HeterogeneousVolumesMeshBatches, WorldVolumeBounds);
	if (HeterogeneousVolumesMeshBatches.IsEmpty())
	{
		return false;
	}

	// Adjust shadow resolution based on minimum MipLevel
	float LODValue = FMath::CeilLogTwo(ShadowMapResolution.X);
	for (auto VolumetricMeshBatch : HeterogeneousVolumesMeshBatches)
	{
		int32 VolumeCount = VolumetricMeshBatch.Mesh->Elements.Num();
		for (int32 VolumeIndex = 0; VolumeIndex < VolumeCount; ++VolumeIndex)
		{
			const IHeterogeneousVolumeInterface* HeterogeneousVolumeInterface = (IHeterogeneousVolumeInterface*)VolumetricMeshBatch.Mesh->Elements[VolumeIndex].UserData;
			LODValue = FMath::Min(LODValue, HeterogeneousVolumes::CalcLOD(View, HeterogeneousVolumeInterface));
		}
	}
	float LODFactor = HeterogeneousVolumes::CalcLODFactor(LODValue);
	ShadowMapResolution /= LODFactor;

	// Build shadow transform
	NumShadowMatrices = ProjectedShadowInfo->OnePassShadowViewProjectionMatrices.Num();
	FMatrix44f ShadowToTranslatedWorld[6];

	if (NumShadowMatrices > 0)
	{
		FVector PreViewTranslation = View.ViewMatrices.GetPreViewTranslation();
		FMatrix TranslatedWorldToWorldMatrix = FTranslationMatrix(-PreViewTranslation);
		FVector LightPosition = LightSceneInfo->Proxy->GetPosition();
		FMatrix WorldToLightMatrix = LightSceneInfo->Proxy->GetWorldToLight();

		// Remove light rotation when building the RectLight projections..
		FMatrix RotationalAdjustmentMatrix = FMatrix::Identity;
		if (LightType == LIGHT_TYPE_RECT)
		{
			FVector LightDirection = LightSceneInfo->Proxy->GetDirection().GetSafeNormal();
			RotationalAdjustmentMatrix = FRotationMatrix(LightDirection.Rotation());
		}

		FMatrix ViewMatrix[] = {
			FLookFromMatrix(FVector::Zero(), FVector(-1, 0, 0), FVector(0, 0, 1)),
			FLookFromMatrix(FVector::Zero(), FVector(1, 0, 0), FVector(0, 0, 1)),
			FLookFromMatrix(FVector::Zero(), FVector(0, -1, 0), FVector(0, 0, 1)),
			FLookFromMatrix(FVector::Zero(), FVector(0, 1, 0), FVector(0, 0, 1)),
			FLookFromMatrix(FVector::Zero(), FVector(0, 0, -1), FVector(1, 0, 0)),
			FLookFromMatrix(FVector::Zero(), FVector(0, 0, 1), FVector(1, 0, 0))
		};

		FMatrix PerspectiveMatrix = FPerspectiveMatrix(
			PI / 4.0f,
			ShadowMapResolution.X,
			ShadowMapResolution.Y,
			1.0,
			LightSceneInfo->Proxy->GetRadius()
		);

		FMatrix ScreenMatrix = FScaleMatrix(FVector(0.5, -0.5, -0.5)) * FTranslationMatrix(FVector(0.5, 0.5, 0.5));

		for (int32 i = 0; i < NumShadowMatrices; ++i)
		{
			FMatrix WorldToShadowMatrix = WorldToLightMatrix * RotationalAdjustmentMatrix * ViewMatrix[i] * PerspectiveMatrix * ScreenMatrix;
			TranslatedWorldToShadow[i] = FMatrix44f(TranslatedWorldToWorldMatrix * WorldToShadowMatrix);
			ShadowToTranslatedWorld[i] = TranslatedWorldToShadow[i].Inverse();
		}
		TranslatedWorldOrigin = FVector3f(PreViewTranslation + LightPosition);
	}
	else if (LightType == LightType_Directional)
	{
		bIsDirectionalLight = true;
		// Build orthographic projection centered around volume..
		FVector PreViewTranslation = View.ViewMatrices.GetPreViewTranslation();
		FMatrix TranslatedWorldToWorldMatrix = FTranslationMatrix(-PreViewTranslation);

		FMatrix TranslationMatrix = FTranslationMatrix(-WorldVolumeBounds.Origin);

		FVector LightDirection = LightSceneInfo->Proxy->GetDirection().GetSafeNormal();
		FMatrix RotationMatrix = FInverseRotationMatrix(LightDirection.Rotation());
		FMatrix ScaleMatrix = FScaleMatrix(FVector(1.0 / WorldVolumeBounds.SphereRadius));

		const FMatrix FaceMatrix(
			FPlane(0, 0, 1, 0),
			FPlane(0, 1, 0, 0),
			FPlane(-1, 0, 0, 0),
			FPlane(0, 0, 0, 1));

		// Invert Z to match reverse-Z for the rest of the shadow types!
		FMatrix ScreenMatrix = FScaleMatrix(FVector(0.5, -0.5, -0.5)) * FTranslationMatrix(FVector(0.5, 0.5, 0.5));
		FMatrix WorldToShadowMatrix = TranslationMatrix * RotationMatrix * ScaleMatrix * FaceMatrix * ScreenMatrix;
		FMatrix TranslatedWorldToShadowMatrix = TranslatedWorldToWorldMatrix * WorldToShadowMatrix;

		NumShadowMatrices = 1;
		TranslatedWorldToShadow[0] = FMatrix44f(TranslatedWorldToShadowMatrix);
		ShadowToTranslatedWorld[0] = TranslatedWorldToShadow[0].Inverse();
		TranslatedWorldOrigin = FVector3f(PreViewTranslation + WorldVolumeBounds.Origin - LightDirection * WorldVolumeBounds.SphereRadius);
	}
	else
	{
		FVector PreViewTranslation = View.ViewMatrices.GetPreViewTranslation();
		FMatrix TranslatedWorldToWorldMatrix = FTranslationMatrix(-PreViewTranslation);
		FVector4f ShadowmapMinMax = FVector4f::Zero();
		FMatrix WorldToShadowMatrix = ProjectedShadowInfo->GetWorldToShadowMatrix(ShadowmapMinMax);
		FMatrix TranslatedWorldToShadowMatrix = TranslatedWorldToWorldMatrix * WorldToShadowMatrix;

		NumShadowMatrices = 1;
		TranslatedWorldToShadow[0] = FMatrix44f(TranslatedWorldToShadowMatrix);
		ShadowToTranslatedWorld[0] = TranslatedWorldToShadow[0].Inverse();
		TranslatedWorldOrigin = FVector3f(View.ViewMatrices.GetPreViewTranslation() - ProjectedShadowInfo->PreShadowTranslation);
	}

	FVector LightDirection = LightSceneInfo->Proxy->GetDirection().GetSafeNormal();
	float W = -FVector3f::DotProduct(TranslatedWorldOrigin, FVector3f(LightDirection));
	TranslatedWorldPlane = FVector4f(LightDirection.X, LightDirection.Y, LightDirection.Z, W);

	// Iterate over shadow-casting volumes
	bool bHasShadowCastingVolume = false;
	if (!HeterogeneousVolumesMeshBatches.IsEmpty())
	{
		auto VolumeMeshBatchItr = HeterogeneousVolumesMeshBatches.begin();

		MaxSampleCount = HeterogeneousVolumes::GetShadowMaxSampleCount();
		int32 VolumetricShadowLinkedListElementCount = ShadowMapResolution.X * ShadowMapResolution.Y * MaxSampleCount;
		if (bIsMultiProjection)
		{
			VolumetricShadowLinkedListElementCount *= 6;
		}
		VolumetricShadowLinkedListBuffer = GraphBuilder.CreateBuffer(
			FRDGBufferDesc::CreateStructuredDesc(sizeof(FAVSMLinkedListPackedData), VolumetricShadowLinkedListElementCount),
			TEXT("HeterogeneousVolume.VolumetricShadowLinkedListBuffer")
		);

		RenderVolumetricShadowMapForLightForHeterogeneousVolumeWithLiveShading(
			GraphBuilder,
			SceneTextures,
			Scene,
			ViewFamily,
			View,
			// Light Info
			LightType,
			LightSceneInfo,
			VisibleLightInfo,
			// Shadow Info
			TranslatedWorldOrigin,
			NumShadowMatrices,
			TranslatedWorldToShadow,
			ShadowToTranslatedWorld,
			ShadowMapResolution,
			MaxSampleCount,
			// Volume
			*VolumeMeshBatchItr,
			// Dispatch
			GroupCount,
			// Output
			BeerShadowMapTexture,
			VolumetricShadowLinkedListBuffer
		);

		++VolumeMeshBatchItr;
		for (; VolumeMeshBatchItr != HeterogeneousVolumesMeshBatches.end(); ++VolumeMeshBatchItr)
		{
			FRDGBufferRef VolumetricShadowLinkedListBuffer1 = GraphBuilder.CreateBuffer(
				FRDGBufferDesc::CreateStructuredDesc(sizeof(FAVSMLinkedListPackedData), VolumetricShadowLinkedListElementCount),
				TEXT("HeterogeneousVolume.VolumetricShadowLinkedListBuffer1")
			);

			RenderVolumetricShadowMapForLightForHeterogeneousVolumeWithLiveShading(
				GraphBuilder,
				SceneTextures,
				Scene,
				ViewFamily,
				View,
				// Light Info
				LightType,
				LightSceneInfo,
				VisibleLightInfo,
				// Shadow Info
				TranslatedWorldOrigin,
				NumShadowMatrices,
				TranslatedWorldToShadow,
				ShadowToTranslatedWorld,
				ShadowMapResolution,
				MaxSampleCount,
				// Volume
				*VolumeMeshBatchItr,
				// Dispatch
				GroupCount,
				// Output
				BeerShadowMapTexture,
				VolumetricShadowLinkedListBuffer1
			);

			FRDGBufferRef VolumetricShadowLinkedListBuffer2 = GraphBuilder.CreateBuffer(
				FRDGBufferDesc::CreateStructuredDesc(sizeof(FAVSMLinkedListPackedData), VolumetricShadowLinkedListElementCount),
				TEXT("HeterogeneousVolume.VolumetricShadowLinkedListBuffer2")
			);

			CombineVolumetricShadowMap(
				GraphBuilder,
				View,
				GroupCount,
				LightType,
				ShadowMapResolution,
				MaxSampleCount,
				VolumetricShadowLinkedListBuffer,
				VolumetricShadowLinkedListBuffer1,
				VolumetricShadowLinkedListBuffer2
			);

			VolumetricShadowLinkedListBuffer = VolumetricShadowLinkedListBuffer2;
		}
	}

	return true;
}

void RenderAdaptiveVolumetricShadowMapWithLiveShading(
	FRDGBuilder& GraphBuilder,
	// Scene data
	const FSceneTextures& SceneTextures,
	FScene* Scene,
	const FSceneViewFamily& ViewFamily,
	FViewInfo& View,
	// Light data
	TArray<FVisibleLightInfo, SceneRenderingAllocator>& VisibleLightInfos
)
{
	RDG_EVENT_SCOPE(GraphBuilder, "Adaptive Volumetric Shadow Maps");
	bool bShouldRenderShadowMaps = !View.ViewRect.IsEmpty();

	// Light culling
	TArray<FLightSceneInfoCompact, TInlineAllocator<64>> LightSceneInfoCompact;
	for (auto LightIt = Scene->Lights.CreateConstIterator(); LightIt; ++LightIt)
	{
		// TODO: Use global bounds information..
		//if (LightIt->AffectsPrimitive(HeterogeneousVolumeInterface->GetBounds(), HeterogeneousVolumeInterface->GetPrimitiveSceneProxy()))
		{
			LightSceneInfoCompact.Add(*LightIt);
		}
	}

	// Light loop:
	int32 NumPasses = LightSceneInfoCompact.Num();
	for (int32 PassIndex = 0; PassIndex < NumPasses; ++PassIndex)
	{
		bool bApplyDirectLighting = !LightSceneInfoCompact.IsEmpty();
		bool bApplyEmissionAndTransmittance = false;
		bool bCastsVolumetricShadow = false;

		uint32 LightType = 0;
		FLightSceneInfo* LightSceneInfo = nullptr;
		const FVisibleLightInfo* VisibleLightInfo = nullptr;
		if (!LightSceneInfoCompact.IsEmpty())
		{
			LightType = LightSceneInfoCompact[PassIndex].LightType;
			LightSceneInfo = LightSceneInfoCompact[PassIndex].LightSceneInfo;
			check(LightSceneInfo != nullptr);

			bool bDynamicallyShadowed = false;
			if (LightSceneInfo)
			{
				VisibleLightInfo = &VisibleLightInfos[LightSceneInfo->Id];
				bCastsVolumetricShadow = LightSceneInfo->Proxy && LightSceneInfo->Proxy->CastsVolumetricShadow();
				bDynamicallyShadowed = HeterogeneousVolumes::IsDynamicShadow(VisibleLightInfo);
			}

			TRDGUniformBufferRef<FAdaptiveVolumetricShadowMapUniformBufferParameters> AdaptiveVolumetricShadowMapUniformBuffer;
			bool bCreateShadowMap = bShouldRenderShadowMaps && bCastsVolumetricShadow && bDynamicallyShadowed && !ShouldRenderRayTracingShadowsForLight(LightSceneInfoCompact[PassIndex]);
			if (bCreateShadowMap)
			{
				FString LightName;
				FSceneRenderer::GetLightNameForDrawEvent(LightSceneInfo->Proxy, LightName);
				RDG_EVENT_SCOPE(GraphBuilder, "%s", *LightName);

				FRDGTextureDesc Desc = SceneTextures.Color.Target->Desc;
				Desc.Format = PF_FloatRGBA;
				Desc.Flags &= ~(TexCreate_FastVRAM);
				FRDGTextureRef BeerShadowMapTexture = GraphBuilder.CreateTexture(Desc, TEXT("BeerShadowMapTexture"));
				AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(BeerShadowMapTexture), FLinearColor::Transparent);

				bool bIsDirectionalLight = false;
				FVector3f TranslatedWorldOrigin = FVector3f::Zero();
				FVector4f TranslatedWorldPlane = FVector4f::Zero();
				FMatrix44f TranslatedWorldToShadow[] =
				{
					FMatrix44f::Identity,
					FMatrix44f::Identity,
					FMatrix44f::Identity,
					FMatrix44f::Identity,
					FMatrix44f::Identity,
					FMatrix44f::Identity
				};
				FIntVector GroupCount = FIntVector::ZeroValue;
				int32 NumShadowMatrices = 0;
				FIntPoint VolumetricShadowMapResolution = FIntPoint::NoneValue;
				uint32 VolumetricShadowMapMaxSampleCount = 0;
				FRDGBufferRef VolumetricShadowMapLinkedListBuffer;
				bool bIsCreated = RenderVolumetricShadowMapForLightWithLiveShading(
					GraphBuilder,
					// Scene data
					SceneTextures,
					Scene,
					ViewFamily,
					View,
					// Light data
					LightType,
					LightSceneInfo,
					VisibleLightInfo,
					// Output
					bIsDirectionalLight,
					TranslatedWorldOrigin,
					TranslatedWorldPlane,
					TranslatedWorldToShadow,
					GroupCount,
					NumShadowMatrices,
					VolumetricShadowMapResolution,
					VolumetricShadowMapMaxSampleCount,
					BeerShadowMapTexture,
					VolumetricShadowMapLinkedListBuffer
				);

				if (bIsCreated)
				{
					FRDGBufferRef VolumetricShadowMapIndirectionBuffer;
					FRDGBufferRef VolumetricShadowMapSampleBuffer;
					CompressVolumetricShadowMap(
						GraphBuilder,
						View,
						GroupCount,
						VolumetricShadowMapResolution,
						VolumetricShadowMapMaxSampleCount,
						VolumetricShadowMapLinkedListBuffer,
						VolumetricShadowMapIndirectionBuffer,
						VolumetricShadowMapSampleBuffer
					);

					CreateAdaptiveVolumetricShadowMapUniformBuffer(
						GraphBuilder,
						TranslatedWorldOrigin,
						TranslatedWorldPlane,
						TranslatedWorldToShadow,
						VolumetricShadowMapResolution,
						NumShadowMatrices,
						VolumetricShadowMapMaxSampleCount,
						bIsDirectionalLight,
						VolumetricShadowMapLinkedListBuffer,
						VolumetricShadowMapIndirectionBuffer,
						VolumetricShadowMapSampleBuffer,
						AdaptiveVolumetricShadowMapUniformBuffer
					);
				}
				else
				{
					AdaptiveVolumetricShadowMapUniformBuffer = HeterogeneousVolumes::CreateEmptyAdaptiveVolumetricShadowMapUniformBuffer(GraphBuilder);
				}
			}
			else
			{
				AdaptiveVolumetricShadowMapUniformBuffer = HeterogeneousVolumes::CreateEmptyAdaptiveVolumetricShadowMapUniformBuffer(GraphBuilder);
			}

			if (View.ViewState)
			{
				TRDGUniformBufferRef<FAdaptiveVolumetricShadowMapUniformBufferParameters>& AdaptiveVolumetricShadowMap = View.ViewState->AdaptiveVolumetricShadowMapUniformBufferMap.FindOrAdd(LightSceneInfo->Id);
				AdaptiveVolumetricShadowMap = AdaptiveVolumetricShadowMapUniformBuffer;
			}
		}
	}
}

void RenderAdaptiveVolumetricCameraMapWithLiveShading(
	FRDGBuilder& GraphBuilder,
	// Scene data
	const FSceneTextures& SceneTextures,
	FScene* Scene,
	const FSceneViewFamily& ViewFamily,
	FViewInfo& View
)
{
	if (View.ViewState == nullptr)
	{
		return;
	}
	RDG_EVENT_SCOPE(GraphBuilder, "Adaptive Volumetric Camera Map");

	TRDGUniformBufferRef<FAdaptiveVolumetricShadowMapUniformBufferParameters> AdaptiveVolumetricShadowMapUniformBuffer;

	// Collect all volumes for view
	bool bCollectForShadowCasting = false;
	TSet<FVolumetricMeshBatch> HeterogeneousVolumesMeshBatches;
	FBoxSphereBounds WorldVolumeBounds;
	CollectHeterogeneousVolumeMeshBatchesForView(View, bCollectForShadowCasting, HeterogeneousVolumesMeshBatches, WorldVolumeBounds);

	bool bShouldRenderCameraMap = !View.ViewRect.IsEmpty() && !HeterogeneousVolumesMeshBatches.IsEmpty();
	if (bShouldRenderCameraMap)
	{
		// Resolution
		FIntPoint ShadowMapResolution = FIntPoint(View.ViewRect.Width(), View.ViewRect.Height());
		float DownsampleFactor = HeterogeneousVolumes::GetCameraDownsampleFactor();
		ShadowMapResolution.X = FMath::Max(ShadowMapResolution.X / DownsampleFactor, 1);
		ShadowMapResolution.Y = FMath::Max(ShadowMapResolution.Y / DownsampleFactor, 1);

		// Transform
		FMatrix ViewToClip = FPerspectiveMatrix(
			FMath::DegreesToRadians(View.FOV * 0.5),
			ShadowMapResolution.X,
			ShadowMapResolution.Y,
			1.0,
			HeterogeneousVolumes::GetMaxTraceDistance()
		);
		FMatrix ClipToView = ViewToClip.Inverse();
		FMatrix ScreenMatrix = FScaleMatrix(FVector(0.5, -0.5, -0.5)) * FTranslationMatrix(FVector(0.5, 0.5, 0.5));

		int32 NumShadowMatrices = 1;
		FMatrix44f TranslatedWorldToShadow[] = {
			FMatrix44f(View.ViewMatrices.GetTranslatedViewMatrix() * ViewToClip * ScreenMatrix)
		};
		FMatrix44f ShadowToTranslatedWorld[] = {
			TranslatedWorldToShadow[0].Inverse()
		};
		FVector3f TranslatedWorldOrigin = ShadowToTranslatedWorld[0].GetOrigin();

		// Dispatch
		FIntVector GroupCount = FIntVector(1);
		GroupCount.X = FMath::DivideAndRoundUp(ShadowMapResolution.X, FRenderVolumetricShadowMapForLightWithLiveShadingCS::GetThreadGroupSize2D());
		GroupCount.Y = FMath::DivideAndRoundUp(ShadowMapResolution.Y, FRenderVolumetricShadowMapForLightWithLiveShadingCS::GetThreadGroupSize2D());

		// Visualization Texture
		FRDGTextureDesc Desc = SceneTextures.Color.Target->Desc;
		Desc.Format = PF_FloatRGBA;
		Desc.Flags &= ~(TexCreate_FastVRAM);
		FRDGTextureRef BeerShadowMapTexture = GraphBuilder.CreateTexture(Desc, TEXT("BeerShadowMapTexture"));
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(BeerShadowMapTexture), FLinearColor::Transparent);

		auto VolumeMeshBatchItr = HeterogeneousVolumesMeshBatches.begin();
		int32 MaxSampleCount = HeterogeneousVolumes::GetShadowMaxSampleCount();
		int32 VolumetricShadowLinkedListElementCount = ShadowMapResolution.X * ShadowMapResolution.Y * MaxSampleCount;

		FRDGBufferRef VolumetricShadowLinkedListBuffer = GraphBuilder.CreateBuffer(
			FRDGBufferDesc::CreateStructuredDesc(sizeof(FAVSMLinkedListPackedData), VolumetricShadowLinkedListElementCount),
			TEXT("HeterogeneousVolume.VolumetricShadowLinkedListBuffer")
		);

		// Build a camera shadow for one volume
		int32 LightType = 0;
		FLightSceneInfo* LightSceneInfo = nullptr;
		FVisibleLightInfo* VisibleLightInfo = nullptr;
		RenderVolumetricShadowMapForLightForHeterogeneousVolumeWithLiveShading(
			GraphBuilder,
			SceneTextures,
			Scene,
			ViewFamily,
			View,
			// Light Info
			LightType,
			LightSceneInfo,
			VisibleLightInfo,
			// Shadow Info
			TranslatedWorldOrigin,
			NumShadowMatrices,
			TranslatedWorldToShadow,
			ShadowToTranslatedWorld,
			ShadowMapResolution,
			MaxSampleCount,
			// Volume
			*VolumeMeshBatchItr,
			// Dispatch
			GroupCount,
			// Output
			BeerShadowMapTexture,
			VolumetricShadowLinkedListBuffer
		);

		// Iterate over volumes, combining each into the existing shadow map
		++VolumeMeshBatchItr;
		for (; VolumeMeshBatchItr != HeterogeneousVolumesMeshBatches.end(); ++VolumeMeshBatchItr)
		{
			FRDGBufferRef VolumetricShadowLinkedListBuffer1 = GraphBuilder.CreateBuffer(
				FRDGBufferDesc::CreateStructuredDesc(sizeof(FAVSMLinkedListPackedData), VolumetricShadowLinkedListElementCount),
				TEXT("HeterogeneousVolume.VolumetricShadowLinkedListBuffer1")
			);

			RenderVolumetricShadowMapForLightForHeterogeneousVolumeWithLiveShading(
				GraphBuilder,
				SceneTextures,
				Scene,
				ViewFamily,
				View,
				// Light Info
				LightType,
				LightSceneInfo,
				VisibleLightInfo,
				// Shadow Info
				TranslatedWorldOrigin,
				NumShadowMatrices,
				TranslatedWorldToShadow,
				ShadowToTranslatedWorld,
				ShadowMapResolution,
				MaxSampleCount,
				// Volume
				*VolumeMeshBatchItr,
				// Dispatch
				GroupCount,
				// Output
				BeerShadowMapTexture,
				VolumetricShadowLinkedListBuffer1
			);

			FRDGBufferRef VolumetricShadowLinkedListBuffer2 = GraphBuilder.CreateBuffer(
				FRDGBufferDesc::CreateStructuredDesc(sizeof(FAVSMLinkedListPackedData), VolumetricShadowLinkedListElementCount),
				TEXT("HeterogeneousVolume.VolumetricShadowLinkedListBuffer2")
			);

			CombineVolumetricShadowMap(
				GraphBuilder,
				View,
				GroupCount,
				LightType,
				ShadowMapResolution,
				MaxSampleCount,
				VolumetricShadowLinkedListBuffer,
				VolumetricShadowLinkedListBuffer1,
				VolumetricShadowLinkedListBuffer2
			);

			VolumetricShadowLinkedListBuffer = VolumetricShadowLinkedListBuffer2;
		}

		FRDGBufferRef VolumetricShadowIndirectionBuffer = GSystemTextures.GetDefaultStructuredBuffer(GraphBuilder, sizeof(FAVSMIndirectionPackedData));
		FRDGBufferRef VolumetricShadowSampleBuffer = GSystemTextures.GetDefaultStructuredBuffer(GraphBuilder, sizeof(FAVSMSamplePackedData));
		CompressVolumetricShadowMap(
			GraphBuilder,
			View,
			GroupCount,
			ShadowMapResolution,
			MaxSampleCount,
			VolumetricShadowLinkedListBuffer,
			VolumetricShadowIndirectionBuffer,
			VolumetricShadowSampleBuffer
		);

		FVector4f TranslatedWorldPlane = FVector4f::Zero();
		bool bIsDirectionalLight = false;
		CreateAdaptiveVolumetricShadowMapUniformBuffer(
			GraphBuilder,
			TranslatedWorldOrigin,
			TranslatedWorldPlane,
			TranslatedWorldToShadow,
			ShadowMapResolution,
			NumShadowMatrices,
			MaxSampleCount,
			bIsDirectionalLight,
			VolumetricShadowLinkedListBuffer,
			VolumetricShadowIndirectionBuffer,
			VolumetricShadowSampleBuffer,
			AdaptiveVolumetricShadowMapUniformBuffer
		);
	}
	else
	{
		AdaptiveVolumetricShadowMapUniformBuffer = HeterogeneousVolumes::CreateEmptyAdaptiveVolumetricShadowMapUniformBuffer(GraphBuilder);
	}

	View.ViewState->AdaptiveVolumetricCameraMapUniformBuffer = AdaptiveVolumetricShadowMapUniformBuffer;
}