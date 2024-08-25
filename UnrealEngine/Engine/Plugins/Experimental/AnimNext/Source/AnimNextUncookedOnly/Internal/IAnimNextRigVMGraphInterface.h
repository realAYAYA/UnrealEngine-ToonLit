// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"
#include "IAnimNextRigVMGraphInterface.generated.h"

class URigVMGraph;
class URigVMEdGraph;

UINTERFACE(meta=(CannotImplementInterfaceInBlueprint))
class ANIMNEXTUNCOOKEDONLY_API UAnimNextRigVMGraphInterface : public UInterface
{
	GENERATED_BODY()
};

// Interface for entries that contain a RigVM graph
class ANIMNEXTUNCOOKEDONLY_API IAnimNextRigVMGraphInterface
{
	GENERATED_BODY()

public:
	// Get the RigVM graph
	virtual URigVMGraph* GetRigVMGraph() const = 0;

	// Get the Editor graph
	virtual URigVMEdGraph* GetEdGraph() const = 0;
};