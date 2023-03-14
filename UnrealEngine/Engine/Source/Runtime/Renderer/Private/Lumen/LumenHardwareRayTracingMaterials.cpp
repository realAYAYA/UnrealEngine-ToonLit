// Copyright Epic Games, Inc. All Rights Reserved.

#include "RHIDefinitions.h"

#if RHI_RAYTRACING

#include "RenderCore.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphUtils.h"
#include "DeferredShadingRenderer.h"
#include "PipelineStateCache.h"
#include "ShaderCompilerCore.h"
#include "Lumen/LumenHardwareRayTracingCommon.h"

class FLumenHardwareRayTracingMaterialCHS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLumenHardwareRayTracingMaterialCHS)
	SHADER_USE_ROOT_PARAMETER_STRUCT(FLumenHardwareRayTracingMaterialCHS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform) && DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}
};

IMPLEMENT_GLOBAL_SHADER(FLumenHardwareRayTracingMaterialCHS, "/Engine/Private/Lumen/LumenHardwareRayTracingMaterials.usf", "LumenHardwareRayTracingMaterialCHS", SF_RayHitGroup);

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

	using FParameters = FEmptyShaderParameters;
};

IMPLEMENT_GLOBAL_SHADER(FLumenHardwareRayTracingMaterialMS, "/Engine/Private/Lumen/LumenHardwareRayTracingMaterials.usf", "LumenHardwareRayTracingMaterialMS", SF_RayMiss);

// TODO: This should be moved into FRayTracingScene and used as a base for other effects. There is not need for it to be Lumen specific.
void BuildHardwareRayTracingHitGroupData(FRHICommandList& RHICmdList, FRayTracingScene& RayTracingScene, TArrayView<FRayTracingLocalShaderBindings> Bindings, FRHIBuffer* DstBuffer)
{
	Lumen::FHitGroupRootConstants* DstBasePtr = (Lumen::FHitGroupRootConstants*)RHILockBuffer(DstBuffer, 0, DstBuffer->GetSize(), RLM_WriteOnly);

	const FRayTracingSceneInitializer2& SceneInitializer = RayTracingScene.GetRHIRayTracingSceneChecked()->GetInitializer();

	for (const FRayTracingLocalShaderBindings& Binding : Bindings)
	{
		const uint32 InstanceIndex = Binding.InstanceIndex;
		const uint32 SegmentIndex = Binding.SegmentIndex;

		const uint32 HitGroupIndex = SceneInitializer.SegmentPrefixSum[InstanceIndex] + SegmentIndex;

		DstBasePtr[HitGroupIndex].BaseInstanceIndex = SceneInitializer.BaseInstancePrefixSum[InstanceIndex];
		DstBasePtr[HitGroupIndex].UserData = Binding.UserData;
	}

	RHIUnlockBuffer(DstBuffer);
}

void FDeferredShadingSceneRenderer::SetupLumenHardwareRayTracingHitGroupBuffer(FViewInfo& View)
{
	const FRayTracingSceneInitializer2& SceneInitializer = Scene->RayTracingScene.GetRHIRayTracingSceneChecked()->GetInitializer();
	const uint32 NumTotalSegments = FMath::Max(SceneInitializer.NumTotalSegments, 1u);

	const uint32 ElementCount = NumTotalSegments;
	const uint32 ElementSize = sizeof(Lumen::FHitGroupRootConstants);

	FRHIResourceCreateInfo CreateInfo(TEXT("LumenHitGroupBuffer"));
	View.LumenHardwareRayTracingHitDataBuffer = RHICreateStructuredBuffer(ElementSize, ElementSize * ElementCount, BUF_ShaderResource | BUF_Static | BUF_KeepCPUAccessible, CreateInfo);
	View.LumenHardwareRayTracingHitDataBufferSRV = RHICreateShaderResourceView(View.LumenHardwareRayTracingHitDataBuffer);
}

