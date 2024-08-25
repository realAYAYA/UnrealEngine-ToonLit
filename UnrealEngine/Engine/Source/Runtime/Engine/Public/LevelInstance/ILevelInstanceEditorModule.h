// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

/**
 * The module holding all of the UI related pieces for LevelInstance management
 */
class ILevelInstanceEditorModule : public IModuleInterface
{
public:
	virtual ~ILevelInstanceEditorModule() {}

	virtual void ActivateEditorMode() = 0;
	virtual void DeactivateEditorMode() = 0;

	virtual void BroadcastTryExitEditorMode() = 0;

	/** Broadcasts before exiting mode */
	DECLARE_EVENT(ILevelInstanceEditorModule, FExitEditorModeEvent);
	virtual FExitEditorModeEvent& OnExitEditorMode() = 0;

	DECLARE_EVENT(ILevelInstanceEditorModule, FTryExitEditorModeEvent);
	virtual FTryExitEditorModeEvent& OnTryExitEditorMode() = 0;

	virtual bool IsEditInPlaceStreamingEnabled() const = 0;
};