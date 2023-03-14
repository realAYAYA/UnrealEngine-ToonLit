// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VolumeRendering.h: Volume rendering definitions.
=============================================================================*/

#pragma once

#include "Shader.h"
#include "ShaderCompilerCore.h"
#include "ShaderParameters.h"
#include "GlobalShader.h"
#include "ShaderParameterUtils.h"
#include "ScreenRendering.h"

/** Represents a subregion of a volume texture. */
struct FVolumeBounds
{
	int32 MinX, MinY, MinZ;
	int32 MaxX, MaxY, MaxZ;

	FVolumeBounds() :
		MinX(0),
		MinY(0),
		MinZ(0),
		MaxX(0),
		MaxY(0),
		MaxZ(0)
	{}

	FVolumeBounds(int32 Max) :
		MinX(0),
		MinY(0),
		MinZ(0),
		MaxX(Max),
		MaxY(Max),
		MaxZ(Max)
	{}

	bool IsValid() const
	{
		return MaxX > MinX && MaxY > MinY && MaxZ > MinZ;
	}
};

/** Vertex shader used to write to a range of slices of a 3d volume texture. */
class FWriteToSliceVS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FWriteToSliceVS,Global,ENGINE_API);
public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) 
	{ 
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5); 
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment )
	{
		FGlobalShader::ModifyCompilationEnvironment( Parameters, OutEnvironment );
		OutEnvironment.CompilerFlags.Add( CFLAG_VertexToGeometryShader );
	}

	FWriteToSliceVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		UVScaleBias.Bind(Initializer.ParameterMap, TEXT("UVScaleBias"));
		MinZ.Bind(Initializer.ParameterMap, TEXT("MinZ"));
	}

	FWriteToSliceVS() {}

	template <typename TRHICommandList>
	void SetParameters(TRHICommandList& RHICmdList, const FVolumeBounds& VolumeBounds, FIntVector VolumeResolution)
	{
		const float InvVolumeResolutionX = 1.0f / VolumeResolution.X;
		const float InvVolumeResolutionY = 1.0f / VolumeResolution.Y;
		SetShaderValue(RHICmdList, RHICmdList.GetBoundVertexShader(), UVScaleBias, FVector4f(
			(VolumeBounds.MaxX - VolumeBounds.MinX) * InvVolumeResolutionX,
			(VolumeBounds.MaxY - VolumeBounds.MinY) * InvVolumeResolutionY,
			VolumeBounds.MinX * InvVolumeResolutionX,
			VolumeBounds.MinY * InvVolumeResolutionY));
		SetShaderValue(RHICmdList, RHICmdList.GetBoundVertexShader(), MinZ, VolumeBounds.MinZ);
	}

private:
	LAYOUT_FIELD(FShaderParameter, UVScaleBias);
	LAYOUT_FIELD(FShaderParameter, MinZ);
};

/** Geometry shader used to write to a range of slices of a 3d volume texture. */
class FWriteToSliceGS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FWriteToSliceGS,Global,ENGINE_API);
public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) 
	{ 
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) && RHISupportsGeometryShaders(Parameters.Platform); 
	}

	FWriteToSliceGS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		MinZ.Bind(Initializer.ParameterMap, TEXT("MinZ"));
	}
	FWriteToSliceGS() {}

	template <typename TRHICommandList>
	void SetParameters(TRHICommandList& RHICmdList, int32 MinZValue)
	{
		SetShaderValue(RHICmdList, RHICmdList.GetBoundGeometryShader(), MinZ, MinZValue);
	}

private:
	LAYOUT_FIELD(FShaderParameter, MinZ);
};

// This function assumes the PSO had a PrimitiveType of PT_TriangleStrip
extern ENGINE_API void RasterizeToVolumeTexture(FRHICommandList& RHICmdList, FVolumeBounds VolumeBounds);

/** Vertex buffer used for rendering into a volume texture. */
class FVolumeRasterizeVertexBuffer : public FVertexBuffer
{
public:

	virtual void InitRHI() override
	{
		// Used as a non-indexed triangle strip, so 4 vertices per quad
		const uint32 Size = 4 * sizeof(FScreenVertex);
		FRHIResourceCreateInfo CreateInfo(TEXT("FVolumeRasterizeVertexBuffer"));
		VertexBufferRHI = RHICreateBuffer(Size, BUF_Static | BUF_VertexBuffer, 0, ERHIAccess::VertexOrIndexBuffer, CreateInfo);
		FScreenVertex* DestVertex = (FScreenVertex*)RHILockBuffer(VertexBufferRHI, 0, Size, RLM_WriteOnly);

		// Setup a full - render target quad
		// A viewport and UVScaleBias will be used to implement rendering to a sub region
		DestVertex[0].Position = FVector2f(1, -GProjectionSignY);
		DestVertex[0].UV = FVector2f(1, 1);
		DestVertex[1].Position = FVector2f(1, GProjectionSignY);
		DestVertex[1].UV = FVector2f(1, 0);
		DestVertex[2].Position = FVector2f(-1, -GProjectionSignY);
		DestVertex[2].UV = FVector2f(0, 1);
		DestVertex[3].Position = FVector2f(-1, GProjectionSignY);
		DestVertex[3].UV = FVector2f(0, 0);

		RHIUnlockBuffer(VertexBufferRHI);      
	}
};

extern ENGINE_API TGlobalResource<FVolumeRasterizeVertexBuffer> GVolumeRasterizeVertexBuffer;