// Copyright Epic Games, Inc. All Rights Reserved.

#include "MediaShaders.h"
#include "RenderGraphBuilder.h"
#include "RHIStaticStates.h"
#include "ShaderParameterUtils.h"




TGlobalResource<FMediaVertexDeclaration> GMediaVertexDeclaration;


/* MediaShaders namespace
 *****************************************************************************/

namespace MediaShaders
{
	/**
	 * YUV to RGB matrices for Rec601, Rec709, and Rec2020. Each are provided with a Scaled and Unscaled version.
	 *
	 * The scaling is according to SMPTE EG36, resulting in (255/219.0, 255/224.0, 255/224.0) being multiplied by the Unscaled matrix.
	 */

	const FMatrix YuvToRgbRec601Unscaled = FMatrix(
		FPlane(1.000000f,  0.00000000000f,  1.4020000000f, 0.000000f),
		FPlane(1.000000f, -0.34414362862f, -0.7141362862f, 0.000000f),
		FPlane(1.000000f,  1.77200000000f,  0.0000000000f, 0.000000f),
		FPlane(0.000000f,  0.000000f,  0.000000f, 0.000000f)
	);

	const FMatrix YuvToRgbRec601Scaled = FMatrix(
		FPlane(1.16438356164f,  0.000000000000f,  1.596026785714f, 0.000000f),
		FPlane(1.16438356164f, -0.391762290095f, -0.812967647238f, 0.000000f),
		FPlane(1.16438356164f,  2.017232142857f,  0.000000000000f, 0.000000f),
		FPlane(0.000000f,  0.000000f,  0.000000f, 0.000000f)
	);

	const FMatrix YuvToRgbRec709Unscaled = FMatrix(
		FPlane(1.000000f,  0.000000000000f,  1.574721988260f, 0.000000f),
		FPlane(1.000000f, -0.187314089535f, -0.468207470556f, 0.000000f),
		FPlane(1.000000f,  1.855615369279f,  0.000000000000f, 0.000000f),
		FPlane(0.000000f, 0.000000f, 0.000000f, 0.000000f)
	);

	const FMatrix YuvToRgbRec709Scaled = FMatrix(
		FPlane(1.16438356164f,  0.000000000000f,  1.792652263418f, 0.000000f),
		FPlane(1.16438356164f, -0.213237021569f, -0.533004040142f, 0.000000f),
		FPlane(1.16438356164f,  2.112419281991f,  0.000000000000f, 0.000000f),
		FPlane(0.000000f, 0.000000f, 0.000000f, 0.000000f)
	);

	const FMatrix YuvToRgbRec2020Unscaled = FMatrix(
		FPlane(1.000000f,  0.000000000000f,  1.474599575977f, 0.000000f),
		FPlane(1.000000f, -0.164558057720f, -0.571355048803f, 0.000000f),
		FPlane(1.000000f,  1.881396567060f,  0.000000000000f, 0.000000f),
		FPlane(0.000000f, 0.000000f, 0.000000f, 0.000000f)
	);

	const FMatrix YuvToRgbRec2020Scaled = FMatrix(
		FPlane(1.16438356164f,  0.000000000000f,  1.678673624439f, 0.000000f),
		FPlane(1.16438356164f, -0.187331717494f, -0.650426506450f, 0.000000f),
		FPlane(1.16438356164f,  2.141768413395f,  0.000000000000f, 0.000000f),
		FPlane(0.000000f, 0.000000f, 0.000000f, 0.000000f)
	);

	// YuvToSrgbPs4 is legacy. It is apparently a YuvToRgbRec709Scaled that was rounded to four digits after radix. 
	const FMatrix YuvToSrgbPs4 = FMatrix(
		FPlane(1.164400f,  0.000000f,  1.792700f, 0.000000f),
		FPlane(1.164400f, -0.213300f, -0.532900f, 0.000000f),
		FPlane(1.164400f,  2.112400f,  0.000000f, 0.000000f),
		FPlane(0.000000f,  0.000000f,  0.000000f, 0.000000f)
	);

	// Inverse of YuvToRgbRec709Scaled
	const FMatrix RgbToYuvRec709Scaled = FMatrix(
		FPlane( 0.18261938151317932966f,  0.61420368882407283539f,  0.062000459074512487279f, 0.000000f),
		FPlane(-0.10066136381366230790f, -0.33855432246084737891f,  0.439215686274509797830f, 0.000000f),
		FPlane( 0.43921568627450979783f, -0.39894445418228852152f, -0.040271232092221290189f, 0.000000f),
		FPlane(0.000000f, 0.000000f, 0.000000f, 0.000000f)
	);

	const FVector YUVOffset8bits = FVector(0.06274509803921568627f, 0.5019607843137254902f, 0.5019607843137254902f);

	const FVector YUVOffset10bits = FVector(0.06256109481915933529f, 0.50048875855327468231f, 0.50048875855327468231f);

