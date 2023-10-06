// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "MVVM/ViewModelPtr.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "ViewModelScriptingStruct.generated.h"


USTRUCT(BlueprintType)
struct FSequencerViewModelScriptingStruct
{
	GENERATED_BODY()

	SEQUENCERCORE_API FSequencerViewModelScriptingStruct();

	SEQUENCERCORE_API FSequencerViewModelScriptingStruct(UE::Sequencer::FViewModelPtr InViewModel);

	friend bool operator==(const FSequencerViewModelScriptingStruct& A, const FSequencerViewModelScriptingStruct& B)
	{
		return A.WeakViewModel == B.WeakViewModel;
	}

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Sequencer Editor")
	FName Type;

	UE::Sequencer::FWeakViewModelPtr WeakViewModel;
};

template<>
struct TStructOpsTypeTraits<FSequencerViewModelScriptingStruct> : public TStructOpsTypeTraitsBase2<FSequencerViewModelScriptingStruct>
{
	enum
	{
		WithIdenticalViaEquality = true,
		WithCopy = true,
	};
};


/**
 * Function library containing methods that should be hoisted onto FSequencerScriptingRanges
 */
UCLASS()
class USequencerViewModelStructExtensions : public UBlueprintFunctionLibrary
{
public:

	GENERATED_BODY()

	UFUNCTION(BlueprintPure, Category = "Sequencer Editor", meta=(ScriptMethod))
	static FString GetLabel(const FSequencerViewModelScriptingStruct& ViewModel);
};