// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "RenderResource.h"
#include "ShaderParameters.h"
#include "Shader.h"
#include "GlobalShader.h"
#include "ShaderParameterUtils.h"
#include "Rendering/RenderingCommon.h"
#include "RHIStaticStates.h"
#include "TextureResource.h"
#include "RenderUtils.h"

extern EColorVisionDeficiency GSlateColorDeficiencyType;
extern int32 GSlateColorDeficiencySeverity;
extern bool GSlateColorDeficiencyCorrection;
extern bool GSlateShowColorDeficiencyCorrectionWithDeficiency;

/**
 * The vertex declaration for the slate vertex shader
 */
class FSlateVertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;

	virtual ~FSlateVertexDeclaration() {}

	/** Initializes the vertex declaration RHI resource */
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;

	/** Releases the vertex declaration RHI resource */
	virtual void ReleaseRHI() override;
};

/**
 * The vertex declaration for the slate instanced vertex shader
 */
class FSlateInstancedVertexDeclaration : public FSlateVertexDeclaration
{
public:
	virtual ~FSlateInstancedVertexDeclaration() {}
	
	/** Initializes the vertex declaration RHI resource */
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;
};

class FSlateMaskingVertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;

	virtual ~FSlateMaskingVertexDeclaration() {}

	/** Initializes the vertex declaration RHI resource */
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;

	/** Releases the vertex declaration RHI resource */
	virtual void ReleaseRHI() override;
};

/** The slate Vertex shader representation */
class FSlateElementVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FSlateElementVS, Global);
public:
	/** Indicates that this shader should be cached */
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return true; }

	/** Constructor.  Binds all parameters used by the shader */
	FSlateElementVS( const ShaderMetaType::CompiledShaderInitializerType& Initializer );

	FSlateElementVS() {}

	/** 
	 * Sets the view projection parameter
	 *
	 * @param InViewProjection	The ViewProjection matrix to use when this shader is bound 
	 */
	void SetViewProjection(FRHIBatchedShaderParameters& BatchedParameters, const FMatrix44f& InViewProjection );

	/** 
	 * Sets shader parameters for use in this shader
	 *
	 * @param ShaderParams	The shader params to be used
	 */
	void SetShaderParameters(FRHIBatchedShaderParameters& BatchedParameters, const FVector4f& ShaderParams );

	/** Serializes the shader data */
	//virtual bool Serialize( FArchive& Ar ) override;

private:
	/** ViewProjection parameter used by the shader */
	LAYOUT_FIELD(FShaderParameter, ViewProjection)
	/** Shader parmeters used by the shader */
	LAYOUT_FIELD(FShaderParameter, VertexShaderParams)
};

/** 
 * Base class slate pixel shader for all elements
 */
