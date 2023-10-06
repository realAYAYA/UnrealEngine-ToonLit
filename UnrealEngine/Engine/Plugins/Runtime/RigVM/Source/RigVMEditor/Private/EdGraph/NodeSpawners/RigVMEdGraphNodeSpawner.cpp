// Copyright Epic Games, Inc. All Rights Reserved.

#include "EdGraph/NodeSpawners/RigVMEdGraphNodeSpawner.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMEdGraphNodeSpawner)

#define LOCTEXT_NAMESPACE "RigVMEdGraphNodeSpawner"

bool URigVMEdGraphNodeSpawner::IsTemplateNodeFilteredOut(FBlueprintActionFilter const& Filter) const
{
	check(RelatedBlueprintClass);

	for(const UBlueprint* Blueprint : Filter.Context.Blueprints)
	{
		if(Blueprint->GetClass() != RelatedBlueprintClass)
		{
			return true;
		}
	}

	return false;
}

void URigVMEdGraphNodeSpawner::SetRelatedBlueprintClass(TSubclassOf<URigVMBlueprint> InClass)
{
	RelatedBlueprintClass = InClass;
}

#undef LOCTEXT_NAMESPACE

