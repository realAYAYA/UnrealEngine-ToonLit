// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleEditorSelection.h"

#include "Algo/Sort.h"
#include "DMXControlConsoleFaderBase.h"
#include "DMXControlConsoleFaderGroup.h"
#include "DMXControlConsoleFaderGroupRow.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutBase.h"
#include "Layouts/DMXControlConsoleEditorLayouts.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "Models/Filter/FilterModel.h"


#define LOCTEXT_NAMESPACE "DMXControlConsoleEditorSelection"

void FDMXControlConsoleEditorSelection::AddToSelection(UDMXControlConsoleFaderGroup* FaderGroup, bool bNotifySelectionChange)
{
	if (FaderGroup && FaderGroup->IsActive())
	{
		SelectedFaderGroups.AddUnique(FaderGroup);

		UpdateMultiSelectAnchor(UDMXControlConsoleFaderGroup::StaticClass());

		if (bNotifySelectionChange)
		{
			OnSelectionChanged.Broadcast();
		}
	}
}

void FDMXControlConsoleEditorSelection::AddToSelection(UDMXControlConsoleFaderBase* Fader, bool bNotifySelectionChange)
{
	if (Fader && Fader->IsActive())
	{
		SelectedFaders.AddUnique(Fader);

		UDMXControlConsoleFaderGroup& FaderGroup = Fader->GetOwnerFaderGroupChecked();
		SelectedFaderGroups.AddUnique(&FaderGroup);

		UpdateMultiSelectAnchor(UDMXControlConsoleFaderBase::StaticClass());

		if (bNotifySelectionChange)
		{
			OnSelectionChanged.Broadcast();
		}
	}
}

void FDMXControlConsoleEditorSelection::AddToSelection(const TArray<UObject*> Elements, bool bNotifySelectionChange)
{
	if (Elements.IsEmpty())
	{
		return;
	}

	for (UObject* Element : Elements)
	{
		if (UDMXControlConsoleFaderGroup* FaderGroup = Cast<UDMXControlConsoleFaderGroup>(Element))
		{
			constexpr bool bNotifyFaderGroupSelectionChange = false;
			AddToSelection(FaderGroup, bNotifyFaderGroupSelectionChange);
		}
		else if (UDMXControlConsoleFaderBase* Fader = Cast<UDMXControlConsoleFaderBase>(Element))
		{
			constexpr bool bNotifyFaderSelectionChange = false;
			AddToSelection(Fader, bNotifyFaderSelectionChange);
		}
	}

	if (bNotifySelectionChange)
	{
		OnSelectionChanged.Broadcast();
	}
}

void FDMXControlConsoleEditorSelection::AddAllFadersFromFaderGroupToSelection(UDMXControlConsoleFaderGroup* FaderGroup, bool bOnlyMatchingFilter, bool bNotifySelectionChange)
{
	if (FaderGroup && FaderGroup->IsActive())
	{
		const TArray<UDMXControlConsoleFaderBase*> AllFaders = FaderGroup->GetAllFaders();
		for (UDMXControlConsoleFaderBase* Fader : AllFaders)
		{
			if (!Fader || !Fader->IsActive())
			{
				continue;
			}

			if (bOnlyMatchingFilter && !Fader->IsMatchingFilter())
			{
				continue;
			}

			SelectedFaders.AddUnique(Fader);
		}

		SelectedFaderGroups.AddUnique(FaderGroup);

		UpdateMultiSelectAnchor(UDMXControlConsoleFaderGroup::StaticClass());

		if (bNotifySelectionChange)
		{
			OnSelectionChanged.Broadcast();
		}
	}
}

void FDMXControlConsoleEditorSelection::RemoveFromSelection(UDMXControlConsoleFaderGroup* FaderGroup, bool bNotifySelectionChange)
{
	if (FaderGroup && SelectedFaderGroups.Contains(FaderGroup))
	{
		constexpr bool bNotifyFadersSelectionChange = false;
		ClearFadersSelection(FaderGroup, bNotifyFadersSelectionChange);
		SelectedFaderGroups.Remove(FaderGroup);

		UpdateMultiSelectAnchor(UDMXControlConsoleFaderGroup::StaticClass());

		if (bNotifySelectionChange)
		{
			OnSelectionChanged.Broadcast();
		}
	}
}

