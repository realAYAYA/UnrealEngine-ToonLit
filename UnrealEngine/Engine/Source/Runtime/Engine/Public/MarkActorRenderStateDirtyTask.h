// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

class FMarkActorRenderStateDirtyTask
{
public:
	explicit FMarkActorRenderStateDirtyTask(UActorComponent* InActorComponent)
		: ActorComponent(InActorComponent)
	{
	}

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		if (UActorComponent* AC = ActorComponent.Get())
		{
			AC->MarkRenderStateDirty();
		}
	}

public:

	TWeakObjectPtr<UActorComponent> ActorComponent;

	static ESubsequentsMode::Type	GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }
	ENamedThreads::Type				GetDesiredThread() { return ENamedThreads::GameThread; }
	FORCEINLINE TStatId				GetStatId() const { return TStatId(); }
};