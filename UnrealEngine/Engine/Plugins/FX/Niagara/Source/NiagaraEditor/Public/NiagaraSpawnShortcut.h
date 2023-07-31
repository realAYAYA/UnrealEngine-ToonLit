// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraScript.h"
#include "Framework/Commands/InputChord.h"
#include "NiagaraSpawnShortcut.generated.h"

USTRUCT()
struct FNiagaraSpawnShortcut
{
	GENERATED_USTRUCT_BODY()
public:
	UPROPERTY(EditAnywhere, Category = Shortcut)
	FString Name;
	UPROPERTY(EditAnywhere, Category = Shortcut)
	FInputChord Input;
};
