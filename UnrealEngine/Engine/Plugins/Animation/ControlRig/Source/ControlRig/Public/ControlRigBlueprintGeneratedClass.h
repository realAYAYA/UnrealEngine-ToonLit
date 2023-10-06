// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "ControlRigDefines.h"
#include "RigVMCore/RigVM.h"
#include "RigVMCore/RigVMGraphFunctionHost.h"
#include "RigVMBlueprintGeneratedClass.h"
#include "ControlRigBlueprintGeneratedClass.generated.h"

UCLASS()
class CONTROLRIG_API UControlRigBlueprintGeneratedClass : public URigVMBlueprintGeneratedClass
{
	GENERATED_UCLASS_BODY()

public:

	// UObject interface
	void Serialize(FArchive& Ar);
};