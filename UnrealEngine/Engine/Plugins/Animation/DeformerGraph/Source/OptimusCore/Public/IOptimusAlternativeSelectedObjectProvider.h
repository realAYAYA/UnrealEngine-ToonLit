// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"

#include "IOptimusAlternativeSelectedObjectProvider.generated.h"


UINTERFACE()
class OPTIMUSCORE_API UOptimusAlternativeSelectedObjectProvider :
	public UInterface
{
	GENERATED_BODY()
};


class OPTIMUSCORE_API IOptimusAlternativeSelectedObjectProvider
{
	GENERATED_BODY()

public:
	virtual UObject* GetObjectToShowWhenSelected() const = 0;
};
