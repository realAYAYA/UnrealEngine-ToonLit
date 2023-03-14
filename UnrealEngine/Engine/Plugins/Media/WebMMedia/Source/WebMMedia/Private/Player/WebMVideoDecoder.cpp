// Copyright Epic Games, Inc. All Rights Reserved.

#include "WebMVideoDecoder.h"

#if WITH_WEBM_LIBS

THIRD_PARTY_INCLUDES_START
#include <vpx/vpx_decoder.h>
#include <vpx/vp8dx.h>
THIRD_PARTY_INCLUDES_END

#include "WebMMediaPrivate.h"
#include "WebMMediaFrame.h"
#include "WebMMediaTextureSample.h"
#include "WebMSamplesSink.h"
#include "MediaShaders.h"
#include "PipelineStateCache.h"
#include "RHIStaticStates.h"
#include "Containers/DynamicRHIResourceArray.h"

namespace
{
	/**
	* RHI resources for rendering texture to polygon.
	*/
	class FMoviePlaybackResources
		: public FRenderResource
	{
	public:

		FVertexDeclarationRHIRef VertexDeclarationRHI;
		FBufferRHIRef VertexBufferRHI;

		virtual ~FMoviePlaybackResources() { }

		virtual void InitRHI() override
		{
			FVertexDeclarationElementList Elements;
			uint16 Stride = sizeof(FMediaElementVertex);
			Elements.Add(FVertexElement(0, STRUCT_OFFSET(FMediaElementVertex, Position), VET_Float4, 0, Stride));
			Elements.Add(FVertexElement(0, STRUCT_OFFSET(FMediaElementVertex, TextureCoordinate), VET_Float2, 1, Stride));
			VertexDeclarationRHI = PipelineStateCache::GetOrCreateVertexDeclaration(Elements);

			TResourceArray<FMediaElementVertex> Vertices;
			Vertices.AddUninitialized(4);
			Vertices[0].Position.Set(-1.0f, 1.0f, 1.0f, 1.0f);
			Vertices[0].TextureCoordinate.Set(0.0f, 0.0f);
			Vertices[1].Position.Set(1.0f, 1.0f, 1.0f, 1.0f);
			Vertices[1].TextureCoordinate.Set(1.0f, 0.0f);
			Vertices[2].Position.Set(-1.0f, -1.0f, 1.0f, 1.0f);
			Vertices[2].TextureCoordinate.Set(0.0f, 1.0f);
			Vertices[3].Position.Set(1.0f, -1.0f, 1.0f, 1.0f);
			Vertices[3].TextureCoordinate.Set(1.0f, 1.0f);
			FRHIResourceCreateInfo CreateInfo(TEXT("FMoviePlaybackResources"), &Vertices);
			VertexBufferRHI = RHICreateVertexBuffer(sizeof(FMediaElementVertex) * 4, BUF_Static, CreateInfo);
		}

		virtual void ReleaseRHI() override
		{
			VertexDeclarationRHI.SafeRelease();
			VertexBufferRHI.SafeRelease();
		}
	};

	/** Singleton instance of the RHI resources. */
	TGlobalResource<FMoviePlaybackResources> GMoviePlayerResources;
}

FWebMVideoDecoder::FWebMVideoDecoder(IWebMSamplesSink& InSamples)
	: VideoSamplePool(new FWebMMediaTextureSamplePool)
	, Samples(InSamples)
	, bTexturesCreated(false)
	, bIsInitialized(false)
{
}

FWebMVideoDecoder::~FWebMVideoDecoder()
{
	Close();
}

bool FWebMVideoDecoder::Initialize(const char* CodecName)
{
	Close();

	const int32 NumOfThreads = 1;
	const vpx_codec_dec_cfg_t CodecConfig = { NumOfThreads, 0, 0 };
	if (FCStringAnsi::Strcmp(CodecName, "V_VP8") == 0)
	{
		verify(vpx_codec_dec_init(Context, vpx_codec_vp8_dx(), &CodecConfig, /*VPX_CODEC_USE_FRAME_THREADING*/ 0) == 0);
	}
	else if (FCStringAnsi::Strcmp(CodecName, "V_VP9") == 0)
	{
		verify(vpx_codec_dec_init(Context, vpx_codec_vp9_dx(), &CodecConfig, /*VPX_CODEC_USE_FRAME_THREADING*/ 0) == 0);
	}
	else
	{
		UE_LOG(LogWebMMedia, Display, TEXT("Unsupported video codec: %s"), CodecName);
		return false;
	}

	bIsInitialized = true;

	return true;
}