	/** Setup YUV Offset in matrix */
	FMatrix CombineColorTransformAndOffset(const FMatrix& InMatrix, const FVector& InYUVOffset)
	{
		FMatrix Result = InMatrix;
		// Offset in last column:
		// 1) to allow for 4x4 matrix multiplication optimization when going from RGB to YUV (hence the 1.0 in the [3][3] matrix location)
		// 2) stored in empty space when going from YUV to RGB
		Result.M[0][3] = InYUVOffset.X;
		Result.M[1][3] = InYUVOffset.Y;
		Result.M[2][3] = InYUVOffset.Z;
		Result.M[3][3] = 1.0f;
		return Result;
	}
}

/* FMediaShadersVS shader
 *****************************************************************************/

IMPLEMENT_SHADER_TYPE(, FMediaShadersVS, TEXT("/Engine/Private/MediaShaders.usf"), TEXT("MainVertexShader"), SF_Vertex);


/* FAYUVConvertPS shader
 *****************************************************************************/

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FAYUVConvertUB, )
SHADER_PARAMETER(FMatrix44f, ColorTransform)
SHADER_PARAMETER(uint32, SrgbToLinear)
SHADER_PARAMETER_TEXTURE(Texture2D, Texture)
SHADER_PARAMETER_SAMPLER(SamplerState, Sampler)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FAYUVConvertUB, "AYUVConvertUB");
IMPLEMENT_SHADER_TYPE(, FAYUVConvertPS, TEXT("/Engine/Private/MediaShaders.usf"), TEXT("AYUVConvertPS"), SF_Pixel);


void FAYUVConvertPS::SetParameters(FRHICommandList& CommandList, TRefCountPtr<FRHITexture2D> AYUVTexture, const FMatrix& ColorTransform, const FVector& YUVOffset, bool SrgbToLinear)
{
	FAYUVConvertUB UB;
	{
		UB.ColorTransform = (FMatrix44f)MediaShaders::CombineColorTransformAndOffset(ColorTransform, YUVOffset);
		UB.Sampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
		UB.SrgbToLinear = SrgbToLinear;
		UB.Texture = AYUVTexture;
	}

	TUniformBufferRef<FAYUVConvertUB> Data = TUniformBufferRef<FAYUVConvertUB>::CreateUniformBufferImmediate(UB, UniformBuffer_SingleFrame);
	SetUniformBufferParameter(CommandList, CommandList.GetBoundPixelShader(), GetUniformBufferParameter<FAYUVConvertUB>(), Data);
}


/* FRGBConvertPS shader
 *****************************************************************************/

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FBMPConvertUB, )
SHADER_PARAMETER(uint32, SrgbToLinear)
SHADER_PARAMETER(FVector2f, UVScale)
SHADER_PARAMETER_TEXTURE(Texture2D, Texture)
SHADER_PARAMETER_SAMPLER(SamplerState, Sampler)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FBMPConvertUB, "BMPConvertUB");
IMPLEMENT_SHADER_TYPE(, FBMPConvertPS, TEXT("/Engine/Private/MediaShaders.usf"), TEXT("BMPConvertPS"), SF_Pixel);


void FBMPConvertPS::SetParameters(FRHICommandList& CommandList, TRefCountPtr<FRHITexture2D> BMPTexture, const FIntPoint& OutputDimensions, bool SrgbToLinear)
{
	FBMPConvertUB UB;
	{
		UB.SrgbToLinear = SrgbToLinear;
		UB.UVScale = FVector2f((float)OutputDimensions.X / (float)BMPTexture->GetSizeX(), (float)OutputDimensions.Y / (float)BMPTexture->GetSizeY());
		UB.Texture = BMPTexture;
		UB.Sampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
	}

	TUniformBufferRef<FBMPConvertUB> Data = TUniformBufferRef<FBMPConvertUB>::CreateUniformBufferImmediate(UB, UniformBuffer_SingleFrame);
	SetUniformBufferParameter(CommandList, CommandList.GetBoundPixelShader(), GetUniformBufferParameter<FBMPConvertUB>(), Data);
}


/* FNV12ConvertPS shader
 *****************************************************************************/

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FNV12ConvertUB, )
SHADER_PARAMETER(FMatrix44f, ColorTransform)
SHADER_PARAMETER(uint32, OutputWidth)
SHADER_PARAMETER(uint32, SrgbToLinear)
SHADER_PARAMETER(FVector2f, UVScale)
SHADER_PARAMETER_SRV(Texture2D, SRV_Y)
SHADER_PARAMETER_SRV(Texture2D, SRV_UV)
SHADER_PARAMETER_SAMPLER(SamplerState, SamplerB)
SHADER_PARAMETER_SAMPLER(SamplerState, SamplerP)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FNV12ConvertUB, "NV12ConvertUB");
IMPLEMENT_SHADER_TYPE(, FNV12ConvertPS, TEXT("/Engine/Private/MediaShaders.usf"), TEXT("NV12ConvertPS"), SF_Pixel);