void FDMXControlConsoleEditorSelection::RemoveFromSelection(UDMXControlConsoleFaderBase* Fader, bool bNotifySelectionChange)
{
	if (Fader && SelectedFaders.Contains(Fader))
	{
		SelectedFaders.Remove(Fader);

		UpdateMultiSelectAnchor(UDMXControlConsoleFaderBase::StaticClass());

		if (bNotifySelectionChange)
		{
			OnSelectionChanged.Broadcast();
		}
	}
}

void FDMXControlConsoleEditorSelection::RemoveFromSelection(const TArray<UObject*> Elements, bool bNotifySelectionChange)
{
	if (Elements.IsEmpty())
	{
		return;
	}

	for (UObject* Element : Elements)
	{
		if (UDMXControlConsoleFaderGroup* FaderGroup = Cast<UDMXControlConsoleFaderGroup>(Element))
		{
			constexpr bool bNotifyFaderGroupSelectionChange = false;
			RemoveFromSelection(FaderGroup, bNotifyFaderGroupSelectionChange);
		}
		else if (UDMXControlConsoleFaderBase* Fader = Cast<UDMXControlConsoleFaderBase>(Element))
		{
			constexpr bool bNotifyFaderSelectionChange = false;
			RemoveFromSelection(Fader, bNotifyFaderSelectionChange);
		}
	}

	if (bNotifySelectionChange)
	{
		OnSelectionChanged.Broadcast();
	}
}

void FDMXControlConsoleEditorSelection::Multiselect(UObject* FaderOrFaderGroupObject)
{
	const UClass* MultiSelectClass = FaderOrFaderGroupObject->GetClass();
	if (!ensureMsgf(MultiSelectClass == UDMXControlConsoleFaderGroup::StaticClass() || FaderOrFaderGroupObject->IsA(UDMXControlConsoleFaderBase::StaticClass()), TEXT("Invalid type when trying to multiselect")))
	{
		return;
	}

	constexpr bool bNotifySelectionChange = false;
	RemoveInvalidObjectsFromSelection(bNotifySelectionChange);

	// Normal selection if nothing is selected or there's no valid anchor
	if (!MultiSelectAnchor.IsValid() ||
		(SelectedFaderGroups.IsEmpty() && SelectedFaders.IsEmpty()))
	{
		if (UDMXControlConsoleFaderGroup* FaderGroup = Cast<UDMXControlConsoleFaderGroup>(FaderOrFaderGroupObject))
		{
			constexpr bool bNotifyFaderGroupSelectionChange = false;
			AddToSelection(FaderGroup, bNotifyFaderGroupSelectionChange);
		}
		else if (UDMXControlConsoleFaderBase* Fader = Cast<UDMXControlConsoleFaderBase>(FaderOrFaderGroupObject))
		{
			constexpr bool bFaderSelectionChange = false;
			AddToSelection(Fader, bFaderSelectionChange);
		}
		return;
	}

	TArray<UObject*> FadersAndFaderGroups;
	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	const UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorConsoleModel->GetEditorConsoleLayouts();
	if (!EditorConsoleLayouts)
	{
		return;
	}

	const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = EditorConsoleLayouts->GetActiveLayout();
	if (!ActiveLayout)
	{
		return;
	}

	for (const TWeakObjectPtr<UDMXControlConsoleFaderGroup> AnyFaderGroup : ActiveLayout->GetAllFaderGroups())
	{
		if (!AnyFaderGroup.IsValid())
		{
			continue;
		}

		FadersAndFaderGroups.AddUnique(AnyFaderGroup.Get());
		for (UDMXControlConsoleFaderBase* AnyFader : AnyFaderGroup->GetAllFaders())
		{
			FadersAndFaderGroups.AddUnique(AnyFader);
		}
	}

	const int32 IndexOfFaderGroupAnchor = FadersAndFaderGroups.IndexOfByPredicate([this](const UObject* Object)
		{
			return MultiSelectAnchor == Object;
		});
	const int32 IndexOfFaderAnchor = FadersAndFaderGroups.IndexOfByPredicate([this](const UObject* Object)
		{
			return MultiSelectAnchor == Object;
		});

	const int32 IndexOfAnchor = FMath::Max(IndexOfFaderGroupAnchor, IndexOfFaderAnchor);
	if (!ensureAlwaysMsgf(IndexOfAnchor != INDEX_NONE, TEXT("No previous selection when multi selecting, cannot multiselect.")))
	{
		return;
	}

	const int32 IndexOfSelection = FadersAndFaderGroups.IndexOfByKey(FaderOrFaderGroupObject);

	const int32 StartIndex = FMath::Min(IndexOfAnchor, IndexOfSelection);
	const int32 EndIndex = FMath::Max(IndexOfAnchor, IndexOfSelection);

	SelectedFaderGroups.Reset();
	SelectedFaders.Reset();
	for (int32 IndexToSelect = StartIndex; IndexToSelect <= EndIndex; IndexToSelect++)
	{
		if (!ensureMsgf(FadersAndFaderGroups.IsValidIndex(IndexToSelect), TEXT("Invalid index when multiselecting")))
		{
			break;
		}

		if (UDMXControlConsoleFaderGroup* FaderGroupToSelect = Cast<UDMXControlConsoleFaderGroup>(FadersAndFaderGroups[IndexToSelect]))
		{
			if (FaderGroupToSelect  && FaderGroupToSelect->IsActive() && FaderGroupToSelect->IsMatchingFilter())
			{
				SelectedFaderGroups.AddUnique(FaderGroupToSelect);
			}
		}
		else if (UDMXControlConsoleFaderBase* FaderToSelect = Cast<UDMXControlConsoleFaderBase>(FadersAndFaderGroups[IndexToSelect]))
		{
			if (FaderToSelect && FaderToSelect->IsActive() && FaderToSelect->IsMatchingFilter())
			{
				SelectedFaders.AddUnique(FaderToSelect);
			}
		}
	}
	if (!SelectedFaders.IsEmpty())
	{
		// Always select the fader group of the first selected fader
		UDMXControlConsoleFaderBase* FirstSelectedFader = CastChecked<UDMXControlConsoleFaderBase>(SelectedFaders[0]);
		SelectedFaderGroups.AddUnique(&FirstSelectedFader->GetOwnerFaderGroupChecked());
	}

	OnSelectionChanged.Broadcast();
}

