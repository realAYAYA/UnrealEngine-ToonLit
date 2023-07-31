// Copyright Epic Games, Inc. All Rights Reserved.

/*==============================================================================
	NormalMapPreview.h: Implementation for previewing normal maps.
==============================================================================*/

#include "GLTFCombinedTexturePreview.h"
#include "Shader.h"
#include "GlobalShader.h"
#include "SimpleElementShaders.h"
#include "ShaderParameterUtils.h"
#include "PipelineStateCache.h"

class FGLTFCombinedTexturePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FGLTFCombinedTexturePS,Global);
public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsPCPlatform(Parameters.Platform);
	}

	FGLTFCombinedTexturePS() {}

	/**
	 * Initialization constructor.
	 * @param Initializer - Shader initialization container.
	 */
	FGLTFCombinedTexturePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		TextureA.Bind(Initializer.ParameterMap,TEXT("TextureA"));
		TextureSamplerA.Bind(Initializer.ParameterMap,TEXT("TextureSamplerA"));
		TextureB.Bind(Initializer.ParameterMap,TEXT("TextureB"));
		TextureSamplerB.Bind(Initializer.ParameterMap,TEXT("TextureSamplerB"));
		ColorTransformA.Bind(Initializer.ParameterMap,TEXT("ColorTransformA"));
		ColorTransformB.Bind(Initializer.ParameterMap,TEXT("ColorTransformB"));
		BackgroundColor.Bind(Initializer.ParameterMap,TEXT("BackgroundColor"));
	}

	/**
	 * Set shader parameters.
	 * @param SampleTexture - The texture to sample.
	 */
	void SetParameters(FRHICommandList& RHICmdList, const FTexture* InTextureA, const FTexture* InTextureB, const FMatrix44f& InColorTransformA, const FMatrix44f& InColorTransformB, const FLinearColor& InBackgroundColor)
	{
		FRHIPixelShader* PixelShaderRHI = RHICmdList.GetBoundPixelShader();
		SetTextureParameter(RHICmdList, PixelShaderRHI,TextureA,TextureSamplerA,InTextureA);
		SetTextureParameter(RHICmdList, PixelShaderRHI,TextureB,TextureSamplerB,InTextureB);
		SetShaderValue(RHICmdList, PixelShaderRHI,ColorTransformA,InColorTransformA);
		SetShaderValue(RHICmdList, PixelShaderRHI,ColorTransformB,InColorTransformB);
		SetShaderValue(RHICmdList, PixelShaderRHI,BackgroundColor,InBackgroundColor);
	}

private:
	/** The texture to sample. */
	LAYOUT_FIELD(FShaderResourceParameter, TextureA);
	LAYOUT_FIELD(FShaderResourceParameter, TextureSamplerA);
	LAYOUT_FIELD(FShaderResourceParameter, TextureB);
	LAYOUT_FIELD(FShaderResourceParameter, TextureSamplerB);
	LAYOUT_FIELD(FShaderParameter, ColorTransformA);
	LAYOUT_FIELD(FShaderParameter, ColorTransformB);
	LAYOUT_FIELD(FShaderParameter, BackgroundColor);
};
IMPLEMENT_SHADER_TYPE(,FGLTFCombinedTexturePS,TEXT("/Plugin/GLTFExporter/Private/CombinedTexturePS.usf"),TEXT("Main"),SF_Pixel);

/** Binds vertex and pixel shaders for this element */
void FGLTFCombinedTexturePreview::BindShaders(
	FRHICommandList& RHICmdList,
	FGraphicsPipelineStateInitializer& GraphicsPSOInit,
	ERHIFeatureLevel::Type InFeatureLevel,
	const FMatrix& InTransform,
	const float InGamma,
	const FMatrix& ColorWeights,
	const FTexture* Texture)
{
	TShaderMapRef<FSimpleElementVS> VertexShader(GetGlobalShaderMap(InFeatureLevel));
	TShaderMapRef<FGLTFCombinedTexturePS> PixelShader(GetGlobalShaderMap(InFeatureLevel));

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GSimpleElementVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();

	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0, EApplyRendertargetOption::CheckApply);

	VertexShader->SetParameters(RHICmdList, InTransform);
	PixelShader->SetParameters(RHICmdList, TextureA, TextureB, ColorTransformA, ColorTransformB, BackgroundColor);
}
