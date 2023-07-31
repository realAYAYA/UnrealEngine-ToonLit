// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "OpenColorIOColorSpace.h"
#include "RHIDefinitions.h"

#if WITH_EDITOR && WITH_OCIO
#include "OpenColorIO/OpenColorIO.h"
#endif

#include "OpenColorIOConfiguration.generated.h"


class FOpenColorIOTransformResource;
class FTextureResource;
class UOpenColorIOColorTransform;
struct FFileChangeData;
class SNotificationItem;

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
	bool GetRenderResources(ERHIFeatureLevel::Type InFeatureLevel, const FOpenColorIOColorConversionSettings& InSettings, FOpenColorIOTransformResource*& OutShaderResource, TSortedMap<int32, FTextureResource*>& OutTextureResources);
	UE_DEPRECATED(5.1, "GetShaderAndLUTResources is deprecated, please use GetRenderResources instead.")
	bool GetShaderAndLUTResources(ERHIFeatureLevel::Type InFeatureLevel, const FString& InSourceColorSpace, const FString& InDestinationColorSpace, FOpenColorIOTransformResource*& OutShaderResource, FTextureResource*& OutLUT3dResource);
	bool HasTransform(const FString& InSourceColorSpace, const FString& InDestinationColorSpace);
	bool HasTransform(const FString& InSourceColorSpace, const FString& InDisplay, const FString& InView, EOpenColorIOViewTransformDirection InDirection);
	bool HasDesiredColorSpace(const FOpenColorIOColorSpace& ColorSpace) const;
	bool HasDesiredDisplayView(const FOpenColorIODisplayView& DisplayView) const;
	bool Validate() const;

	/** This forces to reload colorspaces and corresponding shaders if those are not loaded already. */
	void ReloadExistingColorspaces();

	/*
	* This method is called by directory watcher when any file or folder is changed in the 
	* directory where raw ocio config is located.
	*/
	void ConfigPathChangedEvent(const TArray<FFileChangeData>& InFileChanges, const FString InFileMountPath);

#if WITH_EDITORONLY_DATA && WITH_OCIO
	OCIO_NAMESPACE::ConstConfigRcPtr GetLoadedConfigurationFile() const { return LoadedConfig; }
#endif

protected:
	void CreateColorTransform(const FString& InSourceColorSpace, const FString& InDestinationColorSpace);
	void CreateColorTransform(const FString& InSourceColorSpace, const FString& InDisplay, const FString& InView, EOpenColorIOViewTransformDirection InDirection);
	void CleanupTransforms();

	/** Same as above except user can specify the path manually. */
	void StartDirectoryWatch(const FString& FilePath);

	/** Stop watching the current directory. */
	void StopDirectoryWatch();

public:

	//~ Begin UObject interface
	virtual void PostLoad() override;
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
	void LoadConfigurationFile();

	/** This method resets the status of Notification dialog and reacts depending on user's choice. */
	void OnToastCallback(bool bInReloadColorspaces);

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Config", meta = (FilePathFilter = "ocio", RelativeToGameDir))
	FFilePath ConfigurationFile;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ColorSpace", meta=(OCIOConfigFile="ConfigurationFile"))
	TArray<FOpenColorIOColorSpace> DesiredColorSpaces;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ColorSpace", DisplayName="Desired Display-Views", meta = (OCIOConfigFile = "ConfigurationFile"))
	TArray<FOpenColorIODisplayView> DesiredDisplayViews;

private:

	UPROPERTY()
	TArray<TObjectPtr<UOpenColorIOColorTransform>> ColorTransforms;

#if WITH_EDITORONLY_DATA && WITH_OCIO
	OCIO_NAMESPACE::ConstConfigRcPtr LoadedConfig;
#endif //WITH_EDITORONLY_DATA

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
};