void FDMXControlConsoleEditorSelection::ReplaceInSelection(UDMXControlConsoleFaderGroup* FaderGroup)
{
	if (!FaderGroup || !IsSelected(FaderGroup))
	{
		return;
	}

	RemoveFromSelection(FaderGroup);

	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	const UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorConsoleModel->GetEditorConsoleLayouts();
	if (!EditorConsoleLayouts)
	{
		return;
	}

	const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = EditorConsoleLayouts->GetActiveLayout();
	if (!ActiveLayout)
	{
		return;
	}

	const TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>> AllActiveFaderGroups = ActiveLayout->GetAllActiveFaderGroups();
	if (AllActiveFaderGroups.Num() <= 1)
	{
		return;
	}

	const int32 Index = AllActiveFaderGroups.IndexOfByKey(FaderGroup);

	int32 NewIndex = Index - 1;
	if (!AllActiveFaderGroups.IsValidIndex(NewIndex))
	{
		NewIndex = Index + 1;
	}

	const TWeakObjectPtr<UDMXControlConsoleFaderGroup> NewSelectedFaderGroup = AllActiveFaderGroups.IsValidIndex(NewIndex) ? AllActiveFaderGroups[NewIndex] : nullptr;
	if (!NewSelectedFaderGroup.IsValid())
	{
		return;
	}

	AddToSelection(NewSelectedFaderGroup.Get());
}

void FDMXControlConsoleEditorSelection::ReplaceInSelection(UDMXControlConsoleFaderBase* Fader)
{
	if (!Fader || !IsSelected(Fader))
	{
		return;
	}

	RemoveFromSelection(Fader);

	const UDMXControlConsoleFaderGroup& FaderGroup = Fader->GetOwnerFaderGroupChecked();
	const TArray<UDMXControlConsoleFaderBase*> Faders = FaderGroup.GetAllFaders();
	if (Faders.Num() <= 1)
	{
		return;
	}

	const int32 IndexToReplace = Faders.IndexOfByKey(Fader);
	int32 NewIndex = IndexToReplace - 1;
	if (!Faders.IsValidIndex(NewIndex))
	{
		NewIndex = IndexToReplace + 1;
	}

	UDMXControlConsoleFaderBase* NewSelectedFader = Faders.IsValidIndex(NewIndex) ? Faders[NewIndex] : nullptr;
	AddToSelection(NewSelectedFader);
}

bool FDMXControlConsoleEditorSelection::IsSelected(UDMXControlConsoleFaderGroup* FaderGroup) const
{
	return SelectedFaderGroups.Contains(FaderGroup);
}

bool FDMXControlConsoleEditorSelection::IsSelected(UDMXControlConsoleFaderBase* Fader) const
{
	return SelectedFaders.Contains(Fader);
}

