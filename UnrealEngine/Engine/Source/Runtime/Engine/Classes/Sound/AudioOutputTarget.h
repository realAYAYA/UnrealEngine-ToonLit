// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UObject/ObjectMacros.h"

UENUM()
namespace EAudioOutputTarget
{
	enum Type
	{
		Speaker UMETA(ToolTip = "Sound plays only from speakers."),
		Controller UMETA(ToolTip = "Sound plays only from controller if present."),
		ControllerFallbackToSpeaker UMETA(ToolTip = "Sound plays on the controller if present. If not present, it plays from speakers.")
	};
}
