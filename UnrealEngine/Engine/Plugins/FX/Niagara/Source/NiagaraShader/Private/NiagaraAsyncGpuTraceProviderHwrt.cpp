// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraAsyncGpuTraceProviderHwrt.h"
#include "NiagaraGpuComputeDispatchInterface.h"

#if RHI_RAYTRACING

#include "GlobalShader.h"
#include "NiagaraRenderer.h"
#include "NiagaraSettings.h"
#include "ScenePrivate.h"

static int GNiagaraAsyncGpuTraceHwrtEnabled = 1;
static FAutoConsoleVariableRef CVarNiagaraAsyncGpuTraceHwrtEnabled(
	TEXT("fx.Niagara.AsyncGpuTrace.HWRayTraceEnabled"),
	GNiagaraAsyncGpuTraceHwrtEnabled,
	TEXT("If disabled AsyncGpuTrace will not be supported against the HW ray tracing scene."),
	ECVF_Default
);

/// TODO
///  -get geometry masking working when an environmental mask is implemented

// c++ mirror of the struct defined in RayTracingCommon.ush
struct FVFXTracePayload
{
	float HitT;
	uint32 PrimitiveIndex;
	uint32 InstanceIndex;
	float Barycentrics[2];
	float WorldPosition[3];
	float WorldNormal[3];
};

BEGIN_SHADER_PARAMETER_STRUCT(FNiagaraGPUSceneParameters, )
	SHADER_PARAMETER_SRV(StructuredBuffer<float4>, GPUSceneInstanceSceneData)
	SHADER_PARAMETER_SRV(StructuredBuffer<float4>, GPUSceneInstancePayloadData)
	SHADER_PARAMETER_SRV(StructuredBuffer<float4>, GPUScenePrimitiveSceneData)
	SHADER_PARAMETER(uint32, GPUSceneFrameNumber)
END_SHADER_PARAMETER_STRUCT()

class FNiagaraCollisionRayTraceRG : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FNiagaraCollisionRayTraceRG)
	SHADER_USE_ROOT_PARAMETER_STRUCT(FNiagaraCollisionRayTraceRG, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT_INCLUDE(FNiagaraGPUSceneParameters, GPUSceneParameters)

		SHADER_PARAMETER_SRV(Buffer<UINT>, HashTable)
		SHADER_PARAMETER_SRV(Buffer<UINT>, HashToCollisionGroups)
		SHADER_PARAMETER(uint32, HashTableSize)

		SHADER_PARAMETER_SRV(RaytracingAccelerationStructure, TLAS)
		SHADER_PARAMETER_SRV(Buffer<FNiagaraAsyncGpuTrace>, Rays)
		SHADER_PARAMETER(uint32, RaysOffset)
		SHADER_PARAMETER_UAV(Buffer<FNiagaraAsyncGpuTraceResult>, CollisionOutput)
		SHADER_PARAMETER(uint32, CollisionOutputOffset)
		SHADER_PARAMETER_SRV(Buffer<UINT>, RayTraceCounts)
		SHADER_PARAMETER(uint32, MaxRetraces)
	END_SHADER_PARAMETER_STRUCT()

	class FFakeIndirectDispatch : SHADER_PERMUTATION_BOOL("NIAGARA_RAYTRACE_FAKE_INDIRECT");
	class FSupportsCollisionGroups : SHADER_PERMUTATION_BOOL("NIAGARA_SUPPORTS_COLLISION_GROUPS");
	using FPermutationDomain = TShaderPermutationDomain<FFakeIndirectDispatch, FSupportsCollisionGroups>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
	static TShaderRef< FNiagaraCollisionRayTraceRG> GetShader(FGlobalShaderMap* ShaderMap, bool SupportsCollisionGroups);
	static FRHIRayTracingShader* GetRayTracingShader(FGlobalShaderMap* ShaderMap, bool SupportCollisionGroups);
	static bool SupportsIndirectDispatch();
};

class FNiagaraCollisionRayTraceCH : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FNiagaraCollisionRayTraceCH);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);

	FNiagaraCollisionRayTraceCH() = default;
	FNiagaraCollisionRayTraceCH(const ShaderMetaType::CompiledShaderInitializerType& Initializer);
};

class FNiagaraCollisionRayTraceMiss : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FNiagaraCollisionRayTraceMiss);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);

	FNiagaraCollisionRayTraceMiss() = default;
	FNiagaraCollisionRayTraceMiss(const ShaderMetaType::CompiledShaderInitializerType& Initializer);
};

bool FNiagaraCollisionRayTraceRG::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
}

