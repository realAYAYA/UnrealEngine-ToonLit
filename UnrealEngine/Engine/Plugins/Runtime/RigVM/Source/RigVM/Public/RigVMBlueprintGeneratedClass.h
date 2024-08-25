// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "RigVMCore/RigVM.h"
#include "RigVMCore/RigVMGraphFunctionHost.h"
#include "RigVMBlueprintGeneratedClass.generated.h"

USTRUCT()
struct FRigVMGraphFunctionHeaderArray
{
	GENERATED_BODY()
	
	UPROPERTY()
	TArray<FRigVMGraphFunctionHeader> Headers;
};

UCLASS()
class RIGVM_API URigVMBlueprintGeneratedClass : public UBlueprintGeneratedClass, public IRigVMGraphFunctionHost
{
	GENERATED_UCLASS_BODY()

public:

	// UClass interface
	virtual uint8* GetPersistentUberGraphFrame(UObject* Obj, UFunction* FuncToCheck) const override;
	virtual void PostInitInstance(UObject* InObj, FObjectInstancingGraph* InstanceGraph) override;

	// UObject interface
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
	UE_DEPRECATED(5.4, "Implement the version that takes FAssetRegistryTagsContext instead.")
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	virtual void GetAssetRegistryTags(FAssetRegistryTagsContext Context) const override;

	// IRigVMGraphFunctionHost interface
	virtual const FRigVMGraphFunctionStore* GetRigVMGraphFunctionStore() const override { return &GraphFunctionStore; }
	virtual FRigVMGraphFunctionStore* GetRigVMGraphFunctionStore() override { return &GraphFunctionStore; }

	UPROPERTY()
	FRigVMGraphFunctionStore GraphFunctionStore;
};