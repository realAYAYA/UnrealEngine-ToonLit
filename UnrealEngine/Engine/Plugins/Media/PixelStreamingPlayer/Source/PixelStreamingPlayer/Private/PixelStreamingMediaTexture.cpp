// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelStreamingMediaTexture.h"
#include "PixelStreamingMediaTextureResource.h"
#include "Async/Async.h"
#include "WebRTCIncludes.h"

constexpr int32 DEFAULT_WIDTH = 1920;
constexpr int32 DEFAULT_HEIGHT = 1080;

UPixelStreamingMediaTexture::UPixelStreamingMediaTexture(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetResource(nullptr);
}

void UPixelStreamingMediaTexture::BeginDestroy()
{
	AsyncTask(ENamedThreads::ActualRenderingThread, [this]() {
		FScopeLock Lock(&RenderSyncContext);
		RenderTarget = nullptr;
	});

	Super::BeginDestroy();
}

void UPixelStreamingMediaTexture::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	Super::GetResourceSizeEx(CumulativeResourceSize);

	if (CurrentResource != nullptr)
	{
		CumulativeResourceSize.AddUnknownMemoryBytes(CurrentResource->GetResourceSize());
	}
}

FTextureResource* UPixelStreamingMediaTexture::CreateResource()
{
	if (CurrentResource != nullptr)
	{
		SetResource(nullptr);
		delete CurrentResource;
	}

	CurrentResource = new FPixelStreamingMediaTextureResource(this);
	InitializeResources();

	return CurrentResource;
}

void UPixelStreamingMediaTexture::OnFrame(const webrtc::VideoFrame& frame)
{
	// TODO
	// Currently this will cause all input frames to be converted to I420 (using webrtc::ConvertFromI420)
	// and then render the rgba buffer to a texture, but in some cases we might already have the native
	// texture and can use it or copy it directly. At some point it would be useful to detect and fork this
	// behaviour.
	// Additionally, we're rendering to a texture then updating the texture resource with every frame. We
	// might be able to just map a single dest texture and update it each frame.

	static constexpr auto TEXTURE_PIXEL_FORMAT = PF_B8G8R8A8;
	static constexpr auto WEBRTC_PIXEL_FORMAT = webrtc::VideoType::kARGB;

	SizeX = frame.width();
	SizeY = frame.height();
	Format = TEXTURE_PIXEL_FORMAT;
	NumMips = 0;
	SamplerAddressMode = ESamplerAddressMode::AM_Clamp;

	{
		FScopeLock Lock(&RenderSyncContext);

		const uint32_t Size = webrtc::CalcBufferSize(WEBRTC_PIXEL_FORMAT, SizeX, SizeY);
		if (Size > static_cast<uint32_t>(Buffer.Num()))
		{
			Buffer.SetNum(Size);
		}
		webrtc::ConvertFromI420(frame, WEBRTC_PIXEL_FORMAT, 0, Buffer.GetData());
	}

	AsyncTask(ENamedThreads::ActualRenderingThread, [this]() {
		FScopeLock Lock(&RenderSyncContext);

		const FIntPoint FrameSize = FIntPoint(SizeX, SizeY);
		FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();

		if (!RenderTargetDescriptor.IsValid() || RenderTargetDescriptor.GetSize() != FIntVector(FrameSize.X, FrameSize.Y, 0))
		{
			// Create the RenderTarget descriptor
			RenderTargetDescriptor = FPooledRenderTargetDesc::Create2DDesc(FrameSize,
				Format,
				FClearValueBinding::None,
				TexCreate_None,
				TexCreate_RenderTargetable,
				false);

			// Update the shader resource for the 'SourceTexture'
			FRHITextureCreateDesc RenderTargetTextureDesc =
				FRHITextureCreateDesc::Create2D(TEXT(""), FrameSize.X, FrameSize.Y, Format)
					.SetClearValue(FClearValueBinding(FLinearColor(0.0f, 0.0f, 0.0f)))
					.SetFlags(ETextureCreateFlags::Dynamic | ETextureCreateFlags::ShaderResource | TexCreate_RenderTargetable | ETextureCreateFlags::SRGB)
					.SetInitialState(ERHIAccess::SRVMask);

			SourceTexture = RHICreateTexture(RenderTargetTextureDesc);

			// Find a free target-able texture from the render pool
			GRenderTargetPool.FindFreeElement(RHICmdList,
				RenderTargetDescriptor,
				RenderTarget,
				TEXT("PIXELSTEAMINGPLAYER"));
		}

		// Create the update region structure
		const FUpdateTextureRegion2D Region(0, 0, 0, 0, FrameSize.X, FrameSize.Y);

		// Set the Pixel data of the webrtc Frame to the SourceTexture
		RHIUpdateTexture2D(SourceTexture, 0, Region, FrameSize.X * 4, Buffer.GetData());

		UpdateTextureReference(RHICmdList, SourceTexture);
	});
}

void UPixelStreamingMediaTexture::InitializeResources()
{
	// Set the default video texture to reference nothing
	FTextureRHIRef ShaderTexture2D;
	FTextureRHIRef RenderableTexture;

	FRHITextureCreateDesc RenderTargetTextureDesc =
		FRHITextureCreateDesc::Create2D(TEXT(""), DEFAULT_WIDTH, DEFAULT_HEIGHT, EPixelFormat::PF_B8G8R8A8)
			.SetClearValue(FClearValueBinding(FLinearColor(0.0f, 0.0f, 0.0f)))
			.SetFlags(ETextureCreateFlags::Dynamic | ETextureCreateFlags::ShaderResource | TexCreate_RenderTargetable)
			.SetInitialState(ERHIAccess::SRVMask);

	RenderableTexture = RHICreateTexture(RenderTargetTextureDesc);
	ShaderTexture2D = RenderableTexture;

	CurrentResource->TextureRHI = RenderableTexture;

	ENQUEUE_RENDER_COMMAND(FPixelStreamingMediaTextureUpdateTextureReference)
	([this](FRHICommandListImmediate& RHICmdList) {
		RHIUpdateTextureReference(TextureReference.TextureReferenceRHI, CurrentResource->TextureRHI);
	});
}

void UPixelStreamingMediaTexture::UpdateTextureReference(FRHICommandList& RHICmdList, FTexture2DRHIRef Reference)
{
	if (CurrentResource != nullptr)
	{
		if (Reference.IsValid() && CurrentResource->TextureRHI != Reference)
		{
			CurrentResource->TextureRHI = Reference;
			RHIUpdateTextureReference(TextureReference.TextureReferenceRHI, CurrentResource->TextureRHI);
		}
		else if (!Reference.IsValid())
		{
			if (CurrentResource != nullptr)
			{
				InitializeResources();

				// Make sure RenderThread is executed before continuing
				FlushRenderingCommands();
			}
		}
	}
}
