// Copyright Epic Games, Inc. All Rights Reserved.

#include "ElectraSamplesTextureManager.h"
#include "Misc/ScopeLock.h"

FElectraSamplesTextureManager::FElectraSamplesTextureManager()
{
}

FElectraSamplesTextureManager::~FElectraSamplesTextureManager()
{
}

void FElectraSamplesTextureManager::CleanupMap()
{
	TArray<void*> InvalidKeys;
	InvalidKeys.Reserve(KnownTextures.Num());
	for (const TPair<void*, TWeakPtr<IVideoDecoderTexture, ESPMode::ThreadSafe>>& Item : KnownTextures)
	{
		if (!Item.Value.IsValid())
		{
			InvalidKeys.Add(Item.Key);
		}
	}
	for (void* Key : InvalidKeys)
	{
		KnownTextures.Remove(Key);
	}
}

TSharedPtr<IVideoDecoderTexture, ESPMode::ThreadSafe> FElectraSamplesTextureManager::CreateTexture(const FIntPoint& Dim, EPixelFormat Fmt)
{
	FScopeLock Lock(&AccessCS);

	CleanupMap();
	TSharedPtr<IVideoDecoderTexture, ESPMode::ThreadSafe> NewTexture = PlatformCreateTexture(Dim, Fmt);
	if (NewTexture.IsValid())
	{
		KnownTextures.Add(PlatformGetTexturePlatform(NewTexture.Get()), NewTexture);
	}
	return NewTexture;
}

void FElectraSamplesTextureManager::PlatformAddSharedTextureRef(TSharedPtr<IVideoDecoderTexture, ESPMode::ThreadSafe> InTextureRef)
{
	FScopeLock Lock(&AccessCS);
	if (InTextureRef.IsValid())
	{
		// Only add once if it's not already there.
		KnownTextures.FindOrAdd(PlatformGetTexturePlatform(InTextureRef.Get()), InTextureRef);
	}
}

void FElectraSamplesTextureManager::PlatformRemoveSharedTextureRef(TSharedPtr<IVideoDecoderTexture, ESPMode::ThreadSafe> InTextureRef)
{
	FScopeLock Lock(&AccessCS);
	if (InTextureRef.IsValid())
	{
		KnownTextures.Remove(PlatformGetTexturePlatform(InTextureRef.Get()));
	}
}


FTexture2DRHIRef FElectraSamplesTextureManager::GetRHITextureFromPlatformTexture(void* PlatformTexture)
{
	FScopeLock Lock(&AccessCS);

	CleanupMap();
	TWeakPtr<IVideoDecoderTexture, ESPMode::ThreadSafe> *Texture = KnownTextures.Find(PlatformTexture);
	if (Texture)
	{
		if (TSharedPtr<IVideoDecoderTexture, ESPMode::ThreadSafe> PinnedTexture = Texture->Pin())
		{
			return PlatformGetTextureRHI(PinnedTexture.Get());
		}
	}
	return FTexture2DRHIRef();
}
