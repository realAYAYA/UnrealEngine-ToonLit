// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConsoleVariablesAsset.h"

#include "ConsoleVariablesEditorRuntimeLog.h"
#include "Engine/Engine.h"

void UConsoleVariablesAsset::SetVariableCollectionDescription(const FString& InVariableCollectionDescription)
{
	VariableCollectionDescription = InVariableCollectionDescription;
}

TArray<FString> UConsoleVariablesAsset::GetSavedCommandsAsStringArray(bool bOnlyIncludeChecked) const
{
	TArray<FString> ReturnValue;
	
	// Make a copy because this array can change during iteration
	TArray<FConsoleVariablesEditorAssetSaveData> SavedCommandsLocal = SavedCommands; 
	for (int32 CommandIndex = 0; CommandIndex < SavedCommandsLocal.Num(); CommandIndex++)
	{
		const FConsoleVariablesEditorAssetSaveData& Command = SavedCommandsLocal[CommandIndex];
		
		if (bOnlyIncludeChecked && Command.CheckedState != ECheckBoxState::Checked)
		{
			continue;
		}
		
		ReturnValue.Add(FString::Printf(TEXT("%s %s"), *Command.CommandName, *Command.CommandValueAsString));
	}

	return ReturnValue;
}

FString UConsoleVariablesAsset::GetSavedCommandsAsCommaSeparatedString(bool bOnlyIncludeChecked) const
{
	return FString::Join(GetSavedCommandsAsStringArray(bOnlyIncludeChecked), TEXT(","));
}

void UConsoleVariablesAsset::ExecuteSavedCommands(UObject* WorldContextObject, bool bOnlyIncludeChecked) const
{
	if (const TObjectPtr<UWorld> World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		TArray<FString> Commands = GetSavedCommandsAsStringArray(bOnlyIncludeChecked);
	
		for (const FString& Command : Commands)
		{
			GEngine->Exec(World, *Command);
		}
	}
	else
	{
		UE_LOG(LogConsoleVariablesEditorRuntime, Error, TEXT("%hs: Could not get world from WorldContextObject"), __FUNCTION__);
	}
}

void UConsoleVariablesAsset::ReplaceSavedCommands(const TArray<FConsoleVariablesEditorAssetSaveData>& Replacement)
{
	SavedCommands = Replacement;
}

bool UConsoleVariablesAsset::FindSavedDataByCommandString(
	const FString InCommandString, FConsoleVariablesEditorAssetSaveData& OutValue, const ESearchCase::Type SearchCase) const
{
	for (const FConsoleVariablesEditorAssetSaveData& Command : SavedCommands)
	{
		if (Command.CommandName.TrimStartAndEnd().Equals(InCommandString.TrimStartAndEnd(), SearchCase))
		{
			OutValue = Command;
			return true;
		}
	}
	
	return false;
}

void UConsoleVariablesAsset::AddOrSetConsoleObjectSavedData(const FConsoleVariablesEditorAssetSaveData& InData)
{
	RemoveConsoleVariable(InData.CommandName);
	SavedCommands.Add(InData);
	
	// Make a copy because this array can change during iteration
	TArray<FConsoleVariablesEditorAssetSaveData> SavedCommandsLocal = SavedCommands; 
	for (int32 CommandIndex = 0; CommandIndex < SavedCommandsLocal.Num(); CommandIndex++)
	{
		FConsoleVariablesEditorAssetSaveData& SavedCommand = SavedCommandsLocal[CommandIndex];
		UE_LOG(LogConsoleVariablesEditorRuntime, VeryVerbose, TEXT("%hs: Command named '%s' at Index %i"),
			__FUNCTION__, *SavedCommand.CommandName, CommandIndex);
	}
}

bool UConsoleVariablesAsset::RemoveConsoleVariable(const FString InCommandString)
{
	FConsoleVariablesEditorAssetSaveData ExistingData;
	int32 RemoveCount = 0;
	while (FindSavedDataByCommandString(InCommandString, ExistingData, ESearchCase::IgnoreCase))
	{
		UE_LOG(LogConsoleVariablesEditorRuntime, VeryVerbose, TEXT("%hs: Removing %s from editable asset"),
			__FUNCTION__, *InCommandString);
		
		if (SavedCommands.Remove(ExistingData) > 0)
		{
			RemoveCount++;
		}
	}
	
	return RemoveCount > 0;
}

void UConsoleVariablesAsset::CopyFrom(const UConsoleVariablesAsset* InAssetToCopy)
{
	VariableCollectionDescription = InAssetToCopy->GetVariableCollectionDescription();
	SavedCommands = InAssetToCopy->GetSavedCommands();
}
