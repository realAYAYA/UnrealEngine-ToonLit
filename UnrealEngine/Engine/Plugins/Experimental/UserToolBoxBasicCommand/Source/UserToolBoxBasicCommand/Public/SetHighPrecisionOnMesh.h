// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UTBBaseCommand.h"
#include "SetHighPrecisionOnMesh.generated.h"

/**
 * 
 */
UCLASS()
class USERTOOLBOXBASICCOMMAND_API USetHighPrecisionOnMesh : public UUTBBaseCommand
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite,Category="HighPrecisionCommand")
	bool bHighPrecisionTangent=true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite,Category="HighPrecisionCommand")
	bool bHighPrecisionUV=true;
	USetHighPrecisionOnMesh();
	virtual void Execute() override;
};
