// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "SourceControlSettings.generated.h"

/** Serializes source control window settings. */
UCLASS(config=Editor)
class USourceControlSettings : public UObject
{
	GENERATED_BODY()

public:
	USourceControlSettings() {}

	UPROPERTY(config, EditAnywhere, Category="Source Control Changelist View")
	bool bShowAssetTypeColumn = true;

	UPROPERTY(config, EditAnywhere, Category="Source Control Changelist View")
	bool bShowAssetLastModifiedTimeColumn = true;

	UPROPERTY(config, EditAnywhere, Category="Source Control Changelist View")
	bool bShowAssetCheckedOutByColumn = true;

	UPROPERTY(EditAnywhere, config, Category="Source Control Tools")
	bool bEnableSubmitContentMenuAction = true;
};