void FNV12ConvertPS::SetParameters(FRHICommandList& CommandList, const FIntPoint & TexDim, FShaderResourceViewRHIRef SRV_Y, FShaderResourceViewRHIRef SRV_UV, const FIntPoint& OutputDimensions, const FMatrix& ColorTransform, const FVector& YUVOffset, bool SrgbToLinear)
{
	FNV12ConvertUB UB;
	{
		UB.ColorTransform = (FMatrix44f)MediaShaders::CombineColorTransformAndOffset(ColorTransform, YUVOffset);
		UB.OutputWidth = OutputDimensions.X;
		UB.SamplerB = TStaticSamplerState<SF_Bilinear>::GetRHI();
		UB.SamplerP = TStaticSamplerState<SF_Point>::GetRHI();
		UB.SrgbToLinear = SrgbToLinear;
		UB.SRV_Y = SRV_Y;
		UB.SRV_UV = SRV_UV;
		UB.UVScale = FVector2f((float)OutputDimensions.X / (float)TexDim.X, (float)OutputDimensions.Y / (float)TexDim.Y);
	}

	TUniformBufferRef<FNV12ConvertUB> Data = TUniformBufferRef<FNV12ConvertUB>::CreateUniformBufferImmediate(UB, UniformBuffer_SingleFrame);
	SetUniformBufferParameter(CommandList, CommandList.GetBoundPixelShader(), GetUniformBufferParameter<FNV12ConvertUB>(), Data);
}

/* FNV12ConvertAsBytesPS shader
 *****************************************************************************/

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FNV12ConvertAsBytesUB, )
SHADER_PARAMETER(FMatrix44f, ColorTransform)
SHADER_PARAMETER(uint32, OutputWidth)
SHADER_PARAMETER(uint32, SrgbToLinear)
SHADER_PARAMETER(FVector2f, UVScale)
SHADER_PARAMETER_TEXTURE(Texture2D, Texture)
SHADER_PARAMETER_SAMPLER(SamplerState, SamplerB)
SHADER_PARAMETER_SAMPLER(SamplerState, SamplerP)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FNV12ConvertAsBytesUB, "NV12ConvertAsBytesUB");
IMPLEMENT_SHADER_TYPE(, FNV12ConvertAsBytesPS, TEXT("/Engine/Private/MediaShaders.usf"), TEXT("NV12ConvertAsBytesPS"), SF_Pixel);

void FNV12ConvertAsBytesPS::SetParameters(FRHICommandList& CommandList, TRefCountPtr<FRHITexture2D> NV12Texture, const FIntPoint& OutputDimensions, const FMatrix& ColorTransform, const FVector& YUVOffset, bool SrgbToLinear)
{
	FNV12ConvertAsBytesUB UB;
	{
		UB.ColorTransform = (FMatrix44f)MediaShaders::CombineColorTransformAndOffset(ColorTransform, YUVOffset);
		UB.OutputWidth = OutputDimensions.X;
		UB.SamplerB = TStaticSamplerState<SF_Bilinear>::GetRHI();
		UB.SamplerP = TStaticSamplerState<SF_Point>::GetRHI();
		UB.SrgbToLinear = SrgbToLinear;
		UB.Texture = NV12Texture;
		UB.UVScale = FVector2f((float)OutputDimensions.X / (float)NV12Texture->GetSizeX(), (float)OutputDimensions.Y / (float)NV12Texture->GetSizeY());
	}

	TUniformBufferRef<FNV12ConvertAsBytesUB> Data = TUniformBufferRef<FNV12ConvertAsBytesUB>::CreateUniformBufferImmediate(UB, UniformBuffer_SingleFrame);
	SetUniformBufferParameter(CommandList, CommandList.GetBoundPixelShader(), GetUniformBufferParameter<FNV12ConvertAsBytesUB>(), Data);
}

/* FNV21ConvertPS shader
 *****************************************************************************/

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FNV21ConvertUB, )
SHADER_PARAMETER(FMatrix44f, ColorTransform)
SHADER_PARAMETER(uint32, OutputWidth)
SHADER_PARAMETER(uint32, SrgbToLinear)
SHADER_PARAMETER(FVector2f, UVScale)
SHADER_PARAMETER_TEXTURE(Texture2D, Texture)
SHADER_PARAMETER_SAMPLER(SamplerState, SamplerB)
SHADER_PARAMETER_SAMPLER(SamplerState, SamplerP)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FNV21ConvertUB, "NV21ConvertUB");
IMPLEMENT_SHADER_TYPE(, FNV21ConvertPS, TEXT("/Engine/Private/MediaShaders.usf"), TEXT("NV21ConvertPS"), SF_Pixel);


