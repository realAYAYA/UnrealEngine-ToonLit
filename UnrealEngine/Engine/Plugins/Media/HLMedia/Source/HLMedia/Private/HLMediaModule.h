// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IHLMediaModule.h"

class FHLMediaModule
	: public IHLMediaModule
{
public:
	FHLMediaModule();

	// IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

    // IHLMediaModule
    virtual TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CreatePlayer(IMediaEventSink& EventSink) override;

private:
    void* LibraryHandle;
};
