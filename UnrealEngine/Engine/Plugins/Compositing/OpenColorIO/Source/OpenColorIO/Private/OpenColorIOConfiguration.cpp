// Copyright Epic Games, Inc. All Rights Reserved.

#include "OpenColorIOConfiguration.h"

#include "Containers/SortedMap.h"
#include "EngineAnalytics.h"
#include "Engine/VolumeTexture.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Math/PackedVector.h"
#include "Misc/PathViews.h"
#include "Modules/ModuleManager.h"
#include "OpenColorIOColorTransform.h"
#include "OpenColorIOModule.h"
#include "OpenColorIOSettings.h"
#include "TextureResource.h"
#include "Widgets/Notifications/SNotificationList.h"

#include "UObject/ObjectSaveContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(OpenColorIOConfiguration)

#define LOCTEXT_NAMESPACE "OCIOConfiguration"


#if WITH_EDITOR
#include "OpenColorIOWrapper.h"
#include "DerivedDataCacheInterface.h"
#include "DirectoryWatcherModule.h"
#include "IDirectoryWatcher.h"
#include "Interfaces/ITargetPlatform.h"

namespace OCIODirectoryWatcher
{
	/** OCIO supported extensions we should be checking for when something changes in the OCIO config folder. */
	static const TSet<FString> OcioExtensions =
	{
		"spi1d", "spi3d", "3dl", 
		"cc", "ccc", "csp", 
		"cub", "cube", "lut", 
		"mga", "m3d", "spi1d", 
		"spi3d", "spimtx", "vf",
		"ocio"
	};

	static const FName NAME_DirectoryWatcher = "DirectoryWatcher";
}
#endif

UOpenColorIOConfiguration::UOpenColorIOConfiguration(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	
}

void UOpenColorIOConfiguration::BeginDestroy()
{
	StopDirectoryWatch();
	Super::BeginDestroy();
}

bool UOpenColorIOConfiguration::IsTransformReady(const FOpenColorIOColorConversionSettings& InSettings)
{
	if (const TObjectPtr<UOpenColorIOColorTransform>* TransformPtr = FindTransform(InSettings))
	{
		return (*TransformPtr)->AreRenderResourcesReady();
	}

	return false;
}

bool UOpenColorIOConfiguration::GetRenderResources(ERHIFeatureLevel::Type InFeatureLevel, const FOpenColorIOColorConversionSettings& InSettings, FOpenColorIOTransformResource*& OutShaderResource, TSortedMap<int32, FTextureResource*>& OutTextureResources)
{
	if (const TObjectPtr<UOpenColorIOColorTransform>* TransformPtr = FindTransform(InSettings))
	{
		return (*TransformPtr)->GetRenderResources(InFeatureLevel, OutShaderResource, OutTextureResources);
	}

	return false;
}

bool UOpenColorIOConfiguration::HasTransform(const FString& InSourceColorSpace, const FString& InDestinationColorSpace)
{
	TObjectPtr<UOpenColorIOColorTransform>* TransformData = ColorTransforms.FindByPredicate([&](const UOpenColorIOColorTransform* InTransformData)
	{
		return InTransformData->IsTransform(InSourceColorSpace, InDestinationColorSpace);
	});

	return (TransformData != nullptr);
}

bool UOpenColorIOConfiguration::HasTransform(const FString& InSourceColorSpace, const FString& InDisplay, const FString& InView, EOpenColorIOViewTransformDirection InDirection)
{
	UE_TRANSITIONAL_OBJECT_PTR(UOpenColorIOColorTransform)* TransformData = ColorTransforms.FindByPredicate([&](const UOpenColorIOColorTransform* InTransformData)
		{
			return InTransformData->IsTransform(InSourceColorSpace, InDisplay, InView, InDirection);
		});

	return (TransformData != nullptr);
}

bool UOpenColorIOConfiguration::HasDesiredColorSpace(const FOpenColorIOColorSpace& ColorSpace) const
{
	if (ColorSpace.IsValid())
	{
		return DesiredColorSpaces.Find(ColorSpace) != INDEX_NONE;
	}

	return false;
}

