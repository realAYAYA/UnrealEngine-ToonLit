// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Timespan.h"
#include "Evaluation/MovieScenePropertyTemplate.h"
#include "UObject/WeakObjectPtrTemplates.h"

class IMediaPlayerProxy;
class UMediaPlayer;
class UMediaTexture;
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
	 * Get the layer index we are using (when using a proxy).
	 */
	int32 GetProxyLayerIndex() { return ProxyLayerIndex; }

	/**
	 * Get the texture index we are using (when using a proxy).
	 */
	int32 GetProxyTextureIndex() { return ProxyTextureIndex; }

	/**
	 * Set the time to seek to after opening a media source has finished.
	 *
	 * @param Time The time to seek to.
	 */
	void SeekOnOpen(FTimespan Time);

	/** Set up this persistent data object. */
	void Setup(UMediaPlayer* OverrideMediaPlayer, UObject* InPlayerProxy, int32 InProxyLayerIndex, int32 InProxyTextureIndex);

	/**
	 * Called from FMovieSceneMediaSectionTemplate::Initialize.
	 */
	void Initialize(bool bIsEvaluating);

	/**
	 * Called from FMovieSceneMediaSectionTemplate::TearDown.
	 */
	void TearDown();

	/**
	 * Stores if the aspect ratio has been set yet.
	 */
	bool bIsAspectRatioSet;

private:
	/**
	 * Does the work needed so we can use our proxy media texture.
	 */
	void StartUsingProxyMediaTexture();

	/**
	 * Does the work needed when we no longer use our proxy media texture.
	 */
	void StopUsingProxyMediaTexture();

	/** Callback for media player events. */
	void HandleMediaPlayerEvent(EMediaEvent Event);

private:
	bool bOverrideMediaPlayer;

	/** The media player used by this object. */
	UMediaPlayer* MediaPlayer;
	/** Optional proxy for the media player. */
	TWeakObjectPtr<UObject> PlayerProxy;
	/** Media texture allocated from the proxy. */
	TWeakObjectPtr<UMediaTexture> ProxyMediaTexture;
	/** Layer that this section should reside in. */
	int32 ProxyLayerIndex;
	/** Index of texture allocated from the proxy. */
	int32 ProxyTextureIndex;

	/** The time to seek to after the media source is opened. */
	FTimespan SeekOnOpenTime;
};
