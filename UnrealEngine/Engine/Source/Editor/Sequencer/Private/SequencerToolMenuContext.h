// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolMenuContext.h"
#include "SSequencer.h"
#include "SequencerToolMenuContext.generated.h"

UCLASS()
class USequencerToolMenuContext : public UObject
{
	GENERATED_BODY()
public:
	TWeakPtr<SSequencer> SequencerWidget;
};