// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UTBBaseCommand.h"
#include "ZoomAll.generated.h"

/**
 * 
 */
UCLASS()
class USERTOOLBOXBASICCOMMAND_API UZoomAll : public UUTBBaseCommand
{
	GENERATED_BODY()
	public:
	UZoomAll();
	virtual void Execute() override;
	
};
