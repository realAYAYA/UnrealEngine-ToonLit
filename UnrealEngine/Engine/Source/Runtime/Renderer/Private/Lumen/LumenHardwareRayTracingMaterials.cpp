// Copyright Epic Games, Inc. All Rights Reserved.

#include "RHIDefinitions.h"

#if RHI_RAYTRACING

#include "RenderCore.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RayTracing/RayTracingScene.h"
#include "RenderGraphUtils.h"
#include "DeferredShadingRenderer.h"
#include "PipelineStateCache.h"
#include "ScenePrivate.h"
#include "ShaderCompilerCore.h"
#include "Lumen/LumenHardwareRayTracingCommon.h"
#include "Nanite/NaniteRayTracing.h"
#include "Lumen/LumenReflections.h"

static TAutoConsoleVariable<float> CVarLumenHardwareRayTracingSkipBackFaceHitDistance(
	TEXT("r.Lumen.HardwareRayTracing.SkipBackFaceHitDistance"),
	5.0f,
	TEXT("Distance to trace with backface culling enabled, useful when the Ray Tracing geometry doesn't match the GBuffer (Nanite Proxy geometry)."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarLumenHardwareRayTracingSkipTwoSidedHitDistance(
	TEXT("r.Lumen.HardwareRayTracing.SkipTwoSidedHitDistance"),
	1.0f,
	TEXT("When the SkipBackFaceHitDistance is enabled, the first two-sided material hit within this distance will be skipped. This is useful for avoiding self-intersections with the Nanite fallback mesh on foliage, as SkipBackFaceHitDistance doesn't work on two sided materials."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

namespace LumenHardwareRayTracing
{
	// 0 - hit group with AVOID_SELF_INTERSECTIONS=0
	// 1 - hit group with AVOID_SELF_INTERSECTIONS=1
	constexpr uint32 NumHitGroups = 2;
};

IMPLEMENT_RT_PAYLOAD_TYPE(ERayTracingPayloadType::LumenMinimal, 16);

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FLumenHardwareRayTracingUniformBufferParameters, "LumenHardwareRayTracingUniformBuffer");

class FLumenHardwareRayTracingMaterialHitGroup : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLumenHardwareRayTracingMaterialHitGroup)
	SHADER_USE_ROOT_PARAMETER_STRUCT(FLumenHardwareRayTracingMaterialHitGroup, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FLumenHardwareRayTracingUniformBufferParameters, LumenHardwareRayTracingUniformBuffer)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT_REF(FNaniteRayTracingUniformParameters, NaniteRayTracing)
		SHADER_PARAMETER_STRUCT_REF(FSceneUniformParameters, Scene)
	END_SHADER_PARAMETER_STRUCT()

	class FAvoidSelfIntersections : SHADER_PERMUTATION_BOOL("AVOID_SELF_INTERSECTIONS");
	class FNaniteRayTracing : SHADER_PERMUTATION_BOOL("NANITE_RAY_TRACING");
	using FPermutationDomain = TShaderPermutationDomain<FAvoidSelfIntersections, FNaniteRayTracing>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform) && DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_PRIMITIVE_SCENE_DATA"), 1);
	}

	static ERayTracingPayloadType GetRayTracingPayloadType(const int32 PermutationId)
	{
		return ERayTracingPayloadType::LumenMinimal;
	}
};

IMPLEMENT_GLOBAL_SHADER(FLumenHardwareRayTracingMaterialHitGroup, "/Engine/Private/Lumen/LumenHardwareRayTracingMaterials.usf", "closesthit=LumenHardwareRayTracingMaterialCHS anyhit=LumenHardwareRayTracingMaterialAHS", SF_RayHitGroup);

class FLumenHardwareRayTracingMaterialMS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLumenHardwareRayTracingMaterialMS)
	SHADER_USE_ROOT_PARAMETER_STRUCT(FLumenHardwareRayTracingMaterialMS, FGlobalShader)

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform) && DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

	static ERayTracingPayloadType GetRayTracingPayloadType(const int32 PermutationId)
	{
		return ERayTracingPayloadType::LumenMinimal;
	}

	using FParameters = FEmptyShaderParameters;
};

