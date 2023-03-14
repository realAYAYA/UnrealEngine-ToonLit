// Copyright Epic Games Tools LLC
//   Licenced under the Unreal Engine EULA 
#pragma once

#include "Runtime/Launch/Resources/Version.h"
#include "binkplugin.h"
#include "MoviePlayer.h"

DECLARE_LOG_CATEGORY_EXTERN(LogBinkMoviePlayer, Log, All);

class FBinkMovieStreamer : public IMovieStreamer 
{
public:
	FBinkMovieStreamer();
	virtual ~FBinkMovieStreamer();

	virtual bool Init(const TArray<FString>& MoviePaths, TEnumAsByte<EMoviePlaybackType> inPlaybackType) override;
	virtual FString GetMovieName() override;
	virtual bool IsLastMovieInPlaylist() override;
	virtual void ForceCompletion() override;
	virtual bool Tick(float DeltaTime) override;
	virtual TSharedPtr<class ISlateViewport> GetViewportInterface() override { return MovieViewport; }
	virtual float GetAspectRatio() const override { return (float)MovieViewport->GetSize().X / (float)MovieViewport->GetSize().Y; }
	virtual void Cleanup() override;

	FOnCurrentMovieClipFinished OnCurrentMovieClipFinishedDelegate;
	virtual FOnCurrentMovieClipFinished& OnCurrentMovieClipFinished() override { return OnCurrentMovieClipFinishedDelegate; }

	bool OpenNextMovie();
	void CloseMovie();

	// A list of all the stored movie paths we have enqueued for playing
	TArray<FString> StoredMoviePaths;

	// Texture and viewport data for displaying to Slate
	TSharedPtr<FMovieViewport> MovieViewport;
	TSharedPtr<FSlateTexture2DRHIRef, ESPMode::ThreadSafe> Texture;

	// List of textures pending deletion, need to keep this list because we can't immediately
	// destroy them since they could be getting used on the main thread
	TArray<TSharedPtr<FSlateTexture2DRHIRef, ESPMode::ThreadSafe>> TextureFreeList;

	/** if true, this sequence will loop when finsihed */
	TEnumAsByte<EMoviePlaybackType> PlaybackType;

	// The index in to the playlist (StoredMoviePaths) that is currently playing
	int32 MovieIndex;

	BINKPLUGIN *bnk;
};
