// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleFaderGroupRow.h"

#include "DMXControlConsoleData.h"
#include "DMXControlConsoleFaderGroup.h"


UDMXControlConsoleFaderGroup* UDMXControlConsoleFaderGroupRow::AddFaderGroup(const int32 Index = 0)
{
	if (!ensureMsgf(Index >= 0, TEXT("Invalid index. Cannot add new fader group to '%s' correctly."), *GetName()))
	{
		return nullptr;
	}

	UDMXControlConsoleFaderGroup* FaderGroup = NewObject<UDMXControlConsoleFaderGroup>(this, NAME_None, RF_Transactional);
	FaderGroups.Insert(FaderGroup, Index);
	return FaderGroup;
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
