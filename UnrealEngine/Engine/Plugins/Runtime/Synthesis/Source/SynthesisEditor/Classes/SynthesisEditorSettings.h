// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioEditorSettings.h"
#include "WidgetBlueprint.h"

#include "SynthesisEditorSettings.generated.h"


UCLASS(config = EditorSettings, defaultconfig, meta = (DisplayName = "Synthesis and DSP Plugin"))
class USynthesisEditorSettings : public UDeveloperSettings
{
	GENERATED_BODY()
};
