// Copyright Epic Games, Inc. All Rights Reserved.


#include "ToggleCommand.h"
#include "Engine/Selection.h"
void UToggleCommand::Execute()
{
	if (Commands.IsEmpty())
	{
		return;
	}
	if (CurrentIndex>=Commands.Num())
	{
		CurrentIndex=0;
	}
	UUTBBaseCommand* Current=Commands[CurrentIndex];
	CurrentIndex++;
	if (!IsValid(Current))
	{
		return;
	}
	Current->ExecuteCommand();
}
