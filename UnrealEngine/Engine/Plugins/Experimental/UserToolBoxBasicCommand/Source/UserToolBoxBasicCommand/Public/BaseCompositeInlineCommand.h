// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UTBBaseCommand.h"
#include "BaseCompositeInlineCommand.generated.h"

/**
 * 
 */
UCLASS(Abstract, EditInlineNew)
class USERTOOLBOXBASICCOMMAND_API UBaseCompositeInlineCommand : public UUTBBaseCommand
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Instanced,Category="CompositeCommand")
	TArray<TObjectPtr<UUTBBaseCommand>>		Commands;
	
	UBaseCompositeInlineCommand()
	{
		Name="Toggle Command";
		Tooltip="This command let you execute sequentially some already registered command, one at a time (placeholder section is a good place to register these commands)";
		Category="Builtin";
	}

	

};
