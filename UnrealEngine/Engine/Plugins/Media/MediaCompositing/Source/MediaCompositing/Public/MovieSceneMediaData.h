// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Timespan.h"
#include "Evaluation/MovieScenePropertyTemplate.h"
#include "UObject/WeakObjectPtrTemplates.h"

class IMediaPlayerProxy;
class UMediaPlayer;
enum class EMediaEvent;


/**
 * Persistent data that's stored for each currently evaluating section.
 */
struct MEDIACOMPOSITING_API FMovieSceneMediaData
	: PropertyTemplate::FSectionData
{
	/** Default constructor. */
	FMovieSceneMediaData();

	/** Virtual destructor. */
	virtual ~FMovieSceneMediaData() override;

public:

	/**
	 * Get the media player used by this persistent data.
	 *
	 * @return The currently used media player, if any.
	 */
	UMediaPlayer* GetMediaPlayer();

	/**
	 * Get the optional proxy object used by this persistent data.
	 */
	UObject* GetPlayerProxy() { return PlayerProxy.Get(); }

	/**
	 * Set the time to seek to after opening a media source has finished.
	 *
	 * @param Time The time to seek to.
	 */
	void SeekOnOpen(FTimespan Time);

	/** Set up this persistent data object. */
	void Setup(UMediaPlayer* OverrideMediaPlayer, UObject* InPlayerProxy);

private:

	/** Callback for media player events. */
	void HandleMediaPlayerEvent(EMediaEvent Event);

private:
	bool bOverrideMediaPlayer;

	/** The media player used by this object. */
	UMediaPlayer* MediaPlayer;
	/** Optional proxy for the media player. */
	TWeakObjectPtr<UObject> PlayerProxy;

	/** The time to seek to after the media source is opened. */
	FTimespan SeekOnOpenTime;
};
