// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlateRHIFontTexture.h"
#include "Rendering/SlateRenderer.h"
#include "RenderingThread.h"
#include "RenderUtils.h"

FSlateFontTextureRHIResource::FSlateFontTextureRHIResource(uint32 InWidth, uint32 InHeight, const bool InIsGrayscale)
	: Width(InWidth)
	, Height(InHeight)
	, bIsGrayscale(InIsGrayscale)
{
}

void FSlateFontTextureRHIResource::InitDynamicRHI()
{
	check( IsInRenderingThread() );

	// Create the texture
	if( Width > 0 && Height > 0 )
	{
		const EPixelFormat PixelFormat = GetRHIPixelFormat();

		check( !IsValidRef( ShaderResource) );

		FRHITextureCreateDesc Desc =
			FRHITextureCreateDesc::Create2D(TEXT("FSlateFontTextureRHIResource"), Width, Height, PixelFormat)
			.SetFlags(ETextureCreateFlags::Dynamic);

		if (!bIsGrayscale)
		{
			Desc.AddFlags(ETextureCreateFlags::SRGB);
		}

		ShaderResource = RHICreateTexture(Desc);

		check( IsValidRef( ShaderResource ) );

		// Also assign the reference to the FTextureResource variable so that the Engine can access it
		TextureRHI = ShaderResource;

		// Create the sampler state RHI resource.
		FSamplerStateInitializerRHI SamplerStateInitializer
		(
		  SF_Bilinear,
		  AM_Clamp,
		  AM_Clamp,
		  AM_Wrap,
		  0,
		  1, // Disable anisotropic filtering, since aniso doesn't respect MaxLOD
		  0,
		  0
		);
		SamplerStateRHI = GetOrCreateSamplerState(SamplerStateInitializer);

		// Create a custom sampler state for using this texture in a deferred pass, where ddx / ddy are discontinuous
		FSamplerStateInitializerRHI DeferredPassSamplerStateInitializer
		(
		  SF_Bilinear,
		  AM_Clamp,
		  AM_Clamp,
		  AM_Wrap,
		  0,
		  1, // Disable anisotropic filtering, since aniso doesn't respect MaxLOD
		  0,
		  0
		);
		DeferredPassSamplerStateRHI = GetOrCreateSamplerState(DeferredPassSamplerStateInitializer);

		INC_MEMORY_STAT_BY(STAT_SlateTextureGPUMemory, Width*Height*GPixelFormats[PixelFormat].BlockBytes);
	}
}

void FSlateFontTextureRHIResource::ReleaseDynamicRHI()
{
	check( IsInRenderingThread() );

	// Release the texture
	if( IsValidRef(ShaderResource) )
	{
		const EPixelFormat PixelFormat = GetRHIPixelFormat();

		DEC_MEMORY_STAT_BY(STAT_SlateTextureGPUMemory, Width*Height*GPixelFormats[PixelFormat].BlockBytes);
	}

	ShaderResource.SafeRelease();
}

EPixelFormat FSlateFontTextureRHIResource::GetRHIPixelFormat() const
{
	return bIsGrayscale
		? PF_A8
		: PF_B8G8R8A8;
}

FSlateFontAtlasRHI::FSlateFontAtlasRHI(uint32 Width, uint32 Height, const bool InIsGrayscale)
	: FSlateFontAtlas(Width, Height, InIsGrayscale) 
	, FontTexture(new FSlateFontTextureRHIResource(Width, Height, InIsGrayscale))
{
}

FSlateFontAtlasRHI::~FSlateFontAtlasRHI()
{
}

void FSlateFontAtlasRHI::ReleaseResources()
{
	checkSlow( IsThreadSafeForSlateRendering() );

	BeginReleaseResource( FontTexture.Get() );
}

