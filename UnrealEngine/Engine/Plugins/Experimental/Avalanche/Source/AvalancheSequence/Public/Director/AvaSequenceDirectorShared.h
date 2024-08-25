// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/NameTypes.h"
#include "UObject/SoftObjectPtr.h"
#include "AvaSequenceDirectorShared.generated.h"

class UAvaSequence;

USTRUCT()
struct FAvaSequenceInfo
{
	GENERATED_BODY()

	UPROPERTY()
	FName SequenceName;

	UPROPERTY()
	TSoftObjectPtr<UAvaSequence> Sequence;
};
