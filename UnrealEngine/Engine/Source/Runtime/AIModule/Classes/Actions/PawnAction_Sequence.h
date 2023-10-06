// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Actions/PawnAction.h"
#include "PawnAction_Sequence.generated.h"

UCLASS(MinimalAPI)
class UDEPRECATED_PawnAction_Sequence : public UDEPRECATED_PawnAction
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	TArray<TObjectPtr<UDEPRECATED_PawnAction>> ActionSequence_DEPRECATED;

	UPROPERTY(Category = PawnAction, EditAnywhere, BlueprintReadOnly)
	TEnumAsByte<EPawnActionFailHandling::Type> ChildFailureHandlingMode;

	UPROPERTY(Transient)
	TObjectPtr<UDEPRECATED_PawnAction> RecentActionCopy_DEPRECATED;

	uint32 CurrentActionIndex;

	EPawnSubActionTriggeringPolicy::Type SubActionTriggeringPolicy;

	static AIMODULE_API UDEPRECATED_PawnAction_Sequence* CreateAction(UWorld& World, TArray<UDEPRECATED_PawnAction*>& ActionSequence, EPawnSubActionTriggeringPolicy::Type InSubActionTriggeringPolicy = EPawnSubActionTriggeringPolicy::CopyBeforeTriggering);

protected:
	AIMODULE_API virtual bool Start() override;
	AIMODULE_API virtual bool Resume() override;
	AIMODULE_API virtual void OnChildFinished(UDEPRECATED_PawnAction& Action, EPawnActionResult::Type WithResult) override;

	AIMODULE_API bool PushNextActionCopy();
};
