// Copyright Epic Games, Inc. All Rights Reserved.

#include "StormSyncDrivesModule.h"

#include "Engine/DeveloperSettings.h"
#include "Misc/CoreDelegates.h"
#include "Misc/PackageName.h"
#include "StormSyncDrivesLog.h"
#include "StormSyncDrivesSettings.h"
#include "StormSyncDrivesUtils.h"

#if WITH_EDITOR
#include "MessageLogModule.h"
#include "Logging/MessageLog.h"
#endif

#define LOCTEXT_NAMESPACE "FStormSyncDrivesModule"

void FStormSyncDrivesModule::StartupModule()
{
#if WITH_EDITOR
	// Create a message log for the validation to use
	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	FMessageLogInitializationOptions InitOptions;
	InitOptions.bShowPages = true;
	MessageLogModule.RegisterLogListing(LogName, LOCTEXT("StormSyncLogLabel", "Storm Sync Drives"), InitOptions);
	LogListing = MessageLogModule.GetLogListing(LogName);
#endif

	FCoreDelegates::OnFEngineLoopInitComplete.AddRaw(this, &FStormSyncDrivesModule::OnEngineLoopInitComplete);

#if WITH_EDITOR
	GetMutableDefault<UStormSyncDrivesSettings>()->OnSettingChanged().AddRaw(this, &FStormSyncDrivesModule::OnSettingsChanged);
#endif
}

void FStormSyncDrivesModule::ShutdownModule()
{
#if WITH_EDITOR
	if (FModuleManager::Get().IsModuleLoaded("MessageLog"))
	{
		// unregister message log
		FMessageLogModule& MessageLogModule = FModuleManager::GetModuleChecked<FMessageLogModule>("MessageLog");
		MessageLogModule.UnregisterLogListing(LogName);
	}
#endif

	FCoreDelegates::OnFEngineLoopInitComplete.RemoveAll(this);

#if WITH_EDITOR
	if (UObjectInitialized())
	{
		GetMutableDefault<UStormSyncDrivesSettings>()->OnSettingChanged().RemoveAll(this);
	}
#endif

	MountedDrives.Empty();
}

bool FStormSyncDrivesModule::RegisterMountPoint(const FStormSyncMountPointConfig& InMountPoint, FText& ErrorText)
{
	UE_LOG(LogStormSyncDrives, Display, TEXT("FStormSyncDrivesModule::RegisterMountPoint ... MountPoint: %s, Path: %s"), *InMountPoint.MountPoint, *InMountPoint.MountDirectory.Path);

	// Validate first configuration of the mount point
	FText ValidationError;
	if (!FStormSyncDrivesUtils::ValidateMountPoint(InMountPoint, ValidationError))
	{
		ErrorText = FText::Format(
			LOCTEXT("ValidationError_MountPoint", "Validation failed for MountPoint (MountPoint: {0}, MountDirectory: {1}) - {2}"),
			FText::FromString(InMountPoint.MountPoint),
			FText::FromString(InMountPoint.MountDirectory.Path),
			ValidationError
		);
		AddMessageError(ErrorText);
		return false;
	}

	// Append a trailing slash so that it is consistent with FLongPackagePathsSingleton::ContentPathToRoot entries and avoid a crash
	// with assets and reimport path when right clicking on them

	// ValidateMountPoint should ensure no trailing slash is allowed in user config
	const FString MountPoint = InMountPoint.MountPoint + TEXT("/");

	// Check if root path is already mounted
	if (FPackageName::MountPointExists(MountPoint))
	{
		ErrorText = FText::Format(
			LOCTEXT("ValidationError_MountPointExists", "Mount Point {0} already exists"),
			FText::FromString(MountPoint)
		);
		AddMessageError(ErrorText);
		return false;
	}

	UE_LOG(LogStormSyncDrives, Display, TEXT("FStormSyncDrivesModule::RegisterMountPoint ... Try register %s to %s"), *MountPoint, *InMountPoint.MountDirectory.Path);
	FPackageName::RegisterMountPoint(MountPoint, InMountPoint.MountDirectory.Path);
	return true;
}

bool FStormSyncDrivesModule::UnregisterMountPoint(const FStormSyncMountPointConfig& InMountPoint, FText& ErrorText)
{
	const FString ContentPath = InMountPoint.MountDirectory.Path;

	// Mount points are always added with a trailing slash (eg. /Foo => /Foo/)
	const bool bHasLeadingSlash = InMountPoint.MountPoint.EndsWith(TEXT("/"));
	const FString RootPath = bHasLeadingSlash ? InMountPoint.MountPoint : InMountPoint.MountPoint + TEXT("/");

	if (!FPackageName::MountPointExists(RootPath))
	{
		ErrorText = FText::Format(
			LOCTEXT("ValidationError_MountPointUnexisting", "Mount Point {0} does not exist"),
			FText::FromString(RootPath)
		);
		return false;
	}

	FPackageName::UnRegisterMountPoint(RootPath, ContentPath);
	return true;
}

void FStormSyncDrivesModule::OnEngineLoopInitComplete()
{
	UE_LOG(LogStormSyncDrives, Display, TEXT("FStormSyncDrivesModule::OnEngineLoopInitComplete ..."));
	UE_LOG(LogStormSyncDrives, Display, TEXT("\t Mounting Drives based on config"));

	ResetMountedDrivesFromSettings(GetDefault<UStormSyncDrivesSettings>());
}