class FSlateElementPS : public FGlobalShader
{
	DECLARE_TYPE_LAYOUT(FSlateElementPS, NonVirtual);
public:
	/** Indicates that this shader should be cached */
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) 
	{ 
		return true; 
	}

	FSlateElementPS()
	{
	}

	/** Constructor.  Binds all parameters used by the shader */
	FSlateElementPS( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
		: FGlobalShader( Initializer )
	{
		TextureParameter.Bind(Initializer.ParameterMap, TEXT("ElementTexture"));
		TextureParameterSampler.Bind(Initializer.ParameterMap, TEXT("ElementTextureSampler"));
		InPageTableTexture.Bind(Initializer.ParameterMap, TEXT("InPageTableTexture"));
		VTPackedPageTableUniform.Bind(Initializer.ParameterMap, TEXT("VTPackedPageTableUniform"));
		VTPackedUniform.Bind(Initializer.ParameterMap, TEXT("VTPackedUniform"));
		ShaderParams.Bind(Initializer.ParameterMap, TEXT("ShaderParams"));
		ShaderParams2.Bind(Initializer.ParameterMap, TEXT("ShaderParams2"));
		VTShaderParams.Bind(Initializer.ParameterMap, TEXT("VTShaderParams"));
		GammaAndAlphaValues.Bind(Initializer.ParameterMap,TEXT("GammaAndAlphaValues"));
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);

	/**
	 * Sets the texture used by this shader 
	 *
	 * @param Texture	Texture resource to use when this pixel shader is bound
	 * @param SamplerState	Sampler state to use when sampling this texture
	 */
	void SetTexture(FRHIBatchedShaderParameters& BatchedParameters, FRHITexture* InTexture, const FSamplerStateRHIRef SamplerState )
	{
		SetTextureParameter(BatchedParameters, TextureParameter, TextureParameterSampler, SamplerState, InTexture );
	}

	/**
	 * Sets the texture used by this shader in case a VirtualTexture is used
	 *
	 * @param InVirtualTexture	Virtual Texture resource to use when this pixel shader is bound
	 */
	void SetVirtualTextureParameters(FRHIBatchedShaderParameters& BatchedParameters, FVirtualTexture2DResource* InVirtualTexture)
	{
		if (InVirtualTexture == nullptr)
		{
			return;
		}

		IAllocatedVirtualTexture* AllocatedVT = InVirtualTexture->AcquireAllocatedVT();
		uint32 LayerIndex = 0;

		FRHIShaderResourceView* PhysicalView = AllocatedVT->GetPhysicalTextureSRV(LayerIndex, InVirtualTexture->bSRGB);
		
		SetSRVParameter(BatchedParameters, TextureParameter, PhysicalView);
		SetSamplerParameter(BatchedParameters, TextureParameterSampler, InVirtualTexture->SamplerStateRHI);
		SetTextureParameter(BatchedParameters, InPageTableTexture, AllocatedVT->GetPageTableTexture(0u));
		
		FUintVector4 PageTableUniform[2];
		FUintVector4 Uniform;
		// VTParams.X = MipLevel, VTParams.Y = LayerIndex
		FVector4f VTParams{ 0.f, static_cast<float>(LayerIndex), 0.f, 0.f };

		AllocatedVT->GetPackedPageTableUniform(PageTableUniform);
		AllocatedVT->GetPackedUniform(&Uniform, LayerIndex);

		SetShaderValueArray(BatchedParameters, VTPackedPageTableUniform, PageTableUniform, UE_ARRAY_COUNT(PageTableUniform));
		SetShaderValue(BatchedParameters, VTPackedUniform, Uniform);
		SetShaderValue(BatchedParameters, VTShaderParams, VTParams);
	}

	/**
	 * Sets shader params used by the shader
	 * 
	 * @param InShaderParams Shader params to use
	 */
	void SetShaderParams(FRHIBatchedShaderParameters& BatchedParameters, const FShaderParams& InShaderParams)
	{
		SetShaderValue(BatchedParameters, ShaderParams, InShaderParams.PixelParams);
		SetShaderValue(BatchedParameters, ShaderParams2, InShaderParams.PixelParams2);
	}

	/**
	 * Sets the display gamma.
	 *
	 * @param DisplayGamma The display gamma to use
	 */
	void SetDisplayGammaAndInvertAlphaAndContrast(FRHIBatchedShaderParameters& BatchedParameters, float InDisplayGamma, float bInvertAlpha, float InContrast)
	{
		FVector4f Values( 2.2f / InDisplayGamma, 1.0f/InDisplayGamma, bInvertAlpha, InContrast);

		SetShaderValue(BatchedParameters, GammaAndAlphaValues, Values);
	}

private:
	
	/** Texture parameter used by the shader */
	LAYOUT_FIELD(FShaderResourceParameter, TextureParameter);
	LAYOUT_FIELD(FShaderResourceParameter, TextureParameterSampler);
	LAYOUT_FIELD(FShaderResourceParameter, InPageTableTexture);
	LAYOUT_FIELD(FShaderParameter, VTPackedPageTableUniform);
	LAYOUT_FIELD(FShaderParameter, VTPackedUniform);
	LAYOUT_FIELD(FShaderParameter, ShaderParams);
	LAYOUT_FIELD(FShaderParameter, ShaderParams2);
	LAYOUT_FIELD(FShaderParameter, VTShaderParams);
	LAYOUT_FIELD(FShaderParameter, GammaAndAlphaValues);
};

/** 
 * Pixel shader types for all elements
 */
template<ESlateShader ShaderType, bool bDrawDisabledEffect, bool bUseTextureAlpha=true, bool bUseTextureGrayscale=false, bool bIsVirtualTexture=false>
class TSlateElementPS : public FSlateElementPS
{
	DECLARE_SHADER_TYPE( TSlateElementPS, Global );
public:

