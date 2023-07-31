// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StateTreeTaskBase.h"
#include "StateTreeEvaluatorBase.h"
#include "StateTreeConditionBase.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeLinker.h"
#include "StateTreeTestTypes.generated.h"


struct FTestStateTreeExecutionContext : public FStateTreeExecutionContext
{
	FTestStateTreeExecutionContext(UObject& InOwner, const UStateTree& InStateTree, FStateTreeInstanceData& InInstanceData)
		: FStateTreeExecutionContext(InOwner, InStateTree, InInstanceData)
	{
	}
	
	struct FLogItem
	{
		FLogItem() = default;
		FLogItem(const FName& InName, const FString& InMessage) : Name(InName), Message(InMessage) {}
		FName Name;
		FString Message; 
	};
	TArray<FLogItem> LogItems;
	
	void Log(const FName& Name, const FString& Message)
	{
		LogItems.Emplace(Name, Message);
	}

	void LogClear()
	{
		LogItems.Empty();
	}

	struct FLogOrder
	{
		FLogOrder(const FTestStateTreeExecutionContext& InContext, const int32 InIndex) : Context(InContext), Index(InIndex) {}

		FLogOrder Then(const FName& Name, const FString& Message) const
		{
			int32 NextIndex = Index;
			while (NextIndex < Context.LogItems.Num())
			{
				const FLogItem& Item = Context.LogItems[NextIndex];
				if (Item.Name == Name && Item.Message == Message)
				{
					break;
				}
				NextIndex++;
			}
			return FLogOrder(Context, NextIndex);
		}

		operator bool() const { return Index < Context.LogItems.Num(); }
		
		const FTestStateTreeExecutionContext& Context;
		int32 Index = 0;
	};

	FLogOrder Expect(const FName& Name, const FString& Message) const
	{
		return FLogOrder(*this, 0).Then(Name, Message);
	}

	template <class ...Args>
	bool ExpectInActiveStates(const Args&... States)
	{
		FName ExpectedStateNames[] = { States... };
		const int32 NumExpectedStateNames = sizeof...(States);

		TArray<FName> ActiveStateNames = GetActiveStateNames();

		if (ActiveStateNames.Num() != NumExpectedStateNames)
		{
			return false;
		}

		for (int32 Index = 0; Index != NumExpectedStateNames; Index++)
		{
			if (ExpectedStateNames[Index] != ActiveStateNames[Index])
			{
				return false;
			}
		}

		return true;
	}
	
};

USTRUCT()
struct FTestEval_AInstanceData
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = Parameter)
	float FloatA = 0.0f;

	UPROPERTY(EditAnywhere, Category = Parameter)
	int32 IntA = 0;

	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bBoolA = false;
};

USTRUCT()
struct FTestEval_A : public FStateTreeEvaluatorBase
{
	GENERATED_BODY()

	using FInstanceDataType = FTestEval_AInstanceData;

	FTestEval_A() = default;
	virtual ~FTestEval_A() override {}

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
};

USTRUCT()
struct FTestTask_BInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Parameter)
	float FloatB = 0.0f;

	UPROPERTY(EditAnywhere, Category = Parameter)
	int32 IntB = 0;

	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bBoolB = false;
};

USTRUCT()
struct FTestTask_B : public FStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FTestTask_BInstanceData;

	FTestTask_B() = default;
	virtual ~FTestTask_B() override {}
	
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
};

USTRUCT()
struct FTestTask_StandInstanceData
{
	GENERATED_BODY()

	UPROPERTY()
	int32 CurrentTick = 0;
};

USTRUCT()
struct FTestTask_Stand : public FStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FTestTask_StandInstanceData;
	
	FTestTask_Stand() = default;
	FTestTask_Stand(const FName InName) { Name = InName; }
	virtual ~FTestTask_Stand() {}

	virtual const UStruct* GetInstanceDataType() const { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override
	{
		FTestStateTreeExecutionContext& TestContext = static_cast<FTestStateTreeExecutionContext&>(Context);
		TestContext.Log(Name, TEXT("EnterState"));

		FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
		
		if (Transition.ChangeType == EStateTreeStateChangeType::Changed)
		{
			InstanceData.CurrentTick = 0;
		}
		return EnterStateResult;
	}

	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override
	{
		FTestStateTreeExecutionContext& TestContext = static_cast<FTestStateTreeExecutionContext&>(Context);
		TestContext.Log(Name, TEXT("ExitState"));
	}

	virtual void StateCompleted(FStateTreeExecutionContext& Context, const EStateTreeRunStatus CompletionStatus, const FStateTreeActiveStates& CompletedActiveStates) const override
	{
		FTestStateTreeExecutionContext& TestContext = static_cast<FTestStateTreeExecutionContext&>(Context);
		TestContext.Log(Name, TEXT("StateCompleted"));
	}
	
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override
	{
		FTestStateTreeExecutionContext& TestContext = static_cast<FTestStateTreeExecutionContext&>(Context);
		TestContext.Log(Name, TEXT("Tick"));

		FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
		
		InstanceData.CurrentTick++;
		
		return (InstanceData.CurrentTick >= TicksToCompletion) ? TickResult : EStateTreeRunStatus::Running;
	};

	UPROPERTY(EditAnywhere, Category = Parameter)
	int32 TicksToCompletion = 1;

	UPROPERTY(EditAnywhere, Category = Parameter)
	EStateTreeRunStatus TickResult = EStateTreeRunStatus::Succeeded;

	UPROPERTY(EditAnywhere, Category = Parameter)
	EStateTreeRunStatus EnterStateResult = EStateTreeRunStatus::Running;
};

USTRUCT()
struct FStateTreeTestConditionInstanceData
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = Parameters)
	int32 Count = 1;

	static std::atomic<int32> GlobalCounter;
};

USTRUCT()
struct FStateTreeTestCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeTestConditionInstanceData;

	FStateTreeTestCondition() = default;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override
	{
		FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
		InstanceData.GlobalCounter.fetch_add(InstanceData.Count);
		return true;
	}
};

struct FStateTreeTestRunContext
{
	int32 Count = 0;
};

