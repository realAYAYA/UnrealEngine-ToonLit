#pragma once

#include "MeshPassProcessor.h"
#include "MeshMaterialShader.h"

class FPrimitiveSceneProxy;
class FScene;
class FStaticMeshBatch;
class FViewInfo;

class FToonOutlineMeshPassProcessor : public FMeshPassProcessor
{
public:
	FToonOutlineMeshPassProcessor(
		const FScene* Scene,
		const FSceneView* InViewIfDynamicMeshCommand,
		const FMeshPassProcessorRenderState& InPassDrawRenderState,
		FMeshPassDrawListContext* InDrawListContext
	);

	virtual void AddMeshBatch(
		const FMeshBatch& RESTRICT MeshBatch,
		uint64 BatchElementMask,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		int32 StaticMeshId = -1
	) override final;

private:
	template<bool bPositionOnly, bool bUsesMobileColorValue>
	bool Process(
		const FMeshBatch& MeshBatch,
		uint64 BatchElementMask,
		int32 StaticMeshId,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
		const FMaterial& RESTRICT MaterialResource,
		ERasterizerFillMode MeshFillMode,
		ERasterizerCullMode MeshCullMode
	);
	
	FMeshPassProcessorRenderState PassDrawRenderState;
};

/** Shader Define*/

class FToonOutlinePassShaderElementData : public FMeshMaterialShaderElementData
{
public:
	float ParameterValue;
};

/**
 * Vertex shader for rendering a single, constant color.
 */
class FToonOutlineVS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FToonOutlineVS, MeshMaterial);

public:
	/** Default constructor. */
	FToonOutlineVS() = default;
	FToonOutlineVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMeshMaterialShader(Initializer)
	{
		OutlineWidth.Bind(Initializer.ParameterMap, TEXT("OutlineWidth"));
		OutlineZOffset.Bind(Initializer.ParameterMap, TEXT("OutlineZOffset"));
		OutlineZOffsetMaskRemapStart.Bind(Initializer.ParameterMap, TEXT("OutlineZOffsetMaskRemapStart"));
		OutlineZOffsetMaskRemapEnd.Bind(Initializer.ParameterMap, TEXT("OutlineZOffsetMaskRemapEnd"));
		//BindSceneTextureUniformBufferDependentOnShadingPath(Initializer, PassUniformBuffer, PassUniformBuffer);
	}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		// Set Define in Shader. 
		//OutEnvironment.SetDefine(TEXT("Define"), Value);
	}

	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		//return VertexFactoryType->SupportsPositionOnly() && Material->IsSpecialEngineMaterial();
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) &&
			//Parameters.MaterialParameters.bUseToonOutLine && 
			(Parameters.VertexFactoryType->GetFName() == FName(TEXT("FLocalVertexFactory")) || 
				Parameters.VertexFactoryType->GetFName() == FName(TEXT("TGPUSkinVertexFactoryDefault")));
	}

	// You can call this function to bind every mesh personality data
	void GetShaderBindings(
		const FScene* Scene,
		ERHIFeatureLevel::Type FeatureLevel,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material,
		const FMeshPassProcessorRenderState& DrawRenderState,
		const FToonOutlinePassShaderElementData& ShaderElementData,
		FMeshDrawSingleShaderBindings& ShaderBindings) const
	{
		FMeshMaterialShader::GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, Material, DrawRenderState, ShaderElementData, ShaderBindings);

		// Get ToonOutLine Data from Material
		ShaderBindings.Add(OutlineWidth, Material.GetOutlineWidth());
		ShaderBindings.Add(OutlineZOffset, Material.GetOutlineWidth());
		ShaderBindings.Add(OutlineZOffsetMaskRemapStart, Material.GetOutlineWidth());
		ShaderBindings.Add(OutlineZOffsetMaskRemapEnd, Material.GetOutlineWidth());
	}

	/** The parameter to use for setting the Mesh OutLine Scale. */
	LAYOUT_FIELD(FShaderParameter, OutlineWidth);
	LAYOUT_FIELD(FShaderParameter, OutlineZOffset);
	LAYOUT_FIELD(FShaderParameter, OutlineZOffsetMaskRemapStart);
	LAYOUT_FIELD(FShaderParameter, OutlineZOffsetMaskRemapEnd);
};

/**
 * Pixel shader for rendering a single, constant color.
 */
class FToonOutlinePS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FToonOutlinePS, MeshMaterial);
	
public:

	FToonOutlinePS() = default;
	FToonOutlinePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMeshMaterialShader(Initializer)
	{
		CustomOutlineColor.Bind(Initializer.ParameterMap, TEXT("CustomOutlineColor"));
	}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		// Set Define in Shader. 
		//OutEnvironment.SetDefine(TEXT("Define"), Value);
	}

	// FShader interface.
	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		//return VertexFactoryType->SupportsPositionOnly() && Material->IsSpecialEngineMaterial();
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) &&
			//Parameters.MaterialParameters.bUseToonOutline && 
			(Parameters.VertexFactoryType->GetFName() == FName(TEXT("FLocalVertexFactory")) || 
				Parameters.VertexFactoryType->GetFName() == FName(TEXT("TGPUSkinVertexFactoryDefault")));
	}
	
	void GetShaderBindings(
		const FScene* Scene,
		ERHIFeatureLevel::Type FeatureLevel,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material,
		const FMeshPassProcessorRenderState& DrawRenderState,
		const FToonOutlinePassShaderElementData& ShaderElementData,
		FMeshDrawSingleShaderBindings& ShaderBindings) const
	{
		FMeshMaterialShader::GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, Material, DrawRenderState, ShaderElementData, ShaderBindings);

		// Get ToonOutLine Data from Material
		const FLinearColor OutLineColorFromMat = Material.GetCustomOutlineColor();
		const FVector4f Color(OutLineColorFromMat.R, OutLineColorFromMat.G, OutLineColorFromMat.B, OutLineColorFromMat.A);

		// Bind to Shader
		ShaderBindings.Add(CustomOutlineColor, Color);
	}
	
	/** The parameter to use for setting the Mesh OutLine Color. */
	LAYOUT_FIELD(FShaderParameter, CustomOutlineColor);
};