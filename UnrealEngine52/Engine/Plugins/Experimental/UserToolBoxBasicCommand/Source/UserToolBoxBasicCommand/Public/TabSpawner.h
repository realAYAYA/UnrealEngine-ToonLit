// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UTBBaseCommand.h"
#include "TabSpawner.generated.h"

/**
 * 
 */
UCLASS()
class USERTOOLBOXBASICCOMMAND_API UTabSpawner : public UUTBBaseCommand
{
	GENERATED_BODY()
	UTabSpawner();
	UPROPERTY(EditAnywhere,Category="TabSpawnerCommand")
	FName TabName;
public:
	virtual void Execute() override;
};
