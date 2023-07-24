// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MaterialShader.h: Shader base classes
=============================================================================*/

#pragma once

#include "MaterialShaderType.h"
#include "MaterialShared.h"
#include "SceneView.h"
#include "Shader.h"
#include "RHIFwd.h"
#include "Serialization/MemoryLayout.h"

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_1
#include "SceneRenderTargetParameters.h"
#endif

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "HAL/IConsoleManager.h"
#include "RHI.h"
#include "ShaderParameters.h"
#include "ShaderParameterUtils.h"
#endif

class FScene;
class FUniformExpressionSet;
class FViewUniformShaderParameters;

template<typename TBufferStruct> class TUniformBufferRef;

/**
 * Debug information related to uniform expression sets.
 */
class FDebugUniformExpressionSet
{
	DECLARE_TYPE_LAYOUT(FDebugUniformExpressionSet, NonVirtual);
public:
	FDebugUniformExpressionSet();

	explicit FDebugUniformExpressionSet(const FUniformExpressionSet& InUniformExpressionSet);

	/** Initialize from a uniform expression set. */
	void InitFromExpressionSet(const FUniformExpressionSet& InUniformExpressionSet);

	/** Returns true if the number of uniform expressions matches those with which the debug set was initialized. */
	bool Matches(const FUniformExpressionSet& InUniformExpressionSet) const;
	
	/** The number of each type of expression contained in the set. */
	LAYOUT_FIELD(int32, NumPreshaders);
	LAYOUT_ARRAY(int32, NumTextureExpressions, NumMaterialTextureParameterTypes);
};

struct FMaterialShaderPermutationParameters : public FShaderPermutationParameters
{
	FMaterialShaderParameters MaterialParameters;

	FMaterialShaderPermutationParameters(EShaderPlatform InPlatform, const FMaterialShaderParameters& InMaterialParameters, int32 InPermutationId, EShaderPermutationFlags InFlags)
		: FShaderPermutationParameters(InPlatform, InPermutationId, InFlags)
		, MaterialParameters(InMaterialParameters)
	{}
};

/** Base class of all shaders that need material parameters. */
class RENDERER_API FMaterialShader : public FShader
{
	DECLARE_TYPE_LAYOUT(FMaterialShader, NonVirtual);
public:
	using FPermutationParameters = FMaterialShaderPermutationParameters;
	using ShaderMetaType = FMaterialShaderType;

	static FName UniformBufferLayoutName;

	FMaterialShader();

	FMaterialShader(const FMaterialShaderType::CompiledShaderInitializerType& Initializer);

	FRHIUniformBuffer* GetParameterCollectionBuffer(const FGuid& Id, const FSceneInterface* SceneInterface) const;

	template<typename ShaderRHIParamRef, typename TRHICommandList>
	FORCEINLINE_DEBUGGABLE void SetViewParameters(TRHICommandList& RHICmdList, const ShaderRHIParamRef ShaderRHI, const FSceneView& View, const TUniformBufferRef<FViewUniformShaderParameters>& ViewUniformBuffer)
	{
		const auto& ViewUniformBufferParameter = GetUniformBufferParameter<FViewUniformShaderParameters>();
		SetUniformBufferParameter(RHICmdList, ShaderRHI, ViewUniformBufferParameter, ViewUniformBuffer);

		if (View.bShouldBindInstancedViewUB)
		{
			// When drawing an instanced stereo scene, the instanced view UB should be taken from the same view where it will contains a copy of both left and eye values (see FViewInfo::CreateViewUniformBuffers).
			const auto& InstancedViewUniformBufferParameter = GetUniformBufferParameter<FInstancedViewUniformShaderParameters>();
			SetUniformBufferParameter(RHICmdList, ShaderRHI, InstancedViewUniformBufferParameter, View.GetInstancedViewUniformBuffer());
		}
	}

	/** Sets pixel parameters that are material specific but not FMeshBatch specific. */
	template<typename TRHIShader, typename TRHICommandList>
	void SetParameters(
		TRHICommandList& RHICmdList,
		TRHIShader* ShaderRHI,
		const FMaterialRenderProxy* MaterialRenderProxy, 
		const FMaterial& Material,
		const FSceneView& View);

	void GetShaderBindings(
		const FScene* Scene,
		ERHIFeatureLevel::Type FeatureLevel,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material,
		FMeshDrawSingleShaderBindings& ShaderBindings) const;

private:
	/** If true, cached uniform expressions are allowed. */
	static int32 bAllowCachedUniformExpressions;
	/** Console variable ref to toggle cached uniform expressions. */
	static FAutoConsoleVariableRef CVarAllowCachedUniformExpressions;

#if !(UE_BUILD_TEST || UE_BUILD_SHIPPING || !WITH_EDITOR)
	void VerifyExpressionAndShaderMaps(const FMaterialRenderProxy* MaterialRenderProxy, const FMaterial& Material, const FUniformExpressionCache* UniformExpressionCache) const;
#endif

	LAYOUT_FIELD(TMemoryImageArray<FShaderUniformBufferParameter>, ParameterCollectionUniformBuffers);

	LAYOUT_FIELD(FShaderUniformBufferParameter, MaterialUniformBuffer);

	// Only needed to avoid unbound parameter error
	// This texture is bound as an UAV (RWTexture) and so it must be bound together with any RT. So it actually bound but not as part of the material
	LAYOUT_FIELD(FShaderResourceParameter, VTFeedbackBuffer);

protected:
	LAYOUT_FIELD_EDITORONLY(FDebugUniformExpressionSet, DebugUniformExpressionSet);
	LAYOUT_FIELD_EDITORONLY(FRHIUniformBufferLayoutInitializer, DebugUniformExpressionUBLayout);
	LAYOUT_FIELD_EDITORONLY(FMemoryImageString, DebugDescription);
};
