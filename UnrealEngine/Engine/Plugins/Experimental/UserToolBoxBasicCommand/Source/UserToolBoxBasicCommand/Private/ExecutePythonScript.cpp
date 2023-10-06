// Copyright Epic Games, Inc. All Rights Reserved.


#include "ExecutePythonScript.h"
#include "Engine/Engine.h"

UExecutePythonScript::UExecutePythonScript()
{
	Name="Exec Python";
	Tooltip="Execute a python script";
	Category="Utility";
}

void UExecutePythonScript::Execute()
{
	FString CommandArg;
	if (!ScriptPath.FilePath.IsEmpty())
	{
		CommandArg=FString::Printf(TEXT("py \"%s\""),*ScriptPath.FilePath);
		if (!Args.IsEmpty())
		{
			CommandArg+= " " + FString::Printf(TEXT("\"%s\""),*Args);
		}
	}
	else
	{
		if (!Args.IsEmpty())
		{
			CommandArg+= "py " + Args;
		}	
	}
	
	GEngine->Exec(NULL, *CommandArg);
}
