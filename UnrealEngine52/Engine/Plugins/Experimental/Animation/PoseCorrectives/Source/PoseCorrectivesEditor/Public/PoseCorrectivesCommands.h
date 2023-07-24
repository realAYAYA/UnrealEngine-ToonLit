// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"
#include "Styling/AppStyle.h"

class FPoseCorrectivesCommands : public TCommands<FPoseCorrectivesCommands>
{
public:
	FPoseCorrectivesCommands() : TCommands<FPoseCorrectivesCommands>
	(
		"PoseCorrectives",
		NSLOCTEXT("Contexts", "PoseCorrectives", "Pose Correctives"),
		NAME_None,
		FAppStyle::Get().GetStyleSetName()		
	)
	{}

	
	TSharedPtr< FUICommandInfo > AddCorrectivePose;
	TSharedPtr< FUICommandInfo > SaveCorrective;
	TSharedPtr< FUICommandInfo > CancelCorrective;

	TSharedPtr< FUICommandInfo > MultiSelectCorrectiveCurvesCommand;
	TSharedPtr< FUICommandInfo > MultiSelectDriverCurvesCommand;
	TSharedPtr< FUICommandInfo > MultiSelectCorrectiveBonesCommand;
	TSharedPtr< FUICommandInfo > MultiSelectDriverBonesCommand;
	TSharedPtr< FUICommandInfo > MultiDeselectCorrectiveCurvesCommand;
	TSharedPtr< FUICommandInfo > MultiDeselectDriverCurvesCommand;
	TSharedPtr< FUICommandInfo > MultiDeselectCorrectiveBonesCommand;
	TSharedPtr< FUICommandInfo > MultiDeselectDriverBonesCommand;

	/** initialize commands */
	virtual void RegisterCommands() override;
};
