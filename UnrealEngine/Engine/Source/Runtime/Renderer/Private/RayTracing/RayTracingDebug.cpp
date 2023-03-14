// Copyright Epic Games, Inc. All Rights Reserved.

#include "RHI.h"
#include "ScenePrivate.h"
#include "ScreenPass.h"

#if RHI_RAYTRACING

#include "DeferredShadingRenderer.h"
#include "GlobalShader.h"
#include "PostProcess/SceneRenderTargets.h"
#include "RenderGraphBuilder.h"
#include "SceneUtils.h"
#include "RaytracingDebugDefinitions.h"
#include "RayTracingDebugTypes.h"
#include "RayTracing/RayTracingLighting.h"
#include "RayTracing/RaytracingOptions.h"
#include "RayTracing/RayTracingTraversalStatistics.h"

#define LOCTEXT_NAMESPACE "RayTracingDebugVisualizationMenuCommands"

DECLARE_GPU_STAT(RayTracingDebug);

static TAutoConsoleVariable<FString> CVarRayTracingDebugMode(
	TEXT("r.RayTracing.DebugVisualizationMode"),
	TEXT(""),
	TEXT("Sets the ray tracing debug visualization mode (default = None - Driven by viewport menu) .\n")
	);

TAutoConsoleVariable<int32> CVarRayTracingDebugPickerDomain(
	TEXT("r.RayTracing.Debug.PickerDomain"),
	0,
	TEXT("Changes the picker domain to highlight:\n")
	TEXT("0 - Triangles (default)\n")
	TEXT("1 - Instances\n"),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarRayTracingDebugModeOpaqueOnly(
	TEXT("r.RayTracing.DebugVisualizationMode.OpaqueOnly"),
	1,
	TEXT("Sets whether the view mode rendes opaque objects only (default = 1, render only opaque objects, 0 = render all objects)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarRayTracingDebugTimingScale(
	TEXT("r.RayTracing.DebugTimingScale"),
	1.0f,
	TEXT("Scaling factor for ray timing heat map visualization. (default = 1)\n")
);

static TAutoConsoleVariable<float> CVarRayTracingDebugTraversalBoxScale(
	TEXT("r.RayTracing.DebugTraversalScale.Box"),
	150.0f,
	TEXT("Scaling factor for box traversal heat map visualization. (default = 150)\n")
);

static TAutoConsoleVariable<float> CVarRayTracingDebugTraversalClusterScale(
	TEXT("r.RayTracing.DebugTraversalScale.Cluster"),
	2500.0f,
	TEXT("Scaling factor for cluster traversal heat map visualization. (default = 2500)\n")
);

static TAutoConsoleVariable<float> CVarRayTracingDebugTraversalTriangleScale(
	TEXT("r.RayTracing.DebugTraversalScale.Triangle"),
	30.0f,
	TEXT("Scaling factor for triangle traversal heat map visualization. (default = 30)\n")
);

static int32 GVisualizeProceduralPrimitives = 0;
static FAutoConsoleVariableRef CVarVisualizeProceduralPrimitives(
	TEXT("r.RayTracing.DebugVisualizationMode.ProceduralPrimitives"),
	GVisualizeProceduralPrimitives,
	TEXT("Whether to include procedural primitives in visualization modes.\n")
	TEXT("Currently only supports Nanite primitives in inline barycentrics mode."),
	ECVF_RenderThreadSafe
);

class FRayTracingDebugRGS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRayTracingDebugRGS)
	SHADER_USE_ROOT_PARAMETER_STRUCT(FRayTracingDebugRGS, FGlobalShader)

	class FEnablePicking : SHADER_PERMUTATION_BOOL("ENABLE_PICKING");
	class FEnableInstanceDebugData : SHADER_PERMUTATION_BOOL("ENABLE_INSTANCE_DEBUG_DATA");
	using FPermutationDomain = TShaderPermutationDomain<FEnablePicking, FEnableInstanceDebugData>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, VisualizationMode)
		SHADER_PARAMETER(uint32, UseDebugCHS)
		SHADER_PARAMETER(uint32, PickerDomain)
		SHADER_PARAMETER(int32, ShouldUsePreExposure)
		SHADER_PARAMETER(float, TimingScale)
		SHADER_PARAMETER(float, MaxTraceDistance)
		SHADER_PARAMETER(float, FarFieldMaxTraceDistance)
		SHADER_PARAMETER(FVector3f, FarFieldReferencePos)
		SHADER_PARAMETER(int32, OpaqueOnly)
		SHADER_PARAMETER_SRV(RaytracingAccelerationStructure, TLAS)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, Output)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, InstancesDebugData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FRayTracingPickingFeedback>, PickingBuffer)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}
};
IMPLEMENT_GLOBAL_SHADER(FRayTracingDebugRGS, "/Engine/Private/RayTracing/RayTracingDebug.usf", "RayTracingDebugMainRGS", SF_RayGen);

class FRayTracingDebugCHS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRayTracingDebugCHS);

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	FRayTracingDebugCHS() = default;
	FRayTracingDebugCHS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{}
};
IMPLEMENT_SHADER_TYPE(, FRayTracingDebugCHS, TEXT("/Engine/Private/RayTracing/RayTracingDebug.usf"), TEXT("RayTracingDebugMainCHS"), SF_RayHitGroup);

class FRayTracingDebugTraversalCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRayTracingDebugTraversalCS)
	SHADER_USE_PARAMETER_STRUCT(FRayTracingDebugTraversalCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, Output)
		SHADER_PARAMETER_SRV(RaytracingAccelerationStructure, TLAS)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FNaniteUniformParameters, NaniteUniformBuffer)
		SHADER_PARAMETER_STRUCT_INCLUDE(RaytracingTraversalStatistics::FShaderParameters, TraversalStatistics)

		SHADER_PARAMETER(uint32, VisualizationMode)
		SHADER_PARAMETER(float, TraversalBoxScale)
		SHADER_PARAMETER(float, TraversalClusterScale)
		SHADER_PARAMETER(float, TraversalTriangleScale)

		SHADER_PARAMETER(float, RTDebugVisualizationNaniteCutError)
	END_SHADER_PARAMETER_STRUCT()

	class FSupportProceduralPrimitive : SHADER_PERMUTATION_BOOL("ENABLE_TRACE_RAY_INLINE_PROCEDURAL_PRIMITIVE");
	class FPrintTraversalStatistics : SHADER_PERMUTATION_BOOL("PRINT_TRAVERSAL_STATISTICS");
	using FPermutationDomain = TShaderPermutationDomain<FSupportProceduralPrimitive, FPrintTraversalStatistics>;

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
		OutEnvironment.CompilerFlags.Add(CFLAG_InlineRayTracing);

		OutEnvironment.SetDefine(TEXT("INLINE_RAY_TRACING_THREAD_GROUP_SIZE_X"), ThreadGroupSizeX);
		OutEnvironment.SetDefine(TEXT("INLINE_RAY_TRACING_THREAD_GROUP_SIZE_Y"), ThreadGroupSizeY);
		OutEnvironment.SetDefine(TEXT("ENABLE_TRACE_RAY_INLINE_TRAVERSAL_STATISTICS"), 1);

		OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_PRIMITIVE_SCENE_DATA"), 1);
		OutEnvironment.SetDefine(TEXT("NANITE_USE_UNIFORM_BUFFER"), 1);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		bool bTraversalStats = PermutationVector.Get<FPrintTraversalStatistics>();
		bool bSupportsTraversalStats = FDataDrivenShaderPlatformInfo::GetSupportsRayTracingTraversalStatistics(Parameters.Platform);
		if (bTraversalStats && !bSupportsTraversalStats)
		{
			return false;
		}

		return IsRayTracingEnabledForProject(Parameters.Platform) && RHISupportsRayTracing(Parameters.Platform) && RHISupportsInlineRayTracing(Parameters.Platform);
	}

	static constexpr uint32 ThreadGroupSizeX = 8;
	static constexpr uint32 ThreadGroupSizeY = 4;
	static_assert(ThreadGroupSizeX*ThreadGroupSizeY == 32, "Current inline ray tracing implementation requires 1:1 mapping between thread groups and waves and only supports wave32 mode.");
};
IMPLEMENT_GLOBAL_SHADER(FRayTracingDebugTraversalCS, "/Engine/Private/RayTracing/RayTracingDebugTraversal.usf", "RayTracingDebugTraversalCS", SF_Compute);

class FRayTracingPickingRGS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRayTracingPickingRGS)
	SHADER_USE_ROOT_PARAMETER_STRUCT(FRayTracingPickingRGS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_SRV(RaytracingAccelerationStructure, TLAS)
		SHADER_PARAMETER(int32, OpaqueOnly)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer, PickingOutput)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, InstancesDebugData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, InstanceBuffer)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}
};
IMPLEMENT_GLOBAL_SHADER(FRayTracingPickingRGS, "/Engine/Private/RayTracing/RayTracingDebug.usf", "RayTracingDebugPickingRGS", SF_RayGen);

struct FRayTracingDebugResources : public FRenderResource
{
	const int32 MaxPickingBuffers = 4;
	int32 PickingBufferWriteIndex = 0;
	int32 PickingBufferNumPending = 0;
	TArray<FRHIGPUBufferReadback*> PickingBuffers;

	virtual void InitRHI() override
	{
		PickingBuffers.AddZeroed(MaxPickingBuffers);
	}

	virtual void ReleaseRHI() override
	{
		for (int32 BufferIndex = 0; BufferIndex < PickingBuffers.Num(); ++BufferIndex)
		{
			if (PickingBuffers[BufferIndex])
			{
				delete PickingBuffers[BufferIndex];
				PickingBuffers[BufferIndex] = nullptr;
			}
		}

		PickingBuffers.Reset();
	}
};

TGlobalResource<FRayTracingDebugResources> GRayTracingDebugResources;

void BindRayTracingDebugCHSMaterialBindings(FRHICommandList& RHICmdList, const FViewInfo& View, FRayTracingPipelineState* PipelineState)
{
	const int32 NumTotalBindings = View.VisibleRayTracingMeshCommands.Num();

	FSceneRenderingBulkObjectAllocator Allocator;

	auto Alloc = [&](uint32 Size, uint32 Align)
	{
		return RHICmdList.Bypass()
			? Allocator.Malloc(Size, Align)
			: RHICmdList.Alloc(Size, Align);
	};

	const uint32 MergedBindingsSize = sizeof(FRayTracingLocalShaderBindings) * NumTotalBindings;
	FRayTracingLocalShaderBindings* Bindings = (FRayTracingLocalShaderBindings*)Alloc(MergedBindingsSize, alignof(FRayTracingLocalShaderBindings));

	const uint32 NumUniformBuffers = 1;
	FRHIUniformBuffer** UniformBufferArray = (FRHIUniformBuffer**)Alloc(sizeof(FRHIUniformBuffer*) * NumUniformBuffers, alignof(FRHIUniformBuffer*));
	UniformBufferArray[0] = View.ViewUniformBuffer.GetReference();

	uint32 BindingIndex = 0;
	for (const FVisibleRayTracingMeshCommand VisibleMeshCommand : View.VisibleRayTracingMeshCommands)
	{
		const FRayTracingMeshCommand& MeshCommand = *VisibleMeshCommand.RayTracingMeshCommand;

		FRayTracingLocalShaderBindings Binding = {};
		Binding.InstanceIndex = VisibleMeshCommand.InstanceIndex;
		Binding.SegmentIndex = MeshCommand.GeometrySegmentIndex;
		Binding.UniformBuffers = UniformBufferArray;
		Binding.NumUniformBuffers = NumUniformBuffers;
		Binding.UserData = VisibleMeshCommand.InstanceIndex;

		Bindings[BindingIndex] = Binding;
		BindingIndex++;
	}

	const bool bCopyDataToInlineStorage = false; // Storage is already allocated from RHICmdList, no extra copy necessary
	RHICmdList.SetRayTracingHitGroups(
		View.GetRayTracingSceneChecked(),
		PipelineState,
		NumTotalBindings, Bindings,
		bCopyDataToInlineStorage);
}

