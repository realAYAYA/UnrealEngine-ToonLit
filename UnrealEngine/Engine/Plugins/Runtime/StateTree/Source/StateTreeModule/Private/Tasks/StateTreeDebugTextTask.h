// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StateTreeTaskBase.h"
#include "StateTreeDebugTextTask.generated.h"

USTRUCT()
struct STATETREEMODULE_API FStateTreeDebugTextTaskInstanceData
{
	GENERATED_BODY()
};

/**
 * Draws debug text on the HUD associated to the player controller.
 */
USTRUCT(meta = (DisplayName = "Debug Text Task"))
struct STATETREEMODULE_API FStateTreeDebugTextTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	typedef FStateTreeDebugTextTaskInstanceData InstanceDataType;
	
	FStateTreeDebugTextTask() = default;

	virtual const UStruct* GetInstanceDataType() const override { return InstanceDataType::StaticStruct(); }

	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	TStateTreeExternalDataHandle<AActor, EStateTreeExternalDataRequirement::Optional> ReferenceActorHandle;
	
	UPROPERTY(EditAnywhere, Category = Parameter)
	FString Text;

	UPROPERTY(EditAnywhere, Category = Parameter)
	FColor TextColor = FColor::White;

	UPROPERTY(EditAnywhere, Category = Parameter, meta=(ClampMin = 0, UIMin = 0))
	float FontScale = 1.0f;
	
	UPROPERTY(EditAnywhere, Category = Parameter)
	FVector Offset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bEnabled = true;
};
