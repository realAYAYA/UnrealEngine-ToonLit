// Copyright Epic Games, Inc. All Rights Reserved.

#include "HeterogeneousVolumes.h"

#include "PixelShaderUtils.h"
#include "RayTracingDefinitions.h"
#include "RayTracingInstance.h"
#include "RayTracingInstanceBufferUtil.h"
#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneManagement.h"

class FHeterogeneousVolumesBakeMaterialCS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FHeterogeneousVolumesBakeMaterialCS, MeshMaterial);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		// Scene data
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)

		// Object data
		SHADER_PARAMETER(FMatrix44f, LocalToWorld)
		SHADER_PARAMETER(FMatrix44f, WorldToLocal)
		SHADER_PARAMETER(FVector3f, LocalBoundsOrigin)
		SHADER_PARAMETER(FVector3f, LocalBoundsExtent)
		SHADER_PARAMETER(int32, PrimitiveId)

		// Volume data
		SHADER_PARAMETER(FIntVector, VolumeResolution)

		// Sample data
		SHADER_PARAMETER(int, bJitter)

		// Dispatch data
		SHADER_PARAMETER(FIntVector, GroupCount)

		// Output
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWExtinctionTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWEmissionTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWAlbedoTexture)
		//SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<Volumes::FDebugOutput>, RWDebugOutputBuffer)
	END_SHADER_PARAMETER_STRUCT()

	FHeterogeneousVolumesBakeMaterialCS() = default;

	FHeterogeneousVolumesBakeMaterialCS(
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
	}

	static bool ShouldCompilePermutation(
		const FMaterialShaderPermutationParameters& Parameters
	)
	{
		return DoesPlatformSupportHeterogeneousVolumes(Parameters.Platform)
			&& (Parameters.MaterialParameters.MaterialDomain == MD_Volume)
			// Restricting compilation to materials bound to Niagara meshes
			&& Parameters.MaterialParameters.bIsUsedWithNiagaraMeshParticles;
	}

	static FPermutationDomain RemapPermutation(FPermutationDomain PermutationVector)
	{
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
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_3D"), GetThreadGroupSize3D());

		// This shader takes a very long time to compile with FXC, so we pre-compile it with DXC first and then forward the optimized HLSL to FXC.
		OutEnvironment.CompilerFlags.Add(CFLAG_PrecompileWithDXC);
		OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);

		OutEnvironment.SetDefine(TEXT("GET_PRIMITIVE_DATA_OVERRIDE"), 1);
	}

	void SetParameters(
		FRHIComputeCommandList& RHICmdList,
		FRHIComputeShader* ShaderRHI,
		const FViewInfo& View,
		const FMaterialRenderProxy* MaterialProxy,
		const FMaterial& Material
	)
	{
		FMaterialShader::SetViewParameters(RHICmdList, ShaderRHI, View, View.ViewUniformBuffer);
		FMaterialShader::SetParameters(RHICmdList, ShaderRHI, MaterialProxy, Material, View);
	}

	static int32 GetThreadGroupSize1D() { return GetThreadGroupSize2D() * GetThreadGroupSize2D(); }
	static int32 GetThreadGroupSize2D() { return 8; }
	static int32 GetThreadGroupSize3D() { return 4; }
};

IMPLEMENT_MATERIAL_SHADER_TYPE(, FHeterogeneousVolumesBakeMaterialCS, TEXT("/Engine/Private/HeterogeneousVolumes/HeterogeneousVolumesMaterialBakingPipeline.usf"), TEXT("HeterogeneousVolumesBakeMaterialCS"), SF_Compute);

