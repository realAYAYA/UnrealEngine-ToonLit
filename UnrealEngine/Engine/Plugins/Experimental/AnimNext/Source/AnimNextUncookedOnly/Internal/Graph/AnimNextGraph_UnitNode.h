// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigVMModel/Nodes/RigVMUnitNode.h"

#include "AnimNextGraph_UnitNode.generated.h"

/**
  * Implements AnimNext RigVM unit node extensions
  */
UCLASS(MinimalAPI)
class UAnimNextGraph_UnitNode : public URigVMUnitNode
{
	GENERATED_BODY()

public:
	virtual bool HasNonNativePins() const { return true; }
};
