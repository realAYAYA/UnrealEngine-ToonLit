// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UTBBaseCommand.h"
#include "ConsoleVariable.generated.h"

/**
 * 
 */
UCLASS()
class USERTOOLBOXBASICCOMMAND_API UConsoleVariable : public UUTBBaseCommand
{
	GENERATED_BODY()
	public:
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Console Command")
	TArray<FString> ConsoleCommands;
	
	virtual void Execute() override;

	UConsoleVariable()
	{
		Name="Console Variable Command";
		Tooltip="This console command let you execute any console variable/command";
		Category="Utility";
	}
};
