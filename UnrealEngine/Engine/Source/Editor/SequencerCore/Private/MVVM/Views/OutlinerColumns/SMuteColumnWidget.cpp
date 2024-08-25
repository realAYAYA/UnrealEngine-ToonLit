// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVM/Views/OutlinerColumns/SMuteColumnWidget.h"

#include "MVVM/SharedViewModelData.h"
#include "MVVM/Selection/SequencerCoreSelection.h"
#include "MVVM/Selection/SequencerOutlinerSelection.h"
#include "MVVM/Extensions/IMutableExtension.h"
#include "MVVM/ViewModels/ViewModelIterators.h"
#include "MVVM/ViewModels/EditorViewModel.h"
#include "MVVM/ViewModels/OutlinerColumns/IOutlinerColumn.h"


namespace UE::Sequencer
{

void SMuteColumnWidget::OnToggleOperationComplete()
{
	// refresh the sequencer tree after operation is complete
	RefreshSequencerTree();
}

void SMuteColumnWidget::Construct(const FArguments& InArgs, const TWeakPtr<IOutlinerColumn> InWeakOutlinerColumn, const FCreateOutlinerColumnParams& InParams)
{
	SColumnToggleWidget::Construct(
		SColumnToggleWidget::FArguments(),
		InWeakOutlinerColumn,
		InParams
	);

	WeakMuteStateCacheExtension = CastViewModel<FMuteStateCacheExtension>(InParams.OutlinerExtension.AsModel()->GetSharedData());
}

bool SMuteColumnWidget::IsActive() const
{
	if (TViewModelPtr<FMuteStateCacheExtension> MuteStateCache = WeakMuteStateCacheExtension.Pin())
	{
		return EnumHasAnyFlags(MuteStateCache->GetCachedFlags(ModelID), ECachedMuteState::Muted);
	}

	return false;
}

void SMuteColumnWidget::SetIsActive(const bool bInIsActive)
{
	TViewModelPtr<IOutlinerExtension> OutlinerItem = WeakOutlinerExtension.Pin();
	TSharedPtr<FEditorViewModel>      Editor       = WeakEditor.Pin();
	if (!OutlinerItem || !Editor)
	{
		return;
	}

	const FScopedTransaction Transaction(NSLOCTEXT("Sequencer", "SetNodeMuted", "Set Node Muted"));

	if (OutlinerItem->GetSelectionState() == EOutlinerSelectionState::SelectedDirectly)
	{
		// If selected, modify all selected items
		for (FViewModelPtr Selected : *Editor->GetSelection()->GetOutlinerSelection())
		{
			SetIsActive(Selected, bInIsActive);
		}
	}
	else
	{
		SetIsActive(OutlinerItem.AsModel(), bInIsActive);
	}
}

void SMuteColumnWidget::SetIsActive(const FViewModelPtr& ViewModel, const bool bInIsActive)
{
	TViewModelPtr<IMutableExtension> Mutable = ViewModel.ImplicitCast();
	if (bInIsActive)
	{
		// If this is mutable, mute only this
		if (Mutable)
		{
			Mutable->SetIsMuted(true);
		}
		// Otherwise mute mutable children of this (if any)
		else for (TViewModelPtr<IMutableExtension> Child : ViewModel->GetDescendantsOfType<IMutableExtension>())
		{
			Child->SetIsMuted(true);
		}
	}
	else
	{
		for (TViewModelPtr<IMutableExtension> Child : ViewModel->GetDescendantsOfType<IMutableExtension>(true))
		{
			Child->SetIsMuted(false);
		}
	}
}

bool SMuteColumnWidget::IsChildActive() const
{
	if (TViewModelPtr<FMuteStateCacheExtension> MuteStateCache = WeakMuteStateCacheExtension.Pin())
	{
		return EnumHasAnyFlags(MuteStateCache->GetCachedFlags(ModelID), ECachedMuteState::PartiallyMutedChildren);
	}

	return false;
}

bool SMuteColumnWidget::IsImplicitlyActive() const
{
	if (TViewModelPtr<FMuteStateCacheExtension> MuteStateCache = WeakMuteStateCacheExtension.Pin())
	{
		return EnumHasAnyFlags(MuteStateCache->GetCachedFlags(ModelID), ECachedMuteState::ImplicitlyMutedByParent);
	}

	return false;
}

const FSlateBrush* SMuteColumnWidget::GetActiveBrush() const
{
	static const FName NAME_MutedBrush = TEXT("Sequencer.Column.Mute");
	return FAppStyle::Get().GetBrush(NAME_MutedBrush);
}

} // namespace UE::Sequencer
