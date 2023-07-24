// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UTBBaseCommand.h"
#include "PushComponentMaterialIntoMesh.generated.h"

/**
 * 
 */
UCLASS()
class USERTOOLBOXBASICCOMMAND_API UPushComponentMaterialIntoMesh : public UUTBBaseCommand
{
	GENERATED_BODY()
	public:

	void Execute() override;
	
	UPushComponentMaterialIntoMesh();
};
