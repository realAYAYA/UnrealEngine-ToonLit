// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include "BlendModes.generated.h"

UENUM(BlueprintType)
enum EBlendModes : int
{
	Normal						UMETA(DisplayName = "Copy"),
	Add							UMETA(DisplayName = "Add"),
	Subtract					UMETA(DisplayName = "Subtract"),
	Multiply					UMETA(DisplayName = "Multiply"),
	Divide						UMETA(DisplayName = "Divide"),
	Difference					UMETA(DisplayName = "Difference"),
	Max 						UMETA(DisplayName = "Max"),
	Min 						UMETA(DisplayName = "Min"),
	Step						UMETA(DisplayName = "Step"),
	Overlay						UMETA(DisplayName = "Overlay"),
	// Distort						UMETA(DisplayName = "Distort")
};
