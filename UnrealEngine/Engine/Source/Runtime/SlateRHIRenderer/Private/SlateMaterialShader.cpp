// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlateMaterialShader.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "RHIStaticStates.h"
#include "ShaderParameterUtils.h"

IMPLEMENT_TYPE_LAYOUT(FSlateMaterialShaderVS);
IMPLEMENT_TYPE_LAYOUT(FSlateMaterialShaderPS);

FSlateMaterialShaderVS::FSlateMaterialShaderVS(const FMaterialShaderType::CompiledShaderInitializerType& Initializer)
	: FMaterialShader(Initializer)
{
	ViewProjection.Bind(Initializer.ParameterMap, TEXT("ViewProjection"));
}


void FSlateMaterialShaderVS::ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	// Set defines based on what this shader will be used for
	OutEnvironment.SetDefine( TEXT("USE_MATERIALS"), 1 );
	OutEnvironment.SetDefine( TEXT("NUM_CUSTOMIZED_UVS"), Parameters.MaterialParameters.NumCustomizedUVs );
	OutEnvironment.SetDefine(TEXT("HAS_SCREEN_POSITION"), (bool)Parameters.MaterialParameters.bHasVertexPositionOffsetConnected);

	FMaterialShader::ModifyCompilationEnvironment( Parameters, OutEnvironment );
}

bool FSlateMaterialShaderVS::ShouldCompilePermutation(const FMaterialShaderPermutationParameters& Parameters)
{
	return Parameters.MaterialParameters.MaterialDomain == MD_UI;
}

void FSlateMaterialShaderVS::SetViewProjection(FRHIBatchedShaderParameters& BatchedParameters, const FMatrix44f& InViewProjection )
{
	SetShaderValue(BatchedParameters, ViewProjection, InViewProjection );
}

void FSlateMaterialShaderVS::SetMaterialShaderParameters(FRHIBatchedShaderParameters& BatchedParameters, const FSceneView& View, const FMaterialRenderProxy* MaterialRenderProxy, const FMaterial* Material)
{
	SetViewParameters(BatchedParameters, View, View.ViewUniformBuffer);
	FMaterialShader::SetParameters(BatchedParameters, MaterialRenderProxy, *Material, View);
}

bool FSlateMaterialShaderPS::ShouldCompilePermutation(const FMaterialShaderPermutationParameters& Parameters)
{
	return Parameters.MaterialParameters.MaterialDomain == MD_UI;
}


void FSlateMaterialShaderPS::ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	// Set defines based on what this shader will be used for
	OutEnvironment.SetDefine( TEXT("USE_MATERIALS"), 1 );
	OutEnvironment.SetDefine( TEXT("NUM_CUSTOMIZED_UVS"), Parameters.MaterialParameters.NumCustomizedUVs);

	FMaterialShader::ModifyCompilationEnvironment( Parameters, OutEnvironment );
}

FSlateMaterialShaderPS::FSlateMaterialShaderPS(const FMaterialShaderType::CompiledShaderInitializerType& Initializer)
	: FMaterialShader(Initializer)
{
	ShaderParams.Bind(Initializer.ParameterMap, TEXT("ShaderParams"));
	ShaderParams2.Bind(Initializer.ParameterMap, TEXT("ShaderParams2"));
	GammaAndAlphaValues.Bind(Initializer.ParameterMap, TEXT("GammaAndAlphaValues"));
	DrawFlags.Bind(Initializer.ParameterMap, TEXT("DrawFlags"));
	AdditionalTextureParameter.Bind(Initializer.ParameterMap, TEXT("ElementTexture"));
	TextureParameterSampler.Bind(Initializer.ParameterMap, TEXT("ElementTextureSampler"));
}

