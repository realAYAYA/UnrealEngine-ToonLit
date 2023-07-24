// Copyright Epic Games, Inc. All Rights Reserved.


#include "ConsoleVariable.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UserToolBoxBasicCommand.h"
#include "Engine/Classes/Engine/World.h"
void UConsoleVariable::Execute()
{
	
	for (const FString& Command : ConsoleCommands)
	{
		UE_LOG(LogUserToolBoxBasicCommand, Log, TEXT("Executing Console Command \"%s\" ."), *Command);
		UKismetSystemLibrary::ExecuteConsoleCommand(GetWorld(), Command, nullptr);
	}
	
}
