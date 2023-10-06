// Copyright Epic Games, Inc. All Rights Reserved.

#include "Layouts/DMXControlConsoleEditorGlobalLayoutBase.h"

#include "Algo/Find.h"
#include "DMXControlConsoleData.h"
#include "DMXControlConsoleEditorGlobalLayoutRow.h"
#include "DMXControlConsoleFaderGroup.h"
#include "DMXControlConsoleFaderGroupRow.h"
#include "Layouts/DMXControlConsoleEditorLayouts.h"


#define LOCTEXT_NAMESPACE "DMXControlConsoleEditorGlobalLayoutBase"

void UDMXControlConsoleEditorGlobalLayoutBase::AddToLayout(UDMXControlConsoleFaderGroup* FaderGroup, const int32 RowIndex, const int32 ColumnIndex)
{
	if (!LayoutRows.IsValidIndex(RowIndex))
	{
		return;
	}

	UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow = LayoutRows[RowIndex];
	if (LayoutRow)
	{
		LayoutRow->Modify();
		LayoutRow->AddToLayoutRow(FaderGroup, ColumnIndex);
	}
}

void UDMXControlConsoleEditorGlobalLayoutBase::AddToLayout(UDMXControlConsoleFaderGroup* FaderGroup, const int32 RowIndex)
{
	if (!LayoutRows.IsValidIndex(RowIndex))
	{
		return;
	}

	UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow = LayoutRows[RowIndex];
	if (LayoutRow)
	{
		LayoutRow->Modify();
		LayoutRow->AddToLayoutRow(FaderGroup);
	}
}

UDMXControlConsoleEditorGlobalLayoutRow* UDMXControlConsoleEditorGlobalLayoutBase::AddNewRowToLayout()
{
	UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow = NewObject<UDMXControlConsoleEditorGlobalLayoutRow>(this, NAME_None, RF_Transactional);
	LayoutRows.Add(LayoutRow);

	return LayoutRow;
}

UDMXControlConsoleEditorGlobalLayoutRow* UDMXControlConsoleEditorGlobalLayoutBase::AddNewRowToLayout(const int32 RowIndex)
{
	if (RowIndex < 0 || RowIndex > LayoutRows.Num())
	{
		return nullptr;
	}

	UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow = NewObject<UDMXControlConsoleEditorGlobalLayoutRow>(this, NAME_None, RF_Transactional);
	LayoutRows.Insert(LayoutRow, RowIndex);

	return LayoutRow;
}

void UDMXControlConsoleEditorGlobalLayoutBase::RemoveFromLayout(const UDMXControlConsoleFaderGroup* FaderGroup)
{
	if (!FaderGroup)
	{
		return;
	}

	const int32 RowIndex = GetFaderGroupRowIndex(FaderGroup);
	const int32 ColumnIndex = GetFaderGroupColumnIndex(FaderGroup);
	RemoveFromLayout(RowIndex, ColumnIndex);
}

void UDMXControlConsoleEditorGlobalLayoutBase::RemoveFromLayout(const int32 RowIndex, const int32 ColumnIndex)
{
	if (!LayoutRows.IsValidIndex(RowIndex))
	{
		return;
	}

	UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow = LayoutRows[RowIndex];
	if (LayoutRow)
	{
		LayoutRow->Modify();
		LayoutRow->RemoveFromLayoutRow(ColumnIndex);
	}
}

UDMXControlConsoleEditorGlobalLayoutRow* UDMXControlConsoleEditorGlobalLayoutBase::GetLayoutRow(const UDMXControlConsoleFaderGroup* FaderGroup) const
{
	const int32 RowIndex = GetFaderGroupRowIndex(FaderGroup);
	return GetLayoutRow(RowIndex);
}

UDMXControlConsoleEditorGlobalLayoutRow* UDMXControlConsoleEditorGlobalLayoutBase::GetLayoutRow(const int32 RowIndex) const
{
	return LayoutRows.IsValidIndex(RowIndex) ? LayoutRows[RowIndex] : nullptr;
}

TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>> UDMXControlConsoleEditorGlobalLayoutBase::GetAllFaderGroups() const
{
	TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>> AllFaderGroups;
	for (const UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow : LayoutRows)
	{
		if (LayoutRow)
		{
			AllFaderGroups.Append(LayoutRow->GetFaderGroups());
		}
	}

	return AllFaderGroups;
}

TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>> UDMXControlConsoleEditorGlobalLayoutBase::GetAllActiveFaderGroups() const
{
	TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>> AllActiveFaderGroups = GetAllFaderGroups();
	AllActiveFaderGroups.RemoveAll([](const TWeakObjectPtr<UDMXControlConsoleFaderGroup>& FaderGroup)
		{
			return FaderGroup.IsValid() && !FaderGroup->IsActive();
		});

	return AllActiveFaderGroups;
}

UDMXControlConsoleFaderGroup* UDMXControlConsoleEditorGlobalLayoutBase::GetFaderGroupAt(const int32 RowIndex, const int32 ColIndex) const
{
	if (LayoutRows.IsValidIndex(RowIndex))
	{
		const UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow = LayoutRows[RowIndex];
		return IsValid(LayoutRow) ? LayoutRow->GetFaderGroupAt(ColIndex) : nullptr;
	}

	return nullptr;
}