bool UOpenColorIOConfiguration::HasDesiredDisplayView(const FOpenColorIODisplayView& DisplayView) const
{
	if (DisplayView.IsValid())
	{
		return DesiredDisplayViews.Find(DisplayView) != INDEX_NONE;
	}

	return false;
}

bool UOpenColorIOConfiguration::Validate() const
{
#if WITH_EDITOR
	if (!ConfigurationFile.FilePath.IsEmpty() && Config.IsValid())
	{
		//When loading the configuration file, if any errors are detected, it will throw an exception. Thus, our pointer won't be valid.
		return Config->IsValid();
	}

	return false;
#else
	return true;
#endif // WITH_EDITOR
}

#if WITH_EDITOR
bool UOpenColorIOConfiguration::TransformImage(const FOpenColorIOColorConversionSettings& InSettings, const FImageView& InOutImage) const
{
	if (const TObjectPtr<UOpenColorIOColorTransform>* TransformPtr = FindTransform(InSettings))
	{
		return (*TransformPtr)->TransformImage(InOutImage);
	}

	return false;
}

bool UOpenColorIOConfiguration::TransformImage(const FOpenColorIOColorConversionSettings& InSettings, const FImageView& SrcImage, const FImageView& DestImage) const
{
	if (const TObjectPtr<UOpenColorIOColorTransform>* TransformPtr = FindTransform(InSettings))
	{
		return (*TransformPtr)->TransformImage(SrcImage, DestImage);
	}

	return false;
}
#endif

void UOpenColorIOConfiguration::ReloadExistingColorspaces()
{
#if WITH_EDITOR
	LoadConfiguration();

	if (Config && Config->IsValid())
	{
		FString LoadedConfigHash = Config->GetCacheID() + FString(OpenColorIOWrapper::GetVersion());
		
		const UOpenColorIOSettings* Settings = GetDefault<UOpenColorIOSettings>();
		if (Settings->bSupportInverseViewTransforms)
		{
			LoadedConfigHash += FString(TEXT("_Inv"));
		}

		// Hash is different, proceed with the regeneration...
		if (ConfigHash != LoadedConfigHash)
		{
			ConfigHash = LoadedConfigHash;

			TArray<FOpenColorIOColorSpace> ColorSpacesToBeReloaded = DesiredColorSpaces;
			TArray<FOpenColorIODisplayView> DisplayViewsToBeReloaded = DesiredDisplayViews;
			DesiredColorSpaces.Reset();
			DesiredDisplayViews.Reset();
			ColorTransforms.Reset();

			// This will make sure that all colorspaces are up to date in case an index, family or name is changed.
			for (const FOpenColorIOColorSpace& ExistingColorSpace : ColorSpacesToBeReloaded)
			{
				int ColorSpaceIndex = Config->GetColorSpaceIndex(*ExistingColorSpace.ColorSpaceName);
				if (ColorSpaceIndex < 0)
				{
					// Name not found, therefore we don't need to re-add this colorspace.
					UE_LOG(LogOpenColorIO, Display, TEXT("Removing %s, not found."), *ExistingColorSpace.ToString());
					continue;
				}

				FOpenColorIOColorSpace ColorSpace;
				ColorSpace.ColorSpaceIndex = ColorSpaceIndex;
				ColorSpace.ColorSpaceName = ExistingColorSpace.ColorSpaceName;
				ColorSpace.FamilyName = Config->GetColorSpaceFamilyName(*ExistingColorSpace.ColorSpaceName);
				DesiredColorSpaces.Add(ColorSpace);
			}

			for (const FOpenColorIODisplayView& ExistingDisplayView : DisplayViewsToBeReloaded)
			{
				bool bDisplayViewFound = false;
				for (int32 ViewIndex = 0; ViewIndex < Config->GetNumViews(*ExistingDisplayView.Display); ++ViewIndex)
				{
					const FString ViewName = Config->GetViewName(*ExistingDisplayView.Display, ViewIndex);

					if (ViewName == ExistingDisplayView.View)
					{
						DesiredDisplayViews.Add(ExistingDisplayView);
						bDisplayViewFound = true;
						break;
					}
				}

				if (!bDisplayViewFound)
				{
					UE_LOG(LogOpenColorIO, Display, TEXT("Removing %s, not found."), *ExistingDisplayView.ToString());
				}
			}

			// Genereate new shaders.
			for (int32 indexTop = 0; indexTop < DesiredColorSpaces.Num(); ++indexTop)
			{
				const FOpenColorIOColorSpace& TopColorSpace = DesiredColorSpaces[indexTop];

				for (int32 indexOther = indexTop + 1; indexOther < DesiredColorSpaces.Num(); ++indexOther)
				{
					const FOpenColorIOColorSpace& OtherColorSpace = DesiredColorSpaces[indexOther];

					CreateColorTransform(TopColorSpace.ColorSpaceName, OtherColorSpace.ColorSpaceName);
					CreateColorTransform(OtherColorSpace.ColorSpaceName, TopColorSpace.ColorSpaceName);
				}

				for (const FOpenColorIODisplayView& DisplayView : DesiredDisplayViews)
				{
					CreateColorTransform(TopColorSpace.ColorSpaceName, DisplayView.Display, DisplayView.View, EOpenColorIOViewTransformDirection::Forward);

					if (Settings->bSupportInverseViewTransforms)
					{
						CreateColorTransform(TopColorSpace.ColorSpaceName, DisplayView.Display, DisplayView.View, EOpenColorIOViewTransformDirection::Inverse);
					}
				}
			}
		}
		else
		{
			UE_LOG(LogOpenColorIO, Verbose, TEXT("Reload skipped, no differences identified."));
		}
	}
	else
	{
		DesiredColorSpaces.Reset();
		DesiredDisplayViews.Reset();
		ColorTransforms.Reset();
	}

	if (!MarkPackageDirty())
	{
		UE_LOG(LogOpenColorIO, Display, TEXT("%s should be resaved for faster loads. Changes in the config or the library version were detected."), *GetPathName());
	}
#endif
}

