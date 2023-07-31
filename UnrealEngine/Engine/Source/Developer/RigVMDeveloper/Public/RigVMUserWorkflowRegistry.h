// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "HAL/Platform.h"
#include "RigVMCore/RigVMUserWorkflow.h"
#include "RigVMModel/RigVMNode.h"
#include "Templates/Tuple.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/WeakObjectPtr.h"

#include "RigVMUserWorkflowRegistry.generated.h"

class UScriptStruct;
struct FFrame;

DECLARE_DYNAMIC_DELEGATE_RetVal_OneParam(TArray<FRigVMUserWorkflow>, FRigVMUserWorkflowProvider, const UObject*, InSubject);

UCLASS(BlueprintType)
class RIGVMDEVELOPER_API URigVMUserWorkflowRegistry : public UObject
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintPure, Category = FRigVMUserWorkflowRegistry)
   	static URigVMUserWorkflowRegistry* Get();

	UFUNCTION(BlueprintCallable, Category = FRigVMUserWorkflowRegistry)
	int32 RegisterProvider(const UScriptStruct* InStruct, FRigVMUserWorkflowProvider InProvider);

	UFUNCTION(BlueprintCallable, Category = FRigVMUserWorkflowRegistry)
	void UnregisterProvider(int32 InHandle);

	UFUNCTION(BlueprintPure, Category = FRigVMUserWorkflowRegistry)
	TArray<FRigVMUserWorkflow> GetWorkflows(ERigVMUserWorkflowType InType, const UScriptStruct* InStruct, const UObject* InSubject) const;

private:

	int32 MaxHandle = 0;
	mutable TArray<TTuple<int32, const UScriptStruct*, FRigVMUserWorkflowProvider>> Providers;
};
