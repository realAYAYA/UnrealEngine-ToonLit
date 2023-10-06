// Copyright Epic Games, Inc. All Rights Reserved.

#include "MediaTextureTracker.h"
#include "MediaTexture.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MediaTextureTracker)

FMediaTextureTracker& FMediaTextureTracker::Get()
{
	static FMediaTextureTracker Engine;
	return Engine;
}

void FMediaTextureTracker::RegisterTexture(TSharedPtr<FMediaTextureTrackerObject, ESPMode::ThreadSafe>& InInfo,
	TObjectPtr<UMediaTexture> InTexture)
{
	// Do we have this media texture yet?
	TWeakObjectPtr<UMediaTexture> TexturePtr(InTexture);
	if (MediaTextures.Contains(TexturePtr) == false)
	{
		MediaTextures.Emplace(TexturePtr);
		MapTextureToObject.Emplace(TexturePtr);
	}

	// Add component to our list.
	TArray<TWeakPtr<FMediaTextureTrackerObject, ESPMode::ThreadSafe>>* FoundObjects = MapTextureToObject.Find(TexturePtr);
	FoundObjects->Add(InInfo);
}

void FMediaTextureTracker::UnregisterTexture(TSharedPtr<FMediaTextureTrackerObject, ESPMode::ThreadSafe>& InInfo,
	TObjectPtr<UMediaTexture> InTexture)
{
	TWeakObjectPtr<UMediaTexture> TexturePtr(InTexture);
	TArray<TWeakPtr<FMediaTextureTrackerObject, ESPMode::ThreadSafe>>* FoundObjects = MapTextureToObject.Find(TexturePtr);
	if (FoundObjects != nullptr)
	{
		FoundObjects->RemoveSwap(InInfo);
	}
}

const TArray<TWeakPtr<FMediaTextureTrackerObject, ESPMode::ThreadSafe>>* FMediaTextureTracker::GetObjects(TObjectPtr<UMediaTexture> InTexture) const
{
	const TArray<TWeakPtr<FMediaTextureTrackerObject, ESPMode::ThreadSafe>>* ObjectsPtr = MapTextureToObject.Find(InTexture);
	return ObjectsPtr;
}