IMPLEMENT_GLOBAL_SHADER(FLumenHardwareRayTracingMaterialMS, "/Engine/Private/Lumen/LumenHardwareRayTracingMaterials.usf", "LumenHardwareRayTracingMaterialMS", SF_RayMiss);

void FDeferredShadingSceneRenderer::SetupLumenHardwareRayTracingHitGroupBuffer(FRDGBuilder& GraphBuilder, FViewInfo& View)
{
	const FRayTracingSceneInitializer2& SceneInitializer = Scene->RayTracingScene.GetRHIRayTracingSceneChecked()->GetInitializer();
	const uint32 NumTotalSegments = FMath::Max(SceneInitializer.NumTotalSegments, 1u);

	const uint32 ElementCount = NumTotalSegments;
	const uint32 ElementSize = sizeof(Lumen::FHitGroupRootConstants);
	
	View.LumenHardwareRayTracingHitDataBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredUploadDesc(ElementSize, ElementCount), TEXT("LumenHardwareRayTracingHitDataBuffer"));
}

void FDeferredShadingSceneRenderer::SetupLumenHardwareRayTracingUniformBuffer(FRDGBuilder& GraphBuilder, FViewInfo& View)
{
	FLumenHardwareRayTracingUniformBufferParameters* LumenHardwareRayTracingUniformBufferParameters = GraphBuilder.AllocParameters<FLumenHardwareRayTracingUniformBufferParameters>();
	LumenHardwareRayTracingUniformBufferParameters->SkipBackFaceHitDistance = CVarLumenHardwareRayTracingSkipBackFaceHitDistance.GetValueOnRenderThread();
	LumenHardwareRayTracingUniformBufferParameters->SkipTwoSidedHitDistance = CVarLumenHardwareRayTracingSkipTwoSidedHitDistance.GetValueOnRenderThread();
	LumenHardwareRayTracingUniformBufferParameters->SkipTranslucent         = LumenReflections::UseTranslucentRayTracing(View) ? 0.0f : 1.0f;
	View.LumenHardwareRayTracingUniformBuffer = GraphBuilder.CreateUniformBuffer(LumenHardwareRayTracingUniformBufferParameters);
}

uint32 CalculateLumenHardwareRayTracingUserData(const FRayTracingMeshCommand& MeshCommand)
{
	return (MeshCommand.MaterialShaderIndex & LUMEN_MATERIAL_SHADER_INDEX_MASK)
		| (((MeshCommand.bCastRayTracedShadows != 0) & 0x01) << 29)
		| (((MeshCommand.bTwoSided != 0) & 0x01) << 30)
		| (((MeshCommand.bIsTranslucent != 0) & 0x01) << 31);
}

// TODO: This should be moved into FRayTracingScene and used as a base for other effects. There is not need for it to be Lumen specific.
void FDeferredShadingSceneRenderer::BuildLumenHardwareRayTracingHitGroupData(FRHICommandListBase& RHICmdList, FRayTracingScene& RayTracingScene, const FViewInfo& ReferenceView, FRDGBufferRef DstBuffer)
{
	Lumen::FHitGroupRootConstants* DstBasePtr = (Lumen::FHitGroupRootConstants*)RHICmdList.LockBuffer(DstBuffer->GetRHI(), 0, DstBuffer->GetSize(), RLM_WriteOnly);

	const FRayTracingSceneInitializer2& SceneInitializer = RayTracingScene.GetRHIRayTracingSceneChecked()->GetInitializer();

	for (const FVisibleRayTracingMeshCommand VisibleMeshCommand : ReferenceView.VisibleRayTracingMeshCommands)
	{
		const FRayTracingMeshCommand& MeshCommand = *VisibleMeshCommand.RayTracingMeshCommand;

		const uint32 InstanceIndex = VisibleMeshCommand.InstanceIndex;
		const uint32 SegmentIndex = MeshCommand.GeometrySegmentIndex;

		const uint32 HitGroupIndex = SceneInitializer.SegmentPrefixSum[InstanceIndex] + SegmentIndex;

		DstBasePtr[HitGroupIndex].BaseInstanceIndex = SceneInitializer.BaseInstancePrefixSum[InstanceIndex];
		DstBasePtr[HitGroupIndex].UserData = CalculateLumenHardwareRayTracingUserData(MeshCommand);
	}

	RHICmdList.UnlockBuffer(DstBuffer->GetRHI());
}

