// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/EngineSubsystem.h"

#include "MVVMBindingSubsystem.generated.h"

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
UCLASS(NotBlueprintable, Hidden)
class UMVVMBindingSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void AddViewWithEveryTickBinding(const UMVVMView* View);
	void RemoveViewWithEveryTickBinding(const UMVVMView* View);

	void AddDelayedBinding(const UMVVMView* View, FMVVMViewDelayedBinding CompiledBinding);

private:
	void HandlePreTick(float DeltaTIme);

	using FDelayedBindingList = TArray<FMVVMViewDelayedBinding, TInlineAllocator<8>>;
	using FDelayedMap = TMap<TWeakObjectPtr<const UMVVMView>, FDelayedBindingList>;
	FDelayedMap DelayedBindings;
	TArray<TWeakObjectPtr<const UMVVMView>> EveryTickBindings;
};
