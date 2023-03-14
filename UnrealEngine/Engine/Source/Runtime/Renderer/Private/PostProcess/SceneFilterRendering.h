// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SceneFilterRendering.h: Filter rendering definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RenderResource.h"
#include "RendererInterface.h"
#include "ShaderParameters.h"
#include "Shader.h"
#include "PostProcess/SceneRenderTargets.h"
#include "ShaderParameterUtils.h"

/** Uniform buffer for computing the vertex positional and UV adjustments in the vertex shader. */
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT( FDrawRectangleParameters, RENDERER_API)
	SHADER_PARAMETER( FVector4f, PosScaleBias )
	SHADER_PARAMETER( FVector4f, UVScaleBias )
	SHADER_PARAMETER( FVector4f, InvTargetSizeAndTextureSize )
END_GLOBAL_SHADER_PARAMETER_STRUCT()

/**
 * Draws a quad with the given vertex positions and UVs in denormalized pixel/texel coordinates.
 * The platform-dependent mapping from pixels to texels is done automatically.
 * Note that the positions are affected by the current viewport.
 * NOTE: DrawRectangle should be used in the vertex shader to calculate the correct position and uv for vertices.
 * NOTE2: Assumes previously set PSO has PrimitiveType = PT_TriangleList
 *
 * X, Y							Position in screen pixels of the top left corner of the quad
 * SizeX, SizeY					Size in screen pixels of the quad
 * U, V							Position in texels of the top left corner of the quad's UV's
 * SizeU, SizeV					Size in texels of the quad's UV's
 * TargetSizeX, TargetSizeY		Size in screen pixels of the target surface
 * TextureSize                  Size in texels of the source texture
 * VertexShader					The vertex shader used for rendering
 * Flags						see EDrawRectangleFlags
 * InstanceCount				Number of instances of rectangle
 */
extern RENDERER_API void DrawRectangle(
	FRHICommandList& RHICmdList,
	float X,
	float Y,
	float SizeX,
	float SizeY,
	float U,
	float V,
	float SizeU,
	float SizeV,
	FIntPoint TargetSize,
	FIntPoint TextureSize,
	const TShaderRef<FShader>& VertexShader,
	EDrawRectangleFlags Flags = EDRF_Default,
	uint32 InstanceCount = 1
	);

// NOTE: Assumes previously set PSO has PrimitiveType = PT_TriangleList
extern RENDERER_API void DrawTransformedRectangle(
	FRHICommandListImmediate& RHICmdList,
	float X,
	float Y,
	float SizeX,
	float SizeY,
	const FMatrix& PosTransform,
	float U,
	float V,
	float SizeU,
	float SizeV,
	const FMatrix& TexTransform,
	FIntPoint TargetSize,
	FIntPoint TextureSize
	);

// NOTE: Assumes previously set PSO has PrimitiveType = PT_TriangleList
extern RENDERER_API void DrawHmdMesh(
	FRHICommandList& RHICmdList,
	float X,
	float Y,
	float SizeX,
	float SizeY,
	float U,
	float V,
	float SizeU,
	float SizeV,
	FIntPoint TargetSize,
	FIntPoint TextureSize,
	EStereoscopicPass StereoView,
	const TShaderRef<FShader>& VertexShader
	);

// NOTE: Assumes previously set PSO has PrimitiveType = PT_TriangleList
extern RENDERER_API void DrawPostProcessPass(
	FRHICommandList& RHICmdList,
	float X,
	float Y,
	float SizeX,
	float SizeY,
	float U,
	float V,
	float SizeU,
	float SizeV,
	FIntPoint TargetSize,
	FIntPoint TextureSize,
	const TShaderRef<FShader>& VertexShader,
	int32 StereoViewIndex,
	bool bHasCustomMesh,
	EDrawRectangleFlags Flags = EDRF_Default
	);



/*-----------------------------------------------------------------------------
FMGammaShaderParameters
-----------------------------------------------------------------------------*/

