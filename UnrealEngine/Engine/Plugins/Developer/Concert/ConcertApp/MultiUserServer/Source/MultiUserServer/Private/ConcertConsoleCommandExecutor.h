// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"

namespace UE::MultiUserServer
{
	/** Executes commands put into the output log */
	class FConcertConsoleCommandExecutor : public IConsoleCommandExecutor
	{
	public:
	
		static FName StaticName();

		//~ Begin IConsoleCommandExecutor Interface
		virtual FName GetName() const override;
		virtual FText GetDisplayName() const override;
		virtual FText GetDescription() const override;
		virtual FText GetHintText() const override;
		virtual void GetAutoCompleteSuggestions(const TCHAR* Input, TArray<FString>& Out) override;
		virtual void GetExecHistory(TArray<FString>& Out) override;
		virtual bool Exec(const TCHAR* Input) override;
		virtual bool AllowHotKeyClose() const override;
		virtual bool AllowMultiLine() const override;
		virtual FInputChord GetHotKey() const override;
		virtual FInputChord GetIterateExecutorHotKey() const override;
		//~ End IConsoleCommandExecutor Interface
	};
}
