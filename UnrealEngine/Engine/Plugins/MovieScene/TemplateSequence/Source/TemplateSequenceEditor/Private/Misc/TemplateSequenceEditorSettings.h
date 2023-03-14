// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Engine/EngineTypes.h"
#include "UObject/SoftObjectPath.h"
#include "TemplateSequence.h"
#include "TemplateSequenceEditorSettings.generated.h"

/**
 * Template Sequence Editor settings.
 */
UCLASS(config=EditorPerProjectUserSettings)
class UTemplateSequenceEditorSettings
	: public UObject
{
	GENERATED_BODY()

public:

	UTemplateSequenceEditorSettings(const FObjectInitializer& ObjectInitializer);

	/** Whether to show "outdated assets" by default in the camera animation track's asset picker. */
	UPROPERTY(config)
	bool bShowOutdatedAssetsInCameraAnimationTrackEditor = false;
};

