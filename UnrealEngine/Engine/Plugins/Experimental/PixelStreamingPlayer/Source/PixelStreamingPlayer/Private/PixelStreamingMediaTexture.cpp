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

void UPixelStreamingMediaTexture::OnFrame(FTextureRHIRef Frame)
{
	AsyncTask(ENamedThreads::ActualRenderingThread, [this, Frame]() {
		FScopeLock Lock(&RenderSyncContext);

		FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
		UpdateTextureReference(RHICmdList, Frame);
	});
}

void UPixelStreamingMediaTexture::InitializeResources()
{
	ENQUEUE_RENDER_COMMAND(FPixelStreamingMediaTextureUpdateTextureReference)
	([this](FRHICommandListImmediate& RHICmdList) {
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
