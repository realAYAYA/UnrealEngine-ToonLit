// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/EngineTypes.h"

#include "USDProjectSettings.generated.h"

UENUM( BlueprintType )
enum class EUsdSaveDialogBehavior : uint8
{
	NeverSave,
	AlwaysSave,
	ShowPrompt
};

UCLASS(config=Editor, meta=(DisplayName=USDImporter), MinimalAPI)
class UUsdProjectSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// Additional paths to check for USD plugins
	UPROPERTY( config, EditAnywhere, Category = USD )
	TArray<FDirectoryPath> AdditionalPluginDirectories;

	// Material purposes to show on drop-downs in addition to the standard "preview" and "full"
	UPROPERTY( config, EditAnywhere, Category = USD )
	TArray<FName> AdditionalMaterialPurposes;

	UPROPERTY( config, EditAnywhere, Category = "USD|Dialogs" )
	bool bShowConfirmationWhenClearingLayers = true;

	UPROPERTY( config, EditAnywhere, Category = "USD|Dialogs" )
	bool bShowConfirmationWhenMutingDirtyLayers = true;

	// Whether to show the warning dialog when authoring opinions that could have no effect on the composed stage
	UPROPERTY( config, EditAnywhere, Category = "USD|Dialogs" )
	bool bShowOverriddenOpinionsWarning = true;

	// Whether to show a warning whenever the "Duplicate All Local Layer Specs" option is picked, and the duplicated
	// prim has some specs outside the local layer stack that will not be duplicated.
	UPROPERTY( config, EditAnywhere, Category = "USD|Dialogs" )
	bool bShowWarningOnIncompleteDuplication = true;

	// Whether to display the pop up dialog asking what to do about dirty USD layers when saving the UE level
	UPROPERTY(config, EditAnywhere, Category = "USD|Dialogs" )
	EUsdSaveDialogBehavior ShowSaveLayersDialogWhenSaving = EUsdSaveDialogBehavior::ShowPrompt;

	// Whether to display the pop up dialog asking what to do about dirty USD layers when closing USD stages
	UPROPERTY(config, EditAnywhere, Category = "USD|Dialogs" )
	EUsdSaveDialogBehavior ShowSaveLayersDialogWhenClosing = EUsdSaveDialogBehavior::ShowPrompt;
};