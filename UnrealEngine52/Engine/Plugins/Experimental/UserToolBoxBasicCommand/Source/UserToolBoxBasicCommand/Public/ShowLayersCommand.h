// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UTBBaseCommand.h"
#include "ShowLayersCommand.generated.h"

/**
 * 
 */
UCLASS()
class USERTOOLBOXBASICCOMMAND_API UShowLayersCommand : public UUTBBaseCommand
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LayerCommand")
	TArray<FName>	Layers;

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LayerCommand")
	bool bIsolate=false;
	UShowLayersCommand();
	virtual bool DisplayParameters() override;
	virtual void Execute() override;
};
