// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Actions/PawnAction.h"
#include "PawnAction_BlueprintBase.generated.h"

class APawn;

UCLASS(abstract, Blueprintable, MinimalAPI)
class UDEPRECATED_PawnAction_BlueprintBase : public UDEPRECATED_PawnAction
{
	GENERATED_UCLASS_BODY()

public:

	//----------------------------------------------------------------------//
	// Blueprint interface
	//----------------------------------------------------------------------//

	UFUNCTION(BlueprintImplementableEvent, Category = "AI|PawnActions")
	AIMODULE_API void ActionStart(APawn* ControlledPawn);
	UFUNCTION(BlueprintImplementableEvent, Category = "AI|PawnActions")
	AIMODULE_API void ActionTick(APawn* ControlledPawn, float DeltaSeconds);
	UFUNCTION(BlueprintImplementableEvent, Category = "AI|PawnActions")
	AIMODULE_API void ActionPause(APawn* ControlledPawn);
	UFUNCTION(BlueprintImplementableEvent, Category = "AI|PawnActions")
	AIMODULE_API void ActionResume(APawn* ControlledPawn);
	UFUNCTION(BlueprintImplementableEvent, Category = "AI|PawnActions")
	AIMODULE_API void ActionFinished(APawn* ControlledPawn, EPawnActionResult::Type WithResult);

protected:
	AIMODULE_API virtual void Tick(float DeltaTime) override;
	AIMODULE_API virtual bool Start() override;
	AIMODULE_API virtual bool Pause(const UDEPRECATED_PawnAction* PausedBy) override;
	AIMODULE_API virtual bool Resume() override;
	AIMODULE_API virtual void OnFinished(EPawnActionResult::Type WithResult) override;
};