void FNV21ConvertPS::SetParameters(FRHICommandList& CommandList, TRefCountPtr<FRHITexture2D> NV21Texture, const FIntPoint& OutputDimensions, const FMatrix& ColorTransform, const FVector& YUVOffset, bool SrgbToLinear)
{
	FNV21ConvertUB UB;
	{
		UB.ColorTransform = (FMatrix44f)MediaShaders::CombineColorTransformAndOffset(ColorTransform, YUVOffset);
		UB.OutputWidth = OutputDimensions.Y;
		UB.SamplerB = TStaticSamplerState<SF_Bilinear>::GetRHI();
		UB.SamplerP = TStaticSamplerState<SF_Point>::GetRHI();
		UB.SrgbToLinear = SrgbToLinear;
		UB.Texture = NV21Texture;
		UB.UVScale = FVector2f((float)OutputDimensions.X / (float)NV21Texture->GetSizeX(), (float)OutputDimensions.Y / (float)NV21Texture->GetSizeY());
	}

	TUniformBufferRef<FNV21ConvertUB> Data = TUniformBufferRef<FNV21ConvertUB>::CreateUniformBufferImmediate(UB, UniformBuffer_SingleFrame);
	SetUniformBufferParameter(CommandList, CommandList.GetBoundPixelShader(), GetUniformBufferParameter<FNV21ConvertUB>(), Data);
}


/* FRGBConvertPS shader
 *****************************************************************************/

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FRGBConvertUB, )
SHADER_PARAMETER(FVector2f, UVScale)
SHADER_PARAMETER(uint32, SrgbToLinear)
SHADER_PARAMETER_TEXTURE(Texture2D, Texture)
SHADER_PARAMETER_SAMPLER(SamplerState, Sampler)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FRGBConvertUB, "RGBConvertUB");
IMPLEMENT_SHADER_TYPE(, FRGBConvertPS, TEXT("/Engine/Private/MediaShaders.usf"), TEXT("RGBConvertPS"), SF_Pixel);


void FRGBConvertPS::SetParameters(FRHICommandList& CommandList, TRefCountPtr<FRHITexture2D> RGBTexture, const FIntPoint& OutputDimensions, bool bSrgbToLinear)
{
	FRGBConvertUB UB;
	{
		UB.Sampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
		UB.SrgbToLinear = bSrgbToLinear;
		UB.Texture = RGBTexture;
		UB.UVScale = FVector2f((float)OutputDimensions.X / (float)RGBTexture->GetSizeX(), (float)OutputDimensions.Y / (float)RGBTexture->GetSizeY());
	}

	TUniformBufferRef<FRGBConvertUB> Data = TUniformBufferRef<FRGBConvertUB>::CreateUniformBufferImmediate(UB, UniformBuffer_SingleFrame);
	SetUniformBufferParameter(CommandList, CommandList.GetBoundPixelShader(), GetUniformBufferParameter<FRGBConvertUB>(), Data);
}


/* FYCbCrConvertUB shader
 *****************************************************************************/

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FYCbCrConvertUB, )
SHADER_PARAMETER(FMatrix44f, ColorTransform)
SHADER_PARAMETER(uint32, SrgbToLinear)
SHADER_PARAMETER_TEXTURE(Texture2D, LumaTexture)
SHADER_PARAMETER_TEXTURE(Texture2D, CbCrTexture)
SHADER_PARAMETER_SAMPLER(SamplerState, LumaSampler)
SHADER_PARAMETER_SAMPLER(SamplerState, CbCrSampler)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FYCbCrConvertUB, "YCbCrConvertUB");
IMPLEMENT_SHADER_TYPE(, FYCbCrConvertPS, TEXT("/Engine/Private/MediaShaders.usf"), TEXT("YCbCrConvertPS"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(, FYCbCrConvertPS_4x4Matrix, TEXT("/Engine/Private/MediaShaders.usf"), TEXT("YCbCrConvertPS_4x4Matrix"), SF_Pixel);


void FYCbCrConvertPS::SetParameters(FRHICommandList& CommandList, TRefCountPtr<FRHITexture2D> LumaTexture, TRefCountPtr<FRHITexture2D> CbCrTexture, const FMatrix& ColorTransform, const FVector& YUVOffset, bool SrgbToLinear)
{
	FYCbCrConvertUB UB;
	{
		// Chroma is not usually 1:1 with the output textxure
		UB.CbCrSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
		UB.CbCrTexture = CbCrTexture;
		UB.ColorTransform = (FMatrix44f)MediaShaders::CombineColorTransformAndOffset(ColorTransform, YUVOffset);
		
		// Luma should be 1:1 with the output texture and needs to be point sampled
		UB.LumaSampler = TStaticSamplerState<SF_Point>::GetRHI();
		UB.LumaTexture = LumaTexture;
		UB.SrgbToLinear = SrgbToLinear;
	}

	TUniformBufferRef<FYCbCrConvertUB> Data = TUniformBufferRef<FYCbCrConvertUB>::CreateUniformBufferImmediate(UB, UniformBuffer_SingleFrame);
	SetUniformBufferParameter(CommandList, CommandList.GetBoundPixelShader(), GetUniformBufferParameter<FYCbCrConvertUB>(), Data);	
}


/* FUYVYConvertPS shader
 *****************************************************************************/

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FUYVYConvertUB, )
SHADER_PARAMETER(FMatrix44f, ColorTransform)
SHADER_PARAMETER(uint32, SrgbToLinear)
SHADER_PARAMETER(uint32, Width)
SHADER_PARAMETER_TEXTURE(Texture2D, Texture)
SHADER_PARAMETER_SAMPLER(SamplerState, SamplerB)
SHADER_PARAMETER_SAMPLER(SamplerState, SamplerP)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FUYVYConvertUB, "UYVYConvertUB");
IMPLEMENT_SHADER_TYPE(, FUYVYConvertPS, TEXT("/Engine/Private/MediaShaders.usf"), TEXT("UYVYConvertPS"), SF_Pixel);


