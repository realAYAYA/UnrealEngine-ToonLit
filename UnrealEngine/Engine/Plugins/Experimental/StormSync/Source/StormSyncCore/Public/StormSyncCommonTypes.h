// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "StormSyncCommonTypes.generated.h"

// Common alias to a ThreadSafe SharedPtr to hold a Buffer (array of uint8)
using FStormSyncBufferPtr = TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe>;

/** Engine type values for a storm sync connected device */
UENUM()
enum class EStormSyncEngineType : uint8
{
	Server,
	Commandlet,
	Editor,
	Game,
	Other,
	Unknown
};
