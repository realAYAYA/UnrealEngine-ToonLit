// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include "ClassTemplateEditorSubsystem.h"
#include "SoundModulationGenerator.h"

#include "AudioModulationClassTemplates.generated.h"


UCLASS(Abstract)
class AUDIOMODULATIONEDITOR_API USoundModulationClassTemplate : public UPluginClassTemplate
{
	GENERATED_UCLASS_BODY()
};

UCLASS()
class AUDIOMODULATIONEDITOR_API USoundModulationGeneratorClassTemplate : public USoundModulationClassTemplate
{
	GENERATED_UCLASS_BODY()
};