/** Encapsulates the gamma correction parameters. */
class FGammaShaderParameters
{
	DECLARE_TYPE_LAYOUT(FGammaShaderParameters, NonVirtual);
public:

	/** Default constructor. */
	FGammaShaderParameters() {}

	/** Initialization constructor. */
	FGammaShaderParameters(const FShaderParameterMap& ParameterMap)
	{
		RenderTargetExtent.Bind(ParameterMap,TEXT("RenderTargetExtent"));
		GammaColorScaleAndInverse.Bind(ParameterMap,TEXT("GammaColorScaleAndInverse"));
		GammaOverlayColor.Bind(ParameterMap,TEXT("GammaOverlayColor"));
	}

	/** Set the material shader parameter values. */
	void Set(FRHICommandList& RHICmdList, FRHIPixelShader* RHIShader, const FViewInfo& View, float DisplayGamma, FLinearColor const& ColorScale, FLinearColor const& ColorOverlay)
	{
		// GammaColorScaleAndInverse

		float InvDisplayGamma = 1.f / FMath::Max<float>(DisplayGamma,KINDA_SMALL_NUMBER);
		float OneMinusOverlayBlend = 1.f - ColorOverlay.A;

		FVector4f ColorScaleAndInverse;

		ColorScaleAndInverse.X = ColorScale.R * OneMinusOverlayBlend;
		ColorScaleAndInverse.Y = ColorScale.G * OneMinusOverlayBlend;
		ColorScaleAndInverse.Z = ColorScale.B * OneMinusOverlayBlend;
		ColorScaleAndInverse.W = InvDisplayGamma;

		SetShaderValue(
			RHICmdList,
			RHIShader,
			GammaColorScaleAndInverse,
			ColorScaleAndInverse
			);

		// GammaOverlayColor

		FVector4f OverlayColor;

		OverlayColor.X = ColorOverlay.R * ColorOverlay.A;
		OverlayColor.Y = ColorOverlay.G * ColorOverlay.A;
		OverlayColor.Z = ColorOverlay.B * ColorOverlay.A;
		OverlayColor.W = 0.f; // Unused

		SetShaderValue(
			RHICmdList,
			RHIShader,
			GammaOverlayColor,
			OverlayColor
			);

		FIntPoint BufferSize = GetSceneTextureExtentFromView(View);
		float BufferSizeX = (float)BufferSize.X;
		float BufferSizeY = (float)BufferSize.Y;
		float InvBufferSizeX = 1.0f / BufferSizeX;
		float InvBufferSizeY = 1.0f / BufferSizeY;

		const FVector4f vRenderTargetExtent(BufferSizeX, BufferSizeY,  InvBufferSizeX, InvBufferSizeY);

		SetShaderValue(
			RHICmdList,
			RHIShader,
			RenderTargetExtent,
			vRenderTargetExtent);
	}

	/** Serializer. */
	friend FArchive& operator<<(FArchive& Ar,FGammaShaderParameters& P)
	{
		Ar << P.GammaColorScaleAndInverse;
		Ar << P.GammaOverlayColor;
		Ar << P.RenderTargetExtent;

		return Ar;
	}


private:
	
		LAYOUT_FIELD(FShaderParameter, GammaColorScaleAndInverse)
		LAYOUT_FIELD(FShaderParameter, GammaOverlayColor)
		LAYOUT_FIELD(FShaderParameter, RenderTargetExtent)
	
};


class FTesselatedScreenRectangleIndexBuffer : public FIndexBuffer
{
public:

	// if one of those constants change, UpscaleVS needs to be recompiled

	// number of quads in x
	static const uint32 Width = 32;		// used for CylindricalProjection (smaller FOV could do less tessellation)
	// number of quads in y
	static const uint32 Height = 20;	// to minimize distortion we also tessellate in Y but a perspective distortion could do that with fewer triangles.

	/** Initialize the RHI for this rendering resource */
	void InitRHI() override;

	uint32 NumVertices() const;

	uint32 NumPrimitives() const;
};

