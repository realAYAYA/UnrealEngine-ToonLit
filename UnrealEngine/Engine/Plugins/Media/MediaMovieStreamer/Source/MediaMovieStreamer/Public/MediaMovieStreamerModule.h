// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FMediaMovieStreamer;
class UMediaMovieAssets;

class FMediaMovieStreamerModule : public IModuleInterface
{
public:
	/**
	 * Call this to get the MovieAssets.
	 */
	static UMediaMovieAssets* GetMovieAssets();

	/**
	 * Call this to get the MovieStreamer.
	 */
	MEDIAMOVIESTREAMER_API static const TSharedPtr<FMediaMovieStreamer, ESPMode::ThreadSafe> GetMovieStreamer();

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	static bool bStartedModule;
};