void ComputeHeterogeneousVolumeBakeMaterial(
	FRDGBuilder& GraphBuilder,
	// Scene data
	const FScene* Scene,
	const FViewInfo& View,
	// Object data
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	const FMaterialRenderProxy* MaterialRenderProxy,
	const int32 PrimitiveId,
	const FBoxSphereBounds LocalBoxSphereBounds,
	// Volume data
	FIntVector VolumeResolution,
	// Output
	FRDGTextureRef& HeterogeneousVolumeExtinctionTexture,
	FRDGTextureRef& HeterogeneousVolumeEmissionTexture,
	FRDGTextureRef& HeterogeneousVolumeAlbedoTexture
)
{
	const FMaterial& Material = MaterialRenderProxy->GetMaterialWithFallback(View.GetFeatureLevel(), MaterialRenderProxy);
	check(Material.GetMaterialDomain() == MD_Volume);

	// FMath::DivideAndRoundUp() is not compatible with FIntVector?
	FIntVector GroupSize = FIntVector(FHeterogeneousVolumesBakeMaterialCS::GetThreadGroupSize3D());
	FIntVector GroupCount = (VolumeResolution + GroupSize - FIntVector(1)) / FHeterogeneousVolumesBakeMaterialCS::GetThreadGroupSize3D();

#if 0
	FRDGBufferRef DebugOutputBuffer = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateStructuredDesc(sizeof(Volumes::FDebugOutput), GetVoxelCount(VolumeResolution)),
		TEXT("HeterogeneousVolumes.DebugOutputBuffer"));
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(DebugOutputBuffer), 0);
#endif

	FHeterogeneousVolumesBakeMaterialCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FHeterogeneousVolumesBakeMaterialCS::FParameters>();
	{
		// Scene data
		PassParameters->View = View.ViewUniformBuffer;

		// Object data
		FMatrix44f LocalToWorld = FMatrix44f(PrimitiveSceneProxy->GetLocalToWorld());
		PassParameters->LocalToWorld = LocalToWorld;
		PassParameters->WorldToLocal = LocalToWorld.Inverse();
		PassParameters->LocalBoundsOrigin = FVector3f(LocalBoxSphereBounds.Origin);
		PassParameters->LocalBoundsExtent = FVector3f(LocalBoxSphereBounds.BoxExtent);
		PassParameters->PrimitiveId = PrimitiveId;

		// Volume data
		PassParameters->VolumeResolution = VolumeResolution;

		// Sample data
		PassParameters->bJitter = HeterogeneousVolumes::ShouldJitter();

		// Dispatch data
		PassParameters->GroupCount = GroupCount;

		PassParameters->RWExtinctionTexture = GraphBuilder.CreateUAV(HeterogeneousVolumeExtinctionTexture);
		PassParameters->RWEmissionTexture = GraphBuilder.CreateUAV(HeterogeneousVolumeEmissionTexture);
		PassParameters->RWAlbedoTexture = GraphBuilder.CreateUAV(HeterogeneousVolumeAlbedoTexture);
		//PassParameters->RWDebugOutputBuffer = GraphBuilder.CreateUAV(DebugOutputBuffer);
	}

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("HeterogeneousVolumesBakedMaterialCS"),
		PassParameters,
		ERDGPassFlags::Compute,
		// Why is scene explicitly copied??
		[PassParameters, LocalScene = Scene, &View, MaterialRenderProxy, &Material, GroupCount](FRHIComputeCommandList& RHICmdList)
		{
			FHeterogeneousVolumesBakeMaterialCS::FPermutationDomain PermutationVector;
			TShaderRef<FHeterogeneousVolumesBakeMaterialCS> ComputeShader = Material.GetShader<FHeterogeneousVolumesBakeMaterialCS>(&FLocalVertexFactory::StaticType, PermutationVector, false);

			if (!ComputeShader.IsNull())
			{
				FMeshPassProcessorRenderState DrawRenderState;

				FMeshMaterialShaderElementData ShaderElementData;
				ShaderElementData.FadeUniformBuffer = GDistanceCullFadedInUniformBuffer.GetUniformBufferRHI();
				ShaderElementData.DitherUniformBuffer = GDitherFadedInUniformBuffer.GetUniformBufferRHI();

				FMeshProcessorShaders PassShaders;
				PassShaders.ComputeShader = ComputeShader;

				FMeshDrawShaderBindings ShaderBindings;
				{
					ShaderBindings.Initialize(PassShaders);

					int32 DataOffset = 0;
					FMeshDrawSingleShaderBindings SingleShaderBindings = ShaderBindings.GetSingleShaderBindings(SF_Compute, DataOffset);
					ComputeShader->GetShaderBindings(LocalScene, LocalScene->GetFeatureLevel(), nullptr, *MaterialRenderProxy, Material, DrawRenderState, ShaderElementData, SingleShaderBindings);

					ShaderBindings.Finalize(&PassShaders);
				}
				ShaderBindings.SetOnCommandList(RHICmdList, ComputeShader.GetComputeShader());

				FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *PassParameters, GroupCount);
				//FComputeShaderUtils::DispatchIndirect(RHICmdList, ComputeShader, *PassParameters, IndirectArgs->GetIndirectRHICallBuffer(), IndirectOffset);
			}
		}
	);
}
