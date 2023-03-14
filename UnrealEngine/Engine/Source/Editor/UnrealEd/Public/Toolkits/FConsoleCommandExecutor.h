// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "GlobalEditorCommonCommands.h"


/**
* Executor for Unreal console commands
*/
class UNREALED_API FConsoleCommandExecutor : public IConsoleCommandExecutor
{
public:
	static FName StaticName();
	virtual FName GetName() const override;
	virtual FText GetDisplayName() const override;
	virtual FText GetDescription() const override;
	virtual FText GetHintText() const override;
	virtual void GetAutoCompleteSuggestions(const TCHAR* Input, TArray<FString>& Out) override;
	virtual void GetExecHistory(TArray<FString>& Out) override;
	virtual bool Exec(const TCHAR* Input) override;
	virtual bool AllowHotKeyClose() const override;
	virtual bool AllowMultiLine() const override;

	virtual FInputChord GetHotKey() const override
	{
		return FGlobalEditorCommonCommands::Get().OpenConsoleCommandBox->GetActiveChord(EMultipleKeyBindingIndex::Primary).Get();
	}

	virtual FInputChord GetIterateExecutorHotKey() const override
	{
		return FGlobalEditorCommonCommands::Get().FGlobalEditorCommonCommands::Get().SelectNextConsoleExecutor->GetActiveChord(EMultipleKeyBindingIndex::Primary).Get();
	}
private:
	bool ExecInternal(const TCHAR*) const;
};
