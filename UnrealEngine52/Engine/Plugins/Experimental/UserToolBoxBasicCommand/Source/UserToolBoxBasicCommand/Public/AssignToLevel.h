// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UTBBaseCommand.h"
#include "AssignToLevel.generated.h"

/**
 * 
 */
UCLASS()
class USERTOOLBOXBASICCOMMAND_API UAssignToLevel : public UUTBBaseCommand
{
	GENERATED_BODY()
	public:

	virtual void Execute() override;
	UAssignToLevel();
};
