// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "TestBTStopAction.generated.h"

class UBehaviorTreeComponent;

UENUM()
enum class EBTTestStopAction : uint8
{
	StopTree,
	UnInitialize,
	Cleanup,
	Restart_ForceReevaluateRootNode,
	Restart_Complete,
	StartTree,
};

void DoBTStopAction(UBehaviorTreeComponent& OwnerComp, const EBTTestStopAction StopAction);