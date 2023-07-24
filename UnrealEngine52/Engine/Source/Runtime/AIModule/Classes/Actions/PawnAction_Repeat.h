// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Actions/PawnAction.h"
#include "PawnAction_Repeat.generated.h"

UCLASS()
class AIMODULE_API UDEPRECATED_PawnAction_Repeat : public UDEPRECATED_PawnAction
{
	GENERATED_UCLASS_BODY()

	enum
	{
		LoopForever = -1
	};

	/** Action to repeat. This instance won't really be run, it's a source for copying actions to be actually performed */
	UPROPERTY()
	TObjectPtr<UDEPRECATED_PawnAction> ActionToRepeat_DEPRECATED;

	UPROPERTY(Transient)
	TObjectPtr<UDEPRECATED_PawnAction> RecentActionCopy_DEPRECATED;

	UPROPERTY(Category = PawnAction, EditAnywhere, BlueprintReadOnly)
	TEnumAsByte<EPawnActionFailHandling::Type> ChildFailureHandlingMode;
	
	int32 RepeatsLeft;

	EPawnSubActionTriggeringPolicy::Type SubActionTriggeringPolicy;

	/** @param NumberOfRepeats number of times to repeat action. UDEPRECATED_PawnAction_Repeat::LoopForever loops forever */
	static UDEPRECATED_PawnAction_Repeat* CreateAction(UWorld& World, UDEPRECATED_PawnAction* ActionToRepeat, int32 NumberOfRepeats, EPawnSubActionTriggeringPolicy::Type InSubActionTriggeringPolicy = EPawnSubActionTriggeringPolicy::CopyBeforeTriggering);

protected:
	virtual bool Start() override;
	virtual bool Resume() override;
	virtual void OnChildFinished(UDEPRECATED_PawnAction& Action, EPawnActionResult::Type WithResult) override;

	bool PushSubAction();
};
