// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaTransitionTreeSchema.h"
#include "AvaTransitionContext.h"
#include "AvaTransitionScene.h"
#include "Conditions/AvaTransitionCondition.h"
#include "StateTreeEvaluatorBase.h"
#include "StateTreeTaskBase.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tasks/AvaTransitionTask.h"

bool UAvaTransitionTreeSchema::IsStructAllowed(const UScriptStruct* InScriptStruct) const
{
	return InScriptStruct->IsChildOf(FAvaTransitionTask::StaticStruct())
		|| InScriptStruct->IsChildOf(FAvaTransitionCondition::StaticStruct());
}

bool UAvaTransitionTreeSchema::IsExternalItemAllowed(const UStruct& InStruct) const
{
	return InStruct.IsChildOf(FAvaTransitionContext::StaticStruct())
		|| InStruct.IsChildOf(UWorldSubsystem::StaticClass());
}
