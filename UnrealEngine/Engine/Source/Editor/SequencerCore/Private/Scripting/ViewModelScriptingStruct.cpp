// Copyright Epic Games, Inc. All Rights Reserved.

#include "Scripting/ViewModelScriptingStruct.h"
#include "MVVM/Extensions/IOutlinerExtension.h"

FSequencerViewModelScriptingStruct::FSequencerViewModelScriptingStruct()
{
	Type = "null";
}

FSequencerViewModelScriptingStruct::FSequencerViewModelScriptingStruct(UE::Sequencer::FViewModelPtr InViewModel)
{
	if (InViewModel)
	{
		WeakViewModel = InViewModel;
		Type = InViewModel->GetTypeTable().GetTypeName();
	}
	else
	{
		Type = "null";
	}
}

FString USequencerViewModelStructExtensions::GetLabel(const FSequencerViewModelScriptingStruct& ViewModel)
{
	using namespace UE::Sequencer;

	TViewModelPtr<IOutlinerExtension> Outliner = ViewModel.WeakViewModel.ImplicitPin();
	return Outliner ? Outliner->GetLabel().ToString() : FString();
}