void FWebMVideoDecoder::DecodeVideoFramesAsync(const TArray<TSharedPtr<FWebMFrame>>& VideoFrames)
{
	static bool bUseRenderThread = FPlatformMisc::UseRenderThread();
	if (bUseRenderThread)
	{
		FGraphEventArray Prerequisites;
		if (VideoDecodingTask)
		{
			Prerequisites.Emplace(VideoDecodingTask);
		}
		VideoDecodingTask = FFunctionGraphTask::CreateAndDispatchWhenReady([this,  VideoFrames]()
		{
			DoDecodeVideoFrames(VideoFrames);
		}, TStatId(), &Prerequisites, ENamedThreads::AnyThread);
	}
	else
	{
		DoDecodeVideoFrames(VideoFrames);
	}
}

bool FWebMVideoDecoder::IsBusy() const
{
	return VideoDecodingTask && !VideoDecodingTask->IsComplete();
}

void FWebMVideoDecoder::DoDecodeVideoFrames(const TArray<TSharedPtr<FWebMFrame>>& VideoFrames)
{
	for (const TSharedPtr<FWebMFrame>& VideoFrame : VideoFrames)
	{
		if (vpx_codec_decode(Context, VideoFrame->Data.GetData(), VideoFrame->Data.Num(), nullptr, 0) != 0)
		{
			UE_LOG(LogWebMMedia, Display, TEXT("Error decoding video frame"));
			return;
		}

		const void* ImageIter = nullptr;
		while (const vpx_image_t* Image = vpx_codec_get_frame(Context, &ImageIter))
		{
			FWebMVideoDecoder* Self = this;
			if (!bTexturesCreated)
			{
				// First creation of conversion textures

				bTexturesCreated = true;
				ENQUEUE_RENDER_COMMAND(WebMMediaPlayerCreateTextures)(
					[Self, Image](FRHICommandListImmediate& RHICmdList)
					{
						Self->CreateTextures(Image);
					});
			}

			TSharedRef<FWebMMediaTextureSample, ESPMode::ThreadSafe> VideoSample = VideoSamplePool->AcquireShared();

			VideoSample->Initialize(FIntPoint(Image->d_w, Image->d_h), FIntPoint(Image->d_w, Image->d_h), VideoFrame->Time, VideoFrame->Duration);

			FConvertParams Params;
			Params.VideoSample = VideoSample;
			Params.Image = Image;
			ENQUEUE_RENDER_COMMAND(WebMMediaPlayerConvertYUVToRGB)(
				[Self, Params](FRHICommandListImmediate& RHICmdList)
				{
					Self->ConvertYUVToRGBAndSubmit(Params);
				});
		}
	}
}

void FWebMVideoDecoder::CreateTextures(const vpx_image_t* Image)
{
	const FRHITextureCreateDesc DecodedYDesc =
		FRHITextureCreateDesc::Create2D(TEXT("FWebMVideoDecoder_DecodedY"), Image->stride[0], Image->d_h, PF_G8)
		.SetFlags(ETextureCreateFlags::Dynamic);

	const FRHITextureCreateDesc DecodedUDesc =
		FRHITextureCreateDesc::Create2D(TEXT("FWebMVideoDecoder_DecodedU"), Image->stride[1], Image->d_h / 2, PF_G8)
		.SetFlags(ETextureCreateFlags::Dynamic);

	const FRHITextureCreateDesc DecodedVDesc =
		FRHITextureCreateDesc::Create2D(TEXT("FWebMVideoDecoder_DecodedV"), Image->stride[2], Image->d_h / 2, PF_G8)
		.SetFlags(ETextureCreateFlags::Dynamic);

	DecodedY = RHICreateTexture(DecodedYDesc);
	DecodedU = RHICreateTexture(DecodedUDesc);
	DecodedV = RHICreateTexture(DecodedVDesc);
}

void FWebMVideoDecoder::Close()
{
	if (VideoDecodingTask && !VideoDecodingTask->IsComplete())
	{
		FTaskGraphInterface::Get().WaitUntilTaskCompletes(VideoDecodingTask);
	}

	// Make sure all compute shader decoding is done
	//
	// This function can also be called on a rendering thread (the streamer is ticked there during a startup movie, and decoder gets deleted on StartNextMovie()
	// if there are >1 movie queued). In this case we will ensure that the resources survive for one more frame after use by other means.
	if (IsInGameThread())
	{
		FlushRenderingCommands();
	}

	if (bIsInitialized)
	{
		vpx_codec_destroy(Context);
		bIsInitialized = false;
	}

	bTexturesCreated = false;
}