void FUYVYConvertPS::SetParameters(FRHICommandList& CommandList, TRefCountPtr<FRHITexture2D> UYVYTexture, const FMatrix& ColorTransform, const FVector& YUVOffset, bool SrgbToLinear)
{
	FUYVYConvertUB UB;
	{
		UB.ColorTransform = (FMatrix44f)MediaShaders::CombineColorTransformAndOffset(ColorTransform, YUVOffset);
		UB.SamplerB = TStaticSamplerState<SF_Bilinear>::GetRHI();
		UB.SamplerP = TStaticSamplerState<SF_Point>::GetRHI();
		UB.SrgbToLinear = SrgbToLinear;
		UB.Texture = UYVYTexture;
		UB.Width = UYVYTexture->GetSizeX();
	}

	TUniformBufferRef<FUYVYConvertUB> Data = TUniformBufferRef<FUYVYConvertUB>::CreateUniformBufferImmediate(UB, UniformBuffer_SingleFrame);
	SetUniformBufferParameter(CommandList, CommandList.GetBoundPixelShader(), GetUniformBufferParameter<FUYVYConvertUB>(), Data);
}


/* FYUVConvertPS shader
 *****************************************************************************/

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FYUVConvertUB, )
SHADER_PARAMETER(FMatrix44f, ColorTransform)
SHADER_PARAMETER(uint32, SrgbToLinear)
SHADER_PARAMETER(FVector2f, UVScale)
SHADER_PARAMETER_TEXTURE(Texture2D, YTexture)
SHADER_PARAMETER_TEXTURE(Texture2D, UTexture)
SHADER_PARAMETER_TEXTURE(Texture2D, VTexture)
SHADER_PARAMETER_SAMPLER(SamplerState, YSampler)
SHADER_PARAMETER_SAMPLER(SamplerState, USampler)
SHADER_PARAMETER_SAMPLER(SamplerState, VSampler)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FYUVConvertUB, "YUVConvertUB");
IMPLEMENT_SHADER_TYPE(, FYUVConvertPS, TEXT("/Engine/Private/MediaShaders.usf"), TEXT("YUVConvertPS"), SF_Pixel);


void FYUVConvertPS::SetParameters(FRHICommandList& CommandList, TRefCountPtr<FRHITexture2D> YTexture, TRefCountPtr<FRHITexture2D> UTexture, TRefCountPtr<FRHITexture2D> VTexture, const FIntPoint& OutputDimensions, const FMatrix& ColorTransform, const FVector& YUVOffset, bool SrgbToLinear)
{
	FYUVConvertUB UB;
	{
		UB.ColorTransform = (FMatrix44f)MediaShaders::CombineColorTransformAndOffset(ColorTransform, YUVOffset);
		UB.SrgbToLinear = SrgbToLinear;
		UB.YTexture = YTexture;
		UB.UTexture = UTexture;
		UB.VTexture = VTexture;
		UB.YSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
		UB.USampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
		UB.VSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
		UB.UVScale = FVector2f((float) OutputDimensions.X / (float) YTexture->GetSizeX(), (float) OutputDimensions.Y / (float) YTexture->GetSizeY());
	}

	TUniformBufferRef<FYUVConvertUB> Data = TUniformBufferRef<FYUVConvertUB>::CreateUniformBufferImmediate(UB, UniformBuffer_SingleFrame);
	SetUniformBufferParameter(CommandList, CommandList.GetBoundPixelShader(), GetUniformBufferParameter<FYUVConvertUB>(), Data);
}


/* FYUVv210ConvertPS shader
 *****************************************************************************/

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FYUVv210ConvertUB, )
SHADER_PARAMETER(FMatrix44f, ColorTransform)
SHADER_PARAMETER(uint32, SrgbToLinear)
SHADER_PARAMETER(uint32, OutputDimX)
SHADER_PARAMETER(uint32, OutputDimY)
SHADER_PARAMETER_TEXTURE(Texture2D<uint4>, YUVTexture)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FYUVv210ConvertUB, "YUVv210ConvertUB");
IMPLEMENT_SHADER_TYPE(, FYUVv210ConvertPS, TEXT("/Engine/Private/MediaShaders.usf"), TEXT("YUVv210ConvertPS"), SF_Pixel);


