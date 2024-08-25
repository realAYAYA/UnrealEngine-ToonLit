// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleFaderGroupRow.h"

#include "DMXControlConsoleData.h"
#include "DMXControlConsoleFaderGroup.h"


UDMXControlConsoleFaderGroup* UDMXControlConsoleFaderGroupRow::AddFaderGroup(const int32 Index)
{
	if (!ensureMsgf(Index >= 0, TEXT("Invalid index. Cannot add new fader group to '%s' correctly."), *GetName()))
	{
		return nullptr;
	}

	UDMXControlConsoleFaderGroup* FaderGroup = NewObject<UDMXControlConsoleFaderGroup>(this, NAME_None, RF_Transactional);
	FaderGroups.Insert(FaderGroup, Index);

	const UDMXControlConsoleData& ControlConsoleData = GetOwnerControlConsoleDataChecked();
	ControlConsoleData.OnFaderGroupAdded.Broadcast(FaderGroup);

	return FaderGroup;
}

UDMXControlConsoleFaderGroup* UDMXControlConsoleFaderGroupRow::AddFaderGroup(UDMXControlConsoleFaderGroup* FaderGroup, const int32 Index)
{
	if (!ensureMsgf(Index >= 0, TEXT("Invalid index. Cannot add fader group to '%s' correctly."), *GetName()))
	{
		return nullptr;
	}

	if (!FaderGroup)
	{
		return nullptr;
	}

	UDMXControlConsoleFaderGroupRow& OwnerRow = FaderGroup->GetOwnerFaderGroupRowChecked();
	if (this == &OwnerRow)
	{
		FaderGroups.Remove(FaderGroup);
	}
	else 
	{
#if WITH_EDITOR
		OwnerRow.PreEditChange(UDMXControlConsoleFaderGroupRow::StaticClass()->FindPropertyByName(UDMXControlConsoleFaderGroupRow::GetFaderGroupsPropertyName()));
#endif // WITH_EDITOR

		OwnerRow.FaderGroups.Remove(FaderGroup);
		if (OwnerRow.FaderGroups.IsEmpty())
		{
			OwnerRow.Destroy();
		}

#if WITH_EDITOR
		OwnerRow.PostEditChange();
#endif // WITH_EDITOR
	}
	
#if WITH_EDITOR
	FaderGroup->PreEditChange(nullptr);
#endif // WITH_EDITOR
	FaderGroup->Rename(nullptr, this);
	FaderGroup->SetFlags(RF_Transactional);
	FaderGroups.Insert(FaderGroup, Index);
#if WITH_EDITOR
	FaderGroup->PostEditChange();
#endif // WITH_EDITOR 

	const UDMXControlConsoleData& ControlConsoleData = GetOwnerControlConsoleDataChecked();
	ControlConsoleData.OnFaderGroupAdded.Broadcast(FaderGroup);

	return FaderGroup;
}

UDMXControlConsoleFaderGroup* UDMXControlConsoleFaderGroupRow::DuplicateFaderGroup(const int32 Index)
{
	if (!ensureMsgf(FaderGroups.IsValidIndex(Index), TEXT("Invalid index. Cannot duplicate fader group correctly.")))
	{
		return nullptr;
	}

	const UDMXControlConsoleFaderGroup* FaderGroup = FaderGroups[Index];
	if (!FaderGroup)
	{
		return nullptr;
	}

	UDMXControlConsoleFaderGroup* DuplicatedFaderGroup = DuplicateObject<UDMXControlConsoleFaderGroup>(FaderGroup, this);
	FaderGroups.Insert(DuplicatedFaderGroup, Index + 1);

	const UDMXControlConsoleData& ControlConsoleData = GetOwnerControlConsoleDataChecked();
	ControlConsoleData.OnFaderGroupAdded.Broadcast(DuplicatedFaderGroup);

	return DuplicatedFaderGroup;
}

void UDMXControlConsoleFaderGroupRow::DeleteFaderGroup(UDMXControlConsoleFaderGroup* FaderGroup)
{
	if (!ensureMsgf(FaderGroup, TEXT("Invalid fader group, cannot delete from '%s'."), *GetName()))
	{
		return;
	}

	if (!ensureMsgf(FaderGroups.Contains(FaderGroup), TEXT("'%s' fader group row is not owner of '%s'. Cannot delete fader group correctly."), *GetName(), *FaderGroup->GetName()))
	{
		return;
	}

	FaderGroups.Remove(FaderGroup);

	const UDMXControlConsoleData& ControlConsoleData = GetOwnerControlConsoleDataChecked();
	ControlConsoleData.OnFaderGroupRemoved.Broadcast(FaderGroup);

	// Destroy self when there's no more Faders left in the Group
	if (FaderGroups.IsEmpty())
	{
		Destroy();
	}
}

void UDMXControlConsoleFaderGroupRow::ClearFaderGroups()
{
	FaderGroups.Reset();
}

int32 UDMXControlConsoleFaderGroupRow::GetRowIndex() const
{
	const UDMXControlConsoleData& ControlConsoleData = GetOwnerControlConsoleDataChecked();

	const TArray<UDMXControlConsoleFaderGroupRow*> FaderGroupRows = ControlConsoleData.GetFaderGroupRows();
	return FaderGroupRows.IndexOfByKey(this);
}

UDMXControlConsoleData& UDMXControlConsoleFaderGroupRow::GetOwnerControlConsoleDataChecked() const
{
	UDMXControlConsoleData* Outer = Cast<UDMXControlConsoleData>(GetOuter());
	checkf(Outer, TEXT("Invalid outer for '%s', cannot get fader group row owner correctly."), *GetName());

	return *Outer;
}

void UDMXControlConsoleFaderGroupRow::Destroy()
{
	UDMXControlConsoleData& ControlConsoleData = GetOwnerControlConsoleDataChecked();

#if WITH_EDITOR
	ControlConsoleData.PreEditChange(UDMXControlConsoleData::StaticClass()->FindPropertyByName(UDMXControlConsoleFaderGroup::GetFaderGroupNamePropertyName()));
#endif // WITH_EDITOR

	ControlConsoleData.DeleteFaderGroupRow(this);

#if WITH_EDITOR
	ControlConsoleData.PostEditChange();
#endif // WITH_EDITOR
}
