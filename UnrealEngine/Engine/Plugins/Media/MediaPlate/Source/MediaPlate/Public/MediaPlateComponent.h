// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "MediaPlayerOptions.h"
#include "MediaPlayerProxyInterface.h"
#include "MediaSource.h"
#include "MediaTextureTracker.h"

#include "MediaPlateComponent.generated.h"

class FMediaComponentClockSink;
class UMediaComponent;
class UMediaPlayer;
class UMediaPlaylist;
class UMediaSoundComponent;
class UMediaSource;
class UMediaTexture;


/**
 * This is a component for AMediaPlate that can play and show media in the world.
 */
UCLASS()
class MEDIAPLATE_API UMediaPlateComponent : public UActorComponent,
	public IMediaPlayerProxyInterface
{
	GENERATED_UCLASS_BODY()

public:
	//~ UActorComponent interface.
	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void BeginDestroy() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void OnUnregister() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

	/**
	 * Call this get our media player.
	 */
	UFUNCTION(BlueprintCallable, Category = "Media|MediaPlateComponent")
	UMediaPlayer* GetMediaPlayer();

	/**
	 * Call this get our media texture.
	 */
	UFUNCTION(BlueprintCallable, Category = "Media|MediaPlateComponent")
	UMediaTexture* GetMediaTexture();

	/**
	 * Call this to open the media.
	 */
	UFUNCTION(BlueprintCallable, Category = "Media|MediaPlateComponent")
	void Open();

	/**
	 * Play the next item in the playlist.
	 *
	 * returns	True if it played something.
	 */
	bool Next();

	/**
	 * Call this to start playing.
	 * Open must be called before this.
	 */
	UFUNCTION(BlueprintCallable, Category = "Media|MediaPlateComponent")
	void Play();

	/**
	 * Call this to pause playback.
	 * Play can be called to resume playback.
	 */
	UFUNCTION(BlueprintCallable, Category = "Media|MediaPlateComponent")
	void Pause();

	/**
	 * Play the previous item in the playlist.
	 *
	 * returns	True if it played something.
	 */
	bool Previous();

	/**
	 * Rewinds the media to the beginning.
	 *
	 * This is the same as seeking to zero time.
	 *
	 * @return				True if rewinding, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = "Media|MediaPlateComponent")
	bool Rewind();

	/**
	 * Call this to seek to the specified playback time.
	 * 
	 * @param Time			Time to seek to.
	 * @return				True on success, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = "Media|MediaPlateComponent")
	bool Seek(const FTimespan& Time);

	/**
	 * Call this to close the media.
	 */
	UFUNCTION(BlueprintCallable, Category = "Media|MediaPlateComponent")
	void Close();

	/**
	 * Call this to see if the media plate is playing.
	 */
	UFUNCTION(BlueprintGetter)
	bool IsMediaPlatePlaying() const { return bIsMediaPlatePlaying; }

	/**
	 * Call this to see if we want to loop.
	 */
	UFUNCTION(BlueprintGetter)
	bool GetLoop();

	/**
	 * Call this enable/disable looping.
	 */
	UFUNCTION(BlueprintSetter)
	void SetLoop(bool bInLoop);

	/** If set then play when opening the media. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MediaPlate")
	bool bPlayOnOpen = true;

	/** If set then start playing when this object is active. */
	UPROPERTY(EditAnywhere, Category = "MediaPlate")
	bool bAutoPlay = true;

	/** If set then enable audio. */
	UPROPERTY(EditAnywhere, Category = "MediaPlate")
	bool bEnableAudio = false;

	/** What time to start playing from (in seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MediaPlate", meta = (ClampMin = "0.0"))
	float StartTime = 0.0f;

	/** If true then set the aspect ratio automatically based on the media. */
	UPROPERTY(BlueprintReadWrite, Category = "MediaPlate")
	bool bIsAspectRatioAuto = true;

	/** Holds the component to play sound. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = MediaPlate)
	TObjectPtr<UMediaSoundComponent> SoundComponent;

	/** Holds the component for the mesh. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = MediaPlate)
	TObjectPtr<UStaticMeshComponent> StaticMeshComponent;
	/** Holds the component for the mesh. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = MediaPlate)
	TArray<TObjectPtr<UStaticMeshComponent>> Letterboxes;

	/** What media playlist to play. */
	UPROPERTY(Instanced, BlueprintReadWrite, Category = "MediaPlate")
	TObjectPtr<UMediaPlaylist> MediaPlaylist;

	/** The current index of the source in the play list being played. */
	UPROPERTY(BlueprintReadWrite, Category = "MediaPlate")
	int32 PlaylistIndex = 0;

	/** Override the default cache settings. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "MediaPlate")
	FMediaSourceCacheSettings CacheSettings;

	/** Set the arc size in degrees used for visible mips and tiles calculations, specific to the sphere. */
	UFUNCTION(BlueprintCallable, Category = "Media|MediaPlateComponent")
	void SetMeshRange(FVector2D InMeshRange);

	/** Return the arc size in degrees used for visible mips and tiles calculations, specific to the sphere. */
	UFUNCTION(BlueprintCallable, Category = "Media|MediaPlateComponent")
	FVector2D GetMeshRange() const { return MeshRange; }

	/** Call this to set bPlayOnlyWhenVisible. */
	UFUNCTION(BlueprintCallable, Category = "Media|MediaPlateComponent")
	void SetPlayOnlyWhenVisible(bool bInPlayOnlyWhenVisible);

	/**
	 * Call this to get the aspect ratio of the mesh.
	 */
	float GetAspectRatio();

	/**
	 * Call this to set the aspect ratio of the mesh.
	 */
	void SetAspectRatio(float AspectRatio);

	/**
	 * Call this to get the aspect ratio of the screen.
	 */
	UFUNCTION(BlueprintCallable, Category = "Media|MediaPlateComponent")
	float GetLetterboxAspectRatio() { return LetterboxAspectRatio; }

	/**
	 * Call this to set the aspect ratio of the screen.
	 */
	UFUNCTION(BlueprintCallable, Category = "Media|MediaPlateComponent")
	void SetLetterboxAspectRatio(float AspectRatio);

	/**
	 * Call this to see if this plate wants to play when it becomes visible.
	 */
	bool GetWantsToPlayWhenVisible() const { return bWantsToPlayWhenVisible; }

#if WITH_EDITOR
	/**
	 * Call this to get the mip tile calculations mesh mode.
	 */
	EMediaTextureVisibleMipsTiles GetVisibleMipsTilesCalculations() const { return VisibleMipsTilesCalculations; }
	/**
	 * Call this to set the mip tile calculations mesh mode. (Note: restarts playback to apply changes.)
	 */
	void SetVisibleMipsTilesCalculations(EMediaTextureVisibleMipsTiles InVisibleMipsTilesCalculations);
#endif

	/**
	 * Called from the media clock.
	 */
	void TickOutput();

	//~ IMediaPlayerProxyInterface.
	virtual float GetProxyRate() const override;
	virtual bool SetProxyRate(float Rate) override;
	virtual bool IsExternalControlAllowed() override;
	virtual const FMediaSourceCacheSettings& GetCacheSettings() const override;

private:
	/**
	 * Adds our media texture to the media texture tracker.
	 */
	void RegisterWithMediaTextureTracker();
	/**
	 * Removes our texture from the media texture tracker.
	 */
	void UnregisterWithMediaTextureTracker();

	/**
	 * Should be called when bPlayOnlyWhenVisible changes.
	 */
	void PlayOnlyWhenVisibleChanged();

	void RestartPlayer();

	/**
	 * If true, then we want the media plate to play.
	 * Note that this could be true, but the player is not actually playing because
	 * bPlayOnlyWhenVisible = true and the plate is not visible.
	 */
	UPROPERTY(Blueprintgetter = IsMediaPlatePlaying, Category = "MediaPlate", meta = (AllowPrivateAccess = true))
	bool bIsMediaPlatePlaying = false;

	/** Desired rate of play that we want. */
	float CurrentRate = 0.0f;

	/** If true then only allow playback when the media plate is visible. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MediaPlate", meta = (AllowPrivateAccess = true))
	bool bPlayOnlyWhenVisible = false;

	/** If set then loop when we reach the end. */
	UPROPERTY(EditAnywhere, Blueprintgetter = GetLoop, BlueprintSetter = SetLoop, Category = "MediaPlate", meta = (AllowPrivateAccess = true))
	bool bLoop = true;

	/** Visible mips and tiles calculation mode for the supported mesh types in MediaPlate. (Player restart on change.) */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "MediaPlate", meta = (AllowPrivateAccess = true))
	EMediaTextureVisibleMipsTiles VisibleMipsTilesCalculations;

	/** Media texture mip map bias shared between the (image sequence) loader and the media texture sampler. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "MediaPlate", meta = (AllowPrivateAccess = true, UIMin = "-16.0", UIMax = "15.99"))
	float MipMapBias = 0.0f;

	/** If > 0, then this is the aspect ratio of our screen and 
	 * letterboxes will be added if the media is smaller than the screen. */
	UPROPERTY()
	float LetterboxAspectRatio = 0.0f;

	UPROPERTY()
	FVector2D MeshRange = FVector2D(360.0f, 180.0f);

	/** Name for our media component. */
	static FLazyName MediaComponentName;
	/** Name for our playlist. */
	static FLazyName MediaPlaylistName;

	/** Holds the media player. */
	UPROPERTY(Instanced)
	TObjectPtr<UMediaTexture> MediaTexture;

	/** This component's media player */
	UPROPERTY(Instanced)
	TObjectPtr<UMediaPlayer> MediaPlayer;

	/** Info representing this object. */
	TSharedPtr<FMediaTextureTrackerObject, ESPMode::ThreadSafe> MediaTextureTrackerObject;
	/** Our media clock sink. */
	TSharedPtr<FMediaComponentClockSink, ESPMode::ThreadSafe> ClockSink;
	/** Game time when we paused playback. */
	float TimeWhenPlaybackPaused = 0.0f;
	/** True if our media should be playing when visible. */
	bool bWantsToPlayWhenVisible = false;
	/** True if we should resume where we left off when we open the media. */
	bool bResumeWhenOpened = false;

	/**
	 * Plays a media source.
	 * 
	 * @param	InMediaSource		Media source to play.
	 * @return	True if we played anything.
	 */
	bool PlayMediaSource(UMediaSource* InMediaSource);

	/**
	 * Stops the clock sink so we no longer tick.
	 */
	void StopClockSink();

	/**
	 * Call this to see if this media plate is visible.
	 */
	bool IsVisible();

	/**
	 * Call this to resume playback when the media plate is visible.
	 */
	void ResumeWhenVisible();

	/**
	 * Returns the time to seek to when resuming playback.
	 */
	FTimespan GetResumeTime();

	/**
	 * Updates if we should tick or not based on current state.
	 */
	void UpdateTicking();

	/**
	 * Updates letterboxes based on the current state.
	 */
	void UpdateLetterboxes();

	/**
	 * Adds ability to have letterboxes.
	 */
	void AddLetterboxes();

	/**
	 * Removes ability to have letterboxes.
	 */
	void RemoveLetterboxes();

	/**
	 * Called by the media player when the media opens.
	 */
	UFUNCTION()
	void OnMediaOpened(FString DeviceUrl);

	/**
	 * Called by the media player when the video ends.
	 */
	UFUNCTION()
	void OnMediaEnd();

};
