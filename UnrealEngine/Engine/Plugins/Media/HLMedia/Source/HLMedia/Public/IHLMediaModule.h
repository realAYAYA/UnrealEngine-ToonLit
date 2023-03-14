// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Templates/SharedPointer.h"

class IMediaPlayer;
class IMediaEventSink;

class IHLMediaModule
    : public IModuleInterface
{
public:
    virtual ~IHLMediaModule() { }

    virtual TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CreatePlayer(IMediaEventSink& EventSink) = 0;
};
