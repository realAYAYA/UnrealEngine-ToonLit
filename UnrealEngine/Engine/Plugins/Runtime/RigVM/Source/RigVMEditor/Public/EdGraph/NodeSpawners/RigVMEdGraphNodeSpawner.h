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

private:

	TSubclassOf<URigVMBlueprint> RelatedBlueprintClass;
};