void UOpenColorIOConfiguration::ConfigPathChangedEvent(const TArray<FFileChangeData>& InFileChanges, const FString InFileMountPath)
{
#if WITH_EDITOR
	// We want to stop reacting to these events while the message is still up.
	if (WatchedDirectoryInfo.RawConfigChangedToast.IsValid())
	{
		return;
	}
	for (const FFileChangeData& FileChangeData : InFileChanges)
	{
		const FString FileExtension = FPaths::GetExtension(FileChangeData.Filename);
		if (FileExtension.IsEmpty() || !OCIODirectoryWatcher::OcioExtensions.Contains(FileExtension))
		{
			continue;
		}

		const FText DialogBody = FText::Format(LOCTEXT("OcioConfigChanged",
			"Files associated with OCIO config or luts have been modified externally. \
			\nWould you like to reload '{0}' configuration file?"),
			FText::FromString(GetName()));

		const FText ReloadRawConfigText = LOCTEXT("ReloadRawConfigConfirm", "Reload");
		const FText IgnoreReloadRawConfigText = LOCTEXT("IgnoreReloadRawConfig", "Ignore");


		FSimpleDelegate OnReloadRawConfig = FSimpleDelegate::CreateLambda([this]() { OnToastCallback(true); });
		FSimpleDelegate OnIgnoreReloadRawConfig = FSimpleDelegate::CreateLambda([this]() { OnToastCallback(false); });

		FNotificationInfo Info(DialogBody);
		Info.bFireAndForget = false;
		Info.bUseLargeFont = false;
		Info.bUseThrobber = false;
		Info.bUseSuccessFailIcons = false;
		Info.ButtonDetails.Add(FNotificationButtonInfo(ReloadRawConfigText, FText(), OnReloadRawConfig));
		Info.ButtonDetails.Add(FNotificationButtonInfo(IgnoreReloadRawConfigText, FText(), OnIgnoreReloadRawConfig));

		WatchedDirectoryInfo.RawConfigChangedToast = FSlateNotificationManager::Get().AddNotification(Info);

		if (WatchedDirectoryInfo.RawConfigChangedToast.IsValid())
		{
			WatchedDirectoryInfo.RawConfigChangedToast.Pin()->SetCompletionState(SNotificationItem::CS_Pending);
		}

		break;
	}

#endif
}

