// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/RigUnit_AnimNextGraphSelf.h"
#include "Graph/AnimNextGraphInstance.h"

FRigUnit_AnimNextGraphSelf_Execute()
{
	Self = const_cast<UAnimNextGraph*>(ExecuteContext.GetGraphInstance().GetGraph());
}
