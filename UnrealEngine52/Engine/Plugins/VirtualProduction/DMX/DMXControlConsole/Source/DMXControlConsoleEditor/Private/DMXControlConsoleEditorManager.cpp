// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleEditorManager.h"

#include "DMXControlConsole.h"
#include "DMXControlConsoleData.h"
#include "DMXControlConsoleEditorSelection.h"
#include "Models/DMXControlConsoleEditorModel.h"

#include "ScopedTransaction.h"
#include "Misc/CoreDelegates.h"


#define LOCTEXT_NAMESPACE "DMXControlConsoleEditorManager"

TSharedPtr<FDMXControlConsoleEditorManager> FDMXControlConsoleEditorManager::Instance;

FDMXControlConsoleEditorManager::~FDMXControlConsoleEditorManager()
{
	FCoreDelegates::OnEnginePreExit.RemoveAll(this);
}

FDMXControlConsoleEditorManager& FDMXControlConsoleEditorManager::Get()
{
	if (!Instance.IsValid())
	{
		Instance = MakeShareable(new FDMXControlConsoleEditorManager());
	}
	checkf(Instance.IsValid(), TEXT(" DMX Control Console Manager instance is null."));

	return *Instance.Get();
}

UDMXControlConsole* FDMXControlConsoleEditorManager::GetEditorConsole() const
{
	UDMXControlConsoleEditorModel* EditorConsoleModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
	return EditorConsoleModel->GetEditorConsole();
}

UDMXControlConsoleData* FDMXControlConsoleEditorManager::GetEditorConsoleData() const
{
	if (UDMXControlConsole* EditorConsole = GetEditorConsole())
	{
		return EditorConsole->GetControlConsoleData();
	}

	return nullptr;
}

TSharedRef<FDMXControlConsoleEditorSelection> FDMXControlConsoleEditorManager::GetSelectionHandler()
{
	if (!SelectionHandler.IsValid())
	{
		SelectionHandler = MakeShareable(new FDMXControlConsoleEditorSelection(AsShared()));
	}

	return SelectionHandler.ToSharedRef();
}

void FDMXControlConsoleEditorManager::SendDMX()
{
	UDMXControlConsoleData* EditorConsoleData = GetEditorConsoleData();
	if (ensureMsgf(EditorConsoleData, TEXT("Invalid Editor Control Console Data, can't send DMX correctly.")))
	{
		EditorConsoleData->StartSendingDMX();
	}
}

void FDMXControlConsoleEditorManager::StopDMX()
{
	UDMXControlConsoleData* EditorConsoleData = GetEditorConsoleData();
	if (ensureMsgf(EditorConsoleData, TEXT("Invalid Editor Control Console Data, can't stop DMX correctly.")))
	{
		EditorConsoleData->StopSendingDMX();
	}
}

bool FDMXControlConsoleEditorManager::IsSendingDMX() const
{
	UDMXControlConsoleData* EditorConsoleData = GetEditorConsoleData();
	if (ensureMsgf(EditorConsoleData, TEXT("Invalid Editor Control Console Data, cannot deduce if it is sending DMX.")))
	{
		return EditorConsoleData->IsSendingDMX();
	}
	return false;
}

void FDMXControlConsoleEditorManager::ClearAll()
{
	UDMXControlConsoleData* EditorConsoleData = GetEditorConsoleData();
	if (ensureMsgf(EditorConsoleData, TEXT("Invalid Editor Console Data, cannot clear all its children.")))
	{
		SelectionHandler->ClearSelection();

		const FScopedTransaction ClearAllTransaction(LOCTEXT("ClearAllTransaction", "Clear All"));
		EditorConsoleData->Modify();

		EditorConsoleData->Reset();
	}
}

FDMXControlConsoleEditorManager::FDMXControlConsoleEditorManager()
{
	FCoreDelegates::OnEnginePreExit.AddRaw(this, &FDMXControlConsoleEditorManager::OnEnginePreExit);
}

void FDMXControlConsoleEditorManager::OnEnginePreExit()
{
	UDMXControlConsoleData* EditorConsoleData = GetEditorConsoleData();
	if (EditorConsoleData)
	{
		StopDMX();
	}

	Instance.Reset();
}

#undef LOCTEXT_NAMESPACE
