// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlateShaders.h"
#include "Rendering/RenderingCommon.h"
#include "PipelineStateCache.h"
#include "DataDrivenShaderPlatformInfo.h"

/** Flag to determine if we are running with a color vision deficiency shader on */
EColorVisionDeficiency GSlateColorDeficiencyType = EColorVisionDeficiency::NormalVision;
int32 GSlateColorDeficiencySeverity = 0;
bool GSlateColorDeficiencyCorrection = false;
bool GSlateShowColorDeficiencyCorrectionWithDeficiency = false;

IMPLEMENT_TYPE_LAYOUT(FSlateElementPS);

IMPLEMENT_SHADER_TYPE(, FSlateElementVS, TEXT("/Engine/Private/SlateVertexShader.usf"), TEXT("Main"), SF_Vertex);

IMPLEMENT_SHADER_TYPE(, FSlateDebugOverdrawPS, TEXT("/Engine/Private/SlateElementPixelShader.usf"), TEXT("DebugOverdrawMain"), SF_Pixel );

IMPLEMENT_SHADER_TYPE(, FSlatePostProcessBlurPS, TEXT("/Engine/Private/SlatePostProcessPixelShader.usf"), TEXT("GaussianBlurMain"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(, FSlatePostProcessDownsamplePS, TEXT("/Engine/Private/SlatePostProcessPixelShader.usf"), TEXT("DownsampleMain"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, FSlatePostProcessUpsamplePS<ESlatePostProcessUpsamplePSPermutation::SDR>, TEXT("/Engine/Private/SlatePostProcessPixelShader.usf"), TEXT("UpsampleMain"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, FSlatePostProcessUpsamplePS<ESlatePostProcessUpsamplePSPermutation::HDR_SCRGB>, TEXT("/Engine/Private/SlatePostProcessPixelShader.usf"), TEXT("UpsampleMain"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, FSlatePostProcessUpsamplePS<ESlatePostProcessUpsamplePSPermutation::HDR_PQ10>, TEXT("/Engine/Private/SlatePostProcessPixelShader.usf"), TEXT("UpsampleMain"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(, FSlatePostProcessColorDeficiencyPS, TEXT("/Engine/Private/SlatePostProcessColorDeficiencyPixelShader.usf"), TEXT("ColorDeficiencyMain"), SF_Pixel);

IMPLEMENT_SHADER_TYPE(, FSlateMaskingVS, TEXT("/Engine/Private/SlateMaskingShader.usf"), TEXT("MainVS"), SF_Vertex);
IMPLEMENT_SHADER_TYPE(, FSlateMaskingPS, TEXT("/Engine/Private/SlateMaskingShader.usf"), TEXT("MainPS"), SF_Pixel);

IMPLEMENT_SHADER_TYPE(, FSlateDebugBatchingPS, TEXT("/Engine/Private/SlateElementPixelShader.usf"), TEXT("DebugBatchingMain"), SF_Pixel );

#define IMPLEMENT_SLATE_PIXELSHADER_TYPE(ShaderType, bDrawDisabledEffect, bUseTextureAlpha, bUseTextureGrayscale, bIsVirtualTexture) \
	typedef TSlateElementPS<ESlateShader::ShaderType,bDrawDisabledEffect,bUseTextureAlpha, bUseTextureGrayscale, bIsVirtualTexture> TSlateElementPS##ShaderType##bDrawDisabledEffect##bUseTextureAlpha##bUseTextureGrayscale##bIsVirtualTexture##A; \
	IMPLEMENT_SHADER_TYPE(template<>,TSlateElementPS##ShaderType##bDrawDisabledEffect##bUseTextureAlpha##bUseTextureGrayscale##bIsVirtualTexture##A,TEXT("/Engine/Private/SlateElementPixelShader.usf"),TEXT("Main"),SF_Pixel);

#if WITH_EDITOR
bool FHDREditorConvertPS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

IMPLEMENT_SHADER_TYPE(, FHDREditorConvertPS, TEXT("/Engine/Private/CompositeUIPixelShader.usf"), TEXT("HDREditorConvert"), SF_Pixel);
#endif
/**
* All the different permutations of shaders used by slate. Uses #defines to avoid dynamic branches
*/
IMPLEMENT_SLATE_PIXELSHADER_TYPE(Default, false, true, true, false);
IMPLEMENT_SLATE_PIXELSHADER_TYPE(Default, false, true, true, true);
IMPLEMENT_SLATE_PIXELSHADER_TYPE(Border, false, true, false, false);
IMPLEMENT_SLATE_PIXELSHADER_TYPE(Default, true, true, true, false);
IMPLEMENT_SLATE_PIXELSHADER_TYPE(Default, true, true, true, true);
IMPLEMENT_SLATE_PIXELSHADER_TYPE(Border, true, true, false, false);
IMPLEMENT_SLATE_PIXELSHADER_TYPE(Default, false, false, true, false);
IMPLEMENT_SLATE_PIXELSHADER_TYPE(Default, false, false, true, true);
IMPLEMENT_SLATE_PIXELSHADER_TYPE(Border, false, false, false, false);
IMPLEMENT_SLATE_PIXELSHADER_TYPE(Default, true, false, true, false);
IMPLEMENT_SLATE_PIXELSHADER_TYPE(Default, true, false, true, true);
IMPLEMENT_SLATE_PIXELSHADER_TYPE(Border, true, false, false, false);


IMPLEMENT_SLATE_PIXELSHADER_TYPE(Default, true, true, false, false);
IMPLEMENT_SLATE_PIXELSHADER_TYPE(Default, true, true, false, true);
IMPLEMENT_SLATE_PIXELSHADER_TYPE(Default, true, false, false, false);
IMPLEMENT_SLATE_PIXELSHADER_TYPE(Default, true, false, false, true);
IMPLEMENT_SLATE_PIXELSHADER_TYPE(Default, false, true, false, false);
IMPLEMENT_SLATE_PIXELSHADER_TYPE(Default, false, true, false, true);
IMPLEMENT_SLATE_PIXELSHADER_TYPE(Default, false, false, false, false);
IMPLEMENT_SLATE_PIXELSHADER_TYPE(Default, false, false, false, true);

IMPLEMENT_SLATE_PIXELSHADER_TYPE(GrayscaleFont, false, true, false, false);
IMPLEMENT_SLATE_PIXELSHADER_TYPE(GrayscaleFont, true, true, false, false);

IMPLEMENT_SLATE_PIXELSHADER_TYPE(ColorFont, false, true, false, false);
IMPLEMENT_SLATE_PIXELSHADER_TYPE(ColorFont, true, true, false, false);

IMPLEMENT_SLATE_PIXELSHADER_TYPE(LineSegment, false, true, false, false);
IMPLEMENT_SLATE_PIXELSHADER_TYPE(LineSegment, true, true, false, false);

IMPLEMENT_SLATE_PIXELSHADER_TYPE(RoundedBox, false, true, false, false);
IMPLEMENT_SLATE_PIXELSHADER_TYPE(RoundedBox, true, true, false, false);

IMPLEMENT_SLATE_PIXELSHADER_TYPE(SdfFont, false, true, false, false);
IMPLEMENT_SLATE_PIXELSHADER_TYPE(SdfFont, true, true, false, false);

IMPLEMENT_SLATE_PIXELSHADER_TYPE(MsdfFont, false, true, false, false);
IMPLEMENT_SLATE_PIXELSHADER_TYPE(MsdfFont, true, true, false, false);

/** The Slate vertex declaration. */
TGlobalResource<FSlateVertexDeclaration> GSlateVertexDeclaration;
TGlobalResource<FSlateInstancedVertexDeclaration> GSlateInstancedVertexDeclaration;
TGlobalResource<FSlateMaskingVertexDeclaration> GSlateMaskingVertexDeclaration;


/************************************************************************/
/* FSlateVertexDeclaration                                              */
/************************************************************************/
void FSlateVertexDeclaration::InitRHI(FRHICommandListBase& RHICmdList)
{
	FVertexDeclarationElementList Elements;
	uint16 Stride = sizeof(FSlateVertex);
	Elements.Add(FVertexElement(0, STRUCT_OFFSET(FSlateVertex, TexCoords), VET_Float4, 0, Stride));
	Elements.Add(FVertexElement(0, STRUCT_OFFSET(FSlateVertex, MaterialTexCoords), VET_Float2, 1, Stride));
	Elements.Add(FVertexElement(0, STRUCT_OFFSET(FSlateVertex, Position), VET_Float2, 2, Stride));
	Elements.Add(FVertexElement(0, STRUCT_OFFSET(FSlateVertex, Color), VET_Color, 3, Stride));
	Elements.Add(FVertexElement(0, STRUCT_OFFSET(FSlateVertex, SecondaryColor), VET_Color, 4, Stride));
	Elements.Add(FVertexElement(0, STRUCT_OFFSET(FSlateVertex, PixelSize), VET_UShort2, 5, Stride));

	VertexDeclarationRHI = PipelineStateCache::GetOrCreateVertexDeclaration(Elements);
}

void FSlateVertexDeclaration::ReleaseRHI()
{
	VertexDeclarationRHI.SafeRelease();
}


/************************************************************************/
/* FSlateInstancedVertexDeclaration                                     */
/************************************************************************/
void FSlateInstancedVertexDeclaration::InitRHI(FRHICommandListBase& RHICmdList)
{
	FVertexDeclarationElementList Elements;
	uint16 Stride = sizeof(FSlateVertex);
	Elements.Add(FVertexElement(0, STRUCT_OFFSET(FSlateVertex, TexCoords), VET_Float4, 0, Stride));
	Elements.Add(FVertexElement(0, STRUCT_OFFSET(FSlateVertex, MaterialTexCoords), VET_Float2, 1, Stride));
	Elements.Add(FVertexElement(0, STRUCT_OFFSET(FSlateVertex, Position), VET_Float2, 2, Stride));
	Elements.Add(FVertexElement(0, STRUCT_OFFSET(FSlateVertex, Color), VET_Color, 3, Stride));
	Elements.Add(FVertexElement(0, STRUCT_OFFSET(FSlateVertex, SecondaryColor), VET_Color, 4, Stride));
	Elements.Add(FVertexElement(1, 0, VET_Float4, 5, sizeof(FVector4f), true));
	
	VertexDeclarationRHI = PipelineStateCache::GetOrCreateVertexDeclaration(Elements);
}

void FSlateElementPS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.HDR.Display.OutputDevice"));
	OutEnvironment.SetDefine(TEXT("USE_709"), CVar ? (CVar->GetValueOnGameThread() == (int32)EDisplayOutputFormat::SDR_Rec709) : 1);
}


/************************************************************************/
/* FSlateMaskingVertexDeclaration                                              */
/************************************************************************/
void FSlateMaskingVertexDeclaration::InitRHI(FRHICommandListBase& RHICmdList)
{
	FVertexDeclarationElementList Elements;
	uint16 Stride = sizeof(uint32);
	Elements.Add(FVertexElement(0, 0, VET_UByte4, 0, Stride));

	VertexDeclarationRHI = PipelineStateCache::GetOrCreateVertexDeclaration(Elements);
}

void FSlateMaskingVertexDeclaration::ReleaseRHI()
{
	VertexDeclarationRHI.SafeRelease();
}


/************************************************************************/
/* FSlateDefaultVertexShader                                            */
/************************************************************************/

FSlateElementVS::FSlateElementVS( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
	: FGlobalShader(Initializer)
{
	ViewProjection.Bind(Initializer.ParameterMap, TEXT("ViewProjection"));
	VertexShaderParams.Bind( Initializer.ParameterMap, TEXT("VertexShaderParams"));
}

void FSlateElementVS::SetViewProjection(FRHIBatchedShaderParameters& BatchedParameters, const FMatrix44f& InViewProjection )
{
	SetShaderValue(BatchedParameters, ViewProjection, InViewProjection );
}

void FSlateElementVS::SetShaderParameters(FRHIBatchedShaderParameters& BatchedParameters, const FVector4f& ShaderParams )
{
	SetShaderValue(BatchedParameters, VertexShaderParams, ShaderParams );
}

/** Serializes the shader data */
/*bool FSlateElementVS::Serialize( FArchive& Ar )
{
	bool bShaderHasOutdatedParameters = FGlobalShader::Serialize( Ar );

	Ar << ViewProjection;
	Ar << VertexShaderParams;

	return bShaderHasOutdatedParameters;
}*/


/************************************************************************/
/* FSlateMaskingVS                                            */
/************************************************************************/

FSlateMaskingVS::FSlateMaskingVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	: FGlobalShader(Initializer)
{
	ViewProjection.Bind(Initializer.ParameterMap, TEXT("ViewProjection"));
	MaskRect.Bind(Initializer.ParameterMap, TEXT("MaskRectPacked"));
}

void FSlateMaskingVS::SetViewProjection(FRHIBatchedShaderParameters& BatchedParameters, const FMatrix44f& InViewProjection)
{
	SetShaderValue(BatchedParameters, ViewProjection, InViewProjection);
}

void FSlateMaskingVS::SetMaskRect(FRHIBatchedShaderParameters& BatchedParameters, const FVector2f TopLeft, const FVector2f TopRight, const FVector2f BotLeft, const FVector2f BotRight)
{
	FVector4f MaskRectVal[2] = { FVector4f(TopLeft, TopRight), FVector4f(BotLeft, BotRight) };

	SetShaderValue(BatchedParameters, MaskRect, MaskRectVal);
}

/** Serializes the shader data */
/*bool FSlateMaskingVS::Serialize(FArchive& Ar)
{
	bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);

	Ar << ViewProjection;
	Ar << MaskRect;

	return bShaderHasOutdatedParameters;
}*/
