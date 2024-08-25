// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"
#include "IAvaViewportDataProvider.generated.h"

class UObject;

/** Interface for Objects that use UAvaSequence and need to be handled by IAvaSequencer */
UINTERFACE(MinimalAPI, NotBlueprintType, meta = (CannotImplementInterfaceInBlueprint))
class UAvaViewportDataProvider : public UInterface
{
	GENERATED_BODY()
};

class IAvaViewportDataProvider
{
public:
	GENERATED_BODY()

	virtual UObject* ToUObject() = 0;

	virtual FName GetStartupCameraName() const = 0;

#if WITH_EDITOR
	virtual void SetStartupCameraName(FName InName) = 0;
#endif
};