FOpenColorIOWrapperConfig* UOpenColorIOConfiguration::GetConfigWrapper() const
{
#if WITH_EDITOR
	return Config.Get();
#else
	return nullptr;
#endif
}

void UOpenColorIOConfiguration::StartDirectoryWatch(const FString& FilePath)
{
#if WITH_EDITOR
	FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>(OCIODirectoryWatcher::NAME_DirectoryWatcher);
	if (IDirectoryWatcher* DirectoryWatcher = DirectoryWatcherModule.Get())
	{
		FString FolderPath = FPaths::GetPath(FilePath);

		// Unregister watched folder since ocio config path has changed..
		StopDirectoryWatch();

		WatchedDirectoryInfo.FolderPath = MoveTemp(FolderPath);
		{
			DirectoryWatcher->RegisterDirectoryChangedCallback_Handle(
				WatchedDirectoryInfo.FolderPath, IDirectoryWatcher::FDirectoryChanged::CreateUObject(this, &UOpenColorIOConfiguration::ConfigPathChangedEvent, WatchedDirectoryInfo.FolderPath),
				WatchedDirectoryInfo.DirectoryWatcherHandle,
				/*Flags*/ 0
			);
		}
	}
#endif
}

void UOpenColorIOConfiguration::StopDirectoryWatch()
{
#if WITH_EDITOR
	FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>(OCIODirectoryWatcher::NAME_DirectoryWatcher);
	if (IDirectoryWatcher* DirectoryWatcher = DirectoryWatcherModule.Get())
	{
		if (WatchedDirectoryInfo.DirectoryWatcherHandle.IsValid())
		{
			DirectoryWatcher->UnregisterDirectoryChangedCallback_Handle(WatchedDirectoryInfo.FolderPath, WatchedDirectoryInfo.DirectoryWatcherHandle);
			WatchedDirectoryInfo.FolderPath.Empty();
		}
	}
#endif
}

const TObjectPtr<UOpenColorIOColorTransform>* UOpenColorIOConfiguration::FindTransform(const FOpenColorIOColorConversionSettings& InSettings) const
{
	return ColorTransforms.FindByPredicate([&](const UOpenColorIOColorTransform* InTransform)
		{
			EOpenColorIOViewTransformDirection DisplayViewDirection;
			
			if (InTransform->GetDisplayViewDirection(DisplayViewDirection))
			{
				return InTransform->SourceColorSpace == InSettings.SourceColorSpace.ColorSpaceName && InTransform->Display == InSettings.DestinationDisplayView.Display && InTransform->View == InSettings.DestinationDisplayView.View && InSettings.DisplayViewDirection == DisplayViewDirection;
			}
			else
			{
				return InTransform->SourceColorSpace == InSettings.SourceColorSpace.ColorSpaceName && InTransform->DestinationColorSpace == InSettings.DestinationColorSpace.ColorSpaceName;
			}
		});
}

#if WITH_EDITOR
void UOpenColorIOConfiguration::CreateColorTransform(const FString& InSourceColorSpace, const FString& InDestinationColorSpace)
{
	if (InSourceColorSpace.IsEmpty() || InDestinationColorSpace.IsEmpty())
	{
		return;
	}

	if (HasTransform(InSourceColorSpace, InDestinationColorSpace))
	{
		UE_LOG(LogOpenColorIO, Log, TEXT("OCIOConfig already contains %s to %s transform."), *InSourceColorSpace, *InDestinationColorSpace);
		return;
	}

	UOpenColorIOColorTransform* NewTransform = NewObject<UOpenColorIOColorTransform>(this, NAME_None, RF_NoFlags);
	const bool bSuccess = NewTransform->Initialize(InSourceColorSpace, InDestinationColorSpace);

	if (bSuccess)
	{
		ColorTransforms.Add(NewTransform);
	}
	else
	{
		UE_LOG(LogOpenColorIO, Warning, TEXT("Could not create color space transform from %s to %s. Verify your OCIO config file, it may have errors in it."), *InSourceColorSpace, *InDestinationColorSpace);
	}
}

