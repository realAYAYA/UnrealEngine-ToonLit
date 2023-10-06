// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "Engine/DeveloperSettings.h"
#include "SynthesisEditorSettings.generated.h"


UCLASS(config = EditorSettings, defaultconfig, meta = (DisplayName = "Synthesis and DSP Plugin"))
class USynthesisEditorSettings : public UDeveloperSettings
{
	GENERATED_BODY()
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "AudioEditorSettings.h"
#include "WidgetBlueprint.h"
#endif
