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
#include "TextureResource.h"
#include "Widgets/Notifications/SNotificationList.h"

#include "UObject/ObjectSaveContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(OpenColorIOConfiguration)

#define LOCTEXT_NAMESPACE "OCIOConfiguration"


#if WITH_EDITOR
#include "DerivedDataCacheInterface.h"
#include "DirectoryWatcherModule.h"
#include "IDirectoryWatcher.h"
#include "Interfaces/ITargetPlatform.h"
#endif //WITH_EDITOR

#if WITH_EDITOR && WITH_OCIO
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

bool UOpenColorIOConfiguration::GetRenderResources(ERHIFeatureLevel::Type InFeatureLevel, const FOpenColorIOColorConversionSettings& InSettings, FOpenColorIOTransformResource*& OutShaderResource, TSortedMap<int32, FTextureResource*>& OutTextureResources)
{
	TObjectPtr<UOpenColorIOColorTransform>* TransformPtr = ColorTransforms.FindByPredicate([&](const UOpenColorIOColorTransform* InTransform)
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

	if (TransformPtr == nullptr)
	{
		UE_LOG(LogOpenColorIO, Warning, TEXT("Color transform [%s] was not found."), *InSettings.ToString());
		return false;
	}

	UOpenColorIOColorTransform* Transform = *TransformPtr;
	return Transform->GetRenderResources(InFeatureLevel, OutShaderResource, OutTextureResources);
}

bool UOpenColorIOConfiguration::GetShaderAndLUTResources(ERHIFeatureLevel::Type InFeatureLevel, const FString& InSourceColorSpace, const FString& InDestinationColorSpace, FOpenColorIOTransformResource*& OutShaderResource, FTextureResource*& OutLUT3dResource)
{
	TSortedMap<int32, FTextureResource*> TextureResources;
	FOpenColorIOColorConversionSettings Settings;
	Settings.SourceColorSpace.ColorSpaceName = InSourceColorSpace;
	Settings.DestinationColorSpace.ColorSpaceName = InDestinationColorSpace;
	if (GetRenderResources(InFeatureLevel, Settings, OutShaderResource, TextureResources))
	{
		if (TextureResources.Contains(0))
		{
			OutLUT3dResource = TextureResources[0];
			return true;
		}
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

#if WITH_OCIO
	if (!ConfigurationFile.FilePath.IsEmpty())
	{
		//When loading the configuration file, if any errors are detected, it will throw an exception. Thus, our pointer won't be valid.
		return LoadedConfig != nullptr;
	}

	return false;
#else
	return false;
#endif // WITH_OCIO

#else
	return true;
#endif // WITH_EDITOR
}

void UOpenColorIOConfiguration::ReloadExistingColorspaces()
{
#if WITH_EDITOR && WITH_OCIO
	TArray<FOpenColorIOColorSpace> ColorSpacesToBeReloaded = DesiredColorSpaces;
	TArray<FOpenColorIODisplayView> DisplayViewsToBeReloaded = DesiredDisplayViews;
	DesiredColorSpaces.Reset();
	DesiredDisplayViews.Reset();
	CleanupTransforms();
	LoadConfigurationFile();

	if (!LoadedConfig)
	{
		return;
	}
	// This will make sure that all colorspaces are up to date in case an index, family or name is changed.
	for (const FOpenColorIOColorSpace& ExistingColorSpace : ColorSpacesToBeReloaded)
	{
		const auto ColorSpaceName = StringCast<ANSICHAR>(*ExistingColorSpace.ColorSpaceName);
		int ColorSpaceIndex = LoadedConfig->getIndexForColorSpace(ColorSpaceName.Get());
		OCIO_NAMESPACE::ConstColorSpaceRcPtr LibColorSpace = LoadedConfig->getColorSpace(ColorSpaceName.Get());
		if (!LibColorSpace)
		{
			// Name not found, therefore we don't need to re-add this colorspace.
			continue;
		}

		FOpenColorIOColorSpace ColorSpace;
		ColorSpace.ColorSpaceIndex = ColorSpaceIndex;
		ColorSpace.ColorSpaceName = ExistingColorSpace.ColorSpaceName;
		ColorSpace.FamilyName = StringCast<TCHAR>(LibColorSpace->getFamily()).Get();
		DesiredColorSpaces.Add(ColorSpace);
	}

	for (const FOpenColorIODisplayView& ExistingDisplayView : DisplayViewsToBeReloaded)
	{
		const auto DisplayName = StringCast<ANSICHAR>(*ExistingDisplayView.Display);
		const auto ViewName = StringCast<ANSICHAR>(*ExistingDisplayView.View);
		const auto TransformName = LoadedConfig->getDisplayViewTransformName(DisplayName.Get(), ViewName.Get());
		if (TransformName == nullptr)
		{
			// Name not found, therefore we don't need to re-add this display-view.
			continue;
		}

		DesiredDisplayViews.Add(ExistingDisplayView);
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
			// note: we only support display-view transforms in the forward direction currently.
			CreateColorTransform(TopColorSpace.ColorSpaceName, DisplayView.Display, DisplayView.View, EOpenColorIOViewTransformDirection::Forward);
		}
	}
#endif
}

void UOpenColorIOConfiguration::ConfigPathChangedEvent(const TArray<FFileChangeData>& InFileChanges, const FString InFileMountPath)
{
#if WITH_EDITOR && WITH_OCIO
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

void UOpenColorIOConfiguration::StartDirectoryWatch(const FString& FilePath)
{
#if WITH_EDITOR && WITH_OCIO
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
#if WITH_EDITOR && WITH_OCIO
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
	const bool bSuccess = NewTransform->Initialize(this, InSourceColorSpace, InDestinationColorSpace);

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
	const bool bSuccess = NewTransform->Initialize(this, InSourceColorSpace, InDisplay, InView, InDirection);

	if (bSuccess)
	{
		ColorTransforms.Add(NewTransform);
	}
	else
	{
		UE_LOG(LogOpenColorIO, Warning, TEXT("Could not create color space transform from %s to %s - %s. Verify your OCIO config file, it may have errors in it."), *InSourceColorSpace, *InDisplay, *InView);
	}
}

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

void UOpenColorIOConfiguration::PostLoad()
{
	Super::PostLoad();
	ReloadExistingColorspaces();

	for (UOpenColorIOColorTransform* Transform : ColorTransforms)
	{
		Transform->ConditionalPostLoad();
	}
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
	if (PropertyChangedEvent.MemberProperty && PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UOpenColorIOConfiguration, ConfigurationFile))
	{
		LoadConfigurationFile();
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
					// note: we only support display-view transforms in the forward direction currently.
					CreateColorTransform(TopColorSpace.ColorSpaceName, DisplayView.Display, DisplayView.View, EOpenColorIOViewTransformDirection::Forward);
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
					// note: we only support display-view transforms in the forward direction currently.
					CreateColorTransform(SourceColorSpace.ColorSpaceName, DisplayView.Display, DisplayView.View, EOpenColorIOViewTransformDirection::Forward);
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
#endif //WITH_EDITOR

void UOpenColorIOConfiguration::LoadConfigurationFile()
{
#if WITH_EDITOR && WITH_OCIO
	if (!ConfigurationFile.FilePath.IsEmpty())
	{
#if !PLATFORM_EXCEPTIONS_DISABLED
		try
#endif
		{
			LoadedConfig.reset();

			FString FullPath;
			FString ConfigurationFilePath = ConfigurationFile.FilePath;
			if (ConfigurationFilePath.Contains(TEXT("{Engine}")))
			{
				ConfigurationFilePath = FPaths::ConvertRelativePathToFull(ConfigurationFilePath.Replace(TEXT("{Engine}"), *FPaths::EngineDir()));
			}    

			if (!FPaths::IsRelative(ConfigurationFilePath))
			{
				FullPath = ConfigurationFilePath;
			}
			else
			{
				const FString AbsoluteGameDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
				FullPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(AbsoluteGameDir, ConfigurationFilePath));
			}

			OCIO_NAMESPACE::ConstConfigRcPtr NewConfig = OCIO_NAMESPACE::Config::CreateFromFile(StringCast<ANSICHAR>(*FullPath).Get());
			if (NewConfig)
			{
				UE_LOG(LogOpenColorIO, Verbose, TEXT("Loaded OCIO configuration file %s"), *FullPath);
				LoadedConfig = NewConfig;
				StartDirectoryWatch(FullPath);
			}
			else
			{
				UE_LOG(LogOpenColorIO, Error, TEXT("Could not load OCIO configuration file %s. Verify that the path is good or that the file is valid."), *ConfigurationFile.FilePath);
			}
		}
#if !PLATFORM_EXCEPTIONS_DISABLED
		catch (OCIO_NAMESPACE::Exception& exception)
		{
			UE_LOG(LogOpenColorIO, Error, TEXT("Could not load OCIO configuration file %s. Error message: %s."), *ConfigurationFile.FilePath, StringCast<TCHAR>(exception.what()).Get());
		}
#endif
	}
#endif
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
		ReloadExistingColorspaces();
	}
}

#undef LOCTEXT_NAMESPACE

