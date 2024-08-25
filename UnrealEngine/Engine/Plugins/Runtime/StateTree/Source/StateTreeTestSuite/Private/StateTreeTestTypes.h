// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StateTreeTaskBase.h"
#include "StateTreeEvaluatorBase.h"
#include "StateTreeConditionBase.h"
#include "StateTreeExecutionContext.h"
#include "StateTreePropertyRef.h"
#include "StateTreeTestTypes.generated.h"

class UStateTree;
struct FStateTreeInstanceData;


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
	FTestEval_A(const FName InName) { Name = InName; }
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
	FTestTask_B(const FName InName) { Name = InName; }
	virtual ~FTestTask_B() override {}
	
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override
	{
		FTestStateTreeExecutionContext& TestContext = static_cast<FTestStateTreeExecutionContext&>(Context);
		TestContext.Log(Name,  TEXT("EnterState"));
		return EStateTreeRunStatus::Running;
	}
};

USTRUCT()
struct FTestTask_PrintValueInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter")
	int32 Value = 0;
};

USTRUCT()
struct FTestTask_PrintValue : public FStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FTestTask_PrintValueInstanceData;

	FTestTask_PrintValue() = default;
	FTestTask_PrintValue(const FName InName) { Name = InName; }
	virtual ~FTestTask_PrintValue() override {}
	
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override
	{
		FTestStateTreeExecutionContext& TestContext = static_cast<FTestStateTreeExecutionContext&>(Context);
		const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
		TestContext.Log(Name,  FString::Printf(TEXT("EnterState%d"), InstanceData.Value));
		return EStateTreeRunStatus::Running;
	}

	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override
	{
		FTestStateTreeExecutionContext& TestContext = static_cast<FTestStateTreeExecutionContext&>(Context);
		const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
		TestContext.Log(Name,  FString::Printf(TEXT("ExitState%d"), InstanceData.Value));
	}

	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override
	{
		FTestStateTreeExecutionContext& TestContext = static_cast<FTestStateTreeExecutionContext&>(Context);
		const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
		TestContext.Log(Name,  FString::Printf(TEXT("Tick%d"), InstanceData.Value));
		
		return EStateTreeRunStatus::Running;
	};
};


USTRUCT()
struct FTestTask_StopTreeInstanceData
{
	GENERATED_BODY()
};

USTRUCT()
struct FTestTask_StopTree : public FStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FTestTask_PrintValueInstanceData;

	FTestTask_StopTree() = default;
	explicit FTestTask_StopTree(const FName InName) { Name = InName; }
	virtual ~FTestTask_StopTree() override {}
	
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override
	{
		if (Phase == EStateTreeUpdatePhase::EnterStates)
		{
			return Context.Stop();
		}
		return EStateTreeRunStatus::Running;
	}

	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override
	{
		if (Phase == EStateTreeUpdatePhase::ExitStates)
		{
			Context.Stop();
		}
	}

	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override
	{
		if (Phase == EStateTreeUpdatePhase::TickStateTree)
		{
			return Context.Stop();
		}
		return EStateTreeRunStatus::Running;
	};

	/** Indicates in which phase the call to Stop should be performed. Possible values are EnterStates, ExitStats and TickStateTree */
	UPROPERTY()
	EStateTreeUpdatePhase Phase = EStateTreeUpdatePhase::Unset;
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

		if (Transition.CurrentRunStatus == EStateTreeRunStatus::Succeeded)
		{
			TestContext.Log(Name, TEXT("ExitSucceeded"));
		}
		else if (Transition.CurrentRunStatus == EStateTreeRunStatus::Failed)
		{
			TestContext.Log(Name, TEXT("ExitFailed"));
		}
		else if (Transition.CurrentRunStatus == EStateTreeRunStatus::Stopped)
		{
			TestContext.Log(Name, TEXT("ExitStopped"));
		}
		
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
		
		return (InstanceData.CurrentTick >= TicksToCompletion) ? TickCompletionResult : EStateTreeRunStatus::Running;
	};

	UPROPERTY(EditAnywhere, Category = Parameter)
	int32 TicksToCompletion = 1;

	UPROPERTY(EditAnywhere, Category = Parameter)
	EStateTreeRunStatus TickCompletionResult = EStateTreeRunStatus::Succeeded;

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