FRayTracingLocalShaderBindings* FDeferredShadingSceneRenderer::BuildLumenHardwareRayTracingMaterialBindings(FRHICommandList& RHICmdList, const FViewInfo& View, FRHIUniformBuffer* SceneUniformBuffer)
{
	const FViewInfo& ReferenceView = Views[0];
	const int32 NumTotalBindings = LumenHardwareRayTracing::NumHitGroups * ReferenceView.VisibleRayTracingMeshCommands.Num();

	auto Alloc = [&](uint32 Size, uint32 Align)
	{
		return RHICmdList.Bypass()
			? Allocator.Malloc(Size, Align)
			: RHICmdList.Alloc(Size, Align);
	};

	const uint32 MergedBindingsSize = sizeof(FRayTracingLocalShaderBindings) * NumTotalBindings;
	FRayTracingLocalShaderBindings* Bindings = (FRayTracingLocalShaderBindings*)Alloc(MergedBindingsSize, alignof(FRayTracingLocalShaderBindings));

	struct FBinding
	{
		int32 ShaderIndexInPipeline;
		uint32 NumUniformBuffers;
		FRHIUniformBuffer** UniformBufferArray;
	};

	auto SetupBinding = [&](FLumenHardwareRayTracingMaterialHitGroup::FPermutationDomain PermutationVector)
	{
		auto Shader = View.ShaderMap->GetShader<FLumenHardwareRayTracingMaterialHitGroup>(PermutationVector);
		auto HitGroupShader = Shader.GetRayTracingShader();

		FBinding Binding;
		Binding.ShaderIndexInPipeline = FindRayTracingHitGroupIndex(View.LumenHardwareRayTracingMaterialPipeline, HitGroupShader, true);
		Binding.NumUniformBuffers = Shader->ParameterMapInfo.UniformBuffers.Num();
		Binding.UniformBufferArray = (FRHIUniformBuffer**)Alloc(sizeof(FRHIUniformBuffer*) * Binding.NumUniformBuffers, alignof(FRHIUniformBuffer*));

		const auto& LumenHardwareRayTracingUniformBufferParameter = Shader->GetUniformBufferParameter<FLumenHardwareRayTracingUniformBufferParameters>();
		const auto& ViewUniformBufferParameter = Shader->GetUniformBufferParameter<FViewUniformShaderParameters>();
		const auto& SceneUniformBufferParameter = Shader->GetUniformBufferParameter<FSceneUniformParameters>();
		const auto& NaniteUniformBufferParameter = Shader->GetUniformBufferParameter<FNaniteRayTracingUniformParameters>();

		if (LumenHardwareRayTracingUniformBufferParameter.IsBound())
		{
			check(LumenHardwareRayTracingUniformBufferParameter.GetBaseIndex() < Binding.NumUniformBuffers);
			Binding.UniformBufferArray[LumenHardwareRayTracingUniformBufferParameter.GetBaseIndex()] = View.LumenHardwareRayTracingUniformBuffer->GetRHI();
		}

		if (ViewUniformBufferParameter.IsBound())
		{
			check(ViewUniformBufferParameter.GetBaseIndex() < Binding.NumUniformBuffers);
			Binding.UniformBufferArray[ViewUniformBufferParameter.GetBaseIndex()] = View.ViewUniformBuffer.GetReference();
		}

		if (SceneUniformBufferParameter.IsBound())
		{
			check(SceneUniformBufferParameter.GetBaseIndex() < Binding.NumUniformBuffers);
			Binding.UniformBufferArray[SceneUniformBufferParameter.GetBaseIndex()] = SceneUniformBuffer;
		}

		if (NaniteUniformBufferParameter.IsBound())
		{
			check(NaniteUniformBufferParameter.GetBaseIndex() < Binding.NumUniformBuffers);
			Binding.UniformBufferArray[NaniteUniformBufferParameter.GetBaseIndex()] = Nanite::GRayTracingManager.GetUniformBuffer().GetReference();
		}

		return Binding;
	};

	FBinding ShaderBindings[LumenHardwareRayTracing::NumHitGroups];
	FBinding ShaderBindingsNaniteRT[LumenHardwareRayTracing::NumHitGroups];

	{
		FLumenHardwareRayTracingMaterialHitGroup::FPermutationDomain PermutationVector;

		{
			PermutationVector.Set<FLumenHardwareRayTracingMaterialHitGroup::FAvoidSelfIntersections>(false);
			PermutationVector.Set<FLumenHardwareRayTracingMaterialHitGroup::FNaniteRayTracing>(false);
			ShaderBindings[0] = SetupBinding(PermutationVector);

			PermutationVector.Set<FLumenHardwareRayTracingMaterialHitGroup::FAvoidSelfIntersections>(true);
			PermutationVector.Set<FLumenHardwareRayTracingMaterialHitGroup::FNaniteRayTracing>(false);
			ShaderBindings[1] = SetupBinding(PermutationVector);
		}

		{
			PermutationVector.Set<FLumenHardwareRayTracingMaterialHitGroup::FAvoidSelfIntersections>(false);
			PermutationVector.Set<FLumenHardwareRayTracingMaterialHitGroup::FNaniteRayTracing>(true);
			ShaderBindingsNaniteRT[0] = SetupBinding(PermutationVector);

			PermutationVector.Set<FLumenHardwareRayTracingMaterialHitGroup::FAvoidSelfIntersections>(true);
			PermutationVector.Set<FLumenHardwareRayTracingMaterialHitGroup::FNaniteRayTracing>(true);
			ShaderBindingsNaniteRT[1] = SetupBinding(PermutationVector);
		}
	}

	uint32 BindingIndex = 0;
	for (const FVisibleRayTracingMeshCommand VisibleMeshCommand : ReferenceView.VisibleRayTracingMeshCommands)
	{
		const FRayTracingMeshCommand& MeshCommand = *VisibleMeshCommand.RayTracingMeshCommand;

		for (uint32 HitGroupIndex = 0; HitGroupIndex < LumenHardwareRayTracing::NumHitGroups; ++HitGroupIndex)
		{
			const FBinding& LumenBinding = MeshCommand.IsUsingNaniteRayTracing() ? ShaderBindingsNaniteRT[HitGroupIndex] : ShaderBindings[HitGroupIndex];

			FRayTracingLocalShaderBindings Binding = {};
			Binding.ShaderSlot = HitGroupIndex;
			Binding.ShaderIndexInPipeline = LumenBinding.ShaderIndexInPipeline;
			Binding.InstanceIndex = VisibleMeshCommand.InstanceIndex;
			Binding.SegmentIndex = MeshCommand.GeometrySegmentIndex;
			Binding.UserData = CalculateLumenHardwareRayTracingUserData(MeshCommand);
			Binding.UniformBuffers = LumenBinding.UniformBufferArray;
			Binding.NumUniformBuffers = LumenBinding.NumUniformBuffers;

			Bindings[BindingIndex] = Binding;
			BindingIndex++;
		}
	}

	return Bindings;
}

