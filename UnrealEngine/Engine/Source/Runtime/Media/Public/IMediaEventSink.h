// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Enumerates media player related events.
 */
enum class EMediaEvent
{
	/** The media source started buffering data. */
	MediaBuffering,

	/** The current media source has been closed. */
	MediaClosed,

	/** The player started connecting to the media source. */
	MediaConnecting,

	/** A new media source has been opened. */
	MediaOpened,

	/** A media source failed to open. */
	MediaOpenFailed,

	/** The end of the media (or beginning if playing in reverse) has been reached. */
	PlaybackEndReached,

	/** Playback has been resumed. */
	PlaybackResumed,

	/** Playback has been suspended. */
	PlaybackSuspended,

	/** Seek operation has completed successfully. */
	SeekCompleted,

	/** Media tracks have changed. */
	TracksChanged,

	// - - - - - - - - - - - - - - - - - - - - - - - -

	/** Start of internal events not sent out to user code. */
	Internal_Start,

	/** Player requests purge of outdated video samples from queues. */
	Internal_PurgeVideoSamplesHint = Internal_Start,

	/** Player requests an internal reset of all queues and timing. **/
	Internal_ResetForDiscontinuity,

	/** Player requests render clock to start and stop in preparation for moving playback to a new position. **/
	Internal_RenderClockStart,
	Internal_RenderClockStop,

	/** Player notifies about availability or unavailablity of media samples. **/
	Internal_VideoSamplesAvailable,
	Internal_VideoSamplesUnavailable,
	Internal_AudioSamplesAvailable,
	Internal_AudioSamplesUnavailable
};


/**
 * Interface for classes that receive media player events.
 */
class IMediaEventSink
{
public:

	/**
	 * Receive the given media event.
	 *
	 * @param Event The event to receive.
	 * @note Implementors must be thread-safe as this method may be called from arbitrary worker threads.
	 */
	virtual void ReceiveMediaEvent(EMediaEvent Event) = 0;

public:

	/** Virtual destructor. */
	virtual ~IMediaEventSink() { }
};
