// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaSnapPoint.h"
#include "UObject/Interface.h"
#include "IAvaSnapPointGenerator.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UAvaSnapPointGenerator : public UInterface
{
	GENERATED_BODY()
};

class IAvaSnapPointGenerator
{
	GENERATED_BODY()

public:
	virtual TArray<FAvaSnapPoint> GetLocalSnapPoints() const = 0;
};
