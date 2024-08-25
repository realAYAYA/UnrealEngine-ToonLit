// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaTransitionNode.h"
#include "StateTreeExecutionTypes.h"
#include "StateTreeTaskBase.h"
#include "AvaTransitionTask.generated.h"

USTRUCT(meta=(Hidden))
struct FAvaTransitionTask : public FStateTreeTaskBase, public FAvaTransitionNode
{
	GENERATED_BODY()

	//~ Begin FStateTreeNodeBase
	virtual bool Link(FStateTreeLinker& InLinker) override { return LinkNode(InLinker); }
	//~ End FStateTreeNodeBase
};
