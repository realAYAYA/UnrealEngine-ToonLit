// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"

#include "OptimusActionStack.generated.h"

class IOptimusPathResolver;
struct FOptimusAction;
struct FOptimusCompoundAction;

// Base action class
UCLASS()
class OPTIMUSCORE_API UOptimusActionStack :
	public UObject
{
	GENERATED_BODY()
public:
	UOptimusActionStack();

	/// Run a heap-constructed action created with operator new. 
	/// The action stack takes ownership of the pointer. If the function fails the pointer is
	/// no longer valid.
	bool RunAction(FOptimusAction* InAction);

	template<typename T, typename... ArgsType>
	typename TEnableIf<TPointerIsConvertibleFromTo<T, FOptimusAction>::Value, bool>::Type 
	RunAction(ArgsType&& ...Args)
	{
		return RunAction(MakeShared<T>(Forward<ArgsType>(Args)...));
	}

	IOptimusPathResolver *GetGraphCollectionRoot() const;

	void SetTransactionScopeFunctions(
		TFunction<int32(UObject* TransactObject, const FString& Title)> InBeginScopeFunc,
		TFunction<void(int32 InTransactionId)> InEndScopeFunc
		);

	bool Redo();
	bool Undo();

	// UObject overrides
#if WITH_EDITOR
	void PostTransacted(const FTransactionObjectEvent& TransactionEvent) override;
#endif // WITH_EDITOR

protected:
	friend class FOptimusActionScope;

	// Open a new action scope. All subsequent calls to RunAction will add actions to the scope
	// but they won't be run until the last action scope is closed.
	void OpenActionScope(const FString& InTitle);

	// Close the current action scope. Once all action scopes are closed, the collected actions
	// are finally run in the order they got added.
	bool CloseActionScope();


private:
	bool RunAction(TSharedPtr<FOptimusAction> InAction);

	UPROPERTY()
	int32 TransactedActionIndex = 0;

	int32 CurrentActionIndex = 0;

	bool bIsRunningAction = false;

	TArray<TSharedPtr<FOptimusAction>> Actions;

	TArray<TSharedPtr<FOptimusCompoundAction>> ActionScopes;

	TFunction<int(UObject* TransactObject, const FString& Title)> BeginScopeFunc;
	TFunction<void(int InTransactionId)> EndScopeFunc;
};

class OPTIMUSCORE_API FOptimusActionScope
{
public:
	FOptimusActionScope(UOptimusActionStack& InActionStack, const FString& InTitle);

	~FOptimusActionScope();

private:
	UOptimusActionStack& ActionStack;
};
