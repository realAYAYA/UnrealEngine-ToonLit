// Copyright Epic Games, Inc. All Rights Reserved.
#include "BridgeDragDropHelper.h"

TSharedPtr<FBridgeDragDropHelperImpl> FBridgeDragDropHelper::Instance;

void FBridgeDragDropHelper::Initialize()
{
	if (!Instance.IsValid())
	{
		Instance = MakeShareable(new FBridgeDragDropHelperImpl);
	}
}

void FBridgeDragDropHelperImpl::SetOnAddProgressiveStageData(FOnAddProgressiveStageDataCallbackInternal InDelegate)
{
    OnAddProgressiveStageDataDelegate = InDelegate;
}
