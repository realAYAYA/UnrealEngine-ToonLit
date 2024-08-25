// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaTransitionSequenceTask.h"
#include "AvaTransitionContinueSequenceTask.generated.h"

USTRUCT(DisplayName = "Continue Sequence", Category="Sequence Playback")
struct AVALANCHESEQUENCE_API FAvaTransitionContinueSequenceTask : public FAvaTransitionSequenceTask
{
	GENERATED_BODY()

	//~ Begin FAvaTransitionSequenceTaskBase
	virtual TArray<UAvaSequencePlayer*> ExecuteSequenceTask(FStateTreeExecutionContext& InContext) const override;
	//~ End FAvaTransitionSequenceTaskBase
};
