// Copyright Epic Games, Inc. All Rights Reserved.

/*==============================================================================
	SimpleElementShaders.h: Definitions for simple element shaders.
==============================================================================*/

#include "SimpleElementShaders.h"
#include "ShaderParameterUtils.h"
#include "SceneView.h"
#include "Misc/LargeWorldRenderPosition.h"
#include "SceneRelativeViewMatrices.h"

/*------------------------------------------------------------------------------
	Simple element vertex shader.
------------------------------------------------------------------------------*/

FSimpleElementVS::FSimpleElementVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
	FGlobalShader(Initializer)
{
	RelativeTransform.Bind(Initializer.ParameterMap,TEXT("Transform"), SPF_Mandatory);
	TransformTilePosition.Bind(Initializer.ParameterMap, TEXT("TransformTilePosition"), SPF_Optional); // TransformTilePosition may be optimized out if LWC is disabled
}

void FSimpleElementVS::SetParameters(FRHICommandList& RHICmdList, const FMatrix& WorldToClipMatrix)
{
	SetShaderValue(RHICmdList, RHICmdList.GetBoundVertexShader(), RelativeTransform, FMatrix44f(WorldToClipMatrix));
	SetShaderValue(RHICmdList, RHICmdList.GetBoundVertexShader(), TransformTilePosition, FVector3f::ZeroVector);
}

void FSimpleElementVS::SetParameters(FRHICommandList& RHICmdList, const FRelativeViewMatrices& Matrices)
{
	SetShaderValue(RHICmdList, RHICmdList.GetBoundVertexShader(), RelativeTransform, Matrices.RelativeWorldToClip);
	SetShaderValue(RHICmdList, RHICmdList.GetBoundVertexShader(), TransformTilePosition, Matrices.TilePosition);
}

void FSimpleElementVS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("ENABLE_LWC"), 1); // Currently SimpleElementVertexShader.usf is shared with FCubemapTexturePropertiesVS, so need separate paths for LWC vs non-LWC
}

/*------------------------------------------------------------------------------
	Simple element pixel shaders.
------------------------------------------------------------------------------*/

FSimpleElementPS::FSimpleElementPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
	FGlobalShader(Initializer)
{
	InTexture.Bind(Initializer.ParameterMap,TEXT("InTexture"));
	InTextureSampler.Bind(Initializer.ParameterMap,TEXT("InTextureSampler"));
	TextureComponentReplicate.Bind(Initializer.ParameterMap,TEXT("TextureComponentReplicate"));
	TextureComponentReplicateAlpha.Bind(Initializer.ParameterMap,TEXT("TextureComponentReplicateAlpha"));
}

void FSimpleElementPS::SetEditorCompositingParameters(FRHICommandList& RHICmdList, const FSceneView* View)
{
	if( View )
	{
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, RHICmdList.GetBoundPixelShader(), View->ViewUniformBuffer );
	}
	else
	{
		// Unset the view uniform buffers since we don't have a view
		SetUniformBufferParameter(RHICmdList, RHICmdList.GetBoundPixelShader(), GetUniformBufferParameter<FViewUniformShaderParameters>(), NULL);
	}
}

void FSimpleElementPS::SetParameters(FRHICommandList& RHICmdList, const FTexture* TextureValue)
{
	SetTextureParameter(RHICmdList, RHICmdList.GetBoundPixelShader(),InTexture,InTextureSampler,TextureValue);
	
	SetShaderValue(RHICmdList, RHICmdList.GetBoundPixelShader(),TextureComponentReplicate,TextureValue->bGreyScaleFormat ? FLinearColor(1,0,0,0) : FLinearColor(0,0,0,0));
	SetShaderValue(RHICmdList, RHICmdList.GetBoundPixelShader(),TextureComponentReplicateAlpha,TextureValue->bGreyScaleFormat ? FLinearColor(1,0,0,0) : FLinearColor(0,0,0,1));
}