	TSlateElementPS()
	{
	}

	/** Constructor.  Binds all parameters used by the shader */
	TSlateElementPS( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
		: FSlateElementPS( Initializer )
	{
	}


	/**
	 * Modifies the compilation of this shader
	 */
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		// Set defines based on what this shader will be used for
		OutEnvironment.SetDefine(TEXT("SHADER_TYPE"), (uint32)ShaderType);
		OutEnvironment.SetDefine(TEXT("DRAW_DISABLED_EFFECT"), (uint32)( bDrawDisabledEffect ? 1 : 0 ));
		OutEnvironment.SetDefine(TEXT("USE_TEXTURE_ALPHA"), (uint32)( bUseTextureAlpha ? 1 : 0 ));
		OutEnvironment.SetDefine(TEXT("USE_MATERIALS"), (uint32)0);
		OutEnvironment.SetDefine(TEXT("USE_TEXTURE_GRAYSCALE"), (uint32)(bUseTextureGrayscale ? 1 : 0));
		OutEnvironment.SetDefine(TEXT("SAMPLE_VIRTUAL_TEXTURE"), (uint32)(bIsVirtualTexture ? 1 : 0));
		
		FSlateElementPS::ModifyCompilationEnvironment( Parameters, OutEnvironment );
	}
};

/** 
 * Pixel shader for debugging Slate overdraw
 */
class FSlateDebugOverdrawPS : public FSlateElementPS
{	
	DECLARE_SHADER_TYPE( FSlateDebugOverdrawPS, Global );
public:
	/** Indicates that this shader should be cached */
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) 
	{ 
		return true; 
	}

	FSlateDebugOverdrawPS()
	{
	}

	/** Constructor.  Binds all parameters used by the shader */
	FSlateDebugOverdrawPS( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
		: FSlateElementPS( Initializer )
	{
	}
};

/** 
 * Pixel shader for debugging Slate overdraw
 */
class FSlateDebugBatchingPS : public FSlateElementPS
{	
	DECLARE_SHADER_TYPE(FSlateDebugBatchingPS, Global );
public:
	/** Indicates that this shader should be cached */
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) 
	{ 
		return true; 
	}

	FSlateDebugBatchingPS()
	{
	}

	/** Constructor.  Binds all parameters used by the shader */
	FSlateDebugBatchingPS( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
		: FSlateElementPS( Initializer )
	{
		BatchColor.Bind(Initializer.ParameterMap, TEXT("BatchColor"));
	}

	/**
	* Sets shader params used by the shader
	*
	* @param InShaderParams Shader params to use
	*/
	void SetBatchColor(FRHIBatchedShaderParameters& BatchedParameters, const FLinearColor& InBatchColor)
	{
		SetShaderValue(BatchedParameters, BatchColor, InBatchColor);
	}

private:
	LAYOUT_FIELD(FShaderParameter, BatchColor);
};

const int32 MAX_BLUR_SAMPLES = 127;

class FSlatePostProcessBlurPS : public FSlateElementPS
{
	DECLARE_SHADER_TYPE(FSlatePostProcessBlurPS, Global);
public:
	/** Indicates that this shader should be cached */
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}

	FSlatePostProcessBlurPS()
	{
	}

	/** Constructor.  Binds all parameters used by the shader */
	FSlatePostProcessBlurPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FSlateElementPS(Initializer)
	{
		BufferSizeAndDirection.Bind(Initializer.ParameterMap, TEXT("BufferSizeAndDirection"));
		WeightAndOffsets.Bind(Initializer.ParameterMap, TEXT("WeightAndOffsets"));
		SampleCount.Bind(Initializer.ParameterMap, TEXT("SampleCount"));
		UVBounds.Bind(Initializer.ParameterMap, TEXT("UVBounds"));
	}

	void SetBufferSizeAndDirection(FRHIBatchedShaderParameters& BatchedParameters, const FVector2f InBufferSize, const FVector2f InDir)
	{
		SetShaderValue(BatchedParameters, BufferSizeAndDirection, FVector4f(InBufferSize, InDir));
	}

	void SetWeightsAndOffsets(FRHIBatchedShaderParameters& BatchedParameters, const TArray<FVector4f>& InWeightsAndOffsets, int32 NumSamples )
	{
		check(InWeightsAndOffsets.Num() <= MAX_BLUR_SAMPLES);
		SetShaderValueArray<FVector4f>(BatchedParameters, WeightAndOffsets, InWeightsAndOffsets.GetData(), InWeightsAndOffsets.Num() );
		SetShaderValue(BatchedParameters, SampleCount, NumSamples);
	}

	void SetUVBounds(FRHIBatchedShaderParameters& BatchedParameters, const FVector4f& InUVBounds)
	{
		SetShaderValue(BatchedParameters, UVBounds, InUVBounds);
	}