FRayTracingLocalShaderBindings* FDeferredShadingSceneRenderer::BuildLumenHardwareRayTracingMaterialBindings(FRHICommandList& RHICmdList, const FViewInfo& View, FRHIBuffer* OutHitGroupDataBuffer, bool bInlineOnly)
{
	const FViewInfo& ReferenceView = Views[0];
	const int32 NumTotalBindings = ReferenceView.VisibleRayTracingMeshCommands.Num();

	auto Alloc = [&](uint32 Size, uint32 Align)
	{
		return RHICmdList.Bypass() || bInlineOnly
			? Allocator.Malloc(Size, Align)
			: RHICmdList.Alloc(Size, Align);
	};

	const uint32 MergedBindingsSize = sizeof(FRayTracingLocalShaderBindings) * NumTotalBindings;
	FRayTracingLocalShaderBindings* Bindings = (FRayTracingLocalShaderBindings*)Alloc(MergedBindingsSize, alignof(FRayTracingLocalShaderBindings));

	const uint32 NumUniformBuffers = 1;
	FRHIUniformBuffer** UniformBufferArray = (FRHIUniformBuffer**)Alloc(sizeof(FRHIUniformBuffer*) * NumUniformBuffers, alignof(FRHIUniformBuffer*));
	UniformBufferArray[0] = ReferenceView.ViewUniformBuffer.GetReference();

	uint32 BindingIndex = 0;
	for (const FVisibleRayTracingMeshCommand VisibleMeshCommand : ReferenceView.VisibleRayTracingMeshCommands)
	{
		const FRayTracingMeshCommand& MeshCommand = *VisibleMeshCommand.RayTracingMeshCommand;

		FRayTracingLocalShaderBindings Binding = {};
		Binding.InstanceIndex = VisibleMeshCommand.InstanceIndex;
		Binding.SegmentIndex = MeshCommand.GeometrySegmentIndex;
		uint32 PackedUserData = (MeshCommand.MaterialShaderIndex & 0x3FFFFFFF)
			| (((MeshCommand.bTwoSided != 0) & 0x01) << 30)
			| (((MeshCommand.bIsTranslucent != 0) & 0x01) << 31);
		Binding.UserData = PackedUserData;
		Binding.UniformBuffers = UniformBufferArray;
		Binding.NumUniformBuffers = NumUniformBuffers;

		Bindings[BindingIndex] = Binding;
		BindingIndex++;
	}

	if (OutHitGroupDataBuffer)
	{
		BuildHardwareRayTracingHitGroupData(RHICmdList, Scene->RayTracingScene, MakeArrayView(Bindings, NumTotalBindings), OutHitGroupDataBuffer);
	}

	return Bindings;
}

FRayTracingPipelineState* FDeferredShadingSceneRenderer::CreateLumenHardwareRayTracingMaterialPipeline(FRHICommandList& RHICmdList, const FViewInfo& View, const TArrayView<FRHIRayTracingShader*>& RayGenShaderTable)
{
	SCOPE_CYCLE_COUNTER(STAT_BindRayTracingPipeline);
	
	FRayTracingPipelineStateInitializer Initializer;

	Initializer.SetRayGenShaderTable(RayGenShaderTable);

	Initializer.MaxPayloadSizeInBytes = 20; // sizeof FLumenMinimalPayload

	// Get the ray tracing materials
	auto ClosestHitShader = View.ShaderMap->GetShader<FLumenHardwareRayTracingMaterialCHS>();
	FRHIRayTracingShader* HitShaderTable[] = { ClosestHitShader.GetRayTracingShader() };
	Initializer.SetHitGroupTable(HitShaderTable);
	Initializer.bAllowHitGroupIndexing = true;

	auto MissShader = View.ShaderMap->GetShader<FLumenHardwareRayTracingMaterialMS>();
	FRHIRayTracingShader* MissShaderTable[] = { MissShader.GetRayTracingShader() };
	Initializer.SetMissShaderTable(MissShaderTable);

	FRayTracingPipelineState* PipelineState = PipelineStateCache::GetAndOrCreateRayTracingPipelineState(RHICmdList, Initializer);

	return PipelineState;
}

void FDeferredShadingSceneRenderer::BindLumenHardwareRayTracingMaterialPipeline(FRHICommandListImmediate& RHICmdList, FRayTracingLocalShaderBindings* Bindings, const FViewInfo& View, FRayTracingPipelineState* PipelineState, FRHIBuffer* OutHitGroupDataBuffer)
{
	// If we haven't build bindings before we need to build them here
	if (Bindings == nullptr)
	{
		Bindings = BuildLumenHardwareRayTracingMaterialBindings(RHICmdList, View, OutHitGroupDataBuffer, false);
	}

	const int32 NumTotalBindings = View.VisibleRayTracingMeshCommands.Num();	

	const bool bCopyDataToInlineStorage = false; // Storage is already allocated from RHICmdList, no extra copy necessary
	RHICmdList.SetRayTracingHitGroups(
		View.GetRayTracingSceneChecked(),
		PipelineState,
		NumTotalBindings, Bindings,
		bCopyDataToInlineStorage);
}

#endif // RHI_RAYTRACING