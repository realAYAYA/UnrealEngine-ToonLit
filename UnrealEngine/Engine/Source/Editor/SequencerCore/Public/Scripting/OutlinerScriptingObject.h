// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "UObject/Object.h"
#include "MVVM/ViewModelPtr.h"

#include "OutlinerScriptingObject.generated.h"

namespace UE::Sequencer
{
	class FOutlinerViewModel;
}

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSequencerOutlinerSelectionChanged);

UCLASS(MinimalAPI)
class USequencerOutlinerScriptingObject : public UObject
{
public:

	GENERATED_BODY()

	UPROPERTY(BlueprintAssignable, Category="Sequencer Editor")
	FSequencerOutlinerSelectionChanged OnSelectionChanged;

	SEQUENCERCORE_API void Initialize(UE::Sequencer::TViewModelPtr<UE::Sequencer::FOutlinerViewModel> InOutliner);

	UFUNCTION(BlueprintCallable, Category="Sequencer Editor")
	FSequencerViewModelScriptingStruct GetRootNode() const;

	UFUNCTION(BlueprintCallable, Category="Sequencer Editor")
	TArray<FSequencerViewModelScriptingStruct> GetChildren(FSequencerViewModelScriptingStruct Node, FName TypeName = NAME_None) const;

	UFUNCTION(BlueprintCallable, Category="Sequencer Editor")
	TArray<FSequencerViewModelScriptingStruct> GetSelection() const;

	UFUNCTION(BlueprintCallable, Category="Sequencer Editor")
	void SetSelection(const TArray<FSequencerViewModelScriptingStruct>& InSelection);

protected:

	UE::Sequencer::TWeakViewModelPtr<UE::Sequencer::FOutlinerViewModel> WeakOutliner;

private:

	void BroadcastSelectionChanged();
};