/*bool FSimpleElementPS::Serialize(FArchive& Ar)
{
	bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
	Ar << InTexture;
	Ar << InTextureSampler;
	Ar << TextureComponentReplicate;
	Ar << TextureComponentReplicateAlpha;
	Ar << EditorCompositeDepthTestParameter;
	Ar << ScreenToPixel;
	return bShaderHasOutdatedParameters;
}*/

FSimpleElementAlphaOnlyPS::FSimpleElementAlphaOnlyPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
	FSimpleElementPS(Initializer)
{
}

FSimpleElementGammaBasePS::FSimpleElementGammaBasePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
	FSimpleElementPS(Initializer)
{
	Gamma.Bind(Initializer.ParameterMap,TEXT("Gamma"));
}

void FSimpleElementGammaBasePS::SetParameters(FRHICommandList& RHICmdList, const FTexture* Texture, float GammaValue, ESimpleElementBlendMode BlendMode)
{
	FSimpleElementPS::SetParameters(RHICmdList, Texture);
	SetShaderValue(RHICmdList, RHICmdList.GetBoundPixelShader(),Gamma,GammaValue);
}

/*bool FSimpleElementGammaBasePS::Serialize(FArchive& Ar)
{
	bool bShaderHasOutdatedParameters = FSimpleElementPS::Serialize(Ar);
	Ar << Gamma;
	return bShaderHasOutdatedParameters;
}*/

FSimpleElementMaskedGammaBasePS::FSimpleElementMaskedGammaBasePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
	FSimpleElementGammaBasePS(Initializer)
{
	ClipRef.Bind(Initializer.ParameterMap,TEXT("ClipRef"), SPF_Mandatory);
}

void FSimpleElementMaskedGammaBasePS::SetParameters(FRHICommandList& RHICmdList, const FTexture* Texture, float InGamma, float ClipRefValue, ESimpleElementBlendMode BlendMode)
{
	FSimpleElementGammaBasePS::SetParameters(RHICmdList, Texture,InGamma,BlendMode);
	SetShaderValue(RHICmdList, RHICmdList.GetBoundPixelShader(),ClipRef,ClipRefValue);
}

/*bool FSimpleElementMaskedGammaBasePS::Serialize(FArchive& Ar)
{
	bool bShaderHasOutdatedParameters = FSimpleElementGammaBasePS::Serialize(Ar);
	Ar << ClipRef;
	return bShaderHasOutdatedParameters;
}*/

/**
* Constructor
*
* @param Initializer - shader initialization container
*/
FSimpleElementDistanceFieldGammaPS::FSimpleElementDistanceFieldGammaPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	: FSimpleElementMaskedGammaBasePS(Initializer)
{
	SmoothWidth.Bind(Initializer.ParameterMap,TEXT("SmoothWidth"));
	EnableShadow.Bind(Initializer.ParameterMap,TEXT("EnableShadow"));
	ShadowDirection.Bind(Initializer.ParameterMap,TEXT("ShadowDirection"));
	ShadowColor.Bind(Initializer.ParameterMap,TEXT("ShadowColor"));
	ShadowSmoothWidth.Bind(Initializer.ParameterMap,TEXT("ShadowSmoothWidth"));
	EnableGlow.Bind(Initializer.ParameterMap,TEXT("EnableGlow"));
	GlowColor.Bind(Initializer.ParameterMap,TEXT("GlowColor"));
	GlowOuterRadius.Bind(Initializer.ParameterMap,TEXT("GlowOuterRadius"));
	GlowInnerRadius.Bind(Initializer.ParameterMap,TEXT("GlowInnerRadius"));
}