void FYUVv210ConvertPS::SetParameters(FRHICommandList& CommandList, TRefCountPtr<FRHITexture2D> YUVTexture, const FIntPoint& OutputDimensions, const FMatrix& ColorTransform, const FVector& YUVOffset, bool SrgbToLinear)
{
	FYUVv210ConvertUB UB;
	{
		UB.ColorTransform = (FMatrix44f)MediaShaders::CombineColorTransformAndOffset(ColorTransform, YUVOffset);
		UB.SrgbToLinear = SrgbToLinear;
		UB.OutputDimX = OutputDimensions.X;
		UB.OutputDimY = OutputDimensions.Y;
		UB.YUVTexture = YUVTexture;
	}

	TUniformBufferRef<FYUVv210ConvertUB> Data = TUniformBufferRef<FYUVv210ConvertUB>::CreateUniformBufferImmediate(UB, UniformBuffer_SingleFrame);
	SetUniformBufferParameter(CommandList, CommandList.GetBoundPixelShader(), GetUniformBufferParameter<FYUVv210ConvertUB>(), Data);
}


/* FYUVY416ConvertPS shader
 *****************************************************************************/

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FYUVY416ConvertUB, )
SHADER_PARAMETER(FMatrix44f, ColorTransform)
SHADER_PARAMETER(uint32, SrgbToLinear)
SHADER_PARAMETER_SRV(Texture2D, SRV_Y)
SHADER_PARAMETER_SAMPLER(SamplerState, BilinearClampedSamplerUV)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FYUVY416ConvertUB, "YUVY416ConvertUB");
IMPLEMENT_SHADER_TYPE(, FYUVY416ConvertPS, TEXT("/Engine/Private/MediaShaders.usf"), TEXT("YUVY416ConvertPS"), SF_Pixel);


void FYUVY416ConvertPS::SetParameters(FRHICommandList& CommandList, FShaderResourceViewRHIRef SRV_Y, const FMatrix& ColorTransform, const FVector& YUVOffset, bool SrgbToLinear)
{
	FYUVY416ConvertUB UB;
	{
		UB.ColorTransform = (FMatrix44f)MediaShaders::CombineColorTransformAndOffset(ColorTransform, YUVOffset);
		UB.SrgbToLinear = SrgbToLinear;
		UB.SRV_Y = SRV_Y;
		UB.BilinearClampedSamplerUV = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	}

	TUniformBufferRef<FYUVY416ConvertUB> Data = TUniformBufferRef<FYUVY416ConvertUB>::CreateUniformBufferImmediate(UB, UniformBuffer_SingleFrame);
	SetUniformBufferParameter(CommandList, CommandList.GetBoundPixelShader(), GetUniformBufferParameter<FYUVY416ConvertUB>(), Data);
}


/* FYUY2ConvertPS shader
 *****************************************************************************/

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FYUY2ConvertUB, )
SHADER_PARAMETER(FMatrix44f, ColorTransform)
SHADER_PARAMETER(uint32, OutputWidth)
SHADER_PARAMETER(uint32, SrgbToLinear)
SHADER_PARAMETER(FVector2f, UVScale)
SHADER_PARAMETER_TEXTURE(Texture2D, Texture)
SHADER_PARAMETER_SAMPLER(SamplerState, SamplerB)
SHADER_PARAMETER_SAMPLER(SamplerState, SamplerP)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FYUY2ConvertUB, "YUY2ConvertUB");
IMPLEMENT_SHADER_TYPE(, FYUY2ConvertPS, TEXT("/Engine/Private/MediaShaders.usf"), TEXT("YUY2ConvertPS"), SF_Pixel);


void FYUY2ConvertPS::SetParameters(FRHICommandList& CommandList, TRefCountPtr<FRHITexture2D> YUY2Texture, const FIntPoint& OutputDimensions, const FMatrix& ColorTransform, const FVector& YUVOffset, bool SrgbToLinear)
{
	FYUY2ConvertUB UB;
	{
		UB.ColorTransform = (FMatrix44f)MediaShaders::CombineColorTransformAndOffset(ColorTransform, YUVOffset);
		UB.OutputWidth = OutputDimensions.X;
		UB.SamplerB = TStaticSamplerState<SF_Bilinear>::GetRHI();
		UB.SamplerP = TStaticSamplerState<SF_Point>::GetRHI();
		UB.SrgbToLinear = SrgbToLinear;
		UB.Texture = YUY2Texture;
		UB.UVScale = FVector2f((float)OutputDimensions.X / (2.0f * YUY2Texture->GetSizeX()), (float)OutputDimensions.Y / (float)YUY2Texture->GetSizeY());
	}

	TUniformBufferRef<FYUY2ConvertUB> Data = TUniformBufferRef<FYUY2ConvertUB>::CreateUniformBufferImmediate(UB, UniformBuffer_SingleFrame);
	SetUniformBufferParameter(CommandList, CommandList.GetBoundPixelShader(), GetUniformBufferParameter<FYUY2ConvertUB>(), Data);
}


