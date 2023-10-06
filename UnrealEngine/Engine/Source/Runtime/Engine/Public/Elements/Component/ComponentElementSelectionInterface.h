// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementSelectionInterface.h"
#include "ComponentElementSelectionInterface.generated.h"

UCLASS(MinimalAPI)
class UComponentElementSelectionInterface : public UObject, public ITypedElementSelectionInterface
{
	GENERATED_BODY()
};
