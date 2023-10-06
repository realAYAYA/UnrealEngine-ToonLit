// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "Templates/Function.h"
#include "Templates/SharedPointer.h"

#include "PortableObjectPipeline.h"
#include "Internationalization/LocalizedTextSourceTypes.h"

#include "UserGeneratedContentLocalization.generated.h"

class IPlugin;
class FJsonObject;
class FLocTextHelper;

/**
 * Settings controlling UGC localization.
 */
UCLASS(config=Engine, defaultconfig)
class LOCALIZATION_API UUserGeneratedContentLocalizationSettings : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * List of cultures that should be disabled for UGC localization.
	 * @note You can't disable the native culture for the project.
	 */
	UPROPERTY(EditAnywhere, Category=Localization)
	TArray<FString> CulturesToDisable;

	/**
	 * Should we compile UGC localization (if present) for DLC plugins during cook?
	 */
	UPROPERTY(EditAnywhere, Category=Localization)
	bool bCompileDLCLocalizationDuringCook = true;

	/**
	 * Should we validate UGC localization (if present) for DLC plugins during cook?
	 * @note Validation will happen against a UGC localization descriptor that has had InitializeFromProject called on it.
	 */
	UPROPERTY(EditAnywhere, Category=Localization)
	bool bValidateDLCLocalizationDuringCook = true;
};

/**
 * Minimal descriptor needed to generate a localization target for UGC localization.
 */
USTRUCT()
struct LOCALIZATION_API FUserGeneratedContentLocalizationDescriptor
{
	GENERATED_BODY()
	
public:
	/**
	 * Initialize the NativeCulture and CulturesToGenerate values based on the settings of the currently loaded Unreal project.
	 * @param LocalizationCategory What category is the localization targets being used with this descriptor?
	 */
	void InitializeFromProject(const ELocalizedTextSourceCategory LocalizationCategory = ELocalizedTextSourceCategory::Game);
	
	/**
	 * Validate that this descriptor isn't using cultures that aren't present in the CulturesToGenerate of the given default.
	 *   - If the NativeCulture is invalid, reset it to the value from the default.
	 *   - If CulturesToGenerate contains invalid entries then remove those from the array.
	 * 
	 * @return True if this descriptor was valid and no changes were made. False if this descriptor was invalid and had default changes applied.
	 */
	bool Validate(const FUserGeneratedContentLocalizationDescriptor& DefaultDescriptor);

	/**
	 * Save the settings to a JSON object/file.
	 */
	bool ToJsonObject(TSharedPtr<FJsonObject>& OutJsonObject) const;
	bool ToJsonString(FString& OutJsonString) const;
	bool ToJsonFile(const TCHAR* InFilename) const;

	/**
	 * Load the settings from a JSON object/file.
	 */
	bool FromJsonObject(TSharedRef<const FJsonObject> InJsonObject);
	bool FromJsonString(const FString& InJsonString);
	bool FromJsonFile(const TCHAR* InFilename);
	
	/**
	 * The culture that the source text is authored in.
	 * @note You shouldn't change this once you start to localize your text.
	 */
	UPROPERTY(EditAnywhere, Category=Localization)
	FString NativeCulture;

	/**
	 * The cultures that we should generate localization data for.
	 * @note Will implicitly always contain the native culture during export/compile.
	 */
	UPROPERTY(EditAnywhere, Category=Localization)
	TArray<FString> CulturesToGenerate;

	/**
	 * What format of PO file should we use?
	 * @note You can adjust this later and we'll attempt to preserve any existing localization data by importing with the old setting prior to export.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category=Localization, meta=(DisplayName="PO Format"))
	EPortableObjectFormat PoFormat = EPortableObjectFormat::Unreal;
};

/**
 * UGC localization can be used to provide a simplified localization experience for basic plugins (only providing the PO files to be translated), 
 * and is primarily designed for DLC plugins where the UGC localization will be compiled during cook (@see UUserGeneratedContentLocalizationSettings).
 * 
 * Support for non-DLC plugins can be provided via project specific tooling built upon this base API.
 * Support for complex plugins (such as those containing different kinds of modules, eg) a mix of game/engine and editor) are not supported via this API.
 */
namespace UserGeneratedContentLocalization
{

struct FExportLocalizationOptions
{
	/** Common export options for all plugins */
	FUserGeneratedContentLocalizationDescriptor UGCLocDescriptor;

	/** Optional mapping of plugin names to collection names (to act as a filter for their asset gather step) */
	TMap<FString, FString> PluginNameToCollectionNameFilter;

