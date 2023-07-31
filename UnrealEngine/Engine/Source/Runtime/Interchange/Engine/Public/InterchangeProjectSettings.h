// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettings.h"
#include "Engine/EngineTypes.h"
#include "InterchangeFilePickerBase.h"
#include "InterchangePipelineBase.h"
#include "InterchangePipelineConfigurationBase.h"
#include "UObject/SoftObjectPath.h"

#include "InterchangeProjectSettings.generated.h"

class UInterchangeSourceData;

USTRUCT()
struct FInterchangePipelineStack
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = "Pipelines", meta = (AllowedClasses = "/Script/InterchangeCore.InterchangePipelineBase, /Script/InterchangeEngine.InterchangeBlueprintPipelineBase, /Script/InterchangeEngine.InterchangePythonPipelineAsset"))
	TArray<FSoftObjectPath> Pipelines;
};

USTRUCT()
struct FInterchangeImportSettings
{
	GENERATED_BODY()

	/** All the available pipeline stacks you want to use to import with interchange. The chosen pipeline stack execute all the pipelines from top to bottom order. You can order them by using the grip on the left of any pipelines.*/
	UPROPERTY(EditAnywhere, Category = "Pipeline")
	TMap<FName, FInterchangePipelineStack> PipelineStacks;

	/** This tell interchange which pipeline stack to select when importing.*/
	UPROPERTY(EditAnywhere, Category = "Pipeline")
	FName DefaultPipelineStack = NAME_None;

	/** This tell interchange which pipeline configuration dialog to popup when we need to configure the pipelines.*/
	UPROPERTY(EditAnywhere, Category = "Pipeline")
	TSoftClassPtr <UInterchangePipelineConfigurationBase> PipelineConfigurationDialogClass;

	/** If enabled, the pipeline stacks configuration dialog will show when interchange must choose a pipeline to import or re-import. If disabled interchange will use the DefaultPipelineStack.*/
	UPROPERTY(EditAnywhere, Category = "Pipeline")
	bool bShowPipelineStacksConfigurationDialog = true;
};

USTRUCT()
struct FInterchangeContentImportSettings : public FInterchangeImportSettings
{
	GENERATED_BODY()

	/** This tell interchange which pipeline stack to select when importing.*/
	UPROPERTY(EditAnywhere, Category = "Pipeline", Meta=(DisplayAfter="DefaultPipelineStack"))
	TMap<EInterchangeTranslatorAssetType, FName> DefaultPipelineStackOverride;

	/** This tell interchange which pipeline stack to select when importing.*/
	UPROPERTY(EditAnywhere, Category = "Pipeline", Meta=(DisplayAfter="bShowPipelineStacksConfigurationDialog"))
	TMap<EInterchangeTranslatorAssetType, bool> ShowPipelineStacksConfigurationDialogOverride;
};

UCLASS(config=Engine, meta=(DisplayName=Interchange), MinimalAPI)
class UInterchangeProjectSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/**
	 * Settings used when importing into the content browser.
	 */
	UPROPERTY(EditAnywhere, config, Category = "ImportContent")
	FInterchangeContentImportSettings ContentImportSettings;

	/**
	 * Settings used when importing into a level.
	 */
	UPROPERTY(EditAnywhere, config, Category = "ImportIntoLevel")
	FInterchangeImportSettings SceneImportSettings;

	/** This tells interchange which file picker class to construct when we need to choose a file for a source.*/
	UPROPERTY(EditAnywhere, config, Category = "EditorInterface")
	TSoftClassPtr <UInterchangeFilePickerBase> FilePickerClass;

	/**
	 * If checked, interchange translators and legacy importer will default static mesh geometry to smooth edge when the smoothing information is missing.
	 * This option exist to allows old project to import the same way as before if their workflows need static mesh edges to be hard when the smoothing
	 * info is missing.
	 */
	UPROPERTY(EditAnywhere, config, Category = "Generic|ImportSettings")
	bool bStaticMeshUseSmoothEdgesIfSmoothingInformationIsMissing = true;

	/**
	 * This tells interchange which is the pipeline class to use when editor tools want to import or reimport tools with bake settings.
	 * UnrealEd code depend on this class to be set and this property is only editable in the ini file directly.
	 */
	UPROPERTY(EditAnywhere, config, Category = "Editor Generic Pipeline Class")
	TSoftClassPtr <UInterchangePipelineBase> GenericPipelineClass;
};

class INTERCHANGEENGINE_API FInterchangeProjectSettingsUtils
{
public:
	static const FInterchangeImportSettings& GetImportSettings(const UInterchangeProjectSettings& InterchangeProjectSettings, const bool bIsSceneImport);
	static FInterchangeImportSettings& GetMutableImportSettings(UInterchangeProjectSettings& InterchangeProjectSettings, const bool bIsSceneImport);
	static const FInterchangeImportSettings& GetDefaultImportSettings(const bool bIsSceneImport);
	static FInterchangeImportSettings& GetMutableDefaultImportSettings(const bool bIsSceneImport);

	static FName GetDefaultPipelineStackName(const bool bIsSceneImport, const UInterchangeSourceData& SourceData);

	static bool ShouldShowPipelineStacksConfigurationDialog(const bool bIsSceneImport, const UInterchangeSourceData& SourceData);
};