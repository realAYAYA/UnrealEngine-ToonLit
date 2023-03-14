// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "ControlRigDefines.h"
#include "RigVMCore/RigVM.h"
#include "ControlRigBlueprintGeneratedClass.generated.h"

UCLASS()
class CONTROLRIG_API UControlRigBlueprintGeneratedClass : public UBlueprintGeneratedClass
{
	GENERATED_UCLASS_BODY()

public:

	// UClass interface
	virtual uint8* GetPersistentUberGraphFrame(UObject* Obj, UFunction* FuncToCheck) const override;
	virtual void PostInitInstance(UObject* InObj, FObjectInstancingGraph* InstanceGraph) override;

	// UObject interface
	void Serialize(FArchive& Ar);
};