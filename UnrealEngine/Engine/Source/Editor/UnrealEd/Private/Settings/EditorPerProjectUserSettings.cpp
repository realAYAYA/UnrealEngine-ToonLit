// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/EditorPerProjectUserSettings.h"
#include "Misc/Paths.h"
#include "HAL/IConsoleManager.h"
#include "RHI.h"
#include "UnrealEdMisc.h"
#include "BlueprintPaletteFavorites.h"

#define LOCTEXT_NAMESPACE "EditorPerProjectUserSettings"

static TAutoConsoleVariable<int32> CVarNeverStartInPreviewMode(
	TEXT("r.Editor.NeverStartInPreviewMode"),
	0,
	TEXT("0: Editor can start in preview mode, 1: Editor never starts in preview mode"),
	ECVF_Default);

/// @cond DOXYGEN_WARNINGS

UEditorPerProjectUserSettings::UEditorPerProjectUserSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	//Default to high quality
	BlueprintFavorites = CreateDefaultSubobject<UBlueprintPaletteFavorites>(TEXT("BlueprintFavorites"));
	SCSViewportCameraSpeed = 4;
	AssetViewerProfileIndex = 0;
	bAnimationReimportWarnings = false;

	bDisplayBlackboardKeysInAlphabeticalOrder = false;

	SimplygonServerIP = "127.0.0.1";
	SimplygonSwarmDelay = 5000;
	bEnableSwarmDebugging = false;
	SwarmNumOfConcurrentJobs = 16;
	SwarmMaxUploadChunkSizeInMB = 100;
	SwarmIntermediateFolder = FPaths::ConvertRelativePathToFull(FPaths::ProjectIntermediateDir() + TEXT("Simplygon/"));
	PreviewFeatureLevel = GMaxRHIFeatureLevel;
	PreviewPlatformName = NAME_None;
	PreviewShaderFormatName = NAME_None;
	bPreviewFeatureLevelActive = false;
	bPreviewFeatureLevelWasDefault = true;
	PreviewDeviceProfileName = NAME_None;
	bShowSelectionSubcomponents = true;
}

void UEditorPerProjectUserSettings::PostInitProperties()
{
	Super::PostInitProperties();

	// if we last saved as the default or we somehow are loading a preview feature level higher than we can support or we explictly disabled it with r.Editor.NeverStartInPreviewMode,
	// fall back to the current session's maximum feature level
	if (bPreviewFeatureLevelWasDefault || PreviewFeatureLevel > GMaxRHIFeatureLevel || (CVarNeverStartInPreviewMode.GetValueOnAnyThread() != 0))
	{
		PreviewFeatureLevel = GMaxRHIFeatureLevel;
		PreviewShaderPlatformName = NAME_None;
		PreviewShaderFormatName = NAME_None;
        PreviewPlatformName = NAME_None;
		bPreviewFeatureLevelActive = false;
		bPreviewFeatureLevelWasDefault = true;
		PreviewDeviceProfileName = NAME_None;
	}
}

#if WITH_EDITOR
void UEditorPerProjectUserSettings::PostEditChangeProperty( FPropertyChangedEvent& PropertyChangedEvent )
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName Name = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	if (Name == FName(TEXT("bUseCurvesForDistributions")))
	{
		extern ENGINE_API uint32 GDistributionType;
		//GDistributionType == 0 for curves
		GDistributionType = (bUseCurvesForDistributions) ? 0 : 1;
	}

	if (!FUnrealEdMisc::Get().IsDeletePreferences())
	{
		SaveConfig();
	}

	UserSettingChangedEvent.Broadcast(Name);
}
#endif

/// @endcond

#undef LOCTEXT_NAMESPACE