static bool RequiresRayTracingDebugCHS(uint32 DebugVisualizationMode)
{
	return DebugVisualizationMode == RAY_TRACING_DEBUG_VIZ_INSTANCES || 
		DebugVisualizationMode == RAY_TRACING_DEBUG_VIZ_TRIANGLES ||
		DebugVisualizationMode == RAY_TRACING_DEBUG_VIZ_DYNAMIC_INSTANCES ||
		DebugVisualizationMode == RAY_TRACING_DEBUG_VIZ_PROXY_TYPE ||
		DebugVisualizationMode == RAY_TRACING_DEBUG_VIZ_PICKER;
}

static bool IsRayTracingDebugTraversalMode(uint32 DebugVisualizationMode)
{
	return DebugVisualizationMode == RAY_TRACING_DEBUG_VIZ_TRAVERSAL_NODE ||
		DebugVisualizationMode == RAY_TRACING_DEBUG_VIZ_TRAVERSAL_CLUSTER ||
		DebugVisualizationMode == RAY_TRACING_DEBUG_VIZ_TRAVERSAL_TRIANGLE ||
		DebugVisualizationMode == RAY_TRACING_DEBUG_VIZ_TRAVERSAL_ALL ||
		DebugVisualizationMode == RAY_TRACING_DEBUG_VIZ_TRAVERSAL_STATISTICS;
}

static bool IsRayTracingPickingEnabled(uint32 DebugVisualizationMode)
{
	return DebugVisualizationMode == RAY_TRACING_DEBUG_VIZ_PICKER;
}

void FDeferredShadingSceneRenderer::PrepareRayTracingDebug(const FSceneViewFamily& ViewFamily, TArray<FRHIRayTracingShader*>& OutRayGenShaders)
{
	// Declare all RayGen shaders that require material closest hit shaders to be bound
	bool bEnabled = ViewFamily.EngineShowFlags.RayTracingDebug && ShouldRenderRayTracingEffect(ERayTracingPipelineCompatibilityFlags::FullPipeline);
	if (bEnabled)
	{
		for (int32 EnablePicking = 0; EnablePicking < 2; ++EnablePicking)
		{
			for (int32 EnableInstanceDebugData = 0; EnableInstanceDebugData < 2; ++EnableInstanceDebugData)
			{
				FRayTracingDebugRGS::FPermutationDomain PermutationVector;
				PermutationVector.Set<FRayTracingDebugRGS::FEnablePicking>((bool)EnablePicking);
				PermutationVector.Set<FRayTracingDebugRGS::FEnableInstanceDebugData>((bool)EnableInstanceDebugData);

				auto RayGenShader = GetGlobalShaderMap(ViewFamily.GetShaderPlatform())->GetShader<FRayTracingDebugRGS>(PermutationVector);
				OutRayGenShaders.Add(RayGenShader.GetRayTracingShader());
			}			
		}		

		auto PickingRayGenShader = GetGlobalShaderMap(ViewFamily.GetShaderPlatform())->GetShader<FRayTracingPickingRGS>();
		OutRayGenShaders.Add(PickingRayGenShader.GetRayTracingShader());
	}
}