void FSlateFontAtlasRHI::ConditionalUpdateTexture()
{
	if( bNeedsUpdate )
	{
		if (IsInRenderingThread())
		{
			FontTexture->InitResource();

			uint32 DestStride;
			uint8* TempData = (uint8*)RHILockTexture2D( FontTexture->GetTypedResource(), 0, RLM_WriteOnly, /*out*/ DestStride, false );
			// check( DestStride == Atlas.BytesPerPixel * Atlas.AtlasWidth ); // Temporarily disabling check
			FMemory::Memcpy( TempData, AtlasData.GetData(), BytesPerPixel*AtlasWidth*AtlasHeight );
			RHIUnlockTexture2D( FontTexture->GetTypedResource(),0,false );
		}
		else
		{
			checkSlow( IsThreadSafeForSlateRendering() );

			BeginInitResource( FontTexture.Get() );

			FSlateFontAtlasRHI* Atlas = this;
			ENQUEUE_RENDER_COMMAND(SlateUpdateFontAtlasTextureCommand)(
				[Atlas](FRHICommandListImmediate& RHICmdList)
				{
					uint32 DestStride;
					uint8* TempData = (uint8*)RHILockTexture2D( Atlas->FontTexture->GetTypedResource(), 0, RLM_WriteOnly, /*out*/ DestStride, false );
					// check( DestStride == Atlas.BytesPerPixel * Atlas.AtlasWidth ); // Temporarily disabling check
					FMemory::Memcpy( TempData, Atlas->AtlasData.GetData(), Atlas->BytesPerPixel*Atlas->AtlasWidth*Atlas->AtlasHeight );
					RHIUnlockTexture2D( Atlas->FontTexture->GetTypedResource(),0,false );
				});
		}

		bNeedsUpdate = false;
	}
}

FSlateFontTextureRHI::FSlateFontTextureRHI(const uint32 InWidth, const uint32 InHeight, const bool InIsGrayscale, const TArray<uint8>& InRawData)
	: FontTexture(new FSlateFontTextureRHIResource(InWidth, InHeight, InIsGrayscale))
{
	if (IsInRenderingThread())
	{
		FontTexture->InitResource();
		UpdateTextureFromSource(InWidth, InHeight, InRawData);
	}
	else
	{
		checkSlow(IsThreadSafeForSlateRendering());

		PendingSourceData.Reset(new FPendingSourceData(InWidth, InHeight, InRawData));

		BeginInitResource(FontTexture.Get());

		FSlateFontTextureRHI* This = this;
		ENQUEUE_RENDER_COMMAND(SlateUpdateFontTextureCommand)(
			[This](FRHICommandListImmediate& RHICmdList)
			{
				This->UpdateTextureFromSource(This->PendingSourceData->SourceWidth, This->PendingSourceData->SourceHeight, This->PendingSourceData->SourceData);
				This->PendingSourceData.Reset();
			});
	}
}

FSlateFontTextureRHI::~FSlateFontTextureRHI()
{
}

void FSlateFontTextureRHI::ReleaseResources()
{
	checkSlow(IsThreadSafeForSlateRendering());

	BeginReleaseResource(FontTexture.Get());
}

void FSlateFontTextureRHI::UpdateTextureFromSource(const uint32 SourceWidth, const uint32 SourceHeight, const TArray<uint8>& SourceData)
{
	const uint32 BytesPerPixel = FontTexture->IsGrayscale() ? 1 : 4;

	uint32 DestStride;
	uint8* LockedTextureData = static_cast<uint8*>(RHILockTexture2D(FontTexture->GetTypedResource(), 0, RLM_WriteOnly, /*out*/ DestStride, false));

	// If our stride matches our width, we can copy in a single call, rather than copy each line
	if (BytesPerPixel * SourceWidth == DestStride)
	{
		FMemory::Memcpy(LockedTextureData, SourceData.GetData(), BytesPerPixel * SourceWidth * SourceHeight);
	}
	else
	{
		for (uint32 RowIndex = 0; RowIndex < SourceHeight; ++RowIndex)
		{
			FMemory::Memcpy(LockedTextureData + (RowIndex * DestStride), SourceData.GetData() + (RowIndex * SourceWidth), BytesPerPixel * SourceWidth);
		}
	}
	
	RHIUnlockTexture2D(FontTexture->GetTypedResource(), 0, false);
}
