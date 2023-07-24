// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Logging/LogMacros.h"
#include "Modules/ModuleInterface.h"

AUDIOSYNESTHESIAEDITOR_API DECLARE_LOG_CATEGORY_EXTERN(LogAudioSynesthesiaEditor, Log, All);

extern const FName AudioSynesthesiaEditorAppIdentifier;

/** audio analyzer editor module interface */
class AUDIOSYNESTHESIAEDITOR_API IAudioSynesthesiaEditorModule :	public IModuleInterface
{
public:
	/** Registers audio analyzer asset actions. */
	virtual void RegisterAssetActions() = 0;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
