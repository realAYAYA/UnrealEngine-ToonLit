// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UTBBaseCommand.h"

#include "ExecuteBindableAction.generated.h"

USTRUCT()
struct USERTOOLBOXBASICCOMMAND_API FBindableActionInfo
{
	GENERATED_BODY()
	UPROPERTY()
	FString Context;
	UPROPERTY()
	FString CommandName;
};

/**
 * 
 */
UCLASS()
class USERTOOLBOXBASICCOMMAND_API UExecuteBindableAction : public UUTBBaseCommand
{
	GENERATED_BODY()
public:
	UExecuteBindableAction() ;

	UPROPERTY(EditAnywhere,Category="Bindable Command")
	FBindableActionInfo	CommandInfo;


public:
	virtual void Execute() override;
	
};
