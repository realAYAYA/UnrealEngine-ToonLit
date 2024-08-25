// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"
#include "Styling/AppStyle.h"

class FAvaSequencerCommands : public TCommands<FAvaSequencerCommands>
{
public:
	FAvaSequencerCommands()
		: TCommands<FAvaSequencerCommands>(TEXT("AvaSequencerCommands")
		, NSLOCTEXT("MotionDesignSequencerCommands", "MotionDesignSequencerCommands", "Motion Design Sequencer Commands")
		, NAME_None
		, FAppStyle::GetAppStyleSetName())
	{
	}

	virtual void RegisterCommands() override;	

	TSharedPtr<FUICommandInfo> PlaySelected;

	TSharedPtr<FUICommandInfo> ContinueSelected;

	TSharedPtr<FUICommandInfo> StopSelected;

	TSharedPtr<FUICommandInfo> FixBindingPaths;

	TSharedPtr<FUICommandInfo> FixInvalidBindings;

	TSharedPtr<FUICommandInfo> FixBindingHierarchy;

	TSharedPtr<FUICommandInfo> ExportSequence;

	TSharedPtr<FUICommandInfo> StaggerLayerBars;
};
