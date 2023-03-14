// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

class FLevelSequenceEditorCommands
	: public TCommands<FLevelSequenceEditorCommands>
{
public:

	/** Default constructor. */
	FLevelSequenceEditorCommands();

	/** Initialize commands */
	virtual void RegisterCommands() override;

	TSharedPtr<FUICommandInfo> CreateNewLevelSequenceInLevel;
	TSharedPtr<FUICommandInfo> CreateNewMasterSequenceInLevel;
	TSharedPtr<FUICommandInfo> ToggleCinematicViewportCommand;
	
	TSharedPtr<FUICommandInfo> SnapSectionsToTimelineUsingSourceTimecode;
	TSharedPtr<FUICommandInfo> SyncSectionsUsingSourceTimecode;

	TSharedPtr<FUICommandInfo> BakeTransform;
	TSharedPtr<FUICommandInfo> FixActorReferences;

	TSharedPtr<FUICommandInfo> AddActorsToBinding;
	TSharedPtr<FUICommandInfo> RemoveActorsFromBinding;
	TSharedPtr<FUICommandInfo> ReplaceBindingWithActors;
	TSharedPtr<FUICommandInfo> RemoveAllBindings;
	TSharedPtr<FUICommandInfo> RemoveInvalidBindings;
};
