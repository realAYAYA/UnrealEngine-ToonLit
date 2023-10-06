// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UTBBaseCommand.h"
#include "ChangeViewMode.generated.h"

/**
 * 
 */
UCLASS()
class USERTOOLBOXBASICCOMMAND_API UChangeViewMode : public UUTBBaseCommand
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="ChangeViewCommand")
	TEnumAsByte<EViewModeIndex> ViewMode=EViewModeIndex::VMI_Lit;
	virtual void Execute() override;
	UChangeViewMode();
};
