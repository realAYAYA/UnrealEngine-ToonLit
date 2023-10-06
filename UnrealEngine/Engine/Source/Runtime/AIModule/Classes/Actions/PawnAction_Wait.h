// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "Actions/PawnAction.h"
#include "PawnAction_Wait.generated.h"

/** uses system timers rather then ticking */
UCLASS(MinimalAPI)
class UDEPRECATED_PawnAction_Wait : public UDEPRECATED_PawnAction
{
	GENERATED_UCLASS_BODY()
		
	UPROPERTY()
	float TimeToWait;

	float FinishTimeStamp;

	FTimerHandle TimerHandle;

	/** InTimeToWait < 0 (or just FAISystem::InfiniteInterval) will result in waiting forever */
	static AIMODULE_API UDEPRECATED_PawnAction_Wait* CreateAction(UWorld& World, float InTimeToWait = FAISystem::InfiniteInterval);

	AIMODULE_API virtual bool Start() override;
	AIMODULE_API virtual bool Pause(const UDEPRECATED_PawnAction* PausedBy) override;
	AIMODULE_API virtual bool Resume() override;
	AIMODULE_API virtual EPawnActionAbortState::Type PerformAbort(EAIForceParam::Type ShouldForce) override;

	AIMODULE_API void TimerDone();
};