static FRDGBufferRef RayTracingPerformPicking(FRDGBuilder& GraphBuilder, const FScene* Scene, const FViewInfo& View, FRayTracingPickingFeedback& PickingFeedback)
{
	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	auto RayGenShader = ShaderMap->GetShader<FRayTracingPickingRGS>();

	FRayTracingPipelineStateInitializer Initializer;

	FRHIRayTracingShader* RayGenShaderTable[] = { RayGenShader.GetRayTracingShader() };
	Initializer.SetRayGenShaderTable(RayGenShaderTable);

	auto ClosestHitShader = ShaderMap->GetShader<FRayTracingDebugCHS>();
	FRHIRayTracingShader* HitGroupTable[] = { ClosestHitShader.GetRayTracingShader() };
	Initializer.SetHitGroupTable(HitGroupTable);
	Initializer.bAllowHitGroupIndexing = true; // Required for stable output using GetBaseInstanceIndex().
	Initializer.MaxPayloadSizeInBytes = RAY_TRACING_MAX_ALLOWED_PAYLOAD_SIZE;
	// TODO(UE-157946): This pipeline does not bind any miss shader and relies on the pipeline to do this automatically. This should be made explicit.
	FRayTracingPipelineState* PickingPipeline = PipelineStateCache::GetAndOrCreateRayTracingPipelineState(GraphBuilder.RHICmdList, Initializer);

	FRDGBufferDesc PickingBufferDesc = FRDGBufferDesc::CreateStructuredDesc(sizeof(FRayTracingPickingFeedback), 1);
	PickingBufferDesc.Usage = EBufferUsageFlags(PickingBufferDesc.Usage | BUF_SourceCopy);
	FRDGBufferRef PickingBuffer = GraphBuilder.CreateBuffer(PickingBufferDesc, TEXT("RayTracingDebug.PickingBuffer"));

	FRayTracingPickingRGS::FParameters* RayGenParameters = GraphBuilder.AllocParameters<FRayTracingPickingRGS::FParameters>();
	RayGenParameters->InstancesDebugData = GraphBuilder.CreateSRV(Scene->RayTracingScene.InstanceDebugBuffer);
	RayGenParameters->TLAS = Scene->RayTracingScene.GetLayerSRVChecked(ERayTracingSceneLayer::Base);
	RayGenParameters->OpaqueOnly = CVarRayTracingDebugModeOpaqueOnly.GetValueOnRenderThread();
	RayGenParameters->InstanceBuffer = GraphBuilder.CreateSRV(Scene->RayTracingScene.InstanceBuffer);
	RayGenParameters->ViewUniformBuffer = View.ViewUniformBuffer;
	RayGenParameters->PickingOutput = GraphBuilder.CreateUAV(PickingBuffer);

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("RayTracingPicking"),
		RayGenParameters,
		ERDGPassFlags::Compute,
		[RayGenParameters, RayGenShader, &View, PickingPipeline](FRHIRayTracingCommandList& RHICmdList)
		{
			FRayTracingShaderBindingsWriter GlobalResources;
			SetShaderParameters(GlobalResources, RayGenShader, *RayGenParameters);

			BindRayTracingDebugCHSMaterialBindings(RHICmdList, View, PickingPipeline);
			RHICmdList.SetRayTracingMissShader(View.GetRayTracingSceneChecked(), 0, PickingPipeline, 0 /* ShaderIndexInPipeline */, 0, nullptr, 0);

			RHICmdList.RayTraceDispatch(PickingPipeline, RayGenShader.GetRayTracingShader(), View.GetRayTracingSceneChecked(), GlobalResources, 1, 1);
		});

	const int32 MaxPickingBuffers = GRayTracingDebugResources.MaxPickingBuffers;

	int32& PickingBufferWriteIndex = GRayTracingDebugResources.PickingBufferWriteIndex;
	int32& PickingBufferNumPending = GRayTracingDebugResources.PickingBufferNumPending;

	TArray<FRHIGPUBufferReadback*>& PickingBuffers = GRayTracingDebugResources.PickingBuffers;

	{
		FRHIGPUBufferReadback* LatestPickingBuffer = nullptr;

		// Find latest buffer that is ready
		while (PickingBufferNumPending > 0)
		{
			uint32 Index = (PickingBufferWriteIndex + MaxPickingBuffers - PickingBufferNumPending) % MaxPickingBuffers;
			if (PickingBuffers[Index]->IsReady())
			{
				--PickingBufferNumPending;
				LatestPickingBuffer = PickingBuffers[Index];
			}
			else
			{
				break;
			}
		}

		if (LatestPickingBuffer != nullptr)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(LockBuffer);
			PickingFeedback = *((const FRayTracingPickingFeedback*)LatestPickingBuffer->Lock(sizeof(FRayTracingPickingFeedback)));
			LatestPickingBuffer->Unlock();
		}
	}

	// Skip when queue is full. It is NOT safe to EnqueueCopy on a buffer that already has a pending copy
	if (PickingBufferNumPending != MaxPickingBuffers)
	{
		if (PickingBuffers[PickingBufferWriteIndex] == nullptr)
		{
			FRHIGPUBufferReadback* GPUBufferReadback = new FRHIGPUBufferReadback(TEXT("RayTracingDebug.PickingFeedback"));
			PickingBuffers[PickingBufferWriteIndex] = GPUBufferReadback;
		}

		FRHIGPUBufferReadback* PickingReadback = PickingBuffers[PickingBufferWriteIndex];

		AddEnqueueCopyPass(GraphBuilder, PickingReadback, PickingBuffer, 0u);

		PickingBufferWriteIndex = (PickingBufferWriteIndex + 1) % MaxPickingBuffers;
		PickingBufferNumPending = FMath::Min(PickingBufferNumPending + 1, MaxPickingBuffers);
	}	

	return PickingBuffer;
}

