// Copyright Epic Games, Inc. All Rights Reserved.

#include "ITextureMediaPlayerModule.h"
#include "MediaPlayer.h"
#include "MediaPlayerOptions.h"
#include "StreamMediaSource.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "TextureMediaPlayer.h"

class FTextureMediaPlayerModule: public ITextureMediaPlayerModule
{
public:

	void StartupModule() override
	{
	}

	void ShutdownModule() override
	{
		if (MediaSource != nullptr)
		{
			MediaSource->RemoveFromRoot();
			MediaSource = nullptr;
		}
	}

	TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CreatePlayer(IMediaEventSink& EventSink) override
	{
		return MakeShareable(new FTextureMediaPlayer(EventSink));
	}

	virtual TSharedPtr<ITextureMediaPlayer, ESPMode::ThreadSafe> OpenPlayer(UMediaPlayer* MediaPlayer) override;
	virtual void RegisterVideoSink(TSharedPtr<ITextureMediaPlayer, ESPMode::ThreadSafe> InVideoSink) override;

private:
	/** We use this media source for all the players. */
	TWeakObjectPtr<UStreamMediaSource> MediaSource;
	/** Used only to pass the video sink in OpenPlayer. */
	TSharedPtr<ITextureMediaPlayer, ESPMode::ThreadSafe> VideoSink = nullptr;
};

TSharedPtr<ITextureMediaPlayer, ESPMode::ThreadSafe> FTextureMediaPlayerModule::OpenPlayer(UMediaPlayer* MediaPlayer)
{
	TSharedPtr<ITextureMediaPlayer, ESPMode::ThreadSafe> NewVideoSink = nullptr;

	if (MediaPlayer != nullptr)
	{
		// Create the media source if we don't have it yet.
		if (MediaSource == nullptr)
		{
			MediaSource = NewObject<UStreamMediaSource>(GetTransientPackage());
			MediaSource->AddToRoot();
			MediaSource->StreamUrl = FString(TEXT("Texture://"));
		}

		// Open the player.
		FMediaPlayerOptions Options;
		Options.PlayOnOpen = EMediaPlayerOptionBooleanOverride::Enabled;
		MediaPlayer->OpenSourceWithOptions(MediaSource.Get(), Options);

		// Retrive the video sink if it was set.
		NewVideoSink = VideoSink;
		VideoSink = nullptr;
	}

	return NewVideoSink;
}

void FTextureMediaPlayerModule::RegisterVideoSink(TSharedPtr<ITextureMediaPlayer, ESPMode::ThreadSafe> InVideoSink)
{
	VideoSink = InVideoSink;
}


IMPLEMENT_MODULE(FTextureMediaPlayerModule, TextureMediaPlayer)
