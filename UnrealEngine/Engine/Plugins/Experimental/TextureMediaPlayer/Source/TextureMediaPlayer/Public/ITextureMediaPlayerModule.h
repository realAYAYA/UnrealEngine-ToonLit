// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"
#include "RHI.h"

class ITextureMediaPlayer;
class IMediaEventSink;
class IMediaPlayer;
class UMediaPlayer;

class ITextureMediaPlayerModule: public IModuleInterface
{
public:
	virtual TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CreatePlayer(IMediaEventSink& EventSink) = 0;

	/**
	 * Opens the player and returns the interface which you can then pass to Texture
	 * so the player can receive frames from Texture.
	 */
	virtual TSharedPtr<ITextureMediaPlayer, ESPMode::ThreadSafe> OpenPlayer(UMediaPlayer* MediaPlayer) = 0;

	/**
	 * Players will call this so that this module can return this video sink in OpenPlayer.
	 */
	virtual void RegisterVideoSink(TSharedPtr<ITextureMediaPlayer, ESPMode::ThreadSafe> InVideoSink) = 0;

};