void UOpenColorIOConfiguration::CreateColorTransform(const FString& InSourceColorSpace, const FString& InDisplay, const FString& InView, EOpenColorIOViewTransformDirection InDirection)
{
	if (InSourceColorSpace.IsEmpty() || InDisplay.IsEmpty() || InView.IsEmpty())
	{
		return;
	}

	if (HasTransform(InSourceColorSpace, InDisplay, InView, InDirection))
	{
		UE_LOG(LogOpenColorIO, Log, TEXT("OCIOConfig already contains %s to %s-%s transform."), *InSourceColorSpace, *InDisplay, *InView);
		return;
	}

	UOpenColorIOColorTransform* NewTransform = NewObject<UOpenColorIOColorTransform>(this, NAME_None, RF_NoFlags);
	const bool bSuccess = NewTransform->Initialize(InSourceColorSpace, InDisplay, InView, InDirection);

	if (bSuccess)
	{
		ColorTransforms.Add(NewTransform);
	}
	else
	{
		UE_LOG(LogOpenColorIO, Warning, TEXT("Could not create color space transform from %s to %s - %s. Verify your OCIO config file, it may have errors in it."), *InSourceColorSpace, *InDisplay, *InView);
	}
}
#endif

void UOpenColorIOConfiguration::CleanupTransforms()
{
	ColorTransforms = ColorTransforms.FilterByPredicate([this](const TObjectPtr<UOpenColorIOColorTransform>& InTransform) -> bool
		{
			//Check if the source color space of this transform is desired. If not, remove that transform from the list.
			const FOpenColorIOColorSpace* FoundSourceColorPtr = DesiredColorSpaces.FindByPredicate([InTransform](const FOpenColorIOColorSpace& InOtherColorSpace)
				{
					return InOtherColorSpace.ColorSpaceName == InTransform->SourceColorSpace;
				});

			if (FoundSourceColorPtr == nullptr)
			{
				return false;
			}

			void* DestinationPtr = nullptr;

			//The source was there so check if the destination color space/display view of this transform is desired. If not, remove that transform from the list.
			if (InTransform->bIsDisplayViewType)
			{
				DestinationPtr = DesiredDisplayViews.FindByPredicate([InTransform](const FOpenColorIODisplayView& InOtherDisplayView)
					{
						return InOtherDisplayView.Display.Equals(InTransform->Display) && InOtherDisplayView.View.Equals(InTransform->View);
					});

			}
			else
			{
				DestinationPtr = DesiredColorSpaces.FindByPredicate([InTransform](const FOpenColorIOColorSpace& InOtherColorSpace)
					{
						return InOtherColorSpace.ColorSpaceName == InTransform->DestinationColorSpace;
					});
			}

			if (DestinationPtr == nullptr)
			{
				return false;
			}

			// Both source and destination are in the desired collections, so we keep the transform.
			return true;
		});
}


void UOpenColorIOConfiguration::PostInitProperties()
{
	Super::PostInitProperties();

#if WITH_EDITOR
	if (!HasAnyFlags(RF_NeedLoad | RF_ClassDefaultObject))
	{
		ConfigurationFile.FilePath = TEXT("ocio://default");
		// Ensure the default built-in configuration is loaded.
		LoadConfiguration();
	}
#endif //WITH_EDITOR
}

void UOpenColorIOConfiguration::PostLoad()
{
	Super::PostLoad();
	ReloadExistingColorspaces();

	for (UOpenColorIOColorTransform* Transform : ColorTransforms)
	{
		Transform->ConditionalPostLoad();
	}
}

void UOpenColorIOConfiguration::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	Super::GetAssetRegistryTags(OutTags);

	FString Description;
	if (ConfigurationFile.FilePath.IsEmpty())
	{
		Description = TEXT("No configuration selected.");
	}
	else if (!Validate())
	{
		Description = TEXT("Warning: Configuration is invalid. Verify the selected configuration file.");
	}
	else
	{
		Description = TEXT("Configuration: ") + ConfigurationFile.FilePath;
	}

	OutTags.Add(FAssetRegistryTag(TEXT("ConfigurationFile"), Description, FAssetRegistryTag::TT_Hidden));
}