void FSlateMaterialShaderPS::SetBlendState(FGraphicsPipelineStateInitializer& GraphicsPSOInit, const FMaterial* Material)
{
	EBlendMode BlendMode = Material->GetBlendMode();

	switch (BlendMode)
	{
	default:
	case BLEND_Opaque:
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		break;
	case BLEND_Masked:
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		break;
	case BLEND_Translucent:
		GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_InverseDestAlpha, BF_One>::GetRHI();
		break;
	case BLEND_Additive:
		// Add to the existing scene color
		GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One>::GetRHI();
		break;
	case BLEND_Modulate:
		// Modulate with the existing scene color
		GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGB, BO_Add, BF_Zero, BF_SourceColor>::GetRHI();
		break;
	case BLEND_AlphaComposite:
		// Blend with existing scene color. New color is already pre-multiplied by alpha.
		GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_InverseSourceAlpha, BO_Add, BF_One, BF_InverseSourceAlpha>::GetRHI();
		break;
	case BLEND_AlphaHoldout:
		// Blend by holding out the matte shape of the source alpha
		GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_Zero, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI();
		break;

	};
}

void FSlateMaterialShaderPS::SetParameters(FRHIBatchedShaderParameters& BatchedParameters, const FSceneView& View, const FMaterialRenderProxy* MaterialRenderProxy, const FMaterial* Material, const FShaderParams& InShaderParams)
{
	SetShaderValue(BatchedParameters, ShaderParams, (FVector4f)InShaderParams.PixelParams);
	SetShaderValue(BatchedParameters, ShaderParams2, (FVector4f)InShaderParams.PixelParams2);

	SetViewParameters(BatchedParameters, View, View.ViewUniformBuffer);
	FMaterialShader::SetParameters(BatchedParameters, MaterialRenderProxy, *Material, View);
}

void FSlateMaterialShaderPS::SetAdditionalTexture(FRHIBatchedShaderParameters& BatchedParameters, FRHITexture* InTexture, const FSamplerStateRHIRef SamplerState )
{
	SetTextureParameter(BatchedParameters, AdditionalTextureParameter, TextureParameterSampler, SamplerState, InTexture );
}

void FSlateMaterialShaderPS::SetDisplayGammaAndContrast(FRHIBatchedShaderParameters& BatchedParameters, float InDisplayGamma, float InContrast)
{
	FVector4f InGammaValues(2.2f / InDisplayGamma, 1.0f / InDisplayGamma, 0.0f, InContrast);

	SetShaderValue(BatchedParameters, GammaAndAlphaValues, InGammaValues);
}

void FSlateMaterialShaderPS::SetDrawFlags(FRHIBatchedShaderParameters& BatchedParameters, bool bDrawDisabledEffect)
{
	FVector4f InDrawFlags((bDrawDisabledEffect ? 1.f : 0.f), 0.f, 0.f, 0.f);

	SetShaderValue(BatchedParameters, DrawFlags, InDrawFlags);
}


#define IMPLEMENT_SLATE_VERTEXMATERIALSHADER_TYPE(bUseInstancing) \
	typedef TSlateMaterialShaderVS<bUseInstancing> TSlateMaterialShaderVS##bUseInstancing; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TSlateMaterialShaderVS##bUseInstancing, TEXT("/Engine/Private/SlateVertexShader.usf"), TEXT("Main"), SF_Vertex);

/** Instancing vertex shader */
IMPLEMENT_SLATE_VERTEXMATERIALSHADER_TYPE(true);
/** Non instancing vertex shader */
IMPLEMENT_SLATE_VERTEXMATERIALSHADER_TYPE(false);

#define IMPLEMENT_SLATE_MATERIALSHADER_TYPE(ShaderType) \
	typedef TSlateMaterialShaderPS<ESlateShader::ShaderType> TSlateMaterialShaderPS##ShaderType; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TSlateMaterialShaderPS##ShaderType, TEXT("/Engine/Private/SlateElementPixelShader.usf"), TEXT("Main"), SF_Pixel);

IMPLEMENT_SLATE_MATERIALSHADER_TYPE(Custom)
IMPLEMENT_SLATE_MATERIALSHADER_TYPE(Default);
IMPLEMENT_SLATE_MATERIALSHADER_TYPE(Border);
IMPLEMENT_SLATE_MATERIALSHADER_TYPE(GrayscaleFont);
IMPLEMENT_SLATE_MATERIALSHADER_TYPE(ColorFont);
IMPLEMENT_SLATE_MATERIALSHADER_TYPE(RoundedBox);
IMPLEMENT_SLATE_MATERIALSHADER_TYPE(SdfFont);
IMPLEMENT_SLATE_MATERIALSHADER_TYPE(MsdfFont);
