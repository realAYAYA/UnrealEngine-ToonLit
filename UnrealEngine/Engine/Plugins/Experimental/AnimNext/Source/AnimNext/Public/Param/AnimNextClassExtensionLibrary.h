// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextClassExtensionLibrary.generated.h"

// Base class for function library-like classes that act to adapt UObjects of a specified type, exposing them to AnimNext parameters.
// To do this, implement GetSupportedClass, then provide static UFUNCTION members that take the type as an input, e.g.
// static float GetDeltaSeconds(UWorld* InWorld); 
UCLASS(Abstract)
class UAnimNextClassExtensionLibrary : public UObject
{
	GENERATED_BODY()

public:
	// Get the class that is supported by this extension library
	virtual UClass* GetSupportedClass() const PURE_VIRTUAL(UAnimNextClassExtensionLibrary::GetSupportedClass, return nullptr; )
};

