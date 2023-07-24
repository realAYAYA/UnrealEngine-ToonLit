// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Package.h"
#include "LevelSequenceEditorMenuContext.generated.h"

class FLevelSequenceEditorToolkit;

UCLASS()
class ULevelSequenceEditorMenuContext : public UObject
{
	GENERATED_BODY()
public:
	TWeakPtr<FLevelSequenceEditorToolkit> Toolkit;
};