/**
* Sets all the constant parameters for this shader
*
* @param Texture - 2d tile texture
* @param Gamma - if gamma != 1.0 then a pow(color,Gamma) is applied
* @param ClipRef - reference value to compare with alpha for killing pixels
* @param SmoothWidth - The width to smooth the edge the texture
* @param EnableShadow - Toggles drop shadow rendering
* @param ShadowDirection - 2D vector specifying the direction of shadow
* @param ShadowColor - Color of the shadowed pixels
* @param ShadowSmoothWidth - The width to smooth the edge the shadow of the texture
* @param BlendMode - current batched element blend mode being rendered
*/
void FSimpleElementDistanceFieldGammaPS::SetParameters(
	FRHICommandList& RHICmdList, 
	const FTexture* Texture,
	float InGamma,
	float InClipRef,
	float SmoothWidthValue,
	bool bEnableShadowValue,
	const FVector2D& ShadowDirectionValue,
	const FLinearColor& ShadowColorValue,
	float ShadowSmoothWidthValue,
	const FDepthFieldGlowInfo& GlowInfo,
	ESimpleElementBlendMode BlendMode
	)
{
	FSimpleElementMaskedGammaBasePS::SetParameters(RHICmdList, Texture,InGamma,InClipRef,BlendMode);
	SetShaderValue(RHICmdList, RHICmdList.GetBoundPixelShader(),SmoothWidth,SmoothWidthValue);		
	const uint32 bEnableShadowValueUInt = (bEnableShadowValue ? 1 : 0);
	SetShaderValue(RHICmdList, RHICmdList.GetBoundPixelShader(),EnableShadow,bEnableShadowValueUInt);
	if (bEnableShadowValue)
	{
		SetShaderValue(RHICmdList, RHICmdList.GetBoundPixelShader(),ShadowDirection,FVector2f(ShadowDirectionValue));
		SetShaderValue(RHICmdList, RHICmdList.GetBoundPixelShader(),ShadowColor,ShadowColorValue);
		SetShaderValue(RHICmdList, RHICmdList.GetBoundPixelShader(),ShadowSmoothWidth,ShadowSmoothWidthValue);
	}
	SetShaderValue(RHICmdList, RHICmdList.GetBoundPixelShader(),EnableGlow,GlowInfo.bEnableGlow);
	if (GlowInfo.bEnableGlow)
	{
		SetShaderValue(RHICmdList, RHICmdList.GetBoundPixelShader(),GlowColor,GlowInfo.GlowColor);
		SetShaderValue(RHICmdList, RHICmdList.GetBoundPixelShader(),GlowOuterRadius,FVector2f(GlowInfo.GlowOuterRadius));
		SetShaderValue(RHICmdList, RHICmdList.GetBoundPixelShader(),GlowInnerRadius,FVector2f(GlowInfo.GlowInnerRadius));
	}

	// This shader does not use editor compositing
	SetEditorCompositingParameters(RHICmdList, NULL);
}

/**
* Serialize constant paramaters for this shader
* 
* @param Ar - archive to serialize to
* @return true if any of the parameters were outdated
*/
/*bool FSimpleElementDistanceFieldGammaPS::Serialize(FArchive& Ar)
{
	bool bShaderHasOutdatedParameters = FSimpleElementMaskedGammaBasePS::Serialize(Ar);
	Ar << SmoothWidth;
	Ar << EnableShadow;
	Ar << ShadowDirection;
	Ar << ShadowColor;	
	Ar << ShadowSmoothWidth;
	Ar << EnableGlow;
	Ar << GlowColor;
	Ar << GlowOuterRadius;
	Ar << GlowInnerRadius;
	return bShaderHasOutdatedParameters;
}*/
	
FSimpleElementHitProxyPS::FSimpleElementHitProxyPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
	FGlobalShader(Initializer)
{
	InTexture.Bind(Initializer.ParameterMap,TEXT("InTexture"), SPF_Mandatory);
	InTextureSampler.Bind(Initializer.ParameterMap,TEXT("InTextureSampler"));
}

void FSimpleElementHitProxyPS::SetParameters(FRHICommandList& RHICmdList, const FTexture* TextureValue)
{
	SetTextureParameter(RHICmdList, RHICmdList.GetBoundPixelShader(),InTexture,InTextureSampler,TextureValue);
}

/*bool FSimpleElementHitProxyPS::Serialize(FArchive& Ar)
{
	bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
	Ar << InTexture;
	Ar << InTextureSampler;
	return bShaderHasOutdatedParameters;
}*/


