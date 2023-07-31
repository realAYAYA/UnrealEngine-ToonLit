// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

class FMoviePipelineCommands : public TCommands<FMoviePipelineCommands>
{
public:
	FMoviePipelineCommands();

	TSharedPtr<FUICommandInfo> ResetStatus;

	/** Initialize commands */
	virtual void RegisterCommands() override;
};