/* FYVYUConvertPS shader
 *****************************************************************************/

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FYVYUConvertUB, )
SHADER_PARAMETER(FMatrix44f, ColorTransform)
SHADER_PARAMETER(uint32, SrgbToLinear)
SHADER_PARAMETER(uint32, Width)
SHADER_PARAMETER_TEXTURE(Texture2D, Texture)
SHADER_PARAMETER_SAMPLER(SamplerState, SamplerB)
SHADER_PARAMETER_SAMPLER(SamplerState, SamplerP)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FYVYUConvertUB, "YVYUConvertUB");
IMPLEMENT_SHADER_TYPE(, FYVYUConvertPS, TEXT("/Engine/Private/MediaShaders.usf"), TEXT("YVYUConvertPS"), SF_Pixel);


void FYVYUConvertPS::SetParameters(FRHICommandList& CommandList, TRefCountPtr<FRHITexture2D> YVYUTexture, const FMatrix& ColorTransform, const FVector& YUVOffset, bool SrgbToLinear)
{
	FYVYUConvertUB UB;
	{
		UB.ColorTransform = (FMatrix44f)MediaShaders::CombineColorTransformAndOffset(ColorTransform, YUVOffset);
		UB.SamplerB = TStaticSamplerState<SF_Bilinear>::GetRHI();
		UB.SamplerP = TStaticSamplerState<SF_Point>::GetRHI();
		UB.SrgbToLinear = SrgbToLinear;
		UB.Texture = YVYUTexture;
		UB.Width = YVYUTexture->GetSizeX();
	}

	TUniformBufferRef<FYVYUConvertUB> Data = TUniformBufferRef<FYVYUConvertUB>::CreateUniformBufferImmediate(UB, UniformBuffer_SingleFrame);
	SetUniformBufferParameter(CommandList, CommandList.GetBoundPixelShader(), GetUniformBufferParameter<FYVYUConvertUB>(), Data);
}


/* FRGB8toY8ConvertPS shader
 *****************************************************************************/

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FRGB8toY8ConvertUB, )
SHADER_PARAMETER(FVector4f, ColorTransform)
SHADER_PARAMETER(uint32, LinearToSrgb)
SHADER_PARAMETER_TEXTURE(Texture2D, Texture)
SHADER_PARAMETER_SAMPLER(SamplerState, SamplerP)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FRGB8toY8ConvertUB, "RGB8toY8ConvertUB");
IMPLEMENT_SHADER_TYPE(, FRGB8toY8ConvertPS, TEXT("/Engine/Private/MediaShaders.usf"), TEXT("RGB8toY8ConvertPS"), SF_Pixel);


void FRGB8toY8ConvertPS::SetParameters(FRHICommandList& CommandList, TRefCountPtr<FRHITexture2D> RGBATexture, const FVector4f& ColorTransform, bool LinearToSrgb)
{
	FRGB8toY8ConvertUB UB;
	{
		UB.ColorTransform = ColorTransform;
		UB.SamplerP = TStaticSamplerState<SF_Point>::GetRHI();
		UB.LinearToSrgb = LinearToSrgb;
		UB.Texture = RGBATexture;
	}

	TUniformBufferRef<FRGB8toY8ConvertUB> Data = TUniformBufferRef<FRGB8toY8ConvertUB>::CreateUniformBufferImmediate(UB, UniformBuffer_SingleFrame);
	SetUniformBufferParameter(CommandList, CommandList.GetBoundPixelShader(), GetUniformBufferParameter<FRGB8toY8ConvertUB>(), Data);
}


/* FReadTextureExternalPS shader
 *****************************************************************************/

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FReadTextureExternalUB, )
SHADER_PARAMETER_TEXTURE(Texture2D, Texture)
SHADER_PARAMETER_SAMPLER(SamplerState, SamplerP)
SHADER_PARAMETER(FLinearColor, ScaleRotation)
SHADER_PARAMETER(FVector2f, Offset)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FReadTextureExternalUB, "ReadTextureExternalUB");
IMPLEMENT_SHADER_TYPE(, FReadTextureExternalPS, TEXT("/Engine/Private/MediaShaders.usf"), TEXT("ReadTextureExternalPS"), SF_Pixel);


void FReadTextureExternalPS::SetParameters(FRHICommandList& CommandList, FTextureRHIRef TextureExt, FSamplerStateRHIRef SamplerState, const FLinearColor& ScaleRotation, const FLinearColor& Offset)
{
	FReadTextureExternalUB UB;
	{
		UB.SamplerP = SamplerState;
		UB.Texture = TextureExt;
		UB.ScaleRotation = ScaleRotation;
		UB.Offset = FVector2f(Offset.R, Offset.G);
	}

	TUniformBufferRef<FReadTextureExternalUB> Data = TUniformBufferRef<FReadTextureExternalUB>::CreateUniformBufferImmediate(UB, UniformBuffer_SingleFrame);
	SetUniformBufferParameter(CommandList, CommandList.GetBoundPixelShader(), GetUniformBufferParameter<FReadTextureExternalUB>(), Data);
}

