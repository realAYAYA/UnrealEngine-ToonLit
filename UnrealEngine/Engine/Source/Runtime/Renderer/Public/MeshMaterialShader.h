// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MeshMaterialShader.h: Shader base classes
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "ShaderParameters.h"
#include "VertexFactory.h"
#include "MeshMaterialShaderType.h"
#include "MaterialShader.h"
#include "MeshDrawShaderBindings.h"

class FPrimitiveSceneProxy;
struct FMeshBatchElement;
struct FMeshDrawingRenderState;
struct FMeshPassProcessorRenderState;

template<typename TBufferStruct> class TUniformBufferRef;

class FMeshMaterialShaderElementData
{
public:
	FRHIUniformBuffer* FadeUniformBuffer = nullptr;
	FRHIUniformBuffer* DitherUniformBuffer = nullptr;

	RENDERER_API void InitializeMeshMaterialData(const FSceneView* SceneView, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, const FMeshBatch& RESTRICT MeshBatch, int32 StaticMeshId, bool bAllowStencilDither);
	RENDERER_API void InitializeMeshMaterialData(const FSceneView* SceneView, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId, bool bDitheredLODTransition, bool bAllowStencilDither);
};

struct FMeshMaterialShaderPermutationParameters : public FMaterialShaderPermutationParameters
{
	// Type of vertex factory to compile.
	const FVertexFactoryType* VertexFactoryType;

	FMeshMaterialShaderPermutationParameters(EShaderPlatform InPlatform, const FMaterialShaderParameters& InMaterialParameters, const FVertexFactoryType* InVertexFactoryType, int32 InPermutationId, EShaderPermutationFlags InFlags)
		: FMaterialShaderPermutationParameters(InPlatform, InMaterialParameters, InPermutationId, InFlags)
		, VertexFactoryType(InVertexFactoryType)
	{}
};

struct FVertexFactoryShaderPermutationParameters
{
	const FShaderType* ShaderType;
	const FVertexFactoryType* VertexFactoryType;
	FMaterialShaderParameters MaterialParameters;
	EShaderPlatform Platform;
	EShaderPermutationFlags Flags;

	FVertexFactoryShaderPermutationParameters(
		EShaderPlatform InPlatform,
		const FMaterialShaderParameters& InMaterialParameters, 
		const FVertexFactoryType* InVertexFactoryType,
		const FShaderType* InShaderType,
		EShaderPermutationFlags InFlags
		)
		: ShaderType(InShaderType)
		, VertexFactoryType(InVertexFactoryType)
		, MaterialParameters(InMaterialParameters)
		, Platform(InPlatform)
		, Flags(InFlags)
	{}
};

/** Base class of all shaders that need material and vertex factory parameters. */
class RENDERER_API FMeshMaterialShader : public FMaterialShader
{
	DECLARE_TYPE_LAYOUT(FMeshMaterialShader, NonVirtual);
public:
	using FPermutationParameters = FMeshMaterialShaderPermutationParameters;
	using ShaderMetaType = FMeshMaterialShaderType;

	FMeshMaterialShader() {}

	FMeshMaterialShader(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer);

	void GetShaderBindings(
		const FScene* Scene,
		ERHIFeatureLevel::Type FeatureLevel,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material,
		const FMeshPassProcessorRenderState& DrawRenderState,
		const FMeshMaterialShaderElementData& ShaderElementData,
		FMeshDrawSingleShaderBindings& ShaderBindings) const;

	void GetElementShaderBindings(
		const FShaderMapPointerTable& PointerTable,
		const FScene* Scene, 
		const FSceneView* ViewIfDynamicMeshCommand, 
		const FVertexFactory* VertexFactory,
		const EVertexInputStreamType InputStreamType,
		const FStaticFeatureLevel FeatureLevel,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMeshBatch& MeshBatch,
		const FMeshBatchElement& BatchElement, 
		const FMeshMaterialShaderElementData& ShaderElementData,
		FMeshDrawSingleShaderBindings& ShaderBindings,
		FVertexInputStreamArray& VertexStreams) const;

	template<typename ShaderType, typename PointerTableType, typename ShaderElementDataType>
	static inline void GetElementShaderBindings(const TShaderRefBase<ShaderType, PointerTableType>& Shader,
		const FScene* Scene,
		const FSceneView* ViewIfDynamicMeshCommand,
		const FVertexFactory* VertexFactory,
		const EVertexInputStreamType InputStreamType,
		ERHIFeatureLevel::Type FeatureLevel,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMeshBatch& MeshBatch,
		const FMeshBatchElement& BatchElement,
		const ShaderElementDataType& ShaderElementData,
		FMeshDrawSingleShaderBindings& ShaderBindings,
		FVertexInputStreamArray& VertexStreams)
	{
		Shader->GetElementShaderBindings(Shader.GetPointerTable(), Scene, ViewIfDynamicMeshCommand, VertexFactory, InputStreamType, FeatureLevel, PrimitiveSceneProxy, MeshBatch, BatchElement, ShaderElementData, ShaderBindings, VertexStreams);
	}

private:
	void WriteFrozenVertexFactoryParameters(FMemoryImageWriter& Writer, const TMemoryImagePtr<FVertexFactoryShaderParameters>& InVertexFactoryParameters) const;
	LAYOUT_FIELD_WITH_WRITER(TMemoryImagePtr<FVertexFactoryShaderParameters>, VertexFactoryParameters, WriteFrozenVertexFactoryParameters);

protected:
	LAYOUT_FIELD(FShaderUniformBufferParameter, PassUniformBuffer);
};

