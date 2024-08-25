// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaTransitionSequenceTask.h"
#include "AvaTransitionWaitForSequenceTask.generated.h"

USTRUCT(DisplayName = "Wait for Sequence", Category="Sequence Playback")
struct AVALANCHESEQUENCE_API FAvaTransitionWaitForSequenceTask : public FAvaTransitionSequenceTask
{
	GENERATED_BODY()

	//~ Begin FAvaTransitionSequenceTaskBase
	virtual TArray<UAvaSequencePlayer*> ExecuteSequenceTask(FStateTreeExecutionContext& InContext) const override;
	//~ End FAvaTransitionSequenceTaskBase
};
