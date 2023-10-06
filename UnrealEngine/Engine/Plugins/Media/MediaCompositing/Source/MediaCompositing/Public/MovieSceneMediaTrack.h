// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Guid.h"
#include "MovieSceneNameableTrack.h"
#include "UObject/ObjectMacros.h"
#include "UObject/StrongObjectPtr.h"
#include "Compilation/IMovieSceneTrackTemplateProducer.h"

#include "MovieSceneMediaTrack.generated.h"

class FMovieSceneMediaTrackClockSink;
class UMediaPlayer;
class UMediaSource;
class UMovieSceneMediaSection;
struct FMovieSceneObjectBindingID;


/**
 * Implements a movie scene track for media playback.
 */
UCLASS()
class MEDIACOMPOSITING_API UMovieSceneMediaTrack
	: public UMovieSceneNameableTrack
	, public IMovieSceneTrackTemplateProducer
{
public:

	GENERATED_BODY()

	/**
	 * Create and initialize a new instance.
	 *
	 * @param ObjectInitializer The object initializer.
	 */
	UMovieSceneMediaTrack(const FObjectInitializer& ObjectInitializer);

public:

	/** Adds a new media source to the track. */
	virtual UMovieSceneSection* AddNewMediaSourceOnRow(UMediaSource& MediaSource, FFrameNumber Time, int32 RowIndex);
	/** Adds a new media source to the track. */
	virtual UMovieSceneSection* AddNewMediaSourceProxyOnRow(UMediaSource* MediaSource, const FMovieSceneObjectBindingID& ObjectBinding, int32 MediaSourceProxyIndex, FFrameNumber Time, int32 RowIndex);

	/** Adds a new media source on the next available/non-overlapping row. */
	virtual UMovieSceneSection* AddNewMediaSource(UMediaSource& MediaSource, FFrameNumber Time) { return AddNewMediaSourceOnRow(MediaSource, Time, INDEX_NONE); }
	/** Adds a new media source on the next available/non-overlapping row. */
	virtual UMovieSceneSection* AddNewMediaSourceProxy(UMediaSource* MediaSource, const FMovieSceneObjectBindingID& ObjectBinding, int32 MediaSourceProxyIndex, FFrameNumber Time)
	{
		return AddNewMediaSourceProxyOnRow(MediaSource, ObjectBinding, MediaSourceProxyIndex, Time, INDEX_NONE);
	}

	/**
	 * Called from the media clock.
	 */
	void TickOutput();

public:

	//~ UMovieScenePropertyTrack interface

	virtual void AddSection(UMovieSceneSection& Section) override;
	virtual bool SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const override;
	virtual UMovieSceneSection* CreateNewSection() override;
	virtual FMovieSceneEvalTemplatePtr CreateTemplateForSection(const UMovieSceneSection& InSection) const override;
	virtual const TArray<UMovieSceneSection*>& GetAllSections() const override;
	virtual bool HasSection(const UMovieSceneSection& Section) const override;
	virtual bool IsEmpty() const override;
	virtual void RemoveSection(UMovieSceneSection& Section) override;
	virtual void RemoveSectionAt(int32 SectionIndex) override;
	virtual bool SupportsMultipleRows() const override { return true; }


#if WITH_EDITOR

	virtual void BeginDestroy() override;
	
	virtual EMovieSceneSectionMovedResult OnSectionMoved(UMovieSceneSection& Section, const FMovieSceneSectionMovedParams& Params);

#endif // WITH_EDITOR

private:
	/** Base function to add a new section. */
	UMovieSceneMediaSection* AddNewSectionOnRow(FFrameNumber Time, int32 RowIndex);

	/** List of all media sections. */
	UPROPERTY()
	TArray<TObjectPtr<UMovieSceneSection>> MediaSections;

#if WITH_EDITORONLY_DATA

	/** List of players that are we are trying to get durations from for the corresponding sections. */
	TArray<TPair<TStrongObjectPtr<UMediaPlayer>, TWeakObjectPtr<UMovieSceneSection>>> NewSections;
	/** Our media clock sink. */
	TSharedPtr<FMovieSceneMediaTrackClockSink, ESPMode::ThreadSafe> ClockSink;
	/** If true then don't check for the duration this frame. */
	bool bGetDurationDelay;

#endif // WITH_EDITORONLY_DATA

	/**
	 * Updates all our sections so they have correct texture indices
	 * so the proxy can blend all the sections together correctly.
	 */
	void UpdateSectionTextureIndices();

#if WITH_EDITOR
	/**
	 * Starts the process to get the duration of the media.
	 * It might take a frame or more.
	 *
	 * @param MediaSource		Media to inspect.
	 * @param Section			Will set this sequencer section to the length of the media.
	*/
	void StartGetDuration(UMediaSource* MediaSource, UMovieSceneSection* Section);

	/**
	 * Call this after StartGetDuration to try and get the duration of the media.
	 *
	 * @param MediaPlayer		Player that is opening the media.
	 * @param NewSection		Movie section will be set to the media duration.
	 * @return True if it is done and the player can be removed.
	 */
	bool GetDuration(const TStrongObjectPtr<UMediaPlayer>& MediaPlayer,
		TWeakObjectPtr<UMovieSceneSection>& NewSection);
#else  // WITH_EDITOR
	void StartGetDuration(UMediaSource* MediaSource, UMovieSceneSection* Section) {}
#endif // WITH_EDITOR

};
