// Copyright Epic Games, Inc. All Rights Reserved.

/*================================================================================
	MeshPaintRendering.cpp: Mesh texture paint brush rendering
================================================================================*/

#include "MeshPaintRendering.h"
#include "ShaderParameters.h"
#include "RenderResource.h"
#include "Shader.h"
#include "StaticBoundShaderState.h"
#include "RHIStaticStates.h"
#include "BatchedElements.h"
#include "GlobalShader.h"
#include "ShaderParameterUtils.h"
#include "TextureResource.h"
#include "Engine/TextureRenderTarget2D.h"
#include "PipelineStateCache.h"

namespace MeshPaintRendering
{

	/** Mesh paint vertex shader */
	class TMeshPaintVertexShader : public FGlobalShader
	{
		DECLARE_SHADER_TYPE( TMeshPaintVertexShader, Global );

	public:

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) && !IsConsolePlatform(Parameters.Platform);
		}

		/** Default constructor. */
		TMeshPaintVertexShader() {}

		/** Initialization constructor. */
		TMeshPaintVertexShader( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
			: FGlobalShader( Initializer )
		{
			TransformParameter.Bind( Initializer.ParameterMap, TEXT( "c_Transform" ) );
		}

		void SetParameters(FRHICommandList& RHICmdList, const FMatrix44f& InTransform )
		{
			SetShaderValue(RHICmdList, RHICmdList.GetBoundVertexShader(), TransformParameter, InTransform );
		}

	private:
		LAYOUT_FIELD(FShaderParameter, TransformParameter);
	};


	IMPLEMENT_SHADER_TYPE( , TMeshPaintVertexShader, TEXT( "/Engine/Private/MeshPaintVertexShader.usf" ), TEXT( "Main" ), SF_Vertex);



	/** Mesh paint pixel shader */
	class TMeshPaintPixelShader : public FGlobalShader
	{
		DECLARE_SHADER_TYPE( TMeshPaintPixelShader, Global );
	public:

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) && !IsConsolePlatform(Parameters.Platform);
		}

		/** Default constructor. */
		TMeshPaintPixelShader() {}

		/** Initialization constructor. */
		TMeshPaintPixelShader( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
			: FGlobalShader( Initializer )
		{
			CloneTextureParameter.Bind( Initializer.ParameterMap, TEXT( "s_CloneTexture" ) );
			CloneTextureParameterSampler.Bind( Initializer.ParameterMap, TEXT( "s_CloneTextureSampler" ));
			WorldToBrushMatrixParameter.Bind( Initializer.ParameterMap, TEXT( "c_WorldToBrushMatrix" ) );
			BrushMetricsParameter.Bind( Initializer.ParameterMap, TEXT( "c_BrushMetrics" ) );
			BrushStrengthParameter.Bind( Initializer.ParameterMap, TEXT( "c_BrushStrength" ) );
			BrushColorParameter.Bind( Initializer.ParameterMap, TEXT( "c_BrushColor" ) );
			ChannelFlagsParameter.Bind( Initializer.ParameterMap, TEXT( "c_ChannelFlags") );
			GenerateMaskFlagParameter.Bind( Initializer.ParameterMap, TEXT( "c_GenerateMaskFlag") );
			GammaParameter.Bind( Initializer.ParameterMap, TEXT( "c_Gamma" ) );
		}

		void SetParameters(FRHICommandList& RHICmdList, const float InGamma, const FMeshPaintShaderParameters& InShaderParams )
		{
			FRHIPixelShader* ShaderRHI = RHICmdList.GetBoundPixelShader();

			SetTextureParameter(
				RHICmdList, 
				ShaderRHI,
				CloneTextureParameter,
				CloneTextureParameterSampler,
				TStaticSamplerState< SF_Point, AM_Clamp, AM_Clamp, AM_Clamp >::GetRHI(),
				InShaderParams.CloneTexture->GetRenderTargetResource()->TextureRHI );

			SetShaderValue(RHICmdList, ShaderRHI, WorldToBrushMatrixParameter, (FMatrix44f)InShaderParams.WorldToBrushMatrix );

			FVector4f BrushMetrics;
			BrushMetrics.X = InShaderParams.BrushRadius;
			BrushMetrics.Y = InShaderParams.BrushRadialFalloffRange;
			BrushMetrics.Z = InShaderParams.BrushDepth;
			BrushMetrics.W = InShaderParams.BrushDepthFalloffRange;
			SetShaderValue(RHICmdList, ShaderRHI, BrushMetricsParameter, BrushMetrics );

			FVector4f BrushStrength4( InShaderParams.BrushStrength, 0.0f, 0.0f, 0.0f );
			SetShaderValue(RHICmdList, ShaderRHI, BrushStrengthParameter, BrushStrength4 );

			SetShaderValue(RHICmdList, ShaderRHI, BrushColorParameter, InShaderParams.BrushColor );

			FVector4f ChannelFlags;
			ChannelFlags.X = InShaderParams.RedChannelFlag;
			ChannelFlags.Y = InShaderParams.GreenChannelFlag;
			ChannelFlags.Z = InShaderParams.BlueChannelFlag;
			ChannelFlags.W = InShaderParams.AlphaChannelFlag;
			SetShaderValue(RHICmdList, ShaderRHI, ChannelFlagsParameter, ChannelFlags );
			
			float MaskVal = InShaderParams.GenerateMaskFlag ? 1.0f : 0.0f;
			SetShaderValue(RHICmdList, ShaderRHI, GenerateMaskFlagParameter, MaskVal );

			// @todo MeshPaint
			SetShaderValue(RHICmdList, ShaderRHI, GammaParameter, InGamma );
		}

	private:
		/** Texture that is a clone of the destination render target before we start drawing */
		LAYOUT_FIELD(FShaderResourceParameter, CloneTextureParameter);
		LAYOUT_FIELD(FShaderResourceParameter, CloneTextureParameterSampler);

		/** Brush -> World matrix */
		LAYOUT_FIELD(FShaderParameter, WorldToBrushMatrixParameter);

		/** Brush metrics: x = radius, y = falloff range, z = depth, w = depth falloff range */
		LAYOUT_FIELD(FShaderParameter, BrushMetricsParameter);

		/** Brush strength */
		LAYOUT_FIELD(FShaderParameter, BrushStrengthParameter);

		/** Brush color */
		LAYOUT_FIELD(FShaderParameter, BrushColorParameter);

		/** Flags that control paining individual channels: x = Red, y = Green, z = Blue, w = Alpha */
		LAYOUT_FIELD(FShaderParameter, ChannelFlagsParameter);
		
		/** Flag to control brush mask generation or paint blending */
		LAYOUT_FIELD(FShaderParameter, GenerateMaskFlagParameter);

		/** Gamma */
		// @todo MeshPaint: Remove this?
		LAYOUT_FIELD(FShaderParameter, GammaParameter);
	};


	IMPLEMENT_SHADER_TYPE( , TMeshPaintPixelShader, TEXT( "/Engine/Private/MeshPaintPixelShader.usf" ), TEXT( "Main" ), SF_Pixel );


	/** Mesh paint dilate vertex shader */
	class TMeshPaintDilateVertexShader : public FGlobalShader
	{
		DECLARE_SHADER_TYPE( TMeshPaintDilateVertexShader, Global );

	public:

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) && !IsConsolePlatform(Parameters.Platform);
		}

		/** Default constructor. */
		TMeshPaintDilateVertexShader() {}

		/** Initialization constructor. */
		TMeshPaintDilateVertexShader( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
			: FGlobalShader( Initializer )
		{
			TransformParameter.Bind( Initializer.ParameterMap, TEXT( "c_Transform" ) );
		}

		void SetParameters(FRHICommandList& RHICmdList, const FMatrix44f& InTransform )
		{
			SetShaderValue(RHICmdList, RHICmdList.GetBoundVertexShader(), TransformParameter, InTransform );
		}

	private:

		LAYOUT_FIELD(FShaderParameter, TransformParameter);
	};


	IMPLEMENT_SHADER_TYPE( , TMeshPaintDilateVertexShader, TEXT( "/Engine/Private/meshpaintdilatevertexshader.usf" ), TEXT( "Main" ), SF_Vertex );



	/** Mesh paint pixel shader */
	class TMeshPaintDilatePixelShader : public FGlobalShader
	{
		DECLARE_SHADER_TYPE( TMeshPaintDilatePixelShader, Global );

	public:

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) && !IsConsolePlatform(Parameters.Platform);
		}

		/** Default constructor. */
		TMeshPaintDilatePixelShader() {}

		/** Initialization constructor. */
		TMeshPaintDilatePixelShader( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
			: FGlobalShader( Initializer )
		{
			Texture0Parameter.Bind( Initializer.ParameterMap, TEXT( "Texture0" ) );
			Texture0ParameterSampler.Bind( Initializer.ParameterMap, TEXT( "Texture0Sampler" ) );
			Texture1Parameter.Bind( Initializer.ParameterMap, TEXT( "Texture1") );
			Texture1ParameterSampler.Bind( Initializer.ParameterMap, TEXT( "Texture1Sampler") );
			Texture2Parameter.Bind( Initializer.ParameterMap, TEXT( "Texture2") );
			Texture2ParameterSampler.Bind( Initializer.ParameterMap, TEXT( "Texture2Sampler") );
			WidthPixelOffsetParameter.Bind( Initializer.ParameterMap, TEXT( "WidthPixelOffset") );
			HeightPixelOffsetParameter.Bind( Initializer.ParameterMap, TEXT( "HeightPixelOffset") );
			GammaParameter.Bind( Initializer.ParameterMap, TEXT( "Gamma" ) );
		}

		void SetParameters(FRHICommandList& RHICmdList, const float InGamma, const FMeshPaintDilateShaderParameters& InShaderParams )
		{
			FRHIPixelShader* ShaderRHI = RHICmdList.GetBoundPixelShader();

			SetTextureParameter(
				RHICmdList, 
				ShaderRHI,
				Texture0Parameter,
				Texture0ParameterSampler,
				TStaticSamplerState< SF_Point, AM_Clamp, AM_Clamp, AM_Clamp >::GetRHI(),
				InShaderParams.Texture0->GetRenderTargetResource()->TextureRHI );

			SetTextureParameter(
				RHICmdList, 
				ShaderRHI,
				Texture1Parameter,
				Texture1ParameterSampler,
				TStaticSamplerState< SF_Point, AM_Clamp, AM_Clamp, AM_Clamp >::GetRHI(),
				InShaderParams.Texture1->GetRenderTargetResource()->TextureRHI );

			SetTextureParameter(
				RHICmdList, 
				ShaderRHI,
				Texture2Parameter,
				Texture2ParameterSampler,
				TStaticSamplerState< SF_Point, AM_Clamp, AM_Clamp, AM_Clamp >::GetRHI(),
				InShaderParams.Texture2->GetRenderTargetResource()->TextureRHI );

			SetShaderValue(RHICmdList, ShaderRHI, WidthPixelOffsetParameter, InShaderParams.WidthPixelOffset );
			SetShaderValue(RHICmdList, ShaderRHI, HeightPixelOffsetParameter, InShaderParams.HeightPixelOffset );
			SetShaderValue(RHICmdList, ShaderRHI, GammaParameter, InGamma );
		}


	private:

		/** Texture0 */
		LAYOUT_FIELD(FShaderResourceParameter, Texture0Parameter);
		LAYOUT_FIELD(FShaderResourceParameter, Texture0ParameterSampler);

		/** Texture1 */
		LAYOUT_FIELD(FShaderResourceParameter, Texture1Parameter);
		LAYOUT_FIELD(FShaderResourceParameter, Texture1ParameterSampler);

		/** Texture2 */
		LAYOUT_FIELD(FShaderResourceParameter, Texture2Parameter);
		LAYOUT_FIELD(FShaderResourceParameter, Texture2ParameterSampler);

		/** Pixel size width */
		LAYOUT_FIELD(FShaderParameter, WidthPixelOffsetParameter);
		
		/** Pixel size height */
		LAYOUT_FIELD(FShaderParameter, HeightPixelOffsetParameter);

		/** Gamma */
		// @todo MeshPaint: Remove this?
		LAYOUT_FIELD(FShaderParameter, GammaParameter);
	};


	IMPLEMENT_SHADER_TYPE( , TMeshPaintDilatePixelShader, TEXT( "/Engine/Private/meshpaintdilatepixelshader.usf" ), TEXT( "Main" ), SF_Pixel );


	/** Mesh paint vertex format */
	typedef FSimpleElementVertex FMeshPaintVertex;


	/** Mesh paint vertex declaration resource */
	typedef FSimpleElementVertexDeclaration FMeshPaintVertexDeclaration;


	/** Global mesh paint vertex declaration resource */
	TGlobalResource< FMeshPaintVertexDeclaration > GMeshPaintVertexDeclaration;



	typedef FSimpleElementVertex FMeshPaintDilateVertex;
	typedef FSimpleElementVertexDeclaration FMeshPaintDilateVertexDeclaration;
	TGlobalResource< FMeshPaintDilateVertexDeclaration > GMeshPaintDilateVertexDeclaration;


	/** Binds the mesh paint vertex and pixel shaders to the graphics device */
	void SetMeshPaintShaders(FRHICommandList& RHICmdList, FGraphicsPipelineStateInitializer& GraphicsPSOInit, ERHIFeatureLevel::Type InFeatureLevel, const FMatrix& InTransform,
										   const float InGamma,
										   const FMeshPaintShaderParameters& InShaderParams )
	{
		TShaderMapRef< TMeshPaintVertexShader > VertexShader(GetGlobalShaderMap(InFeatureLevel));
		TShaderMapRef< TMeshPaintPixelShader > PixelShader(GetGlobalShaderMap(InFeatureLevel));

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GMeshPaintDilateVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

		// Set vertex shader parameters
		VertexShader->SetParameters(RHICmdList, FMatrix44f(InTransform) );	// LWC_TODO: Precision loss
		
		// Set pixel shader parameters
		PixelShader->SetParameters(RHICmdList, InGamma, InShaderParams );

		// @todo MeshPaint: Make sure blending/color writes are setup so we can write to ALPHA if needed!
	}

	/** Binds the mesh paint vertex and pixel shaders to the graphics device */
	void SetMeshPaintDilateShaders(FRHICommandList& RHICmdList, FGraphicsPipelineStateInitializer& GraphicsPSOInit, ERHIFeatureLevel::Type InFeatureLevel, const FMatrix& InTransform,
												 const float InGamma,
												 const FMeshPaintDilateShaderParameters& InShaderParams )
	{
		TShaderMapRef< TMeshPaintDilateVertexShader > VertexShader(GetGlobalShaderMap(InFeatureLevel));
		TShaderMapRef< TMeshPaintDilatePixelShader > PixelShader(GetGlobalShaderMap(InFeatureLevel));

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GMeshPaintDilateVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

		// Set vertex shader parameters
		VertexShader->SetParameters(RHICmdList, FMatrix44f(InTransform) );	// LWC_TODO: Precision loss

		// Set pixel shader parameters
		PixelShader->SetParameters(RHICmdList, InGamma, InShaderParams );
	}

}