int32 UDMXControlConsoleEditorGlobalLayoutBase::GetFaderGroupRowIndex(const UDMXControlConsoleFaderGroup* FaderGroup) const
{
	if (!FaderGroup)
	{
		return INDEX_NONE;
	}

	for (const UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow : LayoutRows)
	{
		if (LayoutRow &&
			LayoutRow->GetFaderGroups().Contains(FaderGroup))
		{
			return LayoutRows.IndexOfByKey(LayoutRow);
		}
	}

	return INDEX_NONE;
}

int32 UDMXControlConsoleEditorGlobalLayoutBase::GetFaderGroupColumnIndex(const UDMXControlConsoleFaderGroup* FaderGroup) const
{
	if (!FaderGroup)
	{
		return INDEX_NONE;
	}

	for (const UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow : LayoutRows)
	{
		if (LayoutRow && 
			LayoutRow->GetFaderGroups().Contains(FaderGroup))
		{
			return LayoutRow->GetIndex(FaderGroup);
		}
	}

	return INDEX_NONE;
}

void UDMXControlConsoleEditorGlobalLayoutBase::SetLayoutMode(const EDMXControlConsoleLayoutMode NewLayoutMode)
{
	if (LayoutMode == NewLayoutMode)
	{
		return;
	}

	const UDMXControlConsoleEditorLayouts* OwnerEditorLayouts = Cast<UDMXControlConsoleEditorLayouts>(GetOuter());
	if(!ensureMsgf(OwnerEditorLayouts, TEXT("Invalid outer for '%s', cannot set layout mode correctly."), *GetName()))
	{
		return;
	}

	LayoutMode = NewLayoutMode;
	OwnerEditorLayouts->OnLayoutModeChanged.Broadcast();
}

void UDMXControlConsoleEditorGlobalLayoutBase::SetActiveFaderGroupsInLayout(bool bActive)
{
	const TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>> AllFaderGroups = GetAllFaderGroups();
	for (const TWeakObjectPtr<UDMXControlConsoleFaderGroup>& FaderGroup : AllFaderGroups)
	{
		if (FaderGroup.IsValid())
		{
			FaderGroup->Modify();
			FaderGroup->SetIsActive(bActive);
		}
	}
}

bool UDMXControlConsoleEditorGlobalLayoutBase::ContainsFaderGroup(const UDMXControlConsoleFaderGroup* FaderGroup) const
{
	return GetFaderGroupRowIndex(FaderGroup) != INDEX_NONE;
}

void UDMXControlConsoleEditorGlobalLayoutBase::GenerateLayoutByControlConsoleData(const UDMXControlConsoleData* ControlConsoleData)
{
	if (!ControlConsoleData)
	{
		return;
	}

	LayoutRows.Reset(LayoutRows.Num());

	const TArray<UDMXControlConsoleFaderGroupRow*> FaderGroupRows = ControlConsoleData->GetFaderGroupRows();
	for (const UDMXControlConsoleFaderGroupRow* FaderGroupRow : FaderGroupRows)
	{
		if (!FaderGroupRow)
		{
			continue;
		}

		UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow = NewObject<UDMXControlConsoleEditorGlobalLayoutRow>(this, NAME_None, RF_Transactional);
		for (UDMXControlConsoleFaderGroup* FaderGroup : FaderGroupRow->GetFaderGroups())
		{
			if (FaderGroup)
			{
				LayoutRow->Modify();
				LayoutRow->AddToLayoutRow(FaderGroup);
			}
		}

		LayoutRows.Add(LayoutRow);
	}
}

UDMXControlConsoleFaderGroup* UDMXControlConsoleEditorGlobalLayoutBase::FindFaderGroupByFixturePatch(const UDMXEntityFixturePatch* InFixturePatch) const
{
	if (InFixturePatch)
	{
		const TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>> AllFaderGroups = GetAllFaderGroups();
		const TWeakObjectPtr<UDMXControlConsoleFaderGroup>* FaderGroupPtr = Algo::FindByPredicate(AllFaderGroups, [InFixturePatch](const TWeakObjectPtr<UDMXControlConsoleFaderGroup> FaderGroup)
			{
				return FaderGroup.IsValid() && FaderGroup->GetFixturePatch() == InFixturePatch;
			});

		return FaderGroupPtr ? FaderGroupPtr->Get() : nullptr;
	}

	return nullptr;
}

void UDMXControlConsoleEditorGlobalLayoutBase::ClearAll()
{
	LayoutRows.Reset();
}

void UDMXControlConsoleEditorGlobalLayoutBase::ClearAllPatchedFaderGroups()
{
	for (UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow : LayoutRows)
	{
		if (!LayoutRow)
		{
			continue;
		}

		const TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>> FaderGroups = LayoutRow->GetFaderGroups();
		for (const TWeakObjectPtr<UDMXControlConsoleFaderGroup>& FaderGroup : FaderGroups)
		{
			if (FaderGroup.IsValid() &&
				FaderGroup->HasFixturePatch())
			{
				LayoutRow->Modify();
				LayoutRow->RemoveFromLayoutRow(FaderGroup.Get());
			}
		}
	}
}

void UDMXControlConsoleEditorGlobalLayoutBase::ClearEmptyLayoutRows()
{
	LayoutRows.RemoveAll([](const UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow)
		{
			return LayoutRow && LayoutRow->GetFaderGroups().IsEmpty();
		});
}

#undef LOCTEXT_NAMESPACE
