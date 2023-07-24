// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UTBBaseCommand.h"
#include "Layers/Layer.h"
#include "AssignToLayer.generated.h"

/**
 * 
 */
UCLASS()
class USERTOOLBOXBASICCOMMAND_API UAssignToLayer : public UUTBBaseCommand
{
	GENERATED_BODY()
	public:

	void Execute() override;
	virtual FName GetLayerName(TWeakObjectPtr<ULayer> Layer);
	UAssignToLayer();
};
