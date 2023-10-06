// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"

#include "OptimusAction.generated.h"

class IOptimusPathResolver;

// Base action class. This is a UStruct so that we can use UE's RAII to check for type
// similarity.
USTRUCT()
struct FOptimusAction
{
	GENERATED_BODY()

	FOptimusAction(const FString& InTitle = {});
	virtual ~FOptimusAction();

	const FString& GetTitle() const 
	{
		return Title;
	}

	void SetTitle(const FString& InTitle)
	{
		Title = InTitle;
	}

	template <typename FmtType, typename... ArgTypes>
	void SetTitlef(const FmtType& Fmt, ArgTypes&& ...Args)
	{
		Title = FString::Printf(Fmt, Forward<ArgTypes>(Args)...);
	}

protected:
	friend class UOptimusActionStack;
	friend struct FOptimusCompoundAction;

	/// Performs the action as set by the action's constructor.
	virtual bool Do(IOptimusPathResolver* InRoot) PURE_VIRTUAL(FOptimusAction::Do, return false; );

	/// Reverts the action performed by the action's Do function.
	virtual bool Undo(IOptimusPathResolver* InRoot) PURE_VIRTUAL(FOptimusAction::Undo, return false; );

private:
	/// The title of the action. Should be set by the constructor of the derived objects.
	FString Title;
};

// Mark FOptimusAction as pure virtual, so that the UObject machinery won't attempt to instantiate it.
template<>
struct TStructOpsTypeTraits<FOptimusAction> :
TStructOpsTypeTraitsBase2<FOptimusAction>
{
	enum
	{
		WithPureVirtual = true,
    };
};


USTRUCT()
struct FOptimusCompoundAction :
	public FOptimusAction
{
	GENERATED_BODY()

	FOptimusCompoundAction(const FString& InTitle = {}) :
		FOptimusAction(InTitle)
	{}

	// Copy nothing.
	FOptimusCompoundAction(const FOptimusCompoundAction &) {}
	FOptimusCompoundAction &operator=(const FOptimusCompoundAction &) { return *this; }

	FOptimusCompoundAction(FOptimusCompoundAction&&) = default;
	FOptimusCompoundAction& operator=(FOptimusCompoundAction&&) = default;

	template <typename FmtType, typename... ArgTypes>
	FOptimusCompoundAction(FmtType&& Fmt, ArgTypes&& ...Args) :
		FOptimusAction(FString::Printf(Forward<FmtType>(Fmt), Forward<ArgTypes>(Args)...))
	{ }

	/// Add a sub-action from a heap-constructed action. This takes ownership of the pointer.
	/// @param InAction The action to add.
	void AddSubAction(FOptimusAction* InAction);

	template<typename T, typename... ArgTypes>
	typename TEnableIf<TPointerIsConvertibleFromTo<T, FOptimusAction>::Value, void>::Type 
	AddSubAction(ArgTypes&& ...Args)
	{
		SubActions.Add(MakeShared<T>(Forward<ArgTypes>(Args)...));
	}

	bool HasSubActions() const
	{
		return SubActions.Num() != 0;
	}

protected:
	friend class UOptimusActionStack;
	void AddSubAction(TSharedPtr<FOptimusAction> InAction)
	{
		SubActions.Add(InAction);
	}

	bool Do(IOptimusPathResolver* InRoot) override;
	bool Undo(IOptimusPathResolver* InRoot) override;

private:
	TArray<TSharedPtr<FOptimusAction>> SubActions;
};