FRayTracingPipelineState* FDeferredShadingSceneRenderer::CreateLumenHardwareRayTracingMaterialPipeline(FRHICommandList& RHICmdList, const FViewInfo& View, const TArrayView<FRHIRayTracingShader*>& RayGenShaderTable)
{
	SCOPE_CYCLE_COUNTER(STAT_BindRayTracingPipeline);
	
	FRayTracingPipelineStateInitializer Initializer;

	Initializer.SetRayGenShaderTable(RayGenShaderTable);

	Initializer.MaxPayloadSizeInBytes = GetRayTracingPayloadTypeMaxSize(ERayTracingPayloadType::LumenMinimal);

	// Get the ray tracing materials
	FLumenHardwareRayTracingMaterialHitGroup::FPermutationDomain PermutationVector;

	PermutationVector.Set<FLumenHardwareRayTracingMaterialHitGroup::FAvoidSelfIntersections>(false);
	PermutationVector.Set<FLumenHardwareRayTracingMaterialHitGroup::FNaniteRayTracing>(false);
	auto HitGroupShader = View.ShaderMap->GetShader<FLumenHardwareRayTracingMaterialHitGroup>(PermutationVector);

	PermutationVector.Set<FLumenHardwareRayTracingMaterialHitGroup::FAvoidSelfIntersections>(true);
	PermutationVector.Set<FLumenHardwareRayTracingMaterialHitGroup::FNaniteRayTracing>(false);
	auto HitGroupShaderWithAvoidSelfIntersections = View.ShaderMap->GetShader<FLumenHardwareRayTracingMaterialHitGroup>(PermutationVector);

	PermutationVector.Set<FLumenHardwareRayTracingMaterialHitGroup::FAvoidSelfIntersections>(false);
	PermutationVector.Set<FLumenHardwareRayTracingMaterialHitGroup::FNaniteRayTracing>(true);
	auto HitGroupShaderNaniteRT = View.ShaderMap->GetShader<FLumenHardwareRayTracingMaterialHitGroup>(PermutationVector);

	PermutationVector.Set<FLumenHardwareRayTracingMaterialHitGroup::FAvoidSelfIntersections>(true);
	PermutationVector.Set<FLumenHardwareRayTracingMaterialHitGroup::FNaniteRayTracing>(true);
	auto HitGroupShaderNaniteRTWithAvoidSelfIntersections = View.ShaderMap->GetShader<FLumenHardwareRayTracingMaterialHitGroup>(PermutationVector);

	FRHIRayTracingShader* HitShaderTable[] = {
		HitGroupShader.GetRayTracingShader(),
		HitGroupShaderWithAvoidSelfIntersections.GetRayTracingShader(),
		HitGroupShaderNaniteRT.GetRayTracingShader(),
		HitGroupShaderNaniteRTWithAvoidSelfIntersections.GetRayTracingShader()
	};
	Initializer.SetHitGroupTable(HitShaderTable);
	Initializer.bAllowHitGroupIndexing = true;

	auto MissShader = View.ShaderMap->GetShader<FLumenHardwareRayTracingMaterialMS>();
	FRHIRayTracingShader* MissShaderTable[] = { MissShader.GetRayTracingShader() };
	Initializer.SetMissShaderTable(MissShaderTable);

	FRayTracingPipelineState* PipelineState = PipelineStateCache::GetAndOrCreateRayTracingPipelineState(RHICmdList, Initializer);

	return PipelineState;
}

void FDeferredShadingSceneRenderer::BindLumenHardwareRayTracingMaterialPipeline(FRHICommandListImmediate& RHICmdList, const FViewInfo& View, FRHIUniformBuffer* SceneUniformBuffer, FRayTracingPipelineState* PipelineState)
{
	FRayTracingLocalShaderBindings* Bindings = BuildLumenHardwareRayTracingMaterialBindings(RHICmdList, View, SceneUniformBuffer);

	const int32 NumTotalBindings = LumenHardwareRayTracing::NumHitGroups * View.VisibleRayTracingMeshCommands.Num();	

	const bool bCopyDataToInlineStorage = false; // Storage is already allocated from RHICmdList, no extra copy necessary
	RHICmdList.SetRayTracingHitGroups(
		View.GetRayTracingSceneChecked(),
		PipelineState,
		NumTotalBindings,
		Bindings,
		bCopyDataToInlineStorage);
}

#endif // RHI_RAYTRACING