#if WITH_EDITORONLY_DATA
void UOpenColorIOConfiguration::DeclareConstructClasses(TArray<FTopLevelAssetPath>& OutConstructClasses, const UClass* SpecificSubclass)
{
	Super::DeclareConstructClasses(OutConstructClasses, SpecificSubclass);
	OutConstructClasses.Add(FTopLevelAssetPath(UOpenColorIOColorTransform::StaticClass()));
}
#endif

namespace OpenColorIOConfiguration
{
	static void SendAnalytics(const FString& EventName, const TArray<FOpenColorIOColorSpace>& DesiredColorSpaces)
	{
		if (!FEngineAnalytics::IsAvailable())
		{
			return;
		}

		TArray<FAnalyticsEventAttribute> EventAttributes;
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("NumDesiredColorSpaces"), DesiredColorSpaces.Num()));

		FEngineAnalytics::GetProvider().RecordEvent(EventName, EventAttributes);
	}
}

void UOpenColorIOConfiguration::PreSave(FObjectPreSaveContext SaveContext)
{
	Super::PreSave(SaveContext);

	OpenColorIOConfiguration::SendAnalytics(TEXT("Usage.OpenColorIO.ConfigAssetSaved"), DesiredColorSpaces);
}


#if WITH_EDITOR

void UOpenColorIOConfiguration::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const UOpenColorIOSettings* Settings = GetDefault<UOpenColorIOSettings>();

	if (PropertyChangedEvent.MemberProperty && PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UOpenColorIOConfiguration, ConfigurationFile))
	{
		// Note: Reload calls LoadConfiguration() internally.
		ReloadExistingColorspaces();
	}
	else if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UOpenColorIOConfiguration, DesiredColorSpaces))
	{
		if (PropertyChangedEvent.ChangeType & (EPropertyChangeType::ArrayAdd | EPropertyChangeType::Duplicate | EPropertyChangeType::ValueSet))
		{
			for (int32 indexTop = 0; indexTop < DesiredColorSpaces.Num(); ++indexTop)
			{
				const FOpenColorIOColorSpace& TopColorSpace = DesiredColorSpaces[indexTop];

				for (int32 indexOther = indexTop + 1; indexOther < DesiredColorSpaces.Num(); ++indexOther)
				{
					const FOpenColorIOColorSpace& OtherColorSpace = DesiredColorSpaces[indexOther];

					CreateColorTransform(TopColorSpace.ColorSpaceName, OtherColorSpace.ColorSpaceName);
					CreateColorTransform(OtherColorSpace.ColorSpaceName, TopColorSpace.ColorSpaceName);
				}

				for (const FOpenColorIODisplayView& DisplayView : DesiredDisplayViews)
				{
					CreateColorTransform(TopColorSpace.ColorSpaceName, DisplayView.Display, DisplayView.View, EOpenColorIOViewTransformDirection::Forward);

					if (Settings->bSupportInverseViewTransforms)
					{
						CreateColorTransform(TopColorSpace.ColorSpaceName, DisplayView.Display, DisplayView.View, EOpenColorIOViewTransformDirection::Inverse);
					}
				}
			}
		}

		if (PropertyChangedEvent.ChangeType & (EPropertyChangeType::ArrayRemove | EPropertyChangeType::ArrayClear | EPropertyChangeType::ValueSet))
		{
			CleanupTransforms();
		}
	}
	else if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UOpenColorIOConfiguration, DesiredDisplayViews))
	{
		if (PropertyChangedEvent.ChangeType & (EPropertyChangeType::ArrayAdd | EPropertyChangeType::Duplicate | EPropertyChangeType::ValueSet))
		{
			for (const FOpenColorIODisplayView& DisplayView : DesiredDisplayViews)
			{
				for (const FOpenColorIOColorSpace& SourceColorSpace : DesiredColorSpaces)
				{
					CreateColorTransform(SourceColorSpace.ColorSpaceName, DisplayView.Display, DisplayView.View, EOpenColorIOViewTransformDirection::Forward);

					if (Settings->bSupportInverseViewTransforms)
					{
						CreateColorTransform(SourceColorSpace.ColorSpaceName, DisplayView.Display, DisplayView.View, EOpenColorIOViewTransformDirection::Inverse);
					}
				}
			}
		}

		if (PropertyChangedEvent.ChangeType & (EPropertyChangeType::ArrayRemove | EPropertyChangeType::ArrayClear | EPropertyChangeType::ValueSet))
		{
			CleanupTransforms();
		}
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UOpenColorIOConfiguration::LoadConfiguration()
{
	Config.Reset();

	if (ConfigurationFile.FilePath.IsEmpty())
	{
		return;
	}

	bool bIsBuiltIn = false;
	FString FilePath = ConfigurationFile.FilePath;

	if (ConfigurationFile.FilePath.StartsWith(TEXT("ocio://")))
	{
		bIsBuiltIn = true;
	}
	else if(ConfigurationFile.FilePath.Contains(TEXT("{Engine}")))
	{
		FilePath = FPaths::ConvertRelativePathToFull(ConfigurationFile.FilePath.Replace(TEXT("{Engine}"), *FPaths::EngineDir()));
	}
	else if (FPaths::IsRelative(ConfigurationFile.FilePath))
	{
		const FString AbsoluteGameDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
		FilePath = FPaths::ConvertRelativePathToFull(FPaths::Combine(AbsoluteGameDir, ConfigurationFile.FilePath));
	}

	Config = MakePimpl<FOpenColorIOWrapperConfig>(FilePath, FOpenColorIOWrapperConfig::WCS_AsInterchangeSpace);

	if (Config->IsValid())
	{
		if (!bIsBuiltIn)
		{
			StartDirectoryWatch(FilePath);
		}

		UE_LOG(LogOpenColorIO, Verbose, TEXT("Loaded OCIO configuration file %s"), *FilePath);
	}
	else
	{
		UE_LOG(LogOpenColorIO, Error, TEXT("Could not load OCIO configuration file %s. Verify that the path is good or that the file is valid."), *FilePath);
	}
}

void UOpenColorIOConfiguration::OnToastCallback(bool bInReloadColorspaces)
{
	if (WatchedDirectoryInfo.RawConfigChangedToast.IsValid())
	{
		WatchedDirectoryInfo.RawConfigChangedToast.Pin()->SetCompletionState(SNotificationItem::CS_Success);
		WatchedDirectoryInfo.RawConfigChangedToast.Pin()->ExpireAndFadeout();
		WatchedDirectoryInfo.RawConfigChangedToast.Reset();
	}

	if (bInReloadColorspaces)
	{
		OpenColorIOWrapper::ClearAllCaches();

		ReloadExistingColorspaces();
	}
}


FOpenColorIOEditorConfigurationInspector::FOpenColorIOEditorConfigurationInspector(const UOpenColorIOConfiguration& InConfiguration)
	: Configuration(InConfiguration)
{
}

int32 FOpenColorIOEditorConfigurationInspector::GetNumColorSpaces() const
{
	return Configuration.GetConfigWrapper()->GetNumColorSpaces();
}

FString FOpenColorIOEditorConfigurationInspector::GetColorSpaceName(int32 Index) const
{
	return Configuration.GetConfigWrapper()->GetColorSpaceName(Index);
}

FString FOpenColorIOEditorConfigurationInspector::GetColorSpaceFamilyName(const TCHAR* InColorSpaceName) const
{
	return Configuration.GetConfigWrapper()->GetColorSpaceFamilyName(InColorSpaceName);
}

int32 FOpenColorIOEditorConfigurationInspector::GetNumDisplays() const
{
	return Configuration.GetConfigWrapper()->GetNumDisplays();
}

FString FOpenColorIOEditorConfigurationInspector::GetDisplayName(int32 Index) const
{
	return Configuration.GetConfigWrapper()->GetDisplayName(Index);
}

int32 FOpenColorIOEditorConfigurationInspector::GetNumViews(const TCHAR* InDisplayName) const
{
	return Configuration.GetConfigWrapper()->GetNumViews(InDisplayName);
}

FString FOpenColorIOEditorConfigurationInspector::GetViewName(const TCHAR* InDisplayName, int32 Index) const
{
	return Configuration.GetConfigWrapper()->GetViewName(InDisplayName, Index);
}
#endif //WITH_EDITOR


#undef LOCTEXT_NAMESPACE
