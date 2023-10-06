// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleEditorGlobalLayoutDefault.h"

#include "DMXControlConsoleData.h"
#include "DMXControlConsoleFaderGroup.h"
#include "Library/DMXEntity.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXLibrary.h"
#include "Models/DMXControlConsoleEditorModel.h"


#define LOCTEXT_NAMESPACE "DMXControlConsoleEditorGlobalLayoutDefault"

void UDMXControlConsoleEditorGlobalLayoutDefault::Register()
{
	if (!ensureMsgf(!bIsRegistered, TEXT("Layout already registered to dmx library delegates.")))
	{
		return;
	}

	if (!UDMXLibrary::GetOnEntitiesRemoved().IsBoundToObject(this))
	{
		UDMXLibrary::GetOnEntitiesRemoved().AddUObject(this, &UDMXControlConsoleEditorGlobalLayoutDefault::OnFixturePatchRemovedFromLibrary);
	}

	const UDMXControlConsoleEditorModel* EditorModel = GetDefault<UDMXControlConsoleEditorModel>();
	UDMXControlConsoleData* EditorConsoleData = EditorModel->GetEditorConsoleData();
	if (EditorConsoleData && !EditorConsoleData->GetOnFaderGroupAdded().IsBoundToObject(this))
	{
		EditorConsoleData->GetOnFaderGroupAdded().AddUObject(this, &UDMXControlConsoleEditorGlobalLayoutDefault::OnFaderGroupAddedToData, EditorConsoleData);
	}

	bIsRegistered = true;
}

void UDMXControlConsoleEditorGlobalLayoutDefault::Unregister()
{
	if (!ensureMsgf(bIsRegistered, TEXT("Layout already unregistered from dmx library delegates.")))
	{
		return;
	}

	UDMXLibrary::GetOnEntitiesRemoved().RemoveAll(this);

	const UDMXControlConsoleEditorModel* EditorModel = GetDefault<UDMXControlConsoleEditorModel>();
	UDMXControlConsoleData* EditorConsoleData = EditorModel->GetEditorConsoleData();
	if (EditorConsoleData)
	{
		EditorConsoleData->GetOnFaderGroupAdded().RemoveAll(this);
	}

	bIsRegistered = false;
}

void UDMXControlConsoleEditorGlobalLayoutDefault::AddToActiveFaderGroups(UDMXControlConsoleFaderGroup* FaderGroup)
{
	if (FaderGroup && !ActiveFaderGroups.Contains(FaderGroup))
	{
		ActiveFaderGroups.Add(FaderGroup);
	}
}

void UDMXControlConsoleEditorGlobalLayoutDefault::RemoveFromActiveFaderGroups(UDMXControlConsoleFaderGroup* FaderGroup)
{
	if (FaderGroup && ActiveFaderGroups.Contains(FaderGroup))
	{
		ActiveFaderGroups.Remove(FaderGroup);
	}
}

void UDMXControlConsoleEditorGlobalLayoutDefault::SetActiveFaderGroupsInLayout(bool bActive)
{
	const TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>> AllFaderGroups = GetAllFaderGroups();
	for (const TWeakObjectPtr<UDMXControlConsoleFaderGroup>& FaderGroup : AllFaderGroups)
	{
		if (!FaderGroup.IsValid())
		{
			continue;
		}

		FaderGroup->Modify();
		if (ActiveFaderGroups.Contains(FaderGroup))
		{
			FaderGroup->SetIsActive(bActive);
		}
		else
		{
			FaderGroup->SetIsActive(!bActive);
		}
	}
}

void UDMXControlConsoleEditorGlobalLayoutDefault::GenerateLayoutByControlConsoleData(const UDMXControlConsoleData* ControlConsoleData)
{
	Super::GenerateLayoutByControlConsoleData(ControlConsoleData);

	// Default Layout can't contain not patched Fader Groups
	CleanLayoutFromUnpatchedFaderGroups();
}

void UDMXControlConsoleEditorGlobalLayoutDefault::BeginDestroy()
{
	Super::BeginDestroy();

	ensureMsgf(!bIsRegistered, TEXT("Layout still registered to dmx library delegates before being destroyed."));
}

void UDMXControlConsoleEditorGlobalLayoutDefault::OnFixturePatchRemovedFromLibrary(UDMXLibrary* Library, TArray<UDMXEntity*> Entities)
{
	if (Entities.IsEmpty())
	{
		return;
	}

	const TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>> AllFaderGroups = GetAllFaderGroups();
	for (const TWeakObjectPtr<UDMXControlConsoleFaderGroup>& FaderGroup : AllFaderGroups)
	{
		if (!FaderGroup.IsValid())
		{
			continue;
		}

		const UDMXEntityFixturePatch* FixturePatch = FaderGroup->GetFixturePatch();
		if(!FixturePatch || Entities.Contains(FixturePatch))
		{
			RemoveFromLayout(FaderGroup.Get());
		}
	}

	ClearEmptyLayoutRows();
}

void UDMXControlConsoleEditorGlobalLayoutDefault::OnFaderGroupAddedToData(const UDMXControlConsoleFaderGroup* FaderGroup, UDMXControlConsoleData* ControlConsoleData)
{
	if (FaderGroup && FaderGroup->HasFixturePatch())
	{
		Modify();
		GenerateLayoutByControlConsoleData(ControlConsoleData);
	}
}

void UDMXControlConsoleEditorGlobalLayoutDefault::CleanLayoutFromUnpatchedFaderGroups()
{
	const TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>> AllFaderGroups = GetAllFaderGroups();
	for (const TWeakObjectPtr<UDMXControlConsoleFaderGroup>& FaderGroup : AllFaderGroups)
	{
		if (FaderGroup.IsValid() && !FaderGroup->HasFixturePatch())
		{
			RemoveFromLayout(FaderGroup.Get());
		}
	}

	ClearEmptyLayoutRows();
}

#undef LOCTEXT_NAMESPACE
