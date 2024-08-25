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
struct FInterchangeTranslatorPipelines
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "TranslatorPipelines", meta = (AllowedClasses = "/Script/InterchangeCore.InterchangeTranslatorBase"))
	TSoftClassPtr<UInterchangeTranslatorBase> Translator;

	UPROPERTY(EditAnywhere, Category = "TranslatorPipelines", meta = (AllowedClasses = "/Script/InterchangeCore.InterchangePipelineBase, /Script/InterchangeEngine.InterchangeBlueprintPipelineBase, /Script/InterchangeEngine.InterchangePythonPipelineAsset"))
	TArray<FSoftObjectPath> Pipelines;
};

USTRUCT()
struct FInterchangePipelineStack
{
	GENERATED_BODY()
	
	/** The list of pipelines in this stack. The pipelines are executed in fixed order, from top to bottom. */
	UPROPERTY(EditAnywhere, Category = "Pipelines", meta = (AllowedClasses = "/Script/InterchangeCore.InterchangePipelineBase, /Script/InterchangeEngine.InterchangeBlueprintPipelineBase, /Script/InterchangeEngine.InterchangePythonPipelineAsset"))
	TArray<FSoftObjectPath> Pipelines;

	/** Specifies a different list of pipelines for this stack to use when importing data from specific translators. */
	UPROPERTY(EditAnywhere, Category = "TranslatorPipelines")
	TArray<FInterchangeTranslatorPipelines> PerTranslatorPipelines;
};

USTRUCT()
struct FInterchangeImportSettings
{
	GENERATED_BODY()

	/** Configures the pipeline stacks that are available when importing assets with Interchange. */
	UPROPERTY(EditAnywhere, Category = "Pipeline")
	TMap<FName, FInterchangePipelineStack> PipelineStacks;

	/** Specifies which pipeline stack Interchange should use by default. */
	UPROPERTY(EditAnywhere, Category = "Pipeline")
	FName DefaultPipelineStack = NAME_None;

	/** Specifies the class that should be used to define the configuration dialog that Interchange shows on import. */
	UPROPERTY(EditAnywhere, Category = "Pipeline")
	TSoftClassPtr <UInterchangePipelineConfigurationBase> ImportDialogClass;

	/** If enabled, the import option dialog will show when interchange import or re-import.*/
	UPROPERTY(EditAnywhere, Category = "Pipeline")
	bool bShowImportDialog = true;
};

USTRUCT()
struct FInterchangePerTranslatorDialogOverride
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "DialogOverride", meta = (AllowedClasses = "/Script/InterchangeCore.InterchangeTranslatorBase"))
	TSoftClassPtr<UInterchangeTranslatorBase> Translator;

	UPROPERTY(EditAnywhere, Category = "DialogOverride")
	bool bShowImportDialog = true;
};

USTRUCT()
struct FInterchangeDialogOverride
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "DialogOverride")
	bool bShowImportDialog = true;

	UPROPERTY(EditAnywhere, Category = "DialogOverride", meta = (AllowedClasses = "/Script/InterchangeCore.InterchangePipelineBase, /Script/InterchangeEngine.InterchangeBlueprintPipelineBase, /Script/InterchangeEngine.InterchangePythonPipelineAsset"))
	TArray<FInterchangePerTranslatorDialogOverride> PerTranslatorImportDialogOverride;
};

USTRUCT()
struct FInterchangeContentImportSettings : public FInterchangeImportSettings
{
	GENERATED_BODY()

	/** Specifies a different pipeline stack for Interchange to use by default when importing specific types of assets. */
	UPROPERTY(EditAnywhere, Category = "Pipeline", Meta=(DisplayAfter="DefaultPipelineStack"))
	TMap<EInterchangeTranslatorAssetType, FName> DefaultPipelineStackOverride;

	/** This tell interchange if the import dialog should show or not when importing a particular type of asset.*/
	UPROPERTY(EditAnywhere, Category = "Pipeline", Meta=(DisplayAfter="bShowImportDialog"))
	TMap<EInterchangeTranslatorAssetType, FInterchangeDialogOverride> ShowImportDialogOverride;
};

UCLASS(config=Engine, meta=(DisplayName=Interchange), MinimalAPI)
class UInterchangeProjectSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/**
	 * Settings used when importing into the Content Browser.
	 */
	UPROPERTY(EditAnywhere, config, Category = "ImportContent")
	FInterchangeContentImportSettings ContentImportSettings;

	/**
	 * Settings used when importing into a level.
	 */
	UPROPERTY(EditAnywhere, config, Category = "ImportIntoLevel")
	FInterchangeImportSettings SceneImportSettings;

	/** This tells Interchange which file picker class to construct when we need to choose a file for a source. */
	UPROPERTY(EditAnywhere, config, Category = "EditorInterface")
	TSoftClassPtr <UInterchangeFilePickerBase> FilePickerClass;

	/**
	 * If enabled, both Interchange translators and the legacy import process smooth the edges of static meshes that don't contain smoothing information.
	 * If you have an older project that relies on leaving hard edges by default, you can disable this setting to preserve consistency with older assets.
	 */
	UPROPERTY(EditAnywhere, config, Category = "Generic|ImportSettings")
	bool bStaticMeshUseSmoothEdgesIfSmoothingInformationIsMissing = true;

	/**
	 * Specifies which pipeline class Interchange should use when editor tools import or reimport an asset with base settings.
	 * Unreal Editor depends on this class to be set. You can only edit this property in the .ini file.
	 */
	UPROPERTY(EditAnywhere, config, Category = "Editor Generic Pipeline Class")
	TSoftClassPtr <UInterchangePipelineBase> GenericPipelineClass;
};

class FInterchangeProjectSettingsUtils
{
public:
	static INTERCHANGEENGINE_API const FInterchangeImportSettings& GetImportSettings(const UInterchangeProjectSettings& InterchangeProjectSettings, const bool bIsSceneImport);
	static INTERCHANGEENGINE_API FInterchangeImportSettings& GetMutableImportSettings(UInterchangeProjectSettings& InterchangeProjectSettings, const bool bIsSceneImport);
	static INTERCHANGEENGINE_API const FInterchangeImportSettings& GetDefaultImportSettings(const bool bIsSceneImport);
	static INTERCHANGEENGINE_API FInterchangeImportSettings& GetMutableDefaultImportSettings(const bool bIsSceneImport);

	static INTERCHANGEENGINE_API FName GetDefaultPipelineStackName(const bool bIsSceneImport, const UInterchangeSourceData& SourceData);
	static INTERCHANGEENGINE_API void SetDefaultPipelineStackName(const bool bIsSceneImport, const UInterchangeSourceData& SourceData, const FName StackName);

	static INTERCHANGEENGINE_API bool ShouldShowPipelineStacksConfigurationDialog(const bool bIsSceneImport, const UInterchangeSourceData& SourceData);
};
