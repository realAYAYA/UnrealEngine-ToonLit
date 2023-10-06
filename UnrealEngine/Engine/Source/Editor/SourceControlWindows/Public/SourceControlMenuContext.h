// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "ISourceControlChangelist.h"

#include "SourceControlMenuContext.generated.h"

/**
 * Source control window menu context providing information for menu extenders.
 */
UCLASS()
class SOURCECONTROLWINDOWS_API USourceControlMenuContext : public UObject
{
	GENERATED_BODY()
public:

	USourceControlMenuContext(const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer)
	{
		
	}

	UPROPERTY()
	TArray<FString> SelectedFiles;

	FSourceControlChangelistPtr SelectedChangelist;
};
