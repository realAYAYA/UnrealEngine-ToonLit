// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkeletalMeshNotifier.h"

FSkeletalMeshNotifyDelegate& ISkeletalMeshNotifier::Delegate()
{
	return NotifyDelegate;
}

bool ISkeletalMeshNotifier::Notifying() const
{
	return bNotifying;
}

void ISkeletalMeshNotifier::Notify(const TArray<FName>& BoneNames, const ESkeletalMeshNotifyType InNotifyType) const
{
	if (!bNotifying)
	{
		TGuardValue<bool> RecursionGuard(bNotifying, true);
		NotifyDelegate.Broadcast(BoneNames, InNotifyType);
	}
}