void FWebMVideoDecoder::ConvertYUVToRGBAndSubmit(const FConvertParams& Params)
{
	TSharedPtr<FWebMMediaTextureSample, ESPMode::ThreadSafe> VideoSample = Params.VideoSample;
	check(VideoSample.IsValid());
	const vpx_image_t* Image = Params.Image;

	VideoSample->CreateTexture();

	// render video frame into output texture
	FRHICommandListImmediate& CommandList = GetImmediateCommandList_ForRenderCommand();

	{
		const auto CopyTextureMemory = [Image](
			FRHICommandListImmediate& InCommandList,
			FRHITexture2D* RHITexture,
			int ImageIndex,
			int CopyHeight)
		{
			uint32 Stride = 0;
			unsigned char* TextureMemory = (unsigned char*)GDynamicRHI->LockTexture2D_RenderThread(InCommandList, RHITexture, 0, RLM_WriteOnly, Stride, false);

			if (TextureMemory)
			{
				check(Stride >= (uint32)Image->stride[ImageIndex]);
				if (Stride == Image->stride[ImageIndex])
				{
					FMemory::Memcpy(TextureMemory, Image->planes[ImageIndex], Image->stride[ImageIndex] * CopyHeight);
				}
				else
				{
					for (int h = 0; h < CopyHeight; ++h)
					{
						FMemory::Memcpy(TextureMemory + h * Stride, Image->planes[ImageIndex] + h * Image->stride[ImageIndex], Image->stride[ImageIndex]);
					}
				}
				GDynamicRHI->UnlockTexture2D_RenderThread(InCommandList, RHITexture, 0, false);
			}
		};

		// copy the Y plane out of the video buffer
		CopyTextureMemory(CommandList, DecodedY.GetReference(), 0, Image->d_h);

		// copy the U plane out of the video buffer
		CopyTextureMemory(CommandList, DecodedU.GetReference(), 1, Image->d_h / 2);

		// copy the V plane out of the video buffer
		CopyTextureMemory(CommandList, DecodedV.GetReference(), 2, Image->d_h / 2);

		FRHITexture* RenderTarget = VideoSample->GetTexture();
		CommandList.Transition(FRHITransitionInfo(RenderTarget, ERHIAccess::SRVGraphics, ERHIAccess::RTV));
		FRHIRenderPassInfo RPInfo(RenderTarget, ERenderTargetActions::Load_Store);
		CommandList.BeginRenderPass(RPInfo, TEXT("ConvertYUVtoRGBA"));
		{
			// configure media shaders
			auto ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);

			TShaderMapRef<FYUVConvertPS> PixelShader(ShaderMap);
			TShaderMapRef<FMediaShadersVS> VertexShader(ShaderMap);

			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			{
				CommandList.ApplyCachedRenderTargets(GraphicsPSOInit);
				GraphicsPSOInit.BlendState = TStaticBlendStateWriteMask<CW_RGBA, CW_NONE, CW_NONE, CW_NONE, CW_NONE, CW_NONE, CW_NONE, CW_NONE>::GetRHI();
				GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
				GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
				GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GMoviePlayerResources.VertexDeclarationRHI;
				GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
				GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;
			}

			SetGraphicsPipelineState(CommandList, GraphicsPSOInit, 0);
			PixelShader->SetParameters(CommandList, DecodedY->GetTexture2D(), DecodedU->GetTexture2D(), DecodedV->GetTexture2D(), FIntPoint(Image->d_w, Image->d_h), MediaShaders::YuvToRgbRec709Scaled, MediaShaders::YUVOffset8bits, true);

			// draw full-size quad
			CommandList.SetViewport(0, 0, 0.0f, Image->d_w, Image->d_h, 1.0f);
			CommandList.SetStreamSource(0, GMoviePlayerResources.VertexBufferRHI, 0);
			CommandList.DrawPrimitive(0, 2, 1);
		}
		CommandList.EndRenderPass();
		CommandList.Transition(FRHITransitionInfo(RenderTarget, ERHIAccess::RTV, ERHIAccess::SRVGraphics));

		Samples.AddVideoSampleFromDecodingThread(VideoSample.ToSharedRef());
	}
}

#endif // WITH_WEBM_LIBS
