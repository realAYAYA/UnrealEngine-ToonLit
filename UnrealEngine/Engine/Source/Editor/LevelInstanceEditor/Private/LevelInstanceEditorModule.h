// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "LevelInstance/ILevelInstanceEditorModule.h"
#include "Tools/Modes.h"

class AActor;
class ULevel;
enum class EMapChangeType : uint8;

/**
 * The module holding all of the UI related pieces for LevelInstance management
 */
class FLevelInstanceEditorModule : public ILevelInstanceEditorModule
{
public:
	virtual ~FLevelInstanceEditorModule(){}

	/**
	 * Called right after the module DLL has been loaded and the module object has been created
	 */
	virtual void StartupModule();

	/**
	 * Called before the module is unloaded, right before the module object is destroyed.
	 */
	virtual void ShutdownModule();

	virtual void ActivateEditorMode() override;
	virtual void DeactivateEditorMode() override;
		
	virtual void BroadcastTryExitEditorMode() override;

	DECLARE_DERIVED_EVENT(FLevelInstanceEditorModule, ILevelInstanceEditorModule::FExitEditorModeEvent, FExitEditorModeEvent);
	virtual FExitEditorModeEvent& OnExitEditorMode() override { return ExitEditorModeEvent; }

	DECLARE_DERIVED_EVENT(FLevelInstanceEditorModule, ILevelInstanceEditorModule::FTryExitEditorModeEvent, FTryExitEditorModeEvent);
	virtual FTryExitEditorModeEvent& OnTryExitEditorMode() override { return TryExitEditorModeEvent; }

	virtual bool IsEditInPlaceStreamingEnabled() const override;

private:
	void OnEditorModeIDChanged(const FEditorModeID& InModeID, bool bIsEnteringMode);
	void OnLevelActorDeleted(AActor* Actor);
	void CanMoveActorToLevel(const AActor* ActorToMove, const ULevel* DestLevel, bool& bOutCanMove);

	void ExtendContextMenu();

	FExitEditorModeEvent ExitEditorModeEvent;
	FTryExitEditorModeEvent TryExitEditorModeEvent;
};