FSimpleElementColorChannelMaskPS::FSimpleElementColorChannelMaskPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
: FGlobalShader(Initializer)
{
	InTexture.Bind(Initializer.ParameterMap,TEXT("InTexture"), SPF_Mandatory);
	InTextureSampler.Bind(Initializer.ParameterMap,TEXT("InTextureSampler"));
	ColorWeights.Bind(Initializer.ParameterMap,TEXT("ColorWeights"));
	Gamma.Bind(Initializer.ParameterMap,TEXT("Gamma"));
}

/**
* Sets all the constant parameters for this shader
*
* @param Texture - 2d tile texture
* @param ColorWeights - reference value to compare with alpha for killing pixels
* @param Gamma - if gamma != 1.0 then a pow(color,Gamma) is applied
*/
void FSimpleElementColorChannelMaskPS::SetParameters(FRHICommandList& RHICmdList, const FTexture* TextureValue, const FMatrix& ColorWeightsValue, float GammaValue)
{
	SetTextureParameter(RHICmdList, RHICmdList.GetBoundPixelShader(),InTexture,InTextureSampler,TextureValue);
	SetShaderValue(RHICmdList, RHICmdList.GetBoundPixelShader(),ColorWeights, (FMatrix44f)ColorWeightsValue);
	SetShaderValue(RHICmdList, RHICmdList.GetBoundPixelShader(),Gamma,GammaValue);
}

/*bool FSimpleElementColorChannelMaskPS::Serialize(FArchive& Ar)
{
	bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
	Ar << InTexture;
	Ar << InTextureSampler;
	Ar << ColorWeights;
	Ar << Gamma;
	return bShaderHasOutdatedParameters;
}*/

/*------------------------------------------------------------------------------
	Shader implementations.
------------------------------------------------------------------------------*/

IMPLEMENT_TYPE_LAYOUT(FSimpleElementGammaBasePS);
IMPLEMENT_TYPE_LAYOUT(FSimpleElementMaskedGammaBasePS);

IMPLEMENT_SHADER_TYPE(,FSimpleElementVS,TEXT("/Engine/Private/SimpleElementVertexShader.usf"),TEXT("Main"),SF_Vertex);
IMPLEMENT_SHADER_TYPE(,FSimpleElementPS, TEXT("/Engine/Private/SimpleElementPixelShader.usf"), TEXT("Main"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(,FSimpleElementAlphaOnlyPS, TEXT("/Engine/Private/SimpleElementPixelShader.usf"), TEXT("AlphaOnlyMain"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, FSimpleElementGammaPS_SRGB, TEXT("/Engine/Private/SimpleElementPixelShader.usf"), TEXT("GammaMain"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, FSimpleElementGammaPS_Linear, TEXT("/Engine/Private/SimpleElementPixelShader.usf"), TEXT("GammaMain"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(, FSimpleElementGammaAlphaOnlyPS, TEXT("/Engine/Private/SimpleElementPixelShader.usf"), TEXT("GammaAlphaOnlyMain"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, FSimpleElementMaskedGammaPS_SRGB, TEXT("/Engine/Private/SimpleElementPixelShader.usf"), TEXT("GammaMaskedMain"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, FSimpleElementMaskedGammaPS_Linear, TEXT("/Engine/Private/SimpleElementPixelShader.usf"), TEXT("GammaMaskedMain"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(,FSimpleElementDistanceFieldGammaPS,TEXT("/Engine/Private/SimpleElementPixelShader.usf"),TEXT("GammaDistanceFieldMain"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(,FSimpleElementHitProxyPS,TEXT("/Engine/Private/SimpleElementHitProxyPixelShader.usf"),TEXT("Main"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(,FSimpleElementColorChannelMaskPS,TEXT("/Engine/Private/SimpleElementColorChannelMaskPixelShader.usf"),TEXT("Main"),SF_Pixel);

#undef IMPLEMENT_ENCODEDSHADERS
#undef BLEND_VARIATION
