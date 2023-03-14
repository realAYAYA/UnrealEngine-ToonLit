// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolMenuContext.h"
#include "LevelSequenceEditorToolkit.h"
#include "LevelSequenceEditorMenuContext.generated.h"

UCLASS()
class ULevelSequenceEditorMenuContext : public UObject
{
	GENERATED_BODY()
public:
	TWeakPtr<FLevelSequenceEditorToolkit> Toolkit;
};