private:
	LAYOUT_FIELD(FShaderParameter, BufferSizeAndDirection);
	LAYOUT_FIELD(FShaderParameter, WeightAndOffsets);
	LAYOUT_FIELD(FShaderParameter, SampleCount);
	LAYOUT_FIELD(FShaderParameter, UVBounds);
};


class FSlatePostProcessDownsamplePS : public FSlateElementPS
{
	DECLARE_SHADER_TYPE(FSlatePostProcessDownsamplePS, Global);
public:
	/** Indicates that this shader should be cached */
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}

	FSlatePostProcessDownsamplePS()
	{
	}

	/** Constructor.  Binds all parameters used by the shader */
	FSlatePostProcessDownsamplePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FSlateElementPS(Initializer)
	{
		UVBounds.Bind(Initializer.ParameterMap, TEXT("UVBounds"));
	}

	void SetUVBounds(FRHIBatchedShaderParameters& BatchedParameters, const FVector4f& InUVBounds)
	{
		SetShaderValue(BatchedParameters, UVBounds, InUVBounds);
	}

private:
	LAYOUT_FIELD(FShaderParameter, UVBounds);
};

enum class ESlatePostProcessUpsamplePSPermutation
{
	SDR = 0,
	HDR_SCRGB,
	HDR_PQ10,
};

template<ESlatePostProcessUpsamplePSPermutation SlatePostProcessUpsamplePSPermutation>
class FSlatePostProcessUpsamplePS : public FSlateElementPS
{
	DECLARE_SHADER_TYPE(FSlatePostProcessUpsamplePS, Global);
public:
	/** Indicates that this shader should be cached */
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}

	FSlatePostProcessUpsamplePS()
	{
	}

	/** Constructor.  Binds all parameters used by the shader */
	FSlatePostProcessUpsamplePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FSlateElementPS(Initializer)
	{
	}

	/**
	 * Modifies the compilation of this shader
	 */
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		// Set defines based on what this shader will be used for
		OutEnvironment.SetDefine(TEXT("OUTPUT_TO_UI_TARGET"), (uint32)(SlatePostProcessUpsamplePSPermutation != ESlatePostProcessUpsamplePSPermutation::SDR ? 1 : 0));
		OutEnvironment.SetDefine(TEXT("SCRGB_ENCODING"), SlatePostProcessUpsamplePSPermutation == ESlatePostProcessUpsamplePSPermutation::HDR_SCRGB);
		FSlateElementPS::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

private:
};


class FSlatePostProcessColorDeficiencyPS : public FSlateElementPS
{
	DECLARE_SHADER_TYPE(FSlatePostProcessColorDeficiencyPS, Global);
public:
	/** Indicates that this shader should be cached */
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}

	FSlatePostProcessColorDeficiencyPS()
	{
	}

	/** Constructor.  Binds all parameters used by the shader */
	FSlatePostProcessColorDeficiencyPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FSlateElementPS(Initializer)
	{
		ColorVisionDeficiencyType.Bind(Initializer.ParameterMap, TEXT("ColorVisionDeficiencyType"));
		ColorVisionDeficiencySeverity.Bind(Initializer.ParameterMap, TEXT("ColorVisionDeficiencySeverity"));
		bCorrectDeficiency.Bind(Initializer.ParameterMap, TEXT("bCorrectDeficiency"));
		bSimulateCorrectionWithDeficiency.Bind(Initializer.ParameterMap, TEXT("bSimulateCorrectionWithDeficiency"));
	}

	void SetColorRules(FRHIBatchedShaderParameters& BatchedParameters, bool bCorrect, EColorVisionDeficiency DeficiencyType, int32 Severity)
	{
		SetShaderValue(BatchedParameters, ColorVisionDeficiencyType, (float)DeficiencyType);
		SetShaderValue(BatchedParameters, ColorVisionDeficiencySeverity, (float)Severity);
		SetShaderValue(BatchedParameters, bCorrectDeficiency, bCorrect ? 1.0f : 0.0f);
	}

	void SetShowCorrectionWithDeficiency(FRHIBatchedShaderParameters& BatchedParameters, bool bShowCorrectionWithDeficiency)
	{
		SetShaderValue(BatchedParameters, bSimulateCorrectionWithDeficiency, bShowCorrectionWithDeficiency ? 1.0f : 0.0f);
	}

