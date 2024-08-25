// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaTransitionNode.h"
#include "StateTreeConditionBase.h"
#include "StateTreeExecutionTypes.h"
#include "AvaTransitionCondition.generated.h"

USTRUCT(Category="Transition Logic", meta=(Hidden))
struct FAvaTransitionCondition : public FStateTreeConditionBase, public FAvaTransitionNode
{
	GENERATED_BODY()

	//~ Begin FStateTreeNodeBase
	virtual bool Link(FStateTreeLinker& InLinker) override { return LinkNode(InLinker); }
	//~ End FStateTreeNodeBase
};
