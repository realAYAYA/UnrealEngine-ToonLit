// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "Math/Vector2D.h"


class AUDIOEDITOR_API FSoundClassEditorUtilities
{
public:

	/**
	 * Create a new SoundClass
	 *
	 * @param	Graph		Graph representing sound classes
	 * @param	FromPin		Pin that was dragged to create sound class
	 * @param	Location	Location for new sound class
	 */
	static void CreateSoundClass(const class UEdGraph* Graph, class UEdGraphPin* FromPin, const FVector2D& Location, FString Name);

private:
	FSoundClassEditorUtilities() {}
};
