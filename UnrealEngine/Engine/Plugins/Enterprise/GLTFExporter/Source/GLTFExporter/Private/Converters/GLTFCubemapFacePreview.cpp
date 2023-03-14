// Copyright Epic Games, Inc. All Rights Reserved.

#include "GLTFCubemapFacePreview.h"
#include "Shader.h"
#include "GlobalShader.h"
#include "SimpleElementShaders.h"
#include "ShaderParameterUtils.h"
#include "PipelineStateCache.h"

class FGLTFCubemapFacePreviewPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FGLTFCubemapFacePreviewPS, Global);
public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsPCPlatform(Parameters.Platform);
	}

	FGLTFCubemapFacePreviewPS() {}

	/**
	 * Initialization constructor.
	 * @param Initializer - Shader initialization container.
	 */
	FGLTFCubemapFacePreviewPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		InTextureCube.Bind(Initializer.ParameterMap,TEXT("InTextureCube"));
		InTextureCubeSampler.Bind(Initializer.ParameterMap,TEXT("InTextureCubeSampler"));
		InCubeFaceIndex.Bind(Initializer.ParameterMap,TEXT("InCubeFaceIndex"));
	}
	
	/**
	 * Set shader parameters.
	 * @param TextureCube - The cubemap to render.
	 * @param CubeFace - The cubemap face to render.
	 */
	void SetParameters(FRHICommandList& RHICmdList, const FTexture* TextureCube, ECubeFace CubeFace) const
	{
		FRHIPixelShader* PixelShaderRHI = RHICmdList.GetBoundPixelShader();
		SetTextureParameter(
			RHICmdList,
			PixelShaderRHI,
			InTextureCube,
			InTextureCubeSampler,
			TextureCube);
		SetShaderValue(RHICmdList, PixelShaderRHI, InCubeFaceIndex, CubeFace);
	}

private:
	/** The texture to sample. */
	LAYOUT_FIELD(FShaderResourceParameter, InTextureCube);
	LAYOUT_FIELD(FShaderResourceParameter, InTextureCubeSampler);
	LAYOUT_FIELD(FShaderParameter, InCubeFaceIndex);
};
IMPLEMENT_SHADER_TYPE(,FGLTFCubemapFacePreviewPS,TEXT("/Plugin/GLTFExporter/Private/CubemapFacePS.usf"),TEXT("Main"),SF_Pixel);

/** Binds vertex and pixel shaders for this element */
void FGLTFCubemapFacePreview::BindShaders(
	FRHICommandList& RHICmdList,
	FGraphicsPipelineStateInitializer& GraphicsPSOInit,
	ERHIFeatureLevel::Type InFeatureLevel,
	const FMatrix& InTransform,
	const float InGamma,
	const FMatrix& ColorWeights,
	const FTexture* Texture)
{
	const TShaderMapRef<FSimpleElementVS> VertexShader(GetGlobalShaderMap(InFeatureLevel));
	const TShaderMapRef<FGLTFCubemapFacePreviewPS> PixelShader(GetGlobalShaderMap(InFeatureLevel));

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GSimpleElementVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();

	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0, EApplyRendertargetOption::CheckApply);

	VertexShader->SetParameters(RHICmdList, InTransform);
	PixelShader->SetParameters(RHICmdList, Texture, CubeFace);
}
