// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigVMModel/RigVMController.h"

#include "AnimNextGraph_Controller.generated.h"

/**
  * Implements AnimNext RigVM controller extensions
  */
UCLASS(MinimalAPI)
class UAnimNextGraph_Controller : public URigVMController
{
	GENERATED_BODY()

public:
	// Adds a unit node with a dynamic number of pins
	URigVMUnitNode* AddUnitNodeWithPins(UScriptStruct* InScriptStruct, const FRigVMPinInfoArray& PinArray, const FName& InMethodName = TEXT("Execute"), const FVector2D& InPosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);
};
