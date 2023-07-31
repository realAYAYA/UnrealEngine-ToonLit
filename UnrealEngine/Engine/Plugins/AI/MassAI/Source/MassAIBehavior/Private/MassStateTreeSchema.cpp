// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassStateTreeSchema.h"
#include "MassEntityTypes.h"
#include "MassStateTreeTypes.h"
#include "StateTreeConditionBase.h"
#include "Subsystems/WorldSubsystem.h"

bool UMassStateTreeSchema::IsStructAllowed(const UScriptStruct* InScriptStruct) const
{
	// Only allow Mass evals and tasks,and common conditions.
	return InScriptStruct->IsChildOf(FMassStateTreeEvaluatorBase::StaticStruct())
			|| InScriptStruct->IsChildOf(FStateTreeEvaluatorCommonBase::StaticStruct())
			|| InScriptStruct->IsChildOf(FMassStateTreeTaskBase::StaticStruct())
			|| InScriptStruct->IsChildOf(FStateTreeConditionBase::StaticStruct());
}

bool UMassStateTreeSchema::IsExternalItemAllowed(const UStruct& InStruct) const
{
	// Allow only WorldSubsystems and fragments as external data.
	return InStruct.IsChildOf(UWorldSubsystem::StaticClass())
			|| InStruct.IsChildOf(FMassFragment::StaticStruct())
			|| InStruct.IsChildOf(FMassSharedFragment::StaticStruct());
}