private:
	LAYOUT_FIELD(FShaderParameter, ColorVisionDeficiencyType);
	LAYOUT_FIELD(FShaderParameter, ColorVisionDeficiencySeverity);
	LAYOUT_FIELD(FShaderParameter, bCorrectDeficiency);
	LAYOUT_FIELD(FShaderParameter, bSimulateCorrectionWithDeficiency);
};


class FSlateMaskingVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FSlateMaskingVS, Global);
public:
	/** Indicates that this shader should be cached */
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}

	FSlateMaskingVS()
	{
	}

	/** Constructor.  Binds all parameters used by the shader */
	FSlateMaskingVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer);

	/**
	* Sets the view projection parameter
	*
	* @param InViewProjection	The ViewProjection matrix to use when this shader is bound
	*/
	void SetViewProjection(FRHIBatchedShaderParameters& BatchedParameters, const FMatrix44f& InViewProjection);

	/**
	 * Sets the mask rect positions
	 */
	void SetMaskRect(FRHIBatchedShaderParameters& BatchedParameters, const FVector2f TopLeft, const FVector2f TopRight, const FVector2f BotLeft, const FVector2f BotRight);

	//virtual bool Serialize(FArchive& Ar) override;

private:
	/** Mask rect parameter */
	LAYOUT_FIELD(FShaderParameter, MaskRect)
	/** ViewProjection parameter used by the shader */
	LAYOUT_FIELD(FShaderParameter, ViewProjection)
};

class FSlateMaskingPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FSlateMaskingPS, Global);
public:
	/** Indicates that this shader should be cached */
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}

	FSlateMaskingPS()
	{
	}

	/** Constructor.  Binds all parameters used by the shader */
	FSlateMaskingPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
	}
};


#if WITH_EDITOR
// Pixel shader to convert UI from linear rec709 to PQ 2020 for HDR monitors
class FHDREditorConvertPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FHDREditorConvertPS, Global);
public:

	FHDREditorConvertPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
		SceneTexture.Bind(Initializer.ParameterMap, TEXT("SceneTexture"));
		SceneSampler.Bind(Initializer.ParameterMap, TEXT("SceneSampler"));
		UILevel.Bind(Initializer.ParameterMap, TEXT("UILevel"));
	}
	FHDREditorConvertPS() {}

	void SetParameters(FRHIBatchedShaderParameters& BatchedParameters, FRHITexture* SceneTextureRHI)
	{
		SetTextureParameter(BatchedParameters, SceneTexture, SceneSampler, TStaticSamplerState<SF_Point>::GetRHI(), SceneTextureRHI);
		
		static auto CVarHDRNITLevel = IConsoleManager::Get().FindConsoleVariable(TEXT("Editor.HDRNITLevel"));
		SetShaderValue(BatchedParameters, UILevel, CVarHDRNITLevel->GetFloat());
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("/Engine/Private/CompositeUIPixelShader.usf");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("HDREditorConvert");
	}

private:
	LAYOUT_FIELD(FShaderResourceParameter, SceneTexture);
	LAYOUT_FIELD(FShaderResourceParameter, SceneSampler);
	LAYOUT_FIELD(FShaderParameter, UILevel);
};
#endif

/** The simple element vertex declaration. */
extern TGlobalResource<FSlateVertexDeclaration> GSlateVertexDeclaration;

/** The instanced simple element vertex declaration. */
extern TGlobalResource<FSlateInstancedVertexDeclaration> GSlateInstancedVertexDeclaration;

/** The vertex declaration for rendering stencil masks. */
extern TGlobalResource<FSlateMaskingVertexDeclaration> GSlateMaskingVertexDeclaration;
