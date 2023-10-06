// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/Widget.h"

#include "CommonVideoPlayer.generated.h"

class SImage;
class UMaterial;
class UMediaSource;
class UMediaPlayer;
class UMediaTexture;
class UMediaSoundComponent;
class USoundClass;

enum class EMediaEvent;

UCLASS(ClassGroup = UI, meta = (Category = "Common UI"))
class COMMONUI_API UCommonVideoPlayer : public UWidget
{
	GENERATED_BODY()

public:
	UCommonVideoPlayer(const FObjectInitializer& Initializer);
	virtual void PostInitProperties() override;

	void SetVideo(UMediaSource* NewVideo);
	void Seek(float PlaybackTime);
	void Close();

	void SetPlaybackRate(float PlaybackRate);
	void SetLooping(bool bShouldLoopPlayback);
	void SetIsMuted(bool bInIsMuted);

	void Play();
	void Reverse();
	void Pause();
	void PlayFromStart();

	float GetVideoDuration() const;
	float GetPlaybackTime() const;
	float GetPlaybackRate() const;

	bool IsLooping() const;
	bool IsPaused() const;
	bool IsPlaying() const;
	bool IsMuted() const { return bIsMuted; }

	FSimpleMulticastDelegate& OnPlaybackResumed() { return OnPlaybackResumedEvent; }
	FSimpleMulticastDelegate& OnPlaybackPaused() { return OnPlaybackPausedEvent; }
	FSimpleMulticastDelegate& OnPlaybackComplete() { return OnPlaybackCompleteEvent; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void SynchronizeProperties() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

	void PlayInternal() const;
	const UMediaPlayer& GetMediaPlayer() const { return *MediaPlayer; }
	virtual void HandleMediaPlayerEvent(EMediaEvent EventType);
	virtual void PlaybackTick(double InCurrentTime, float InDeltaTime);

private:
	EActiveTimerReturnType HandlePlaybackTick(double InCurrentTime, float InDeltaTime);

private:
	UPROPERTY(EditAnywhere, Category = VideoPlayer)
	TObjectPtr<UMediaSource> Video;

	UPROPERTY(Transient)
	TObjectPtr<UMediaPlayer> MediaPlayer;

	UPROPERTY(Transient)
	TObjectPtr<UMediaTexture> MediaTexture;

	UPROPERTY(Transient)
	TObjectPtr<UMaterial> VideoMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMediaSoundComponent> SoundComponent;

	UPROPERTY(Transient)
	FSlateBrush VideoBrush;

	mutable FSimpleMulticastDelegate OnPlaybackResumedEvent;
	mutable FSimpleMulticastDelegate OnPlaybackPausedEvent;
	mutable FSimpleMulticastDelegate OnPlaybackCompleteEvent;


	bool bIsMuted = false;
	TSharedPtr<SImage> MyImage;
};
