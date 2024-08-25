// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"

#include "IOptimusNodePairProvider.generated.h"

UINTERFACE()
class OPTIMUSCORE_API UOptimusNodePairProvider :
	public UInterface
{
	GENERATED_BODY()
};

/**
* Interface that provides a mechanism to add pins to node from existing pins
*/
class OPTIMUSCORE_API IOptimusNodePairProvider
{
	GENERATED_BODY()

public:

	virtual void PairToCounterpartNode(const IOptimusNodePairProvider* NodePairProvider) = 0;
};