void FNiagaraCollisionRayTraceRG::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("NIAGARA_SUPPORTS_RAY_TRACING"), 1);
	OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_PRIMITIVE_SCENE_DATA"), 1);
	OutEnvironment.SetDefine(TEXT("USE_GLOBAL_GPU_SCENE_DATA"), 1);
}

TShaderRef<FNiagaraCollisionRayTraceRG> FNiagaraCollisionRayTraceRG::GetShader(FGlobalShaderMap* ShaderMap, bool SupportsCollisionGroups)
{
	FPermutationDomain PermutationVector;
	PermutationVector.Set<FFakeIndirectDispatch>(!SupportsIndirectDispatch());
	PermutationVector.Set<FSupportsCollisionGroups>(SupportsCollisionGroups);

	return ShaderMap->GetShader<FNiagaraCollisionRayTraceRG>(PermutationVector);
}

FRHIRayTracingShader* FNiagaraCollisionRayTraceRG::GetRayTracingShader(FGlobalShaderMap* ShaderMap, bool SupportCollisionGroups)
{
	return GetShader(ShaderMap, SupportCollisionGroups).GetRayTracingShader();
}

bool FNiagaraCollisionRayTraceRG::SupportsIndirectDispatch()
{
	return GRHISupportsRayTracingDispatchIndirect;
}

bool FNiagaraCollisionRayTraceCH::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
}

void FNiagaraCollisionRayTraceCH::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("NIAGARA_SUPPORTS_RAY_TRACING"), 1);
}

FNiagaraCollisionRayTraceCH::FNiagaraCollisionRayTraceCH(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	: FGlobalShader(Initializer)
{}

bool FNiagaraCollisionRayTraceMiss::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
}

void FNiagaraCollisionRayTraceMiss::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("NIAGARA_SUPPORTS_RAY_TRACING"), 1);
}

