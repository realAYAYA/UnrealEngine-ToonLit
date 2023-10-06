// Copyright Epic Games, Inc. All Rights Reserved.

#include "Scripting/OutlinerScriptingObject.h"
#include "Scripting/ViewModelScriptingStruct.h"

#include "MVVM/ViewModels/ViewModel.h"
#include "MVVM/ViewModels/ViewModelIterators.h"
#include "MVVM/ViewModels/OutlinerViewModel.h"
#include "MVVM/ViewModels/EditorViewModel.h"
#include "MVVM/ViewModelPtr.h"

#include "MVVM/Extensions/IOutlinerExtension.h"
#include "MVVM/Extensions/ISelectableExtension.h"

#include "MVVM/Selection/SequencerCoreSelection.h"
#include "MVVM/Selection/SequencerOutlinerSelection.h"

#include "SequencerCoreLog.h"

#define LOCTEXT_NAMESPACE "SequencerOutlinerScriptingObject"

void USequencerOutlinerScriptingObject::Initialize(UE::Sequencer::TViewModelPtr<UE::Sequencer::FOutlinerViewModel> InOutliner)
{
	using namespace UE::Sequencer;

	InOutliner->GetEditor()->GetSelection()->GetOutlinerSelection()->OnChanged.AddUObject(this, &USequencerOutlinerScriptingObject::BroadcastSelectionChanged);
	WeakOutliner = InOutliner;
}

void USequencerOutlinerScriptingObject::BroadcastSelectionChanged()
{
	OnSelectionChanged.Broadcast();
}

FSequencerViewModelScriptingStruct USequencerOutlinerScriptingObject::GetRootNode() const
{
	using namespace UE::Sequencer;
	TViewModelPtr<FOutlinerViewModel> Outliner = WeakOutliner.Pin();
	if (!Outliner)
	{
		FFrame::KismetExecutionMessage(*LOCTEXT("OutlinerInvalid", "Outliner is no longer valid.").ToString(), ELogVerbosity::Error);
		return FSequencerViewModelScriptingStruct();
	}

	return FSequencerViewModelScriptingStruct( Outliner->GetRootItem() );
}

TArray<FSequencerViewModelScriptingStruct> USequencerOutlinerScriptingObject::GetChildren(FSequencerViewModelScriptingStruct InViewModel, FName TypeName) const
{
	using namespace UE::Sequencer;

	FViewModelPtr ViewModel = InViewModel.WeakViewModel.Pin();
	if (!ViewModel)
	{
		FFrame::KismetExecutionMessage(*LOCTEXT("ViewModelNotValid", "View Model is no longer valid.").ToString(), ELogVerbosity::Error);
		return TArray<FSequencerViewModelScriptingStruct>();
	}

	TArray<FSequencerViewModelScriptingStruct> ViewModels;

	const FCastableTypeTable* TypeInfo = nullptr;
	if (TypeName != NAME_None)
	{
		TypeInfo = FCastableTypeTable::FindTypeByName(TypeName);
		if (!TypeInfo)
		{
			FFrame::KismetExecutionMessage(*FText::Format(LOCTEXT("InvalidTypeName", "Invalid type name {0} specified."), FText::FromName(TypeName)).ToString(), ELogVerbosity::Error);
			return TArray<FSequencerViewModelScriptingStruct>();
		}
	}

	for (FViewModelPtr Child : ViewModel->GetChildren(EViewModelListType::Outliner))
	{
		if (!TypeInfo || Child->GetTypeTable().Cast(Child.Get(), TypeInfo->GetTypeID()))
		{
			ViewModels.Add(Child);
		}
	}
	return ViewModels;
}

TArray<FSequencerViewModelScriptingStruct> USequencerOutlinerScriptingObject::GetSelection() const
{
	using namespace UE::Sequencer;

	TArray<FSequencerViewModelScriptingStruct> SelectedNodes;

	TViewModelPtr<FOutlinerViewModel> Outliner = WeakOutliner.Pin();
	if (!Outliner)
	{
		FFrame::KismetExecutionMessage(*LOCTEXT("OutlinerInvalid", "Outliner is no longer valid.").ToString(), ELogVerbosity::Error);
		return SelectedNodes;
	}

	TSharedPtr<FOutlinerSelection> OutlinerSelection = Outliner->GetEditor()->GetSelection()->GetOutlinerSelection();
	if (OutlinerSelection)
	{
		SelectedNodes.Reserve(OutlinerSelection->Num());
		for (TViewModelPtr<IOutlinerExtension> OutlinerItem : *OutlinerSelection)
		{
			SelectedNodes.Emplace(OutlinerItem);
		}
	}
	return SelectedNodes;
}

void USequencerOutlinerScriptingObject::SetSelection(const TArray<FSequencerViewModelScriptingStruct>& InSelection)
{
	using namespace UE::Sequencer;

	TViewModelPtr<FOutlinerViewModel> Outliner = WeakOutliner.Pin();
	if (!Outliner)
	{
		FFrame::KismetExecutionMessage(*LOCTEXT("OutlinerInvalid", "Outliner is no longer valid.").ToString(), ELogVerbosity::Error);
		return;
	}

	TSharedPtr<FSequencerCoreSelection> Selection         = Outliner->GetEditor()->GetSelection();
	TSharedPtr<FOutlinerSelection>      OutlinerSelection = Selection->GetOutlinerSelection();

	if (OutlinerSelection)
	{
		FSelectionEventSuppressor Suppressor = Selection->SuppressEvents();

		OutlinerSelection->Empty();
		for (const FSequencerViewModelScriptingStruct& Item : InSelection)
		{
			TViewModelPtr<IOutlinerExtension> OutlinerItem = Item.WeakViewModel.ImplicitPin();
			if (OutlinerItem)
			{
				TViewModelPtr<ISelectableExtension> Selectable = Item.WeakViewModel.ImplicitPin();
				if (Selectable && !EnumHasAnyFlags(Selectable->IsSelectable(), ESelectionIntent::PersistentSelection))
				{
					UE_LOG(LogSequencerCore, Warning, TEXT("Refusing to select item %s because it is not selectable."), *OutlinerItem->GetLabel().ToString());
					continue;
				}

				OutlinerSelection->Select(OutlinerItem);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE