// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/RigUnit_AnimNextGraphRoot.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_AnimNextGraphRoot)

FName FRigUnit_AnimNextGraphRoot::EventName = TEXT("DummyExecute");
FName FRigUnit_AnimNextGraphRoot::DefaultEntryPoint = TEXT("Root");

FRigUnit_AnimNextGraphRoot_DummyExecute()
{
	// This RigUnit should never execute, it is only used in the editor
	ensure(false);
}
