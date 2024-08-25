// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleEditorGlobalLayoutRow.h"

#include "Controllers/DMXControlConsoleFaderGroupController.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutBase.h"


#define LOCTEXT_NAMESPACE "DMXControlConsoleEditorGlobalLayoutRow"

UDMXControlConsoleFaderGroupController* UDMXControlConsoleEditorGlobalLayoutRow::CreateFaderGroupController(UDMXControlConsoleFaderGroup* InFaderGroup, const FString& ControllerName, const int32 Index)
{
	if (!InFaderGroup)
	{
		return nullptr;
	}

	const TArray<UDMXControlConsoleFaderGroup*> FaderGroupAsArray = { InFaderGroup };
	UDMXControlConsoleFaderGroupController* FaderGroupController = CreateFaderGroupController(FaderGroupAsArray, ControllerName, Index);
	return FaderGroupController;
}

UDMXControlConsoleFaderGroupController* UDMXControlConsoleEditorGlobalLayoutRow::CreateFaderGroupController(const TArray<UDMXControlConsoleFaderGroup*> InFaderGroups, const FString& ControllerName, const int32 Index)
{
	if (InFaderGroups.IsEmpty() || Index > FaderGroupControllers.Num())
	{
		return nullptr;
	}

	UDMXControlConsoleFaderGroupController* FaderGroupController = NewObject<UDMXControlConsoleFaderGroupController>(this, NAME_None, RF_Transactional);
	FaderGroupController->Possess(InFaderGroups);
		
	const FString NewName = ControllerName.IsEmpty() ? FString::FromInt(FaderGroupControllers.Num() + 1) : ControllerName;
	FaderGroupController->SetUserName(NewName);

	const int32 ValidIndex = Index < 0 ? FaderGroupControllers.Num() : Index;
	FaderGroupControllers.Insert(FaderGroupController, ValidIndex);

	return FaderGroupController;
}

void UDMXControlConsoleEditorGlobalLayoutRow::DeleteFaderGroupController(UDMXControlConsoleFaderGroupController* FaderGroupController)
{
	if (!ensureMsgf(FaderGroupController, TEXT("Invalid fader group controller, cannot delete from '%s'."), *GetName()))
	{
		return;
	}

	if (!ensureMsgf(FaderGroupControllers.Contains(FaderGroupController), TEXT("'%s' layout row is not owner of '%s'. Cannot delete fader group controller correctly."), *GetName(), *FaderGroupController->GetUserName()))
	{
		return;
	}

	FaderGroupControllers.Remove(FaderGroupController);
}

int32 UDMXControlConsoleEditorGlobalLayoutRow::GetRowIndex() const
{
	const UDMXControlConsoleEditorGlobalLayoutBase& OwnerLayout = GetOwnerLayoutChecked();

	const TArray<UDMXControlConsoleEditorGlobalLayoutRow*> LayoutRows = OwnerLayout.GetLayoutRows();
	return LayoutRows.IndexOfByKey(this);
}

UDMXControlConsoleEditorGlobalLayoutBase& UDMXControlConsoleEditorGlobalLayoutRow::GetOwnerLayoutChecked() const
{
	UDMXControlConsoleEditorGlobalLayoutBase* Outer = Cast<UDMXControlConsoleEditorGlobalLayoutBase>(GetOuter());
	checkf(Outer, TEXT("Invalid outer for '%s', cannot get layout row owner correctly."), *GetName());

	return *Outer;
}

UDMXControlConsoleFaderGroupController* UDMXControlConsoleEditorGlobalLayoutRow::GetFaderGroupControllerAt(int32 Index) const
{ 
	return FaderGroupControllers.IsValidIndex(Index) ? FaderGroupControllers[Index].Get() : nullptr;
}

int32 UDMXControlConsoleEditorGlobalLayoutRow::GetIndex(const UDMXControlConsoleFaderGroupController* FaderGroupController) const
{
	return FaderGroupControllers.IndexOfByKey(FaderGroupController);
}

#undef LOCTEXT_NAMESPACE
