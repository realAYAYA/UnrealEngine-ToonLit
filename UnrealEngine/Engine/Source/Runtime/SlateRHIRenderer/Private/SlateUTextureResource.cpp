// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlateUTextureResource.h"

FSlateBaseUTextureResource::FSlateBaseUTextureResource(UTexture* InTexture)
	: TextureObject(InTexture)
{
#if SLATE_CHECK_UOBJECT_RENDER_RESOURCES
	ObjectWeakPtr = InTexture;
	UpdateDebugName();
#endif
}

FSlateBaseUTextureResource::~FSlateBaseUTextureResource()
{
}

uint32 FSlateBaseUTextureResource::GetWidth() const
{
	return TextureObject->GetSurfaceWidth();
}

uint32 FSlateBaseUTextureResource::GetHeight() const
{
	return TextureObject->GetSurfaceHeight();
}

ESlateShaderResource::Type FSlateBaseUTextureResource::GetType() const
{
	return ESlateShaderResource::TextureObject;
}

#if SLATE_CHECK_UOBJECT_RENDER_RESOURCES
void FSlateBaseUTextureResource::UpdateDebugName()
{
	if (TextureObject)
	{
		DebugName = TextureObject->GetFName();
	}
	else
	{
		DebugName = NAME_None;
	}
}

void FSlateBaseUTextureResource::CheckForStaleResources() const
{
	if (DebugName != NAME_None && GSlateCheckUObjectRenderResources)
	{
		// pending kill objects may still be rendered for a frame so it is valid for the check to pass
		const bool bEvenIfPendingKill = true;
		// This test needs to be thread safe.  It doesn't give us as many chances to trap bugs here but it is still useful
		const bool bThreadSafe = true;
		if (!ObjectWeakPtr.IsValid(bEvenIfPendingKill, bThreadSafe))
		{
			if (GSlateCheckUObjectRenderResourcesShouldLogFatal)
			{
				UE_LOG(LogSlate, Fatal, TEXT("%s"), TEXT("Texture %s has become invalid. This means the resource was garbage collected while slate was using it"), *DebugName.ToString());
			}
			else
			{
				checkf(false, TEXT("Texture %s has become invalid. This means the resource was garbage collected while slate was using it"), *DebugName.ToString());
			}
		}
	}
}
#endif


TSharedPtr<FSlateUTextureResource> FSlateUTextureResource::NullResource = MakeShareable( new FSlateUTextureResource(nullptr) );

FSlateUTextureResource::FSlateUTextureResource(UTexture* InTexture)
	: FSlateBaseUTextureResource(InTexture)
	, Proxy(new FSlateShaderResourceProxy)
{
	if(TextureObject)
	{
		Proxy->ActualSize = FIntPoint(InTexture->GetSurfaceWidth(), InTexture->GetSurfaceHeight());
		Proxy->Resource = this;
	}
}

FSlateUTextureResource::~FSlateUTextureResource()
{
	if ( Proxy )
	{
		delete Proxy;
	}
}

void FSlateUTextureResource::UpdateTexture(UTexture* InTexture)
{
	TextureObject = InTexture;

#if SLATE_CHECK_UOBJECT_RENDER_RESOURCES
	ObjectWeakPtr = TextureObject;
	UpdateDebugName();
#endif

	if (!Proxy)
	{
		Proxy = new FSlateShaderResourceProxy;
	}

	FTexture* TextureResource = InTexture->GetResource();

	Proxy->Resource = this;
	// If the RHI data has changed, it's possible the underlying size of the texture has changed,
	// if that's true we need to update the actual size recorded on the proxy as well, otherwise 
	// the texture will continue to render using the wrong size.
	Proxy->ActualSize = FIntPoint(TextureResource->GetSizeX(), TextureResource->GetSizeY());
}

void FSlateUTextureResource::ResetTexture()
{
	TextureObject = nullptr;

#if SLATE_CHECK_UOBJECT_RENDER_RESOURCES
	ObjectWeakPtr = nullptr;
	UpdateDebugName();
#endif

	if (Proxy)
	{
		delete Proxy;
	}
	Proxy = nullptr;
}

