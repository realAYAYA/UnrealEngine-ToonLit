// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Events.h"
#include "AnimNotifyPanelContextMenuContext.generated.h"

class SAnimNotifyTrack;
struct INodeObjectInterface;
struct FAnimNotifyEvent; 

UCLASS()
class UAnimNotifyPanelContextMenuContext : public UObject
{
	GENERATED_BODY()

public:
	TWeakPtr<SAnimNotifyTrack> NotifyTrack = nullptr;
	INodeObjectInterface* NodeObject = nullptr;
	FAnimNotifyEvent* NotifyEvent = nullptr;
	int32 NotifyIndex = INDEX_NONE;
	int32 NodeIndex = INDEX_NONE;
	FPointerEvent MouseEvent; 
};
