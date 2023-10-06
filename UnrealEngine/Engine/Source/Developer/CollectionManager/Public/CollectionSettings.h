// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettings.h"
#include "CollectionSettings.generated.h"

UCLASS(minimalapi, config=EditorSettings)
class UCollectionSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:

#if WITH_EDITOR
	FText GetSectionText() const override;
	FText GetSectionDescription() const override;
#endif
	
	/** When enabled, Shared and Private collections will automatically commit their changes to source control */
	UPROPERTY(config, EditAnywhere, Category=Collections, meta = (DisplayName="Auto-Commit to SourceControl on Save"))
	bool bAutoCommitOnSave = true;
};