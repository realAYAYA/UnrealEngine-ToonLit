// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaSequenceShared.h"
#include "AvaTransitionSequenceTask.h"
#include "AvaTransitionInitializeSequence.generated.h"

USTRUCT(DisplayName = "Initialize Sequence", Category="Sequence Playback")
struct AVALANCHESEQUENCE_API FAvaTransitionInitializeSequence : public FAvaTransitionSequenceTaskBase
{
	GENERATED_BODY()

	//~ Begin FAvaTransitionSequenceTaskBase
	virtual EAvaTransitionSequenceWaitType GetWaitType() const { return EAvaTransitionSequenceWaitType::NoWait; }
	virtual TArray<UAvaSequencePlayer*> ExecuteSequenceTask(FStateTreeExecutionContext& InContext) const override;
	//~ End FAvaTransitionSequenceTaskBase

	UPROPERTY(EditAnywhere, Category = "Motion Design Sequence")
	FAvaSequenceTime InitializeTime = FAvaSequenceTime(0.0);

	UPROPERTY(EditAnywhere, Category="Parameter")
	EAvaSequencePlayMode PlayMode = EAvaSequencePlayMode::Forward;
};
