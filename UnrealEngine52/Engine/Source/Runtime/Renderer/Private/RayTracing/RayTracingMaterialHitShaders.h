// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RHIDefinitions.h"

#if RHI_RAYTRACING

#include "DataDrivenShaderPlatformInfo.h"
#include "LightMapRendering.h"
#include "MaterialDomain.h"
#include "MeshMaterialShader.h"
#include "MeshPassProcessor.inl"
#include "RayTracingInstanceMask.h"
#include "RayTracingPayloadType.h"
#include "ShaderParameterStruct.h"

enum class ERayTracingMeshCommandsMode : uint8;

FRHIRayTracingShader* GetRayTracingDefaultMissShader(const FGlobalShaderMap* ShaderMap);
FRHIRayTracingShader* GetRayTracingDefaultOpaqueShader(const FGlobalShaderMap* ShaderMap);
FRHIRayTracingShader* GetRayTracingDefaultHiddenShader(const FGlobalShaderMap* ShaderMap);

class RENDERER_API FRayTracingMeshProcessor
{
public:
	FRayTracingMeshProcessor(FRayTracingMeshCommandContext* InCommandContext, const FScene* InScene, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassProcessorRenderState InPassDrawRenderState, ERayTracingMeshCommandsMode InRayTracingMeshCommandsMode);
	virtual ~FRayTracingMeshProcessor();

	void AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy);

protected:
	FRayTracingMeshCommandContext* CommandContext;
	const FScene* Scene;
	const FSceneView* ViewIfDynamicMeshCommand;
	ERHIFeatureLevel::Type FeatureLevel;
	FMeshPassProcessorRenderState PassDrawRenderState;
	ERayTracingMeshCommandsMode RayTracingMeshCommandsMode;

	bool Process(
		const FMeshBatch& RESTRICT MeshBatch,
		uint64 BatchElementMask,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
		const FMaterial& RESTRICT MaterialResource,
		const FUniformLightMapPolicy& RESTRICT LightMapPolicy);

	template<typename PassShadersType, typename ShaderElementDataType>
	void BuildRayTracingMeshCommands(
		const FMeshBatch& RESTRICT MeshBatch,
		uint64 BatchElementMask,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
		const FMaterial& RESTRICT MaterialResource,
		const FMeshPassProcessorRenderState& RESTRICT DrawRenderState,
		PassShadersType PassShaders,
		const ShaderElementDataType& ShaderElementData,
		ERayTracingViewMaskMode MaskMode)
	{
		const FVertexFactory* RESTRICT VertexFactory = MeshBatch.VertexFactory;

		checkf(MaterialRenderProxy.ImmutableSamplerState.ImmutableSamplers[0] == nullptr, TEXT("Immutable samplers not yet supported in Mesh Draw Command pipeline"));

		FRayTracingMeshCommand SharedCommand;

		SetupRayTracingMeshCommandMaskAndStatus(SharedCommand, MeshBatch, PrimitiveSceneProxy, MaterialResource, MaskMode);

		if (GRHISupportsRayTracingShaders)
		{
			SharedCommand.SetShaders(PassShaders.GetUntypedShaders());
		}

		FVertexInputStreamArray VertexStreams;
		VertexFactory->GetStreams(ERHIFeatureLevel::SM5, EVertexInputStreamType::Default, VertexStreams);

		int32 DataOffset = 0;
		if (PassShaders.RayTracingShader.IsValid())
		{
			FMeshDrawSingleShaderBindings ShaderBindings = SharedCommand.ShaderBindings.GetSingleShaderBindings(SF_RayHitGroup, DataOffset);
			PassShaders.RayTracingShader->GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, MaterialResource, DrawRenderState, ShaderElementData, ShaderBindings);
		}

		const int32 NumElements = MeshBatch.Elements.Num();

		for (int32 BatchElementIndex = 0; BatchElementIndex < NumElements; BatchElementIndex++)
		{
			if ((1ull << BatchElementIndex) & BatchElementMask)
			{
				const FMeshBatchElement& BatchElement = MeshBatch.Elements[BatchElementIndex];
				FRayTracingMeshCommand& RayTracingMeshCommand = CommandContext->AddCommand(SharedCommand);

				DataOffset = 0;
				if (PassShaders.RayTracingShader.IsValid())
				{
					FMeshDrawSingleShaderBindings RayHitGroupShaderBindings = RayTracingMeshCommand.ShaderBindings.GetSingleShaderBindings(SF_RayHitGroup, DataOffset);
					FMeshMaterialShader::GetElementShaderBindings(PassShaders.RayTracingShader, Scene, ViewIfDynamicMeshCommand, VertexFactory, EVertexInputStreamType::Default, FeatureLevel, PrimitiveSceneProxy, MeshBatch, BatchElement, ShaderElementData, RayHitGroupShaderBindings, VertexStreams);
				}

				RayTracingMeshCommand.GeometrySegmentIndex = uint32(MeshBatch.SegmentIndex) + BatchElementIndex;
				RayTracingMeshCommand.bIsTranslucent = MeshBatch.IsTranslucent(MaterialResource.GetFeatureLevel());
				CommandContext->FinalizeCommand(RayTracingMeshCommand);
			}
		}
	}

private:
	bool ProcessPathTracing(
		const FMeshBatch& RESTRICT MeshBatch,
		uint64 BatchElementMask,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
		const FMaterial& RESTRICT MaterialResource);

	bool TryAddMeshBatch(
		const FMeshBatch& RESTRICT MeshBatch,
		uint64 BatchElementMask,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		int32 StaticMeshId,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material
	);
};

