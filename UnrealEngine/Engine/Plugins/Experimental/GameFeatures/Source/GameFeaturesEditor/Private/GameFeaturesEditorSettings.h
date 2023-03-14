// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettings.h"
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/EngineTypes.h"
#include "GameFeaturesEditorSettings.generated.h"

class UGameFeatureData;

/*
* Data for specifying a usable plugin template. 
*	-Plugin templates are a folder/file structure that are duplicated and renamed
*	 by the plugin creation wizard to easily create new plugins with a standard
*	 format.
* See PluginUtils.h for more information.
*/
USTRUCT()
struct FPluginTemplateData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = PluginTemplate, meta = (RelativePath))
	FDirectoryPath Path;

	UPROPERTY(EditAnywhere, Category = PluginTemplate)
	FText Label;

	UPROPERTY(EditAnywhere, Category = PluginTemplate)
	FText Description;

	/** The default class of game feature data to create for new game feature plugins (if not set, UGameFeatureData will be used) */
	UPROPERTY(config, EditAnywhere, Category = Plugins)
	TSubclassOf<UGameFeatureData> DefaultGameFeatureDataClass;

	/** The default name of the created game feature data assets. If empty, will use the plugin name. */
	UPROPERTY(config, EditAnywhere, Category = Plugins)
	FString DefaultGameFeatureDataName;

	/** If true, the created plugin will be enabled by default without needing to be added to the project file. */
	UPROPERTY(config, EditAnywhere, Category = Plugins)
	bool bIsEnabledByDefault = false;

};


UCLASS(config = Editor, defaultconfig)
class GAMEFEATURESEDITOR_API UGameFeaturesEditorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// Array of Plugin templates. Allows projects to specify reusable plugin templates for the plugin creation wizard.
	UPROPERTY(config, EditAnywhere, Category = Plugins)
	TArray<FPluginTemplateData> PluginTemplates;
};