// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"

#include "IOptimusNonCopyableNode.generated.h"

UINTERFACE()
class OPTIMUSCORE_API UOptimusNonCopyableNode :
	public UInterface
{
	GENERATED_BODY()
};

/**
* Interface that provides a mechanism to prevent node from being included in a subgraph
*/
class OPTIMUSCORE_API IOptimusNonCopyableNode
{
	GENERATED_BODY()

};

