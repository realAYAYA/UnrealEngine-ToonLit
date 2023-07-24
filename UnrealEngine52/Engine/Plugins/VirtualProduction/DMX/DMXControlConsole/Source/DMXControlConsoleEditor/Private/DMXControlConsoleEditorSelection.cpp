// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleEditorSelection.h"

#include "DMXControlConsole.h"
#include "DMXControlConsoleData.h"
#include "DMXControlConsoleEditorManager.h"
#include "DMXControlConsoleFaderBase.h"
#include "DMXControlConsoleFaderGroup.h"
#include "DMXControlConsoleFaderGroupRow.h"

#include "Algo/Sort.h"


#define LOCTEXT_NAMESPACE "DMXControlConsoleEditorSelection"

FDMXControlConsoleEditorSelection::FDMXControlConsoleEditorSelection(const TSharedRef<FDMXControlConsoleEditorManager>& InControlConsoleManager)
{
	WeakControlConsoleManager = InControlConsoleManager;
}

void FDMXControlConsoleEditorSelection::AddToSelection(UDMXControlConsoleFaderGroup* FaderGroup)
{
	if (FaderGroup)
	{
		SelectedFaderGroups.AddUnique(FaderGroup);

		UpdateMultiSelectAnchor(UDMXControlConsoleFaderGroup::StaticClass());

		OnSelectionChanged.Broadcast();
	}
}

void FDMXControlConsoleEditorSelection::AddToSelection(UDMXControlConsoleFaderBase* Fader)
{
	if (Fader)
	{
		SelectedFaders.AddUnique(Fader);

		UDMXControlConsoleFaderGroup& FaderGroup = Fader->GetOwnerFaderGroupChecked();
		SelectedFaderGroups.AddUnique(&FaderGroup);

		UpdateMultiSelectAnchor(UDMXControlConsoleFaderBase::StaticClass());

		OnSelectionChanged.Broadcast();
	}
}

void FDMXControlConsoleEditorSelection::RemoveFromSelection(UDMXControlConsoleFaderGroup* FaderGroup)
{
	if (FaderGroup && SelectedFaderGroups.Contains(FaderGroup))
	{
		ClearFadersSelection(FaderGroup);
		SelectedFaderGroups.Remove(FaderGroup);

		UpdateMultiSelectAnchor(UDMXControlConsoleFaderGroup::StaticClass());

		OnSelectionChanged.Broadcast();
	}
}

void FDMXControlConsoleEditorSelection::RemoveFromSelection(UDMXControlConsoleFaderBase* Fader)
{
	if (Fader && SelectedFaders.Contains(Fader))
	{
		SelectedFaders.Remove(Fader);

		UpdateMultiSelectAnchor(UDMXControlConsoleFaderBase::StaticClass());

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

	SelectedFaderGroups.Remove(nullptr);
	SelectedFaders.Remove(nullptr);

	// Normal selection if nothing is selected or there's no valid anchor
	if (!MultiSelectAnchor.IsValid() ||
		(SelectedFaderGroups.IsEmpty() && SelectedFaders.IsEmpty()))
	{
		if (UDMXControlConsoleFaderGroup* FaderGroup = Cast<UDMXControlConsoleFaderGroup>(FaderOrFaderGroupObject))
		{
			AddToSelection(FaderGroup);
		}
		else if (UDMXControlConsoleFaderBase* Fader = Cast<UDMXControlConsoleFaderBase>(FaderOrFaderGroupObject))
		{
			AddToSelection(Fader);
		}
		return;
	}

	TArray<UObject*> FadersAndFaderGroups;
	UDMXControlConsoleData* EditorConsoleData = FDMXControlConsoleEditorManager::Get().GetEditorConsoleData();
	for (UDMXControlConsoleFaderGroup* AnyFaderGroup : EditorConsoleData->GetAllFaderGroups())
	{
		FadersAndFaderGroups.AddUnique(AnyFaderGroup);
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
			SelectedFaderGroups.AddUnique(FaderGroupToSelect);
		}
		else if (UDMXControlConsoleFaderBase* FaderToSelect = Cast<UDMXControlConsoleFaderBase>(FadersAndFaderGroups[IndexToSelect]))
		{
			SelectedFaders.AddUnique(FaderToSelect);
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

	const UDMXControlConsoleData* EditorConsoleData = FDMXControlConsoleEditorManager::Get().GetEditorConsoleData();
	if (!EditorConsoleData)
	{
		return;
	}

	const TArray<UDMXControlConsoleFaderGroup*> AllFaderGroups = EditorConsoleData->GetAllFaderGroups();
	if (AllFaderGroups.Num() <= 1)
	{
		return;
	}

	const int32 Index = AllFaderGroups.IndexOfByKey(FaderGroup);

	int32 NewIndex = Index - 1;
	if (!AllFaderGroups.IsValidIndex(NewIndex))
	{
		NewIndex = Index + 1;
	}

	UDMXControlConsoleFaderGroup* NewSelectedFaderGroup = AllFaderGroups.IsValidIndex(NewIndex) ? AllFaderGroups[NewIndex] : nullptr;
	AddToSelection(NewSelectedFaderGroup);
	return;
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

void FDMXControlConsoleEditorSelection::ClearFadersSelection(UDMXControlConsoleFaderGroup* FaderGroup)
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

	OnSelectionChanged.Broadcast();
}

void FDMXControlConsoleEditorSelection::ClearSelection()
{
	SelectedFaderGroups.Reset();
	SelectedFaders.Reset();

	OnSelectionChanged.Broadcast();
}

UDMXControlConsoleFaderGroup* FDMXControlConsoleEditorSelection::GetFirstSelectedFaderGroup() const
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
	return Cast<UDMXControlConsoleFaderGroup>(CurrentSelectedFaderGroups[0]);
}

UDMXControlConsoleFaderBase* FDMXControlConsoleEditorSelection::GetFirstSelectedFader() const
{
	const UDMXControlConsoleData* EditorConsoleData = FDMXControlConsoleEditorManager::Get().GetEditorConsoleData();
	if (!EditorConsoleData)
	{
		return nullptr;
	}

	const TArray<UDMXControlConsoleFaderGroup*> AllFaderGroups = EditorConsoleData->GetAllFaderGroups();
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
	return Cast<UDMXControlConsoleFaderBase>(CurrentSelectedFaders[0]);
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