class RENDERER_API FHiddenMaterialHitGroup : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FHiddenMaterialHitGroup)
	SHADER_USE_ROOT_PARAMETER_STRUCT(FHiddenMaterialHitGroup, FGlobalShader)

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	static ERayTracingPayloadType GetRayTracingPayloadType(const int32 PermutationId)
	{
		return ERayTracingPayloadType::RayTracingMaterial;
	}

	using FParameters = FEmptyShaderParameters;
};

class RENDERER_API FOpaqueShadowHitGroup : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FOpaqueShadowHitGroup)
	SHADER_USE_ROOT_PARAMETER_STRUCT(FOpaqueShadowHitGroup, FGlobalShader)

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	static ERayTracingPayloadType GetRayTracingPayloadType(const int32 PermutationId)
	{
		return ERayTracingPayloadType::RayTracingMaterial;
	}

	using FParameters = FEmptyShaderParameters;
};

class RENDERER_API FDefaultCallableShader : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FDefaultCallableShader)
	SHADER_USE_ROOT_PARAMETER_STRUCT(FDefaultCallableShader, FGlobalShader)

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingCallableShadersForProject(Parameters.Platform);
	}

	static ERayTracingPayloadType GetRayTracingPayloadType(const int32 PermutationId)
	{
		return ERayTracingPayloadType::Decals;
	}

	using FParameters = FEmptyShaderParameters;
};

class FRayTracingLocalShaderBindingWriter
{
public:

	FRayTracingLocalShaderBindingWriter()
	{}

	FRayTracingLocalShaderBindingWriter(const FRayTracingLocalShaderBindingWriter&) = delete;
	FRayTracingLocalShaderBindingWriter& operator = (const FRayTracingLocalShaderBindingWriter&) = delete;
	FRayTracingLocalShaderBindingWriter(FRayTracingLocalShaderBindingWriter&&) = delete;
	FRayTracingLocalShaderBindingWriter& operator = (FRayTracingLocalShaderBindingWriter&&) = delete;

	~FRayTracingLocalShaderBindingWriter() = default;

	FRayTracingLocalShaderBindings& AddWithInlineParameters(uint32 NumUniformBuffers, uint32 LooseDataSize = 0)
	{
		FRayTracingLocalShaderBindings* Result = AllocateInternal();

		if (NumUniformBuffers)
		{
			uint32 AllocSize = sizeof(FRHIUniformBuffer*) * NumUniformBuffers;
			Result->UniformBuffers = (FRHIUniformBuffer**)ParameterMemory.Alloc(AllocSize, alignof(FRHIUniformBuffer*));
			FMemory::Memset(Result->UniformBuffers, 0, AllocSize);
		}
		Result->NumUniformBuffers = NumUniformBuffers;

		if (LooseDataSize)
		{
			Result->LooseParameterData = (uint8*)ParameterMemory.Alloc(LooseDataSize, alignof(void*));
		}
		Result->LooseParameterDataSize = LooseDataSize;

		return *Result;
	}

	FRayTracingLocalShaderBindings& AddWithExternalParameters()
	{
		return *AllocateInternal();
	}

	void Commit(FRHICommandList& RHICmdList, FRHIRayTracingScene* Scene, FRayTracingPipelineState* Pipeline, bool bCopyDataToInlineStorage) const
	{
		const FChunk* Chunk = FirstChunk;
		while (Chunk)
		{
			RHICmdList.SetRayTracingHitGroups(Scene, Pipeline, Chunk->Num, Chunk->Bindings, bCopyDataToInlineStorage);
			Chunk = Chunk->Next;
		}
	}

	struct FChunk
	{
		static constexpr uint32 MaxNum = 1024;

		// Note: constructors for elements of this array are called explicitly in AllocateInternal(). Destructors are not called.
		static_assert(TIsTriviallyDestructible<FRayTracingLocalShaderBindings>::Value, "FRayTracingLocalShaderBindings must be trivially destructible, as no destructor will be called.");
		FRayTracingLocalShaderBindings Bindings[MaxNum];
		FChunk* Next;
		uint32 Num;
	};

	const FChunk* GetFirstChunk() const
	{
		return FirstChunk;
	}

private:

	FChunk* FirstChunk = nullptr;
	FChunk* CurrentChunk = nullptr;

	FMemStackBase ParameterMemory;

	friend class FRHICommandList;
	friend struct FRHICommandSetRayTracingBindings;

	FRayTracingLocalShaderBindings* AllocateInternal()
	{
		if (!CurrentChunk || CurrentChunk->Num == FChunk::MaxNum)
		{
			FChunk* OldChunk = CurrentChunk;

			static_assert(TIsTriviallyDestructible<FChunk>::Value, "Chunk must be trivially destructible, as no destructor will be called.");
			CurrentChunk = (FChunk*)ParameterMemory.Alloc(sizeof(FChunk), alignof(FChunk));
			CurrentChunk->Next = nullptr;
			CurrentChunk->Num = 0;

			if (FirstChunk == nullptr)
			{
				FirstChunk = CurrentChunk;
			}

			if (OldChunk)
			{
				OldChunk->Next = CurrentChunk;
			}
		}

		FRayTracingLocalShaderBindings* ResultMemory = &CurrentChunk->Bindings[CurrentChunk->Num++];
		return new(ResultMemory) FRayTracingLocalShaderBindings;
	}
};

#endif // RHI_RAYTRACING
