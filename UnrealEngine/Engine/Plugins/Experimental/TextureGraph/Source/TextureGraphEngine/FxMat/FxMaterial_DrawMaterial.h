// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include <Materials/Material.h>
#include <Materials/MaterialRenderProxy.h>
#include <MaterialDomain.h>
#include <TextureResource.h>
#include <SceneManagement.h>
#include <PostProcess/DrawRectangle.h>

#include <DataDrivenShaderPlatformInfo.h>
#include <MaterialShader.h>
#include <MaterialShared.h>
#include "Engine/TextureRenderTarget2D.h"

#include "FxMaterial.h"
#include "TileInfo_FX.h"

#include "Device/Device.h"

class TEXTUREGRAPHENGINE_API FTextureGraphMaterialShaderVS : public FMaterialShader
{
public:
	DECLARE_SHADER_TYPE(FTextureGraphMaterialShaderVS, Material)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
	END_SHADER_PARAMETER_STRUCT()

	SHADER_USE_PARAMETER_STRUCT_WITH_LEGACY_BASE(FTextureGraphMaterialShaderVS, FMaterialShader);

	static bool ShouldCompilePermutation(const FMaterialShaderPermutationParameters& Parameters)
	{
		return IsPCPlatform(Parameters.Platform) && EnumHasAllFlags(Parameters.Flags, EShaderPermutationFlags::HasEditorOnlyData);
	}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

	static bool ShouldCache(EShaderPlatform Platform)
	{
		// Only cache the shader for the given platform if it is supported
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	void SetParameters(FRHIBatchedShaderParameters& BatchedParameters, const FSceneView& View)
	{
		UE::Renderer::PostProcess::SetDrawRectangleParameters(BatchedParameters, this, View);
		FMaterialShader::SetViewParameters(BatchedParameters, View, View.ViewUniformBuffer);
	}
};

class TEXTUREGRAPHENGINE_API FTextureGraphMaterialShaderPS : public FMaterialShader
{
public:
	DECLARE_SHADER_TYPE(FTextureGraphMaterialShaderPS, Material)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FLinearColor, PSControl)
		SHADER_PARAMETER_STRUCT(FTileInfo, TileInfo)
	END_SHADER_PARAMETER_STRUCT()

	SHADER_USE_PARAMETER_STRUCT_WITH_LEGACY_BASE(FTextureGraphMaterialShaderPS, FMaterialShader);

	static FName PSCONTROL_ARG;

	static bool ShouldCompilePermutation(const FMaterialShaderPermutationParameters& Parameters)
	{
		return IsPCPlatform(Parameters.Platform) && EnumHasAllFlags(Parameters.Flags, EShaderPermutationFlags::HasEditorOnlyData);
	}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

	static bool ShouldCache(EShaderPlatform Platform)
	{
		// Only cache the shader for the given platform if it is supported
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	void SetParameters(FRHIBatchedShaderParameters& BatchedParameters, const FSceneView& View, const FMaterialRenderProxy* MaterialProxy, const FMaterial& Material)
	{
		auto& PrimitivePS = GetUniformBufferParameter<FPrimitiveUniformShaderParameters>();
		SetUniformBufferParameter(BatchedParameters, PrimitivePS, GIdentityPrimitiveUniformBuffer);
		FMaterialShader::SetViewParameters(BatchedParameters, View, View.ViewUniformBuffer);
		FMaterialShader::SetParameters(BatchedParameters, MaterialProxy, Material, View);
	}
};

class FxMaterial_DrawMaterialBase : public FxMaterial
{
public:
	FxMaterial_DrawMaterialBase()
		: FxMaterial(TEXT("VSH_TypeVSH_TypeVSH_TypeVSH_Type"), TEXT("FSH_TypeFSH_TypeFSH_TypeFSH_Type"))
	{
	}

	FSceneView* CreateSceneView(UTextureRenderTarget2D* RenderTarget, const FIntPoint& TargetSizeXY);


	static bool ValidateMaterialShaderMap(UMaterial* InMaterial, FShaderType* InPixelShaderType);
};

template <typename VSH_Type, typename FSH_Type>
class FxMaterial_DrawMaterial : public FxMaterial_DrawMaterialBase
{
public:



	// type name for the permutation domain
	using VSHPermutationDomain = typename VSH_Type::FPermutationDomain;
	using FSHPermutationDomain = typename FSH_Type::FPermutationDomain;

	TObjectPtr<UMaterialInterface>	Material;

protected:
	VSHPermutationDomain			VSHPermDomain;		/// Vertex shader Permutation Domain value
	FSHPermutationDomain			FSHPermDomain;		/// Fragment shader Permutation Domain value
	typename VSH_Type::FParameters	VSHParams;			/// Params for the vertex shader
	typename FSH_Type::FParameters	FSHParams;			/// Params for the fragment shader


public:

	FxMaterial_DrawMaterial()
		: FxMaterial_DrawMaterialBase()
	{
	}

	FxMaterial_DrawMaterial(UMaterialInterface* InMaterial, const VSHPermutationDomain& InVSHPermutationDomain, const FSHPermutationDomain& InFSHPermutationDomain)
		: 	FxMaterial_DrawMaterialBase(),
		Material(InMaterial),
		VSHPermDomain(InVSHPermutationDomain),
		FSHPermDomain(InFSHPermutationDomain)
	{
	}

	virtual std::shared_ptr<FxMaterial> Clone() override
	{
		return std::static_pointer_cast<FxMaterial>(std::make_shared<FxMaterial_DrawMaterial<VSH_Type, FSH_Type>>(Material, VSHPermDomain, FSHPermDomain));
	}

	virtual FxMetadataSet GetMetadata() const override
	{
		return {
			{ VSH_Type::FParameters::FTypeInfo::GetStructMetadata(), (char*)&VSHParams }, 
			{ FSH_Type::FParameters::FTypeInfo::GetStructMetadata(), (char*)&FSHParams }
		};
	}

	virtual void Blit(FRHICommandListImmediate& RHI, FRHITexture2D* Target, const RenderMesh* MeshObj, int32 TargetId, FGraphicsPipelineStateInitializer* InPSO = nullptr) override
	{
		
	}

	void MyBlit(FRHICommandListImmediate& RHI, UTextureRenderTarget2D* RenderTarget, FRHITexture* Target, const RenderMesh* MeshObj, int32 TargetId, FGraphicsPipelineStateInitializer* InPSO = nullptr)
	{
		BindTexturesForBlitting();

		// Make a Scene view for this pass
		FSceneView* SceneView = CreateSceneView(RenderTarget, Target->GetSizeXY());

		//const typename FSH_Type::FParameters* params = reinterpret_cast<typename FSH_Type::FParameters*>(params_);
		QUICK_SCOPE_CYCLE_COUNTER(STAT_ShaderPlugin_PixelShader); // Used to gather CPU profiling data for the UE4 session frontend
		SCOPED_DRAW_EVENT(RHI, ShaderPlugin_Pixel); // Used to profile GPU activity and add metadata to be consumed by for example RenderDoc


		// Pixel shader combined with Material
		const FMaterialRenderProxy* MaterialProxy = Material->GetRenderProxy();
		MaterialProxy->UpdateUniformExpressionCacheIfNeeded(SceneView->GetFeatureLevel());
		FMaterialRenderProxy::UpdateDeferredCachedUniformExpressions();

		const FMaterial& RenderMaterial = MaterialProxy->GetMaterialWithFallback(SceneView->GetFeatureLevel(), MaterialProxy);
		const FMaterialShaderMap* const MaterialShaderMap = RenderMaterial.GetRenderingThreadShaderMap();

		TShaderRef<VSH_Type> VertexShader = MaterialShaderMap->GetShader<VSH_Type>();
		FRHIVertexShader* RHIVertexShader = VertexShader.GetVertexShader();
		TShaderRef<FSH_Type> PixelShader = MaterialShaderMap->GetShader<FSH_Type>();
		FRHIPixelShader* RHIPixelShader = PixelShader.GetPixelShader();


		FRHIRenderPassInfo passInfo(Target, ERenderTargetActions::Clear_Store);
		RHI.BeginRenderPass(passInfo, TEXT("FxMaterial_UMaterial"));

	
		// Set the graphic pipeline state.
		FGraphicsPipelineStateInitializer PSO;
		
		InitPSO_Default(PSO, RHIVertexShader, RHIPixelShader);

		RHI.ApplyCachedRenderTargets(PSO);

		SetGraphicsPipelineState(RHI, PSO, 0);

		SetShaderParametersMixedVS(RHI, VertexShader, VSHParams, *SceneView);
		SetShaderParametersMixedPS(RHI, PixelShader, FSHParams, *SceneView, MaterialProxy, RenderMaterial);

		RHI.SetStreamSource(0, GQuadBuffer.VertexBufferRHI, 0);
		RHI.DrawPrimitive(0, 2, 1);

		RHI.EndRenderPass();
	}

	static bool ValidateMaterial(UMaterialInterface* InMaterial)
	{
		check(IsInRenderingThread());

		UMaterial* Material = InMaterial->GetMaterial();

		return ValidateMaterialShaderMap(Material, &FSH_Type::GetStaticType());
	}
};

typedef FxMaterial_DrawMaterial< FTextureGraphMaterialShaderVS, FTextureGraphMaterialShaderPS> FxMaterial_QuadDrawMaterial;
typedef std::shared_ptr<FxMaterial_QuadDrawMaterial>	FxMaterialQuadDrawMaterialPtr;


