// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaBroadcastTabFactory.h"

#include "Broadcast/AvaBroadcastEditor.h"

FAvaBroadcastTabFactory::FAvaBroadcastTabFactory(const FName& InTabID, const TSharedPtr<FAvaBroadcastEditor>& InBroadcastEditor)
	: FWorkflowTabFactory(InTabID, InBroadcastEditor)
	, BroadcastEditorWeak(InBroadcastEditor)
{
}
