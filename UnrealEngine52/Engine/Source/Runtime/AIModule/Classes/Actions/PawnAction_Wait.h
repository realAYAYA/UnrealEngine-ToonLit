// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "Actions/PawnAction.h"
#include "PawnAction_Wait.generated.h"

/** uses system timers rather then ticking */
UCLASS()
class AIMODULE_API UDEPRECATED_PawnAction_Wait : public UDEPRECATED_PawnAction
{
	GENERATED_UCLASS_BODY()
		
	UPROPERTY()
	float TimeToWait;

	float FinishTimeStamp;

	FTimerHandle TimerHandle;

	/** InTimeToWait < 0 (or just FAISystem::InfiniteInterval) will result in waiting forever */
	static UDEPRECATED_PawnAction_Wait* CreateAction(UWorld& World, float InTimeToWait = FAISystem::InfiniteInterval);

	virtual bool Start() override;
	virtual bool Pause(const UDEPRECATED_PawnAction* PausedBy) override;
	virtual bool Resume() override;
	virtual EPawnActionAbortState::Type PerformAbort(EAIForceParam::Type ShouldForce) override;

	void TimerDone();
};