USTRUCT(meta = (Hidden))
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


USTRUCT()
struct FStateTreeTest_PropertyStructB
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "")
	int32 B = 0;
};

USTRUCT()
struct FStateTreeTest_PropertyStruct
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "")
	int32 A = 0;

	UPROPERTY(EditAnywhere, Category = "")
	int32 B = 0;

	UPROPERTY(EditAnywhere, Category = "")
	FStateTreeTest_PropertyStructB StructB;
};

UCLASS(HideDropdown)
class UStateTreeTest_PropertyObjectInstanced : public UObject
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, Category = "")
	int32 A = 0;

	UPROPERTY(EditAnywhere, Category = "")
	FInstancedStruct InstancedStruct;

	UPROPERTY(EditAnywhere, Category = "")
	TArray<FGameplayTag> ArrayOfTags;
};

UCLASS(HideDropdown)
class UStateTreeTest_PropertyObjectInstancedWithB : public UStateTreeTest_PropertyObjectInstanced
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, Category = "")
	int32 B = 0;
};

UCLASS(HideDropdown)
class UStateTreeTest_PropertyObject : public UObject
{
	GENERATED_BODY()
public:
	
	UPROPERTY(EditAnywhere, Instanced, Category = "")
	TObjectPtr<UStateTreeTest_PropertyObjectInstanced> InstancedObject;

	UPROPERTY(EditAnywhere, Instanced, Category = "")
	TArray<TObjectPtr<UStateTreeTest_PropertyObjectInstanced>> ArrayOfInstancedObjects;

	UPROPERTY(EditAnywhere, Category = "")
	TArray<int32> ArrayOfInts;

	UPROPERTY(EditAnywhere, Category = "")
	FInstancedStruct InstancedStruct;

	UPROPERTY(EditAnywhere, Category = "")
	TArray<FInstancedStruct> ArrayOfInstancedStructs;

	UPROPERTY(EditAnywhere, Category = "")
	FStateTreeTest_PropertyStruct Struct;

	UPROPERTY(EditAnywhere, Category = "")
	TArray<FStateTreeTest_PropertyStruct> ArrayOfStruct;
};

UCLASS(HideDropdown)
class UStateTreeTest_PropertyObject2 : public UObject
{
	GENERATED_BODY()
public:
};

USTRUCT()
struct FStateTreeTest_PropertyCopy
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "")
	FStateTreeTest_PropertyStruct Item;

	UPROPERTY(EditAnywhere, Category = "")
	TArray<FStateTreeTest_PropertyStruct> Array;
};

USTRUCT()
struct FStateTreeTest_PropertyRefSourceStruct
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "")
	FStateTreeTest_PropertyStruct Item;

	UPROPERTY(EditAnywhere, Category = "Output")
	FStateTreeTest_PropertyStruct OutputItem;

	UPROPERTY(EditAnywhere, Category = "")
	TArray<FStateTreeTest_PropertyStruct> Array;
};

USTRUCT()
struct FStateTreeTest_PropertyRefTargetStruct
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "", meta = (RefType = "/Script/StateTreeTestSuite.StateTreeTest_PropertyStruct"))
	FStateTreePropertyRef RefToStruct;

	UPROPERTY(EditAnywhere, Category = "", meta = (RefType = "Int32"))
	FStateTreePropertyRef RefToInt;

	UPROPERTY(EditAnywhere, Category = "", meta = (RefType = "/Script/StateTreeTestSuite.StateTreeTest_PropertyStruct", IsRefToArray))
	FStateTreePropertyRef RefToStructArray;
};

USTRUCT()
struct FStateTreeTest_PropertyCopyObjects
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "")
	TObjectPtr<UObject> Object;

	UPROPERTY(EditAnywhere, Category = "")
	TSubclassOf<UObject> Class;

	UPROPERTY(EditAnywhere, Category = "")
	TSoftObjectPtr<UObject> SoftObject;

	UPROPERTY(EditAnywhere, Category = "")
	TSoftClassPtr<UObject> SoftClass;
};
