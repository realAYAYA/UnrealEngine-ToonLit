// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementSelectionInterface.h"
#include "ObjectElementSelectionInterface.generated.h"

UCLASS(MinimalAPI)
class UObjectElementSelectionInterface : public UObject, public ITypedElementSelectionInterface
{
	GENERATED_BODY()

public:
};
