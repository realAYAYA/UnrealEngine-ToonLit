// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

class FNetworkPredictionInsightsCommands : public TCommands<FNetworkPredictionInsightsCommands>
{
public:
	/** Default constructor. */
	FNetworkPredictionInsightsCommands();

	/** Initialize commands. */
	virtual void RegisterCommands() override;

public:
	//////////////////////////////////////////////////
	// Global commands need to implement following method:
	//     void Map_<CommandName>_Global();
	// Custom commands needs to implement also the following method:
	//     const FUIAction <CommandName>_Custom(...) const;
	//////////////////////////////////////////////////
	
	TSharedPtr<FUICommandInfo> ToggleAutoScrollSimulationFrames;
	TSharedPtr<FUICommandInfo> ToggleCompactView;


	TSharedPtr<FUICommandInfo> NextEngineFrame;
	TSharedPtr<FUICommandInfo> PrevEngineFrame;
	TSharedPtr<FUICommandInfo> FirstEngineFrame;
	TSharedPtr<FUICommandInfo> LastEngineFrame;
};

