// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigVMBlueprint.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintNodeSignature.h"
#include "BlueprintActionFilter.h"
#include "RigVMEdGraphNodeSpawner.generated.h"

class URigVMEdGraphNode;

UCLASS(Transient)
class RIGVMEDITOR_API URigVMEdGraphNodeSpawner : public UBlueprintNodeSpawner
{
	GENERATED_BODY()

public:

	virtual bool IsTemplateNodeFilteredOut(FBlueprintActionFilter const& Filter) const override;

	void SetRelatedBlueprintClass(TSubclassOf<URigVMBlueprint> InClass);

	struct FPinInfo
	{
		FName Name;
		ERigVMPinDirection Direction;
		FName CPPType;
		UObject* CPPTypeObject;

		FPinInfo()
			: Name(NAME_None)
			, Direction(ERigVMPinDirection::Invalid)
			, CPPType(NAME_None)
			, CPPTypeObject(nullptr)
		{}

		FPinInfo(const FName& InName, ERigVMPinDirection InDirection, const FName& InCPPType, UObject* InCPPTypeObject)
		: Name(InName)
		, Direction(InDirection)
		, CPPType(InCPPType)
		, CPPTypeObject(InCPPTypeObject)
		{}
	};
	
	static URigVMEdGraphNode* SpawnTemplateNode(UEdGraph* InParentGraph, const TArray<FPinInfo>& InPins, const FName& InNodeName = NAME_None);
	
private:

	TSubclassOf<URigVMBlueprint> RelatedBlueprintClass;
};
