// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ClassTemplateEditorSubsystem.h"
#include "Containers/UnrealString.h"
#include "HAL/Platform.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "SoundClassTemplates.generated.h"

class UObject;


UCLASS()
class USoundEffectSourcePresetClassTemplate : public UClassTemplate
{
	GENERATED_UCLASS_BODY()

public:
	FString GetFilename() const override
	{
		return TEXT("SoundEffectSourceClass");
	}
};

UCLASS()
class USoundEffectSubmixPresetClassTemplate : public UClassTemplate
{
	GENERATED_UCLASS_BODY()

public:
	FString GetFilename() const override
	{
		return TEXT("SoundEffectSubmixClass");
	}
};

UCLASS()
class USynthComponentClassTemplate : public UClassTemplate
{
	GENERATED_UCLASS_BODY()

public:
	FString GetFilename() const override
	{
		return TEXT("SynthComponentClass");
	}
};