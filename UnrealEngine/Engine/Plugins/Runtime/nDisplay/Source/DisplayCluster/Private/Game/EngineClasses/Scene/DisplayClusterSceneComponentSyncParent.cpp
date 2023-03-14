// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/DisplayClusterSceneComponentSyncParent.h"
#include "GameFramework/Actor.h"

#include "Misc/DisplayClusterHelpers.h"
#include "Misc/DisplayClusterLog.h"


UDisplayClusterSceneComponentSyncParent::UDisplayClusterSceneComponentSyncParent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterClusterSyncObject
//////////////////////////////////////////////////////////////////////////////////////////////
bool UDisplayClusterSceneComponentSyncParent::IsDirty() const
{
	USceneComponent* const pParent = GetAttachParent();
	if (IsValid(pParent))
	{
		const bool bIsDirty = (LastSyncLoc != pParent->GetRelativeLocation() || LastSyncRot != pParent->GetRelativeRotation() || LastSyncScale != pParent->GetRelativeScale3D());
		UE_LOG(LogDisplayClusterGame, Verbose, TEXT("SYNC_PARENT: %s dirty state is %s"), *GetSyncId(), *DisplayClusterHelpers::str::BoolToStr(bIsDirty));
		return bIsDirty;
	}

	return false;
}

void UDisplayClusterSceneComponentSyncParent::ClearDirty()
{
	USceneComponent* const pParent = GetAttachParent();
	if (IsValid(pParent))
	{
		LastSyncLoc   = pParent->GetRelativeLocation();
		LastSyncRot   = pParent->GetRelativeRotation();
		LastSyncScale = pParent->GetRelativeScale3D();
	}
}


//////////////////////////////////////////////////////////////////////////////////////////////
// UDisplayClusterSceneComponentSync
//////////////////////////////////////////////////////////////////////////////////////////////
FString UDisplayClusterSceneComponentSyncParent::GenerateSyncId()
{
	return FString::Printf(TEXT("SP_%s.%s"), *GetOwner()->GetName(), *GetAttachParent()->GetName());
}

FTransform UDisplayClusterSceneComponentSyncParent::GetSyncTransform() const
{
	return GetAttachParent()->GetRelativeTransform();
}

void UDisplayClusterSceneComponentSyncParent::SetSyncTransform(const FTransform& t)
{
	GetAttachParent()->SetRelativeTransform(t);
}
