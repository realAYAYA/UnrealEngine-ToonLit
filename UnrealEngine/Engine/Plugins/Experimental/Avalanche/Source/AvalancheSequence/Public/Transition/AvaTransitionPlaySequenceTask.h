// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaSequenceShared.h"
#include "AvaTransitionSequenceTask.h"
#include "AvaTransitionPlaySequenceTask.generated.h"

USTRUCT(DisplayName = "Play Sequence", Category="Sequence Playback")
struct AVALANCHESEQUENCE_API FAvaTransitionPlaySequenceTask : public FAvaTransitionSequenceTask
{
	GENERATED_BODY()

	//~ Begin FAvaTransitionTask
	virtual FText GenerateDescription(const FAvaTransitionNodeContext& InContext) const override;
	//~ End FAvaTransitionTask

	//~ Begin FAvaTransitionSequenceTaskBase
	virtual TArray<UAvaSequencePlayer*> ExecuteSequenceTask(FStateTreeExecutionContext& InContext) const override;
	//~ End FAvaTransitionSequenceTaskBase

	/** Sequence Play Settings */
	UPROPERTY(EditAnywhere, Category="Parameter")
	FAvaSequencePlayParams PlaySettings;
};