void FDMXControlConsoleEditorSelection::SelectAll(bool bOnlyMatchingFilter)
{
	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	const UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorConsoleModel->GetEditorConsoleLayouts();
	if (!EditorConsoleLayouts)
	{
		return;
	}

	const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = EditorConsoleLayouts->GetActiveLayout();
	if (!ActiveLayout)
	{
		return;
	}

	ClearSelection(false);

	const TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>> AllFaderGroups = ActiveLayout->GetAllFaderGroups();
	for (const TWeakObjectPtr<UDMXControlConsoleFaderGroup>& FaderGroup : AllFaderGroups)
	{
		if (FaderGroup.IsValid() && FaderGroup->IsActive())
		{
			constexpr bool bNotifyFaderSelectionChange = false;
			AddAllFadersFromFaderGroupToSelection(FaderGroup.Get(), bOnlyMatchingFilter, bNotifyFaderSelectionChange);
		}
	}

	OnSelectionChanged.Broadcast();
}

void FDMXControlConsoleEditorSelection::RemoveInvalidObjectsFromSelection(bool bNotifySelectionChange)
{
	SelectedFaderGroups.Remove(nullptr);
	SelectedFaders.Remove(nullptr);

	if (bNotifySelectionChange)
	{
		OnSelectionChanged.Broadcast();
	}
}

void FDMXControlConsoleEditorSelection::ClearFadersSelection(UDMXControlConsoleFaderGroup* FaderGroup, bool bNotifySelectionChange)
{
	if (!FaderGroup || !SelectedFaderGroups.Contains(FaderGroup))
	{
		return;
	}

	TArray<UDMXControlConsoleFaderBase*> Faders = FaderGroup->GetAllFaders();

	auto IsFaderGroupOwnerLambda = [Faders](const TWeakObjectPtr<UObject> SelectedObject)
	{
		const UDMXControlConsoleFaderBase* SelectedFader = Cast<UDMXControlConsoleFaderBase>(SelectedObject);
		if (!SelectedFader)
		{
			return true;
		}

		if (Faders.Contains(SelectedFader))
		{
			return true;
		}

		return false;
	};

	SelectedFaders.RemoveAll(IsFaderGroupOwnerLambda);

	if (!MultiSelectAnchor.IsValid() || MultiSelectAnchor->IsA(UDMXControlConsoleFaderBase::StaticClass()))
	{
		UpdateMultiSelectAnchor(UDMXControlConsoleFaderBase::StaticClass());
	}

	if (bNotifySelectionChange)
	{
		OnSelectionChanged.Broadcast();
	}
}

void FDMXControlConsoleEditorSelection::ClearSelection(bool bNotifySelectionChange)
{
	SelectedFaderGroups.Reset();
	SelectedFaders.Reset();

	if (bNotifySelectionChange)
	{
		OnSelectionChanged.Broadcast();
	}
}

UDMXControlConsoleFaderGroup* FDMXControlConsoleEditorSelection::GetFirstSelectedFaderGroup(bool bReverse) const
{
	TArray<TWeakObjectPtr<UObject>> CurrentSelectedFaderGroups = GetSelectedFaderGroups();
	if (CurrentSelectedFaderGroups.IsEmpty())
	{
		return nullptr;
	}

	auto SortSelectedFaderGroupsLambda = [](TWeakObjectPtr<UObject> FaderGroupObjectA, TWeakObjectPtr<UObject> FaderGroupObjectB)
		{
			const UDMXControlConsoleFaderGroup* FaderGroupA = Cast<UDMXControlConsoleFaderGroup>(FaderGroupObjectA);
			const UDMXControlConsoleFaderGroup* FaderGroupB = Cast<UDMXControlConsoleFaderGroup>(FaderGroupObjectB);

			if (!FaderGroupA || !FaderGroupB)
			{
				return false;
			}

			const int32 RowIndexA = FaderGroupA->GetOwnerFaderGroupRowChecked().GetRowIndex();
			const int32 RowIndexB = FaderGroupB->GetOwnerFaderGroupRowChecked().GetRowIndex();

			if (RowIndexA != RowIndexB)
			{
				return RowIndexA < RowIndexB;
			}

			const int32 IndexA = FaderGroupA->GetIndex();
			const int32 IndexB = FaderGroupB->GetIndex();

			return IndexA < IndexB;
		};

	Algo::Sort(CurrentSelectedFaderGroups, SortSelectedFaderGroupsLambda);
	const TWeakObjectPtr<UObject> FirstFaderGroup = bReverse ? CurrentSelectedFaderGroups.Last() : CurrentSelectedFaderGroups[0];
	return Cast<UDMXControlConsoleFaderGroup>(FirstFaderGroup);
}

