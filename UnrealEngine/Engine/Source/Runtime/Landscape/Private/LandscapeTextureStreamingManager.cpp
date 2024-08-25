// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeTextureStreamingManager.h"
#include "Engine/Texture.h"
#include "TextureCompiler.h"

namespace UE::Landscape
{
	// double check that a texture is forced resident
	static inline void EnsureTextureForcedResident(UTexture *Texture)
	{
		// if other systems mess with this flag, then restore it to what it should be
		// Any code that is directly messing with the flag on one of our
		// landscape related textures should go through this streaming system instead
		if (!ensure(Texture->bForceMiplevelsToBeResident))
		{
			Texture->bForceMiplevelsToBeResident = true;
		}
	}
}

bool FLandscapeTextureStreamingManager::RequestTextureFullyStreamedIn(UTexture* Texture, bool bWaitForStreaming)
{
	TWeakObjectPtr<UTexture> TexturePtr = Texture;
	FTextureState& State = TextureStates.FindOrAdd(TexturePtr);

	if (State.RequestCount == 0)
	{
		Texture->bForceMiplevelsToBeResident = true;
	}
	else
	{
		UE::Landscape::EnsureTextureForcedResident(Texture);
	}
	State.RequestCount++;

	if (IsTextureFullyStreamedIn(Texture))
	{
		return true;
	}
	else if (bWaitForStreaming)
	{
		Texture->WaitForStreaming();
		return IsTextureFullyStreamedIn(Texture);
	}
	return false;
}

bool FLandscapeTextureStreamingManager::RequestTextureFullyStreamedInForever(UTexture* Texture, bool bWaitForStreaming)
{
	TWeakObjectPtr<UTexture> TexturePtr = Texture;
	FTextureState& State = TextureStates.FindOrAdd(TexturePtr);
	State.bForever = true;
	Texture->bForceMiplevelsToBeResident = true;

	if (IsTextureFullyStreamedIn(Texture))
	{
		return true;
	}
	else if (bWaitForStreaming)
	{
		Texture->WaitForStreaming();
		return IsTextureFullyStreamedIn(Texture);
	}
	return false;
}

void FLandscapeTextureStreamingManager::UnrequestTextureFullyStreamedIn(UTexture* Texture)
{
	if (Texture == nullptr)
	{
		return;
	}

	TWeakObjectPtr<UTexture> TexturePtr = Texture;
	FTextureState* State = TextureStates.Find(TexturePtr);
	if (State)
	{
		if (State->RequestCount > 0)
		{
			State->RequestCount--;
			if (!State->bForever && State->RequestCount <= 0)
			{
				// allow stream out, remove tracking
				Texture->bForceMiplevelsToBeResident = false;
				TextureStates.Remove(TexturePtr);
			}
			else
			{
				UE::Landscape::EnsureTextureForcedResident(Texture);
			}
		}
		else
		{
			// only way the request count should get to zero is if the texture is flagged as forever streamed.
			ensure(State->bForever);
			UE::Landscape::EnsureTextureForcedResident(Texture);
		}
	}
}

bool FLandscapeTextureStreamingManager::WaitForTextureStreaming()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(LandscapeTextureStreamingaAnager_WaitForTextureStreaming);
	bool bFullyStreamed = true;
	for (auto It = TextureStates.CreateIterator(); It; ++It)
	{
		UTexture* Texture = It.Key().Get();
		if (Texture)
		{
			UE::Landscape::EnsureTextureForcedResident(Texture);
			if (!IsTextureFullyStreamedIn(Texture))
			{
#if WITH_EDITOR
				// in editor, textures can be not compiled yet
				FTextureCompilingManager::Get().FinishCompilation({ Texture });
#endif // WITH_EDITOR
				Texture->WaitForStreaming();
			}
			bFullyStreamed = bFullyStreamed && IsTextureFullyStreamedIn(Texture);
		}
		else
		{
			// the texture was unloaded... we can remove this entry
			It.RemoveCurrent();
		}
	}
	return bFullyStreamed;
}

void FLandscapeTextureStreamingManager::CleanupInvalidEntries()
{
	for (auto It = TextureStates.CreateIterator(); It; ++It)
	{
		TWeakObjectPtr<UTexture>& TexPtr = It.Key();
		if (!TexPtr.IsValid())
		{
			It.RemoveCurrent();
		}
	}
}

void FLandscapeTextureStreamingManager::CheckRequestedTextures()
{
#if WITH_EDITOR
	if (UndoDetector.bUndoRedoPerformed)
	{
		// the force mip levels resident flag sometimes gets cleared on an undo after landscape creation, but we can fix it
		// (otherwise we may wait forever for them to become resident)
		for (auto It = TextureStates.CreateIterator(); It; ++It)
		{
			if (UTexture* Texture = It.Key().Get())
			{
				FTextureState& State = It.Value();
				if (State.bForever || State.RequestCount > 0)
				{
					if (!Texture->bForceMiplevelsToBeResident)
					{
						Texture->bForceMiplevelsToBeResident = true;
					}
				}
			}
		}
		UndoDetector.bUndoRedoPerformed = false;
	}
#endif // WITH_EDITOR
}

bool FLandscapeTextureStreamingManager::IsTextureFullyStreamedIn(UTexture* InTexture)
{
	return InTexture &&
#if WITH_EDITOR
		!InTexture->IsDefaultTexture() &&
#endif // WITH_EDITOR
		!InTexture->HasPendingInitOrStreaming() && InTexture->IsFullyStreamedIn();
}

FLandscapeTextureStreamingManager::~FLandscapeTextureStreamingManager()
{
	if (!ensure(TextureStates.IsEmpty()))
	{
		// clear force stream flag on all textures, just in case
		for (auto It = TextureStates.CreateIterator(); It; ++It)
		{
			UTexture* Texture = It.Key().Get();
			if (Texture)
			{
				Texture->bForceMiplevelsToBeResident = false;
			}
		}
	}
}
