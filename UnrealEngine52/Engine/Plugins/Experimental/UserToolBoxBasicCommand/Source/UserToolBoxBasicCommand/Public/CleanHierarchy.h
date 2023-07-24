// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UTBBaseCommand.h"
#include "CleanHierarchy.generated.h"

/**
 * 
 */
UCLASS()
class USERTOOLBOXBASICCOMMAND_API UCleanHierarchy : public UUTBBaseCommand
{
	GENERATED_BODY()
	public:
	UPROPERTY(EditAnywhere,Category="CleanHierarchyCommand")
	TArray<FString> DSMetadataToPreserve;
	UPROPERTY(EditAnywhere,Category="CleanHierarchyCommand")
	bool RemoveEmptyBranches;
	UPROPERTY(EditAnywhere,Category="CleanHierarchyCommand")
	bool RemoveIntermediaryActorsWithoutGeometry;
	UCleanHierarchy();
	virtual void Execute() override;
};