/* FRGB8toUYVY8ConvertPS shader
 *****************************************************************************/

IMPLEMENT_GLOBAL_SHADER(FRGB8toUYVY8ConvertPS, "/Engine/Private/MediaShaders.usf", "RGB8toUYVY8ConvertPS", SF_Pixel);

FRGB8toUYVY8ConvertPS::FParameters* FRGB8toUYVY8ConvertPS::AllocateAndSetParameters(FRDGBuilder& GraphBuilder, FRDGTextureRef RGBATexture, const FMatrix& ColorTransform, const FVector& YUVOffset, bool bDoLinearToSrgb, FRDGTextureRef OutputTexture)
{
	FRGB8toUYVY8ConvertPS::FParameters* Parameters = GraphBuilder.AllocParameters<FRGB8toUYVY8ConvertPS::FParameters>();

	Parameters->RGBToYUVConversion.InputTexture = RGBATexture;
	Parameters->RGBToYUVConversion.InputSampler = TStaticSamplerState<SF_Point>::GetRHI();
	Parameters->RGBToYUVConversion.ColorTransform = (FMatrix44f)MediaShaders::CombineColorTransformAndOffset(ColorTransform, YUVOffset);
	Parameters->RGBToYUVConversion.DoLinearToSrgb = bDoLinearToSrgb;
	Parameters->RGBToYUVConversion.OnePixelDeltaX = 1.0f / (float)RGBATexture->Desc.Extent.X;
	Parameters->RenderTargets[0] = FRenderTargetBinding{ OutputTexture, ERenderTargetLoadAction::ENoAction };

	return Parameters;
}


/* FRGB10toYUVv210ConvertPS shader
 *****************************************************************************/

IMPLEMENT_GLOBAL_SHADER(FRGB10toYUVv210ConvertPS, "/Engine/Private/MediaShaders.usf", "RGB10toYUVv210ConvertPS", SF_Pixel);

FRGB10toYUVv210ConvertPS::FParameters* FRGB10toYUVv210ConvertPS::AllocateAndSetParameters(FRDGBuilder& GraphBuilder, FRDGTextureRef RGBATexture, const FMatrix& ColorTransform, const FVector& YUVOffset, bool bDoLinearToSrgb, FRDGTextureRef OutputTexture)
{
	FRGB10toYUVv210ConvertPS::FParameters* Parameters = GraphBuilder.AllocParameters<FRGB10toYUVv210ConvertPS::FParameters>();

	Parameters->RGBToYUVConversion.InputTexture = RGBATexture;
	Parameters->RGBToYUVConversion.InputSampler = TStaticSamplerState<SF_Point>::GetRHI();
	Parameters->RGBToYUVConversion.ColorTransform = (FMatrix44f)MediaShaders::CombineColorTransformAndOffset(ColorTransform, YUVOffset);
	Parameters->RGBToYUVConversion.DoLinearToSrgb = bDoLinearToSrgb;
	Parameters->RGBToYUVConversion.OnePixelDeltaX = 1.0f / (float)RGBATexture->Desc.Extent.X;

	//Output texture will be based on a size dividable by 48 (i.e 1280 -> 1296) and divided by 6 (i.e 1296 / 6 = 216)
	//To map output texture UVs, we get a scale from the source texture original size to the mapped output size
	//And use the source texture size to get the pixel delta
	const float PaddedResolution = float(uint32((RGBATexture->Desc.Extent.X + 47) / 48) * 48);
	Parameters->PaddingScale = PaddedResolution / (float)RGBATexture->Desc.Extent.X;

	Parameters->RenderTargets[0] = FRenderTargetBinding{ OutputTexture, ERenderTargetLoadAction::ENoAction };

	return Parameters;
}


/* FModifyAlphaSwizzleRgbaPS shader
 *****************************************************************************/

IMPLEMENT_GLOBAL_SHADER(FModifyAlphaSwizzleRgbaPS, "/Engine/Private/MediaShaders.usf", "SwizzleRgbPS", SF_Pixel);

FModifyAlphaSwizzleRgbaPS::FParameters* FModifyAlphaSwizzleRgbaPS::AllocateAndSetParameters(FRDGBuilder& GraphBuilder, FRDGTextureRef RGBATexture, FRDGTextureRef OutputTexture)
{
	FModifyAlphaSwizzleRgbaPS::FParameters* Parameters = GraphBuilder.AllocParameters<FModifyAlphaSwizzleRgbaPS::FParameters>();

	Parameters->InputTexture = RGBATexture;
	Parameters->InputSampler = TStaticSamplerState<SF_Point>::GetRHI();
	Parameters->RenderTargets[0] = FRenderTargetBinding{ OutputTexture, ERenderTargetLoadAction::ENoAction };

	return Parameters;
}


