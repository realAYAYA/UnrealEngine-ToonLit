// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/WorldSubsystem.h"

#include "MVVMViewWorldSubsystem.generated.h"

class UMVVMView;

/** */
struct FMVVMViewDelayedBinding
{
	FMVVMViewDelayedBinding(int32 InCompiledBindingIndex)
		: CompiledBindingIndex(InCompiledBindingIndex)
	{
	}

	int32 GetCompiledBindingIndex() const
	{
		return CompiledBindingIndex;
	}

	bool operator==(const FMVVMViewDelayedBinding& Other) const
	{
		return CompiledBindingIndex == Other.CompiledBindingIndex;
	}

	bool operator!=(const FMVVMViewDelayedBinding& Other) const
	{
		return CompiledBindingIndex != Other.CompiledBindingIndex;
	}

private:
	int32 CompiledBindingIndex = INDEX_NONE;
};

/** */
UCLASS()
class UMVVMViewWorldSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin UEngineSubsystem interface
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	//~ End UEngineSubsystem interface

	void AddViewWithEveryTickBinding(const UMVVMView* View);
	void RemoveViewWithEveryTickBinding(const UMVVMView* View);

	void AddDelayedBinding(const UMVVMView* View, FMVVMViewDelayedBinding CompiledBinding);

private:
	using FDelayedBindingList = TArray<FMVVMViewDelayedBinding, TInlineAllocator<8>>;
	using FDelayedMap = TMap<TWeakObjectPtr<const UMVVMView>, FDelayedBindingList>;
	FDelayedMap DelayedBindings;
	TArray<TWeakObjectPtr<const UMVVMView>> EveryTickBindings;
};
