// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Templates/SharedPointer.h"

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

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#endif
