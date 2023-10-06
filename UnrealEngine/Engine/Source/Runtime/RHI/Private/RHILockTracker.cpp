// Copyright Epic Games, Inc. All Rights Reserved.

#include "RHILockTracker.h"
#include "RHI.h"

FRHILockTracker GRHILockTracker;

void FRHILockTracker::RaiseMismatchError()
{
	UE_LOG(LogRHI, Fatal, TEXT("Mismatched RHI buffer locks."));
}