void FDeferredShadingSceneRenderer::RenderRayTracingDebug(FRDGBuilder& GraphBuilder, const FViewInfo& View, FRDGTextureRef SceneColorTexture, FRayTracingPickingFeedback& PickingFeedback)
{
	static TMap<FName, uint32> RayTracingDebugVisualizationModes;
	if (RayTracingDebugVisualizationModes.Num() == 0)
	{
		RayTracingDebugVisualizationModes.Emplace(FName(*LOCTEXT("Radiance", "Radiance").ToString()),											RAY_TRACING_DEBUG_VIZ_RADIANCE);
		RayTracingDebugVisualizationModes.Emplace(FName(*LOCTEXT("World Normal", "World Normal").ToString()),									RAY_TRACING_DEBUG_VIZ_WORLD_NORMAL);
		RayTracingDebugVisualizationModes.Emplace(FName(*LOCTEXT("BaseColor", "BaseColor").ToString()),											RAY_TRACING_DEBUG_VIZ_BASE_COLOR);
		RayTracingDebugVisualizationModes.Emplace(FName(*LOCTEXT("DiffuseColor", "DiffuseColor").ToString()),									RAY_TRACING_DEBUG_VIZ_DIFFUSE_COLOR);
		RayTracingDebugVisualizationModes.Emplace(FName(*LOCTEXT("SpecularColor", "SpecularColor").ToString()),									RAY_TRACING_DEBUG_VIZ_SPECULAR_COLOR);
		RayTracingDebugVisualizationModes.Emplace(FName(*LOCTEXT("Opacity", "Opacity").ToString()),												RAY_TRACING_DEBUG_VIZ_OPACITY);
		RayTracingDebugVisualizationModes.Emplace(FName(*LOCTEXT("Metallic", "Metallic").ToString()),											RAY_TRACING_DEBUG_VIZ_METALLIC);
		RayTracingDebugVisualizationModes.Emplace(FName(*LOCTEXT("Specular", "Specular").ToString()),											RAY_TRACING_DEBUG_VIZ_SPECULAR);
		RayTracingDebugVisualizationModes.Emplace(FName(*LOCTEXT("Roughness", "Roughness").ToString()),											RAY_TRACING_DEBUG_VIZ_ROUGHNESS);
		RayTracingDebugVisualizationModes.Emplace(FName(*LOCTEXT("Ior", "Ior").ToString()),														RAY_TRACING_DEBUG_VIZ_IOR);
		RayTracingDebugVisualizationModes.Emplace(FName(*LOCTEXT("ShadingModelID", "ShadingModelID").ToString()),								RAY_TRACING_DEBUG_VIZ_SHADING_MODEL);
		RayTracingDebugVisualizationModes.Emplace(FName(*LOCTEXT("BlendingMode", "BlendingMode").ToString()),									RAY_TRACING_DEBUG_VIZ_BLENDING_MODE);
		RayTracingDebugVisualizationModes.Emplace(FName(*LOCTEXT("PrimitiveLightingChannelMask", "PrimitiveLightingChannelMask").ToString()),	RAY_TRACING_DEBUG_VIZ_LIGHTING_CHANNEL_MASK);
		RayTracingDebugVisualizationModes.Emplace(FName(*LOCTEXT("CustomData", "CustomData").ToString()),										RAY_TRACING_DEBUG_VIZ_CUSTOM_DATA);
		RayTracingDebugVisualizationModes.Emplace(FName(*LOCTEXT("GBufferAO", "GBufferAO").ToString()),											RAY_TRACING_DEBUG_VIZ_GBUFFER_AO);
		RayTracingDebugVisualizationModes.Emplace(FName(*LOCTEXT("IndirectIrradiance", "IndirectIrradiance").ToString()),						RAY_TRACING_DEBUG_VIZ_INDIRECT_IRRADIANCE);
		RayTracingDebugVisualizationModes.Emplace(FName(*LOCTEXT("World Position", "World Position").ToString()),								RAY_TRACING_DEBUG_VIZ_WORLD_POSITION);
		RayTracingDebugVisualizationModes.Emplace(FName(*LOCTEXT("HitKind", "HitKind").ToString()),												RAY_TRACING_DEBUG_VIZ_HITKIND);
		RayTracingDebugVisualizationModes.Emplace(FName(*LOCTEXT("Barycentrics", "Barycentrics").ToString()),									RAY_TRACING_DEBUG_VIZ_BARYCENTRICS);
		RayTracingDebugVisualizationModes.Emplace(FName(*LOCTEXT("PrimaryRays", "PrimaryRays").ToString()),										RAY_TRACING_DEBUG_VIZ_PRIMARY_RAYS);
		RayTracingDebugVisualizationModes.Emplace(FName(*LOCTEXT("World Tangent", "World Tangent").ToString()),									RAY_TRACING_DEBUG_VIZ_WORLD_TANGENT);
		RayTracingDebugVisualizationModes.Emplace(FName(*LOCTEXT("Anisotropy", "Anisotropy").ToString()),										RAY_TRACING_DEBUG_VIZ_ANISOTROPY);
		RayTracingDebugVisualizationModes.Emplace(FName(*LOCTEXT("Instances", "Instances").ToString()),											RAY_TRACING_DEBUG_VIZ_INSTANCES);
		RayTracingDebugVisualizationModes.Emplace(FName(*LOCTEXT("Performance", "Performance").ToString()),										RAY_TRACING_DEBUG_VIZ_PERFORMANCE);
		RayTracingDebugVisualizationModes.Emplace(FName(*LOCTEXT("Triangles", "Triangles").ToString()),											RAY_TRACING_DEBUG_VIZ_TRIANGLES);
		RayTracingDebugVisualizationModes.Emplace(FName(*LOCTEXT("FarField", "FarField").ToString()),											RAY_TRACING_DEBUG_VIZ_FAR_FIELD);
		RayTracingDebugVisualizationModes.Emplace(FName(*LOCTEXT("Traversal Node", "Traversal Node").ToString()),								RAY_TRACING_DEBUG_VIZ_TRAVERSAL_NODE);
		RayTracingDebugVisualizationModes.Emplace(FName(*LOCTEXT("Traversal Cluster", "Traversal Cluster").ToString()),							RAY_TRACING_DEBUG_VIZ_TRAVERSAL_CLUSTER);
		RayTracingDebugVisualizationModes.Emplace(FName(*LOCTEXT("Traversal Triangle", "Traversal Triangle").ToString()),						RAY_TRACING_DEBUG_VIZ_TRAVERSAL_TRIANGLE);
		RayTracingDebugVisualizationModes.Emplace(FName(*LOCTEXT("Traversal All", "Traversal All").ToString()),									RAY_TRACING_DEBUG_VIZ_TRAVERSAL_ALL);
		RayTracingDebugVisualizationModes.Emplace(FName(*LOCTEXT("Traversal Statistics", "Traversal Statistics").ToString()),					RAY_TRACING_DEBUG_VIZ_TRAVERSAL_STATISTICS);
		RayTracingDebugVisualizationModes.Emplace(FName(*LOCTEXT("Dynamic Instances", "Dynamic Instances").ToString()),							RAY_TRACING_DEBUG_VIZ_DYNAMIC_INSTANCES);
		RayTracingDebugVisualizationModes.Emplace(FName(*LOCTEXT("Proxy Type", "Proxy Type").ToString()),										RAY_TRACING_DEBUG_VIZ_PROXY_TYPE);
		RayTracingDebugVisualizationModes.Emplace(FName(*LOCTEXT("Picker", "Picker").ToString()),												RAY_TRACING_DEBUG_VIZ_PICKER);
	}

	uint32 DebugVisualizationMode;
	
	FString ConsoleViewMode = CVarRayTracingDebugMode.GetValueOnRenderThread();

	if (!ConsoleViewMode.IsEmpty())
	{
		DebugVisualizationMode = RayTracingDebugVisualizationModes.FindRef(FName(*ConsoleViewMode));
	}
	else if(View.CurrentRayTracingDebugVisualizationMode != NAME_None)
	{
		DebugVisualizationMode = RayTracingDebugVisualizationModes.FindRef(View.CurrentRayTracingDebugVisualizationMode);
	}
	else
	{
		// Set useful default value
		DebugVisualizationMode = RAY_TRACING_DEBUG_VIZ_BASE_COLOR;
	}

	if (DebugVisualizationMode == RAY_TRACING_DEBUG_VIZ_BARYCENTRICS)
	{
		return RenderRayTracingBarycentrics(GraphBuilder, View, SceneColorTexture, (bool)GVisualizeProceduralPrimitives);
	}

	if (IsRayTracingDebugTraversalMode(DebugVisualizationMode) && ShouldRenderRayTracingEffect(ERayTracingPipelineCompatibilityFlags::Inline))
	{
		const bool bPrintTraversalStats = FDataDrivenShaderPlatformInfo::GetSupportsRayTracingTraversalStatistics(GMaxRHIShaderPlatform)
			&& (DebugVisualizationMode == RAY_TRACING_DEBUG_VIZ_TRAVERSAL_STATISTICS);

		FRayTracingDebugTraversalCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FRayTracingDebugTraversalCS::FParameters>();
		PassParameters->Output = GraphBuilder.CreateUAV(SceneColorTexture);
		PassParameters->TLAS = Scene->RayTracingScene.GetLayerSRVChecked(ERayTracingSceneLayer::Base);
		PassParameters->ViewUniformBuffer = View.ViewUniformBuffer;
		PassParameters->NaniteUniformBuffer = CreateDebugNaniteUniformBuffer(GraphBuilder, Scene->GPUScene.InstanceSceneDataSOAStride);

		PassParameters->VisualizationMode = DebugVisualizationMode;
		PassParameters->TraversalBoxScale = CVarRayTracingDebugTraversalBoxScale.GetValueOnAnyThread();
		PassParameters->TraversalClusterScale = CVarRayTracingDebugTraversalClusterScale.GetValueOnAnyThread();
		PassParameters->TraversalTriangleScale = CVarRayTracingDebugTraversalTriangleScale.GetValueOnAnyThread();

		PassParameters->RTDebugVisualizationNaniteCutError = 0.0f;

		RaytracingTraversalStatistics::FTraceRayInlineStatisticsData TraversalData;
		if (bPrintTraversalStats)
		{
			RaytracingTraversalStatistics::Init(GraphBuilder, TraversalData);
			RaytracingTraversalStatistics::SetParameters(GraphBuilder, TraversalData, PassParameters->TraversalStatistics);
		}

		FIntRect ViewRect = View.ViewRect;

		RDG_GPU_STAT_SCOPE(GraphBuilder, RayTracingDebug);

		const FIntPoint GroupSize(FRayTracingDebugTraversalCS::ThreadGroupSizeX, FRayTracingDebugTraversalCS::ThreadGroupSizeY);
		const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(ViewRect.Size(), GroupSize);

		FRayTracingDebugTraversalCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FRayTracingDebugTraversalCS::FSupportProceduralPrimitive>((bool)GVisualizeProceduralPrimitives);
		PermutationVector.Set<FRayTracingDebugTraversalCS::FPrintTraversalStatistics>(bPrintTraversalStats);
		
		TShaderRef<FRayTracingDebugTraversalCS> ComputeShader = View.ShaderMap->GetShader<FRayTracingDebugTraversalCS>(PermutationVector);

		FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("RayTracingDebug"), ComputeShader, PassParameters, GroupCount);

		if (bPrintTraversalStats)
		{
			RaytracingTraversalStatistics::AddPrintPass(GraphBuilder, View, TraversalData);
		}

		return;
	}

	// Debug modes other than barycentrics and traversal require pipeline support.
	const bool bRayTracingPipeline = ShouldRenderRayTracingEffect(ERayTracingPipelineCompatibilityFlags::FullPipeline);
	if (!bRayTracingPipeline)
	{
		return;
	}

	if (DebugVisualizationMode == RAY_TRACING_DEBUG_VIZ_PRIMARY_RAYS) 
	{
		FRDGTextureRef OutputColor = nullptr;
		FRDGTextureRef HitDistanceTexture = nullptr;

		RenderRayTracingPrimaryRaysView(
				GraphBuilder, View, &OutputColor, &HitDistanceTexture, 1, 1, 1,
			ERayTracingPrimaryRaysFlag::PrimaryView);

		AddDrawTexturePass(GraphBuilder, View, OutputColor, SceneColorTexture, View.ViewRect.Min, View.ViewRect.Min, View.ViewRect.Size());
		return;
	}

	FRDGBufferRef PickingBuffer = nullptr;
	if (IsRayTracingPickingEnabled(DebugVisualizationMode) && Scene->RayTracingScene.InstanceDebugBuffer != nullptr)
	{
		PickingBuffer = RayTracingPerformPicking(GraphBuilder, Scene, View, PickingFeedback);
	}

	FRayTracingDebugRGS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FRayTracingDebugRGS::FEnablePicking>(PickingBuffer != nullptr);
	PermutationVector.Set<FRayTracingDebugRGS::FEnableInstanceDebugData>(Scene->RayTracingScene.InstanceDebugBuffer != nullptr);

	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(FeatureLevel);
	auto RayGenShader = ShaderMap->GetShader<FRayTracingDebugRGS>(PermutationVector);

	FRayTracingPipelineState* Pipeline = View.RayTracingMaterialPipeline;
	bool bRequiresBindings = false;

	const bool bRequiresDebugCHS = RequiresRayTracingDebugCHS(DebugVisualizationMode);
	if (bRequiresDebugCHS)
	{
		FRayTracingPipelineStateInitializer Initializer;

		FRHIRayTracingShader* RayGenShaderTable[] = { RayGenShader.GetRayTracingShader() };
		Initializer.SetRayGenShaderTable(RayGenShaderTable);

		auto ClosestHitShader = ShaderMap->GetShader<FRayTracingDebugCHS>();
		FRHIRayTracingShader* HitGroupTable[] = { ClosestHitShader.GetRayTracingShader() };
		Initializer.SetHitGroupTable(HitGroupTable);
		Initializer.bAllowHitGroupIndexing = true; // Required for stable output using GetBaseInstanceIndex().
		Initializer.MaxPayloadSizeInBytes = RAY_TRACING_MAX_ALLOWED_PAYLOAD_SIZE;
		// TODO(UE-157946): This pipeline does not bind any miss shader and relies on the pipeline to do this automatically. This should be made explicit.
		Pipeline = PipelineStateCache::GetAndOrCreateRayTracingPipelineState(GraphBuilder.RHICmdList, Initializer);
		bRequiresBindings = true;
	}

	FRayTracingDebugRGS::FParameters* RayGenParameters = GraphBuilder.AllocParameters<FRayTracingDebugRGS::FParameters>();

	RayGenParameters->VisualizationMode = DebugVisualizationMode;
	RayGenParameters->UseDebugCHS = bRequiresDebugCHS;
	RayGenParameters->PickerDomain = CVarRayTracingDebugPickerDomain.GetValueOnRenderThread();
	RayGenParameters->ShouldUsePreExposure = View.Family->EngineShowFlags.Tonemapper;
	RayGenParameters->TimingScale = CVarRayTracingDebugTimingScale.GetValueOnAnyThread() / 25000.0f;
	RayGenParameters->OpaqueOnly = CVarRayTracingDebugModeOpaqueOnly.GetValueOnRenderThread();
	
	if (Scene->RayTracingScene.InstanceDebugBuffer)
	{
		RayGenParameters->InstancesDebugData = GraphBuilder.CreateSRV(Scene->RayTracingScene.InstanceDebugBuffer);
	}

	if (PickingBuffer)
	{		
		RayGenParameters->PickingBuffer = GraphBuilder.CreateSRV(PickingBuffer);
	}	

	if (Lumen::UseFarField(ViewFamily))
	{
		RayGenParameters->MaxTraceDistance = Lumen::GetMaxTraceDistance(View);
		RayGenParameters->FarFieldMaxTraceDistance = Lumen::GetFarFieldMaxTraceDistance();
		RayGenParameters->FarFieldReferencePos = (FVector3f)Lumen::GetFarFieldReferencePos();	// LWC_TODO: Precision Loss
	}
	else
	{
		RayGenParameters->MaxTraceDistance = 0.0f;
		RayGenParameters->FarFieldMaxTraceDistance = 0.0f;
		RayGenParameters->FarFieldReferencePos = FVector3f(0.0f);
	}
	
	RayGenParameters->TLAS = Scene->RayTracingScene.GetLayerSRVChecked(ERayTracingSceneLayer::Base);
	RayGenParameters->ViewUniformBuffer = View.ViewUniformBuffer;
	RayGenParameters->Output = GraphBuilder.CreateUAV(SceneColorTexture);

	FIntRect ViewRect = View.ViewRect;

	RDG_GPU_STAT_SCOPE(GraphBuilder, RayTracingDebug);

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("RayTracingDebug"),
		RayGenParameters,
		ERDGPassFlags::Compute,
		[this, RayGenParameters, RayGenShader, &View, Pipeline, ViewRect, bRequiresBindings](FRHIRayTracingCommandList& RHICmdList)
	{
		FRayTracingShaderBindingsWriter GlobalResources;
		SetShaderParameters(GlobalResources, RayGenShader, *RayGenParameters);

		if (bRequiresBindings)
		{
			BindRayTracingDebugCHSMaterialBindings(RHICmdList, View, Pipeline);
			RHICmdList.SetRayTracingMissShader(View.GetRayTracingSceneChecked(), 0, Pipeline, 0 /* ShaderIndexInPipeline */, 0, nullptr, 0);
		}

		RHICmdList.RayTraceDispatch(Pipeline, RayGenShader.GetRayTracingShader(), View.GetRayTracingSceneChecked(), GlobalResources, ViewRect.Size().X, ViewRect.Size().Y);
	});
}

void FDeferredShadingSceneRenderer::RayTracingDisplayPicking(const FRayTracingPickingFeedback& PickingFeedback, FScreenMessageWriter& Writer)
{
	if (PickingFeedback.InstanceIndex == ~uint32(0))
	{
		return;
	}
	
	int32 PickerDomain = CVarRayTracingDebugPickerDomain.GetValueOnRenderThread();
	switch (PickerDomain)
	{
	case RAY_TRACING_DEBUG_PICKER_DOMAIN_TRIANGLE:
		Writer.DrawLine(FText::FromString(TEXT("Domain [Triangle]")), 10, FColor::Yellow);
		break;

	case RAY_TRACING_DEBUG_PICKER_DOMAIN_SEGMENT:
		Writer.DrawLine(FText::FromString(TEXT("Domain [Segment]")), 10, FColor::Yellow);
		break;

	case RAY_TRACING_DEBUG_PICKER_DOMAIN_INSTANCE:
		Writer.DrawLine(FText::FromString(TEXT("Domain [Instance]")), 10, FColor::Yellow);
		break;

	default:
		break; // Invalid picking domain
	}

	Writer.EmptyLine();

	Writer.DrawLine(FText::FromString(TEXT("(Use r.RayTracing.Debug.PickerDomain to change domain)")), 10, FColor::Yellow);

	Writer.EmptyLine();

	Writer.DrawLine(FText::FromString(TEXT("[Hit]")), 10, FColor::Yellow);	

	Writer.EmptyLine();

	Writer.DrawLine(FText::FromString(FString::Printf(TEXT("Instance Index: %u"), PickingFeedback.InstanceIndex)), 10, FColor::Yellow);
	Writer.DrawLine(FText::FromString(FString::Printf(TEXT("Geometry Instance Index: %u"), PickingFeedback.GeometryInstanceIndex)), 10, FColor::Yellow);
	Writer.DrawLine(FText::FromString(FString::Printf(TEXT("Segment Index: %u"), PickingFeedback.GeometryIndex)), 10, FColor::Yellow);
	Writer.DrawLine(FText::FromString(FString::Printf(TEXT("Triangle Index: %u"), PickingFeedback.TriangleIndex)), 10, FColor::Yellow);

	Writer.EmptyLine();

	FRHIRayTracingGeometry* Geometry = nullptr;
	for (const FRayTracingGeometryInstance& Instance : Scene->RayTracingScene.Instances)
	{
		if (Instance.GeometryRHI)
		{
			const uint64 GeometryAddress = uint64(Instance.GeometryRHI.GetReference());
			if (PickingFeedback.GeometryAddress == GeometryAddress)
			{
				Geometry = Instance.GeometryRHI;
				break;
			}
		}
	}
	
	Writer.DrawLine(FText::FromString(TEXT("[BLAS]")), 10, FColor::Yellow);
	Writer.EmptyLine();

	if (Geometry)
	{
		const FRayTracingGeometryInitializer& Initializer = Geometry->GetInitializer();
		Writer.DrawLine(FText::FromString(FString::Printf(TEXT("Name: %s"), *Initializer.DebugName.ToString())), 10, FColor::Yellow);
		Writer.DrawLine(FText::FromString(FString::Printf(TEXT("Num Segments: %d"), Initializer.Segments.Num())), 10, FColor::Yellow);
		Writer.DrawLine(FText::FromString(FString::Printf(TEXT("Total Primitive Count: %u"), Initializer.TotalPrimitiveCount)), 10, FColor::Yellow);
		Writer.DrawLine(FText::FromString(FString::Printf(TEXT("Fast Build: %d"), Initializer.bFastBuild)), 10, FColor::Yellow);
		Writer.DrawLine(FText::FromString(FString::Printf(TEXT("Allow Update: %d"), Initializer.bAllowUpdate)), 10, FColor::Yellow);
		Writer.DrawLine(FText::FromString(FString::Printf(TEXT("Allow Compaction: %d"), Initializer.bAllowCompaction)), 10, FColor::Yellow);

		Writer.EmptyLine();

		FRayTracingAccelerationStructureSize SizeInfo = Geometry->GetSizeInfo();
		Writer.DrawLine(FText::FromString(FString::Printf(TEXT("Result Size: %u"), SizeInfo.ResultSize)), 10, FColor::Yellow);
		Writer.DrawLine(FText::FromString(FString::Printf(TEXT("Build Scratch Size: %u"), SizeInfo.BuildScratchSize)), 10, FColor::Yellow);
		Writer.DrawLine(FText::FromString(FString::Printf(TEXT("Update Scratch Size: %u"), SizeInfo.UpdateScratchSize)), 10, FColor::Yellow);
	}
	else
	{
		Writer.DrawLine(FText::FromString(TEXT("UNKNOWN")), 10, FColor::Yellow);
	}	

	Writer.EmptyLine();

	Writer.DrawLine(FText::FromString(TEXT("[TLAS]")), 10, FColor::Yellow);

	Writer.EmptyLine();

	Writer.DrawLine(FText::FromString(FString::Printf(TEXT("InstanceId: %u"), PickingFeedback.InstanceId)), 10, FColor::Yellow);
	Writer.DrawLine(FText::FromString(FString::Printf(TEXT("Mask: %u"), PickingFeedback.Mask)), 10, FColor::Yellow);
	Writer.DrawLine(FText::FromString(FString::Printf(TEXT("ContributionToHitGroup: %u"), PickingFeedback.InstanceContributionToHitGroupIndex)), 10, FColor::Yellow);
	{
		const ERayTracingInstanceFlags Flags = (ERayTracingInstanceFlags)PickingFeedback.Flags;
		FString FlagNames(TEXT(""));
		if (EnumHasAnyFlags(Flags, ERayTracingInstanceFlags::TriangleCullDisable))
		{
			FlagNames += FString(TEXT("CullDisable "));
		}

		if (EnumHasAnyFlags(Flags, ERayTracingInstanceFlags::TriangleCullReverse))
		{
			FlagNames += FString(TEXT("CullReverse "));
		}

		if (EnumHasAnyFlags(Flags, ERayTracingInstanceFlags::ForceOpaque))
		{
			FlagNames += FString(TEXT("ForceOpaque "));
		}

		if (EnumHasAnyFlags(Flags, ERayTracingInstanceFlags::ForceNonOpaque))
		{
			FlagNames += FString(TEXT("ForceNonOpaque "));
		}

		Writer.DrawLine(FText::FromString(FString::Printf(TEXT("Flags: %u - %s"), PickingFeedback.Flags, *FlagNames)), 10, FColor::Yellow);
	}	

	Writer.EmptyLine();
}

bool IsRayTracingInstanceDebugDataEnabled(const FViewInfo& View)
{
#if !UE_BUILD_SHIPPING	
	static TArray<FName> RayTracingDebugVisualizationModes;
	if (RayTracingDebugVisualizationModes.Num() == 0)
	{
		RayTracingDebugVisualizationModes.Add(*LOCTEXT("Dynamic Instances", "Dynamic Instances").ToString());
		RayTracingDebugVisualizationModes.Add(*LOCTEXT("Proxy Type", "Proxy Type").ToString());
		RayTracingDebugVisualizationModes.Add(*LOCTEXT("Picker", "Picker").ToString());
	}

	FString ConsoleViewMode = CVarRayTracingDebugMode.GetValueOnRenderThread();
	if (!ConsoleViewMode.IsEmpty())
	{
		return RayTracingDebugVisualizationModes.Contains(FName(*ConsoleViewMode));
	}
	else if (View.CurrentRayTracingDebugVisualizationMode != NAME_None)
	{
		return RayTracingDebugVisualizationModes.Contains(View.CurrentRayTracingDebugVisualizationMode);
	}
#endif
	{
		return false;
	}
}

#undef LOCTEXT_NAMESPACE

#endif