UDMXControlConsoleFaderBase* FDMXControlConsoleEditorSelection::GetFirstSelectedFader(bool bReverse) const
{
	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	const UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorConsoleModel->GetEditorConsoleLayouts();
	if (!EditorConsoleLayouts)
	{
		return nullptr;
	}

	const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = EditorConsoleLayouts->GetActiveLayout();
	if (!ActiveLayout)
	{
		return nullptr;
	}

	const TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>> AllFaderGroups = ActiveLayout->GetAllFaderGroups();
	if (AllFaderGroups.IsEmpty())
	{
		return nullptr;
	}

	TArray<TWeakObjectPtr<UObject>> CurrentSelectedFaders = GetSelectedFaders();
	if (CurrentSelectedFaders.IsEmpty())
	{
		return nullptr;
	}

	auto SortSelectedFadersLambda = [AllFaderGroups](TWeakObjectPtr<UObject> FaderObjectA, TWeakObjectPtr<UObject> FaderObjectB)
		{
			const UDMXControlConsoleFaderBase* FaderA = Cast<UDMXControlConsoleFaderBase>(FaderObjectA);
			const UDMXControlConsoleFaderBase* FaderB = Cast<UDMXControlConsoleFaderBase>(FaderObjectB);

			if (!FaderA || !FaderB)
			{
				return false;
			}

			const UDMXControlConsoleFaderGroup& FaderGroupA = FaderA->GetOwnerFaderGroupChecked();
			const UDMXControlConsoleFaderGroup& FaderGroupB = FaderB->GetOwnerFaderGroupChecked();

			const int32 FaderGroupIndexA = AllFaderGroups.IndexOfByKey(&FaderGroupA);
			const int32 FaderGroupIndexB = AllFaderGroups.IndexOfByKey(&FaderGroupB);

			if (FaderGroupIndexA != FaderGroupIndexB)
			{
				return FaderGroupIndexA < FaderGroupIndexB;
			}

			const int32 IndexA = FaderGroupA.GetAllFaders().IndexOfByKey(FaderA);
			const int32 IndexB = FaderGroupB.GetAllFaders().IndexOfByKey(FaderB);

			return IndexA < IndexB;
		};

	Algo::Sort(CurrentSelectedFaders, SortSelectedFadersLambda);
	const TWeakObjectPtr<UObject> FirstFader = bReverse ? CurrentSelectedFaders.Last() : CurrentSelectedFaders[0];
	return Cast<UDMXControlConsoleFaderBase>(FirstFader);
}

TArray<UDMXControlConsoleFaderBase*> FDMXControlConsoleEditorSelection::GetSelectedFadersFromFaderGroup(UDMXControlConsoleFaderGroup* FaderGroup) const
{
	TArray<UDMXControlConsoleFaderBase*> CurrentSelectedFaders;

	if (!FaderGroup)
	{
		return CurrentSelectedFaders;
	}

	TArray<UDMXControlConsoleFaderBase*> Faders = FaderGroup->GetAllFaders();
	for (UDMXControlConsoleFaderBase* Fader : Faders)
	{
		if (!Fader)
		{
			continue;
		}

		if (!SelectedFaders.Contains(Fader))
		{
			continue;
		}

		CurrentSelectedFaders.Add(Fader);
	}

	return CurrentSelectedFaders;
}

void FDMXControlConsoleEditorSelection::UpdateMultiSelectAnchor(UClass* PreferedClass)
{
	if (!ensureMsgf(PreferedClass == UDMXControlConsoleFaderGroup::StaticClass() || PreferedClass == UDMXControlConsoleFaderBase::StaticClass(), TEXT("Invalid class when trying to update multi select anchor")))
	{
		return;
	}

	if (PreferedClass == UDMXControlConsoleFaderGroup::StaticClass() && !SelectedFaderGroups.IsEmpty())
	{
		MultiSelectAnchor = SelectedFaderGroups.Last();
	}
	else if (!SelectedFaders.IsEmpty())
	{
		MultiSelectAnchor = SelectedFaders.Last();
	}
	else if (!SelectedFaderGroups.IsEmpty())
	{
		MultiSelectAnchor = SelectedFaderGroups.Last();
	}
	else
	{
		MultiSelectAnchor = nullptr;
	}
}

#undef LOCTEXT_NAMESPACE