	/** True to gather localization from source code (if a plugin has a Config or Source folder) */
	bool bGatherSource = true;

	/** True to gather localization from assets */
	bool bGatherAssets = true;

	/** True to gather localization from Verse */
	bool bGatherVerse = true;

	/** True to update the plugin descriptors (if needed) so that they contain the exported localization target */
	bool bUpdatePluginDescriptor = true;

	/** True to automatically clean-up any scratch data created during the localization export */
	bool bAutoCleanup = true;

	/** The category to use for the exported localization target (only used when bUpdatePluginDescriptor is true) */
	ELocalizedTextSourceCategory LocalizationCategory = ELocalizedTextSourceCategory::Game;

	/** An optional copyright notice to insert into the exported files */
	FString CopyrightNotice;
};

enum class ELoadLocalizationResult : uint8
{
	/** There was no source localization data to load */
	NoData,
	/** There was source localization data to load, but we failed to load it */
	Failed,
	/** There was source localization data to load, and we successfully loaded it */
	Success,
};

/**
 * Export UGC localization for the given plugins.
 * 
 * @param Plugins				The list of plugins to export.
 * @param ExportOptions			Options controlling how to export the localization data.
 * @param CommandletExecutor	Callback used to actually execute the gather commandlet:
 *									This should execute an editor with `-run=GatherText -config="..."`, where the config argument is the first argument passed to this callback.
 *									The second argument should be filled with the raw log output from running the commandlet process.
 *									The return value is the exit code of the commandlet process (where zero means success).
 */
LOCALIZATION_API bool ExportLocalization(TArrayView<const TSharedRef<IPlugin>> Plugins, const FExportLocalizationOptions& ExportOptions, TFunctionRef<int32(const FString&, FString&)> CommandletExecutor);

/**
 * Compile UGC localization (if present) for the given plugins, producing LocMeta and LocRes files for consumption by the engine.
 * 
 * @param Plugins				The list of plugins to compile.
 * @param DefaultDescriptor		An optional default UGC localization descriptor to validate any loaded UGC localization descriptors against prior to compiling the localization data.
 */
LOCALIZATION_API bool CompileLocalization(TArrayView<const TSharedRef<IPlugin>> Plugins, const FUserGeneratedContentLocalizationDescriptor* DefaultDescriptor = nullptr);

/**
 * Compile UGC localization (if present) for the given plugin, producing LocMeta and LocRes files for consumption by the engine.
 *
 * @param PluginName					The plugin name being compiled.
 * @param PluginInputContentDirectory	The content directory we'll read the source localization data from when compiling the plugin.
 * @param PluginOutputContentDirectory	The content directory we'll write the LocMeta and LocRes data to when compiling the plugin.
 * @param DefaultDescriptor				An optional default UGC localization descriptor to validate any loaded UGC localization descriptors against prior to compiling the localization data.
 */
LOCALIZATION_API bool CompileLocalization(const FString& PluginName, const FString& PluginInputContentDirectory, const FString& PluginOutputContentDirectory, const FUserGeneratedContentLocalizationDescriptor* DefaultDescriptor = nullptr);

/**
 * Load UGC localization source data for the given plugin.
 * @note This is typically only needed for compilation (which does it internally), but can also be useful if you have other processes that need to read the source data.
 *
 * @param PluginName					The plugin name being loaded.
 * @param PluginInputContentDirectory	The content directory we'll read the source localization data from.
 * @param OutLocTextHelper				The LocTextHelper to fill with manifest/archive data, re-generated from the source data.
 * @param DefaultDescriptor				An optional default UGC localization descriptor to validate any loaded UGC localization descriptors against prior to loading the localization data.
 */
LOCALIZATION_API ELoadLocalizationResult LoadLocalization(const FString& PluginName, const FString& PluginContentDirectory, TSharedPtr<FLocTextHelper>& OutLocTextHelper, const FUserGeneratedContentLocalizationDescriptor* DefaultDescriptor = nullptr);

/**
 * Cleanup UGC localization that is no longer relevant based on the given descriptor.
 *
 * @param Plugins				The list of plugins to cleanup.
 * @param DefaultDescriptor		The default UGC localization descriptor to filter existing UGC localization data against (things that don't pass the filter will be cleaned).
 * @param bSilent				True to silently delete localization data that doesn't pass the filter, or false to confirm with the user (via a dialog).
 */
LOCALIZATION_API void CleanupLocalization(TArrayView<const TSharedRef<IPlugin>> Plugins, const FUserGeneratedContentLocalizationDescriptor& DefaultDescriptor, const bool bSilent = false);

}