FNiagaraCollisionRayTraceMiss::FNiagaraCollisionRayTraceMiss(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	: FGlobalShader(Initializer)
{}

IMPLEMENT_GLOBAL_SHADER(FNiagaraCollisionRayTraceRG, "/Plugin/FX/Niagara/Private/NiagaraRayTracingShaders.usf", "NiagaraCollisionRayTraceRG", SF_RayGen);
IMPLEMENT_GLOBAL_SHADER(FNiagaraCollisionRayTraceCH, "/Plugin/FX/Niagara/Private/NiagaraRayTracingShaders.usf", "NiagaraCollisionRayTraceCH", SF_RayHitGroup);
IMPLEMENT_GLOBAL_SHADER(FNiagaraCollisionRayTraceMiss, "/Plugin/FX/Niagara/Private/NiagaraRayTracingShaders.usf", "NiagaraCollisionRayTraceMiss", SF_RayMiss);

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

static FRayTracingPipelineState* CreateNiagaraRayTracingPipelineState(
	EShaderPlatform Platform,
	FRHICommandList& RHICmdList,
	FRHIRayTracingShader* RayGenShader,
	FRHIRayTracingShader* ClosestHitShader,
	FRHIRayTracingShader* MissShader)
{
	FRayTracingPipelineStateInitializer Initializer;
	Initializer.MaxPayloadSizeInBytes = sizeof(FVFXTracePayload);

	FRHIRayTracingShader* RayGenShaderTable[] = { RayGenShader };
	Initializer.SetRayGenShaderTable(RayGenShaderTable);

	FRHIRayTracingShader* HitGroupTable[] = { ClosestHitShader };
	Initializer.SetHitGroupTable(HitGroupTable);

	FRHIRayTracingShader* MissTable[] = { MissShader };
	Initializer.SetMissShaderTable(MissTable);

	Initializer.bAllowHitGroupIndexing = true; // Use the same hit shader for all geometry in the scene by disabling SBT indexing.

	return PipelineStateCache::GetAndOrCreateRayTracingPipelineState(RHICmdList, Initializer);
}

static void BindNiagaraRayTracingMeshCommands(
	FRHICommandList& RHICmdList,
	FRayTracingSceneRHIRef RayTracingScene,
	FRHIUniformBuffer* ViewUniformBuffer,
	TConstArrayView<FVisibleRayTracingMeshCommand> RayTracingMeshCommands,
	FRayTracingPipelineState* Pipeline,
	TFunctionRef<uint32(const FRayTracingMeshCommand&)> PackUserData)
{
	const int32 NumTotalBindings = RayTracingMeshCommands.Num();

	const uint32 MergedBindingsSize = sizeof(FRayTracingLocalShaderBindings) * NumTotalBindings;

	const uint32 NumUniformBuffers = 1;
	const uint32 UniformBufferArraySize = sizeof(FRHIUniformBuffer*) * NumUniformBuffers;

	FConcurrentLinearBulkObjectAllocator Allocator;
	FRayTracingLocalShaderBindings* Bindings = nullptr;
	FRHIUniformBuffer** UniformBufferArray = nullptr;

	if (RHICmdList.Bypass())
	{
		Bindings = reinterpret_cast<FRayTracingLocalShaderBindings*>(Allocator.Malloc(MergedBindingsSize, alignof(FRayTracingLocalShaderBindings)));
		UniformBufferArray = reinterpret_cast<FRHIUniformBuffer**>(Allocator.Malloc(UniformBufferArraySize, alignof(FRHIUniformBuffer*)));
	}
	else
	{
		Bindings = reinterpret_cast<FRayTracingLocalShaderBindings*>(RHICmdList.Alloc(MergedBindingsSize, alignof(FRayTracingLocalShaderBindings)));
		UniformBufferArray = reinterpret_cast<FRHIUniformBuffer**>(RHICmdList.Alloc(UniformBufferArraySize, alignof(FRHIUniformBuffer*)));
	}

	UniformBufferArray[0] = ViewUniformBuffer;

	uint32 BindingIndex = 0;
	for (const FVisibleRayTracingMeshCommand VisibleMeshCommand : RayTracingMeshCommands)
	{
		const FRayTracingMeshCommand& MeshCommand = *VisibleMeshCommand.RayTracingMeshCommand;

		FRayTracingLocalShaderBindings Binding = {};
		Binding.InstanceIndex = VisibleMeshCommand.InstanceIndex;
		Binding.SegmentIndex = MeshCommand.GeometrySegmentIndex;
		Binding.UserData = PackUserData(MeshCommand);
		Binding.UniformBuffers = UniformBufferArray;
		Binding.NumUniformBuffers = NumUniformBuffers;

		Bindings[BindingIndex] = Binding;
		BindingIndex++;
	}

	const bool bCopyDataToInlineStorage = false; // Storage is already allocated from RHICmdList, no extra copy necessary
	RHICmdList.SetRayTracingHitGroups(
		RayTracingScene,
		Pipeline,
		NumTotalBindings,
		Bindings,
		bCopyDataToInlineStorage);
	RHICmdList.SetRayTracingMissShader(RayTracingScene, 0, Pipeline, 0 /* ShaderIndexInPipeline */, 0, nullptr, 0);
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

const FNiagaraAsyncGpuTraceProvider::EProviderType FNiagaraAsyncGpuTraceProviderHwrt::Type = ENDICollisionQuery_AsyncGpuTraceProvider::HWRT;

FNiagaraAsyncGpuTraceProviderHwrt::FNiagaraAsyncGpuTraceProviderHwrt(EShaderPlatform InShaderPlatform, FNiagaraGpuComputeDispatchInterface* Dispatcher)
	: FNiagaraAsyncGpuTraceProvider(InShaderPlatform, Dispatcher)
{
}

bool FNiagaraAsyncGpuTraceProviderHwrt::IsSupported()
{
	return GNiagaraAsyncGpuTraceHwrtEnabled && IsRayTracingEnabled();
}

bool FNiagaraAsyncGpuTraceProviderHwrt::IsAvailable() const
{
	if (!GNiagaraAsyncGpuTraceHwrtEnabled)
	{
		return false;
	}

	if (!Dispatcher->RequiresRayTracingScene())
	{
		return false;
	}

	FScene* Scene = Dispatcher->GetScene();

	return Scene ? Scene->RayTracingScene.IsCreated() : false;
}

void FNiagaraAsyncGpuTraceProviderHwrt::PostRenderOpaque(FRHICommandList& RHICmdList, TConstArrayView<FViewInfo> Views, FCollisionGroupHashMap* CollisionGroupHash)
{
	check(IsAvailable());
	check(Views.Num() > 0);

	const FViewInfo& ReferenceView = Views[0];
	FScene* Scene = Dispatcher->GetScene();

	if (Scene && Scene->RayTracingScene.IsCreated())
	{
		RayTracingScene = Scene->RayTracingScene.GetRHIRayTracingSceneChecked();
		RayTracingSceneView = Scene->RayTracingScene.GetLayerSRVChecked(ERayTracingSceneLayer::Base);
		ViewUniformBuffer = ReferenceView.ViewUniformBuffer;

		FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(ShaderPlatform);
		auto RayGenShader = FNiagaraCollisionRayTraceRG::GetRayTracingShader(ShaderMap, CollisionGroupHash != nullptr);
		auto ClosestHitShader = ShaderMap->GetShader<FNiagaraCollisionRayTraceCH>().GetRayTracingShader();
		auto MissShader = ShaderMap->GetShader<FNiagaraCollisionRayTraceMiss>().GetRayTracingShader();

		RayTracingPipelineState = CreateNiagaraRayTracingPipelineState(
			ShaderPlatform,
			RHICmdList,
			RayGenShader,
			ClosestHitShader,
			MissShader);

		// some options for what we want with our per MeshCommand user data.  For now we'll ignore it, but possibly
		// something we'd want to incorporate.  Some examples could be if the material is translucent, or possibly the physical material?
		auto BakeTranslucent = [&](const FRayTracingMeshCommand& MeshCommand) {	return (MeshCommand.bIsTranslucent != 0) & 0x1;	};
		auto BakeDefault = [&](const FRayTracingMeshCommand& MeshCommand) { return 0; };

		BindNiagaraRayTracingMeshCommands(
			RHICmdList,
			RayTracingScene,
			ViewUniformBuffer,
			ReferenceView.VisibleRayTracingMeshCommands,
			RayTracingPipelineState,
			BakeDefault);
	}
	else
	{
		Reset();
	}
}

void FNiagaraAsyncGpuTraceProviderHwrt::IssueTraces(FRHICommandList& RHICmdList, const FDispatchRequest& Request, FCollisionGroupHashMap* CollisionGroupHash)
{
	check(IsAvailable());
	check(RayTracingPipelineState);
	check(RayTracingScene);
	check(RayTracingSceneView);

	if (Request.MaxTraceCount == 0)
	{
		return;
	}

	SCOPED_DRAW_EVENT(RHICmdList, NiagaraIssueTracesHwrt);

	FScene* Scene = Dispatcher->GetScene();
	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(ShaderPlatform);

	TShaderRef<FNiagaraCollisionRayTraceRG> RGShader = FNiagaraCollisionRayTraceRG::GetShader(ShaderMap, CollisionGroupHash != nullptr);

	FNiagaraCollisionRayTraceRG::FParameters Params;

	Params.View = GetShaderBinding(ViewUniformBuffer);
	Params.GPUSceneParameters.GPUSceneInstanceSceneData = Scene->GPUScene.InstanceSceneDataBuffer->GetSRV();
	Params.GPUSceneParameters.GPUSceneInstancePayloadData = Scene->GPUScene.InstancePayloadDataBuffer->GetSRV();
	Params.GPUSceneParameters.GPUScenePrimitiveSceneData = Scene->GPUScene.PrimitiveBuffer->GetSRV();
	Params.GPUSceneParameters.GPUSceneFrameNumber = Scene->GPUScene.GetSceneFrameNumber();

	if (CollisionGroupHash)
	{
		Params.HashTable = CollisionGroupHash->PrimIdHashTable.SRV;
		Params.HashTableSize = CollisionGroupHash->HashTableSize;
		Params.HashToCollisionGroups = CollisionGroupHash->HashToCollisionGroups.SRV;
	}

	Params.TLAS = RayTracingSceneView;
	Params.Rays = Request.TracesBuffer->SRV;
	Params.RaysOffset = Request.TracesOffset;
	Params.CollisionOutput = Request.ResultsBuffer->UAV;
	Params.CollisionOutputOffset = Request.ResultsOffset;
	Params.MaxRetraces = Request.MaxRetraceCount;

	if (FNiagaraCollisionRayTraceRG::SupportsIndirectDispatch())
	{
		FRayTracingShaderBindingsWriter GlobalResources;
		SetShaderParameters(GlobalResources, RGShader, Params);

		//Can we wrangle things so we can have one indirect dispatch with each internal dispatch pointing to potentially different Ray and Results buffers?
		//For now have a each as a unique dispatch.
		RHICmdList.RayTraceDispatchIndirect(
			RayTracingPipelineState,
			RGShader.GetRayTracingShader(),
			RayTracingScene,
			GlobalResources,
			Request.TraceCountsBuffer->Buffer,
			Request.TraceCountsOffset * sizeof(uint32));
	}
	else
	{
		Params.RayTraceCounts = Request.TraceCountsBuffer->SRV;

		FRayTracingShaderBindingsWriter GlobalResources;
		SetShaderParameters(GlobalResources, RGShader, Params);

		RHICmdList.RayTraceDispatch(
			RayTracingPipelineState,
			RGShader.GetRayTracingShader(),
			RayTracingScene,
			GlobalResources,
			Request.MaxTraceCount,
			1
		);
	}
}

void FNiagaraAsyncGpuTraceProviderHwrt::Reset()
{
	RayTracingPipelineState = nullptr;
	RayTracingScene = nullptr;
	RayTracingSceneView = nullptr;
	ViewUniformBuffer = TUniformBufferRef<FViewUniformShaderParameters>();
}

#endif // RHI_RAYTRACING