void FStormSyncDrivesModule::OnSettingsChanged(UObject* InSettings, FPropertyChangedEvent& InPropertyChangedEvent)
{
	const FName PropertyName = InPropertyChangedEvent.GetPropertyName();
	const FName MemberPropertyName = (InPropertyChangedEvent.MemberProperty != nullptr) ? InPropertyChangedEvent.MemberProperty->GetFName() : NAME_None;
	const FName MountPointsPropertyName = GET_MEMBER_NAME_CHECKED(UStormSyncDrivesSettings, MountPoints);


	if (PropertyName == MountPointsPropertyName || MemberPropertyName == MountPointsPropertyName)
	{
		ResetMountedDrivesFromSettings(CastChecked<UStormSyncDrivesSettings>(InSettings));
	}
}

bool FStormSyncDrivesModule::ValidateMountPoint(const FStormSyncMountPointConfig& InMountPoint, int32 Index) const
{
	const FString RootPath = InMountPoint.MountPoint;
	const FString Directory = InMountPoint.MountDirectory.Path;

	FText ErrorText;
	if (!FStormSyncDrivesUtils::ValidateMountPoint(InMountPoint, ErrorText))
	{
		const FText ErrorLog = FText::Format(
			LOCTEXT("ValidationError_MountPointIndex", "Validation failed for MountPoint at index {0} (MountPoint: {1}, MountDirectory: {2}) - {3}"),
			FText::AsNumber(Index),
			FText::FromString(RootPath),
			FText::FromString(Directory),
			ErrorText
		);
		AddMessageError(ErrorLog);
		UE_LOG(LogStormSyncDrives, Error, TEXT("FStormSyncDrivesModule::ValidateMountPoint - %s"), *ErrorLog.ToString());
		return false;
	}

	return true;
}

bool FStormSyncDrivesModule::ValidateMountPoints(const TArray<FStormSyncMountPointConfig>& InMountPoints) const
{
#if WITH_EDITOR
	// Reset log listing and clear message on each validation run
	if (LogListing.IsValid())
	{
		LogListing->ClearMessages();
	}
#endif

	bool bResult = true;

	// First check for mount point validity individually
	int32 Index = 0;
	for (const FStormSyncMountPointConfig& Entry : InMountPoints)
	{
		UE_LOG(LogStormSyncDrives, Display, TEXT("Try validate entry %s %s"), *Entry.MountPoint, *Entry.MountDirectory.Path);

		// First check for mount point validity individually
		bResult &= ValidateMountPoint(Entry, Index);
		Index++;
	}

	// Then check for duplicates
	TArray<FText> ValidationErrors;
	if (!FStormSyncDrivesUtils::ValidateNonDuplicates(InMountPoints, ValidationErrors))
	{
		bResult = false;

		// Log any returned errors to message log
		for (const FText& ValidationError : ValidationErrors)
		{
			AddMessageError(ValidationError);
			UE_LOG(LogStormSyncDrives, Error, TEXT("FStormSyncDrivesModule::ValidateMountPoints - %s"), *ValidationError.ToString());
		}
	}

#if WITH_EDITOR
	// Got some errors, notify user
	if (!bResult)
	{
		LogListing->NotifyIfAnyMessages(LOCTEXT("Validation_Error", "Storm Sync: Error validating Mount Points settings"), EMessageSeverity::Error, true);
	}
#endif

	return bResult;
}

void FStormSyncDrivesModule::ResetMountedDrivesFromSettings(const UStormSyncDrivesSettings* InSettings)
{
	UE_LOG(LogStormSyncDrives, Display, TEXT("FStormSyncDrivesModule::ResetMountedDrives ..."));
	check(InSettings);

	if (ValidateMountPoints(InSettings->MountPoints))
	{
		UnregisterMountedDrives();
		CacheMountedDrives(InSettings->MountPoints);
		RegisterMountedDrives();
	}
}

void FStormSyncDrivesModule::UnregisterMountedDrives()
{
	UE_LOG(LogStormSyncDrives, Display, TEXT("FStormSyncDrivesModule::UnregisterMountedDrives ..."));
	for (const TSharedPtr<FStormSyncMountPointConfig>& MountedDrive : MountedDrives)
	{
		if (!MountedDrive.IsValid())
		{
			continue;
		}

		FText ErrorText;
		if (!UnregisterMountPoint(*MountedDrive.Get(), ErrorText))
		{
			UE_LOG(LogStormSyncDrives, Error, TEXT("FStormSyncDrivesModule::UnregisterMountedDrives failed with Error: %s"), *ErrorText.ToString());
		}
	}
}

void FStormSyncDrivesModule::CacheMountedDrives(const TArray<FStormSyncMountPointConfig>& InMountPoints)
{
	UE_LOG(LogStormSyncDrives, Display, TEXT("FStormSyncDrivesModule::CacheMountedDrives ..."));
	MountedDrives.Reset();

	for (const FStormSyncMountPointConfig& Entry : InMountPoints)
	{
		MountedDrives.Add(MakeShared<FStormSyncMountPointConfig>(Entry));
	}
}

void FStormSyncDrivesModule::RegisterMountedDrives()
{
	UE_LOG(LogStormSyncDrives, Display, TEXT("FStormSyncDrivesModule::RegisterMountedDrives ..."));
	for (TSharedPtr<FStormSyncMountPointConfig> MountedDrive : MountedDrives)
	{
		if (!MountedDrive.IsValid())
		{
			continue;
		}

		FText ErrorText;
		if (!RegisterMountPoint(*MountedDrive.Get(), ErrorText))
		{
			UE_LOG(LogStormSyncDrives, Error, TEXT("FStormSyncDrivesModule::RegisterMountedDrives failed with Error: %s"), *ErrorText.ToString());
		}
	}
}

void FStormSyncDrivesModule::AddMessageError(const FText& Text)
{
#if WITH_EDITOR
	FMessageLog MessageLog(LogName);
	MessageLog.Error(Text);
#endif
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FStormSyncDrivesModule, StormSyncDrives)
