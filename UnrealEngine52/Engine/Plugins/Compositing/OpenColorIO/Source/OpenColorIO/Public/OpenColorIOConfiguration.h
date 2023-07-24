// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "OpenColorIOColorSpace.h"
#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "RHIDefinitions.h"
#endif

#include "OpenColorIOConfiguration.generated.h"


class FOpenColorIONativeConfiguration;
class FOpenColorIOTransformResource;
class FTextureResource;
class UOpenColorIOColorTransform;
struct FFileChangeData;
class SNotificationItem;
namespace ERHIFeatureLevel { enum Type : int; }

/**
 * Asset to manage allowed OpenColorIO color spaces. This will create required transform objects.
 */
UCLASS(BlueprintType)
class OPENCOLORIO_API UOpenColorIOConfiguration : public UObject
{
	GENERATED_BODY()

public:

	UOpenColorIOConfiguration(const FObjectInitializer& ObjectInitializer);

public:
	bool IsTransformReady(const FOpenColorIOColorConversionSettings& InSettings);
	bool GetRenderResources(ERHIFeatureLevel::Type InFeatureLevel, const FOpenColorIOColorConversionSettings& InSettings, FOpenColorIOTransformResource*& OutShaderResource, TSortedMap<int32, FTextureResource*>& OutTextureResources);
	UE_DEPRECATED(5.1, "GetShaderAndLUTResources is deprecated, please use GetRenderResources instead.")
	bool GetShaderAndLUTResources(ERHIFeatureLevel::Type InFeatureLevel, const FString& InSourceColorSpace, const FString& InDestinationColorSpace, FOpenColorIOTransformResource*& OutShaderResource, FTextureResource*& OutLUT3dResource);
	bool HasTransform(const FString& InSourceColorSpace, const FString& InDestinationColorSpace);
	bool HasTransform(const FString& InSourceColorSpace, const FString& InDisplay, const FString& InView, EOpenColorIOViewTransformDirection InDirection);
	bool HasDesiredColorSpace(const FOpenColorIOColorSpace& ColorSpace) const;
	bool HasDesiredDisplayView(const FOpenColorIODisplayView& DisplayView) const;
	bool Validate() const;

	/** This forces to reload colorspaces and corresponding shaders if those are not loaded already. */
	UFUNCTION(BlueprintCallable, Category = "OpenColorIO")
	void ReloadExistingColorspaces();

	/*
	* This method is called by directory watcher when any file or folder is changed in the 
	* directory where raw ocio config is located.
	*/
	void ConfigPathChangedEvent(const TArray<FFileChangeData>& InFileChanges, const FString InFileMountPath);

	/** Internal only: Replacement for previous `GetLoadedConfiguration()` and `GetLoadedConfigurationFile()` functions, returning the private implementation of the native OCIO config. */
	FOpenColorIONativeConfiguration* GetNativeConfig_Internal() const;
protected:

	const TObjectPtr<UOpenColorIOColorTransform>* FindTransform(const FOpenColorIOColorConversionSettings& InSettings) const;
	void CreateColorTransform(const FString& InSourceColorSpace, const FString& InDestinationColorSpace);
	void CreateColorTransform(const FString& InSourceColorSpace, const FString& InDisplay, const FString& InView, EOpenColorIOViewTransformDirection InDirection);
	void CleanupTransforms();

	/** Same as above except user can specify the path manually. */
	void StartDirectoryWatch(const FString& FilePath);

	/** Stop watching the current directory. */
	void StopDirectoryWatch();

public:

	//~ Begin UObject interface
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
#if WITH_EDITORONLY_DATA
	static void DeclareConstructClasses(TArray<FTopLevelAssetPath>& OutConstructClasses, const UClass* SpecificSubclass);
#endif

	virtual void PreSave(FObjectPreSaveContext SaveContext) override;
	virtual void BeginDestroy() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif //WITH_EDITOR
	//~ End UObject interface

private:
	void LoadConfiguration();

	/** This method resets the status of Notification dialog and reacts depending on user's choice. */
	void OnToastCallback(bool bInReloadColorspaces);

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Config", meta = (FilePathFilter = "Config Files (*.ocio, *.ocioz)|*.ocio;*.ocioz", RelativeToGameDir))
	FFilePath ConfigurationFile;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ColorSpace", meta=(OCIOConfigFile="ConfigurationFile"))
	TArray<FOpenColorIOColorSpace> DesiredColorSpaces;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ColorSpace", DisplayName="Desired Display-Views", meta = (OCIOConfigFile = "ConfigurationFile"))
	TArray<FOpenColorIODisplayView> DesiredDisplayViews;

private:

	UPROPERTY()
	TArray<TObjectPtr<UOpenColorIOColorTransform>> ColorTransforms;

private:
	struct FOCIOConfigWatchedDirInfo
	{
		/** A handle to the directory watcher. Gives us the ability to control directory watching status. */
		FDelegateHandle DirectoryWatcherHandle;

		/** Currently watched folder. */
		FString FolderPath;

		/** A handle to Notification message that pops up to notify user of raw config file going out of date. */
		TWeakPtr<SNotificationItem> RawConfigChangedToast;
	};

	/** Information about the currently watched directory. Helps us manage the directory change events. */
	FOCIOConfigWatchedDirInfo WatchedDirectoryInfo;

	/** Private implementation of the native OpenColorIO config object. */
	TPimplPtr<FOpenColorIONativeConfiguration> NativeConfig;
};
