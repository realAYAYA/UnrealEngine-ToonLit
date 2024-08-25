// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimNextComponentParameter.h"
#include "Components/ActorComponent.h"
#include "Scheduler/AnimNextSchedule.h"
#include "Scheduler/ScheduleHandle.h"
#include "AnimNextComponent.generated.h"

class UAnimNextSchedule;
struct FAnimNextComponentInstanceData;

namespace UE::AnimNext::UncookedOnly
{
	struct FUtils;
}

UENUM()
enum class EAnimNextParameterScopeOrdering : uint8
{
	// Value will be pushed before the scope, allowing the static scope to potentially override the value
	Before,

	// Value will be pushed after the scope, potentially overriding the static scope
	After,
};

UCLASS(MinimalAPI, meta = (BlueprintSpawnableComponent))
class UAnimNextComponent : public UActorComponent
{
	GENERATED_BODY()

	// UActorComponent interface
	virtual void OnRegister() override;
	virtual void OnUnregister() override;

public:
	// Sets a parameter's value in the supplied scope.
	// @param    Scope    Scopes corresponding to an existing scope in a schedule, or "None". Passing "None" will apply the parameter to the whole schedule.
	// @param    Ordering Where to apply the parameter in relation to the supplied scope. Ignored for scope "None".
	// @param    Name     The name of the parameter to apply
	// @param    Value    The value to set the parameter to
	UFUNCTION(BlueprintCallable, Category = "AnimNext", CustomThunk, meta = (CustomStructureParam = Value, UnsafeDuringActorConstruction))
	void SetParameterInScope(UPARAM(meta = (CustomWidget = "ParamName", AllowedParamType = "FAnimNextScope")) FName Scope, EAnimNextParameterScopeOrdering Ordering, UPARAM(meta = (CustomWidget = "ParamName")) FName Name, int32 Value);

	// Enable or disable this component's update
	UFUNCTION(BlueprintCallable, Category = "AnimNext")
	void Enable(bool bEnabled);
	
private:
	DECLARE_FUNCTION(execSetParameterInScope);

private:
	friend struct UE::AnimNext::UncookedOnly::FUtils;

	// The execution schedule that this component will run
	UPROPERTY(EditAnywhere, Category="Schedule")
	TObjectPtr<UAnimNextSchedule> Schedule = nullptr;

	// Parameters to apply on schedule/component registration
	UPROPERTY(Instanced, EditAnywhere, Category = "Parameters")
	TArray<TObjectPtr<UAnimNextComponentParameter>> Parameters;

	// How to initialize the schedule
	UPROPERTY(EditAnywhere, Category="Schedule")
	EAnimNextScheduleInitMethod InitMethod = EAnimNextScheduleInitMethod::InitializeAndPauseInEditor;

	// Handle to the registered results/schedule
	UE::AnimNext::FScheduleHandle SchedulerHandle;
};
