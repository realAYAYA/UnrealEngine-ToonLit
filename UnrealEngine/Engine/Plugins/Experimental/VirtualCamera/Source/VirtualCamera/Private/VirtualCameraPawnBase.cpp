// Copyright Epic Games, Inc. All Rights Reserved.

#include "VirtualCameraPawnBase.h"
#include "UnrealClient.h"
#include "UnrealEngine.h"
#include "Kismet/GameplayStatics.h"

LLM_DEFINE_TAG(VirtualCamera_VirtualCameraPawnBase);

int32 AVirtualCameraPawnBase::PresetIndex = 1;
int32 AVirtualCameraPawnBase::WaypointIndex = 1;
int32 AVirtualCameraPawnBase::ScreenshotIndex = 1;

AVirtualCameraPawnBase::AVirtualCameraPawnBase(const FObjectInitializer& ObjectInitializer)
{
	LLM_SCOPE_BYTAG(VirtualCamera_VirtualCameraPawnBase);
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	// Create Components
	DefaultSceneRoot = CreateDefaultSubobject<USceneComponent>("DefaultSceneRoot");
	SetRootComponent(DefaultSceneRoot);

	CineCamera = CreateDefaultSubobject<UVirtualCameraCineCameraComponent>("Cinematic Camera");
	CineCamera->SetupAttachment(DefaultSceneRoot);

	MovementComponent = CreateDefaultSubobject<UVirtualCameraMovementComponent>("Movement Component");
	MovementComponent->UpdatedComponent = CineCamera;
	MovementComponent->SetRootComponent(DefaultSceneRoot);

	// Set Default Variables
	HomeWaypointName = "";

	// Set formatter to pad with leading zeros to make integers 3 characters long.
	MinimumIntegralDigits = 3;

	SavedSettingsSlotName = "SavedVirtualCameraSettings";
	bSaveSettingsWhenClosing = false;
	DesiredDistanceUnits = EUnit::Meters;

	// By default, allow focus visualization
	bAllowFocusVisualization = true;

	FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(ToRawPtr(CineCamera), &UVirtualCameraCineCameraComponent::AllowCameraViewUpdates);
}

void AVirtualCameraPawnBase::BeginPlay()
{
	Super::BeginPlay();

	if (bSaveSettingsWhenClosing)
	{
		LoadSettings();
	}

	LoadFinished();
}

void AVirtualCameraPawnBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	if (bSaveSettingsWhenClosing)
	{
		SaveSettings();
	}
}

void AVirtualCameraPawnBase::GetWaypointInfo(const FString WaypointName, FVirtualCameraWaypoint &OutWaypointInfo) const 
{
	const FVirtualCameraWaypoint* WaypointInfoPtr = Waypoints.Find(WaypointName);
	if (WaypointInfoPtr)
	{
		OutWaypointInfo = *WaypointInfoPtr;
	}
}

void AVirtualCameraPawnBase::GetWaypointNames(TArray<FString>& OutArray) const 
{
	Waypoints.GetKeys(OutArray);
	OutArray.Sort();
}

void AVirtualCameraPawnBase::GetScreenshotInfo(const FString ScreenshotName, FVirtualCameraScreenshot &OutScreenshotInfo) const 
{
	const FVirtualCameraScreenshot* ScreenshotInfoPtr = Screenshots.Find(ScreenshotName);
	if (ScreenshotInfoPtr)
	{
		OutScreenshotInfo = *ScreenshotInfoPtr;
	}
}

void AVirtualCameraPawnBase::GetScreenshotNames(TArray<FString>& OutArray) const
{
	Screenshots.GetKeys(OutArray);
	OutArray.Sort();
}

bool AVirtualCameraPawnBase::RenameWaypoint(const FString& TargetWaypoint, const FString& NewWaypointName)
{
	if (Waypoints.Contains(TargetWaypoint))
	{
		Waypoints.FindOrAdd(NewWaypointName) = Waypoints[TargetWaypoint];
		Waypoints[NewWaypointName].Name = NewWaypointName;
		Waypoints.Remove(TargetWaypoint);
		return true;
	}

	return false;
}

FString AVirtualCameraPawnBase::SaveWaypoint()
{
	// Convert index to string with leading zeros
	FString WaypointNum = LeftPadWithZeros(WaypointIndex, MinimumIntegralDigits);

	// Another waypoint has been created
	WaypointIndex++;
	FVirtualCameraWaypoint::NextIndex++;

	// Create the waypoint
	FVirtualCameraWaypoint NewWaypoint;
	NewWaypoint.DateCreated = FDateTime::UtcNow();
	NewWaypoint.Name = "Waypoint-" + WaypointNum;
	NewWaypoint.WaypointTransform = CineCamera->GetComponentTransform();
	Waypoints.Add(NewWaypoint.Name, NewWaypoint);
	return NewWaypoint.Name;
}

FString AVirtualCameraPawnBase::SavePreset(const bool bSaveCameraSettings, const bool bSaveStabilization, const bool bSaveAxisLocking, const bool bSaveMotionScale)
{
	// Convert index to string with leading zeros
	FString PresetNum = LeftPadWithZeros(PresetIndex, MinimumIntegralDigits);
	FString PresetName = "Preset-" + PresetNum;
	
	// Another preset has been created
	PresetIndex++;
	FVirtualCameraSettingsPreset::NextIndex++;

	FVirtualCameraSettingsPreset PresetToAdd;
	PresetToAdd.DateCreated = FDateTime::UtcNow();

	PresetToAdd.bIsCameraSettingsSaved = bSaveCameraSettings;
	PresetToAdd.bIsStabilizationSettingsSaved = bSaveStabilization;
	PresetToAdd.bIsAxisLockingSettingsSaved = bSaveAxisLocking;
	PresetToAdd.bIsMotionScaleSettingsSaved = bSaveMotionScale;

	if (CineCamera)
	{
		PresetToAdd.CameraSettings.FocalLength = CineCamera->GetCurrentFocalLength();
		PresetToAdd.CameraSettings.Aperture = CineCamera->GetCurrentAperture();
		PresetToAdd.CameraSettings.FilmbackWidth = CineCamera->Filmback.SensorWidth;
		PresetToAdd.CameraSettings.FilmbackHeight = CineCamera->Filmback.SensorHeight;	
	}

	if (MovementComponent)
	{
		PresetToAdd.CameraSettings.AxisSettings = MovementComponent->AxisSettings;
	}

	SettingsPresets.Add(PresetName, PresetToAdd);

	SaveSettings();

	return PresetName;
}

TMap<FString, FVirtualCameraSettingsPreset> AVirtualCameraPawnBase::GetSettingsPresets()
{
	SettingsPresets.KeySort([](const FString& a, const FString& b) -> bool
	{
		return a < b;
	});

	return SettingsPresets;
}

bool AVirtualCameraPawnBase::LoadPreset(const FString& PresetName)
{
	FVirtualCameraSettingsPreset* LoadedPreset = SettingsPresets.Find(PresetName);

	if (LoadedPreset)
	{
		UpdateSettingsFromPreset(LoadedPreset);
		return true;
	}
	
	return false;
}

int32 AVirtualCameraPawnBase::DeletePreset(const FString& PresetName)
{
	return SettingsPresets.Remove(PresetName);
}

int32 AVirtualCameraPawnBase::DeleteScreenshot(const FString& ScreenshotName)
{
	return Screenshots.Remove(ScreenshotName);
}

int32 AVirtualCameraPawnBase::DeleteWaypoint(const FString& WaypointName)
{
	return Waypoints.Remove(WaypointName);
}

void AVirtualCameraPawnBase::UpdateSettingsFromPreset(FVirtualCameraSettingsPreset* PresetToLoad)
{
	if (MovementComponent) 
	{
		// Load all settings for all axes
		for (TPair<EVirtualCameraAxis, FVirtualCameraAxisSettings>& AxisSettingPair : MovementComponent->AxisSettings)
		{
			// Locking Settings
			if (PresetToLoad->bIsAxisLockingSettingsSaved)
			{
				AxisSettingPair.Value.bIsLocked = PresetToLoad->CameraSettings.AxisSettings[AxisSettingPair.Key].bIsLocked;	
			}
			
			// Movement Scaling
			if (PresetToLoad->bIsMotionScaleSettingsSaved)
			{
				AxisSettingPair.Value.MovementScale = PresetToLoad->CameraSettings.AxisSettings[AxisSettingPair.Key].MovementScale;
			}
			
			// Stabilization Scaling
			if (PresetToLoad->bIsStabilizationSettingsSaved)
			{
				AxisSettingPair.Value.StabilizationScale = PresetToLoad->CameraSettings.AxisSettings[AxisSettingPair.Key].StabilizationScale;
			}
		}
	}

	// Camera Settings
	if (CineCamera)
	{
		if (PresetToLoad->bIsCameraSettingsSaved)
		{
			CineCamera->CurrentAperture = PresetToLoad->CameraSettings.Aperture;
			CineCamera->CurrentFocalLength = PresetToLoad->CameraSettings.FocalLength;
			CineCamera->Filmback.SensorWidth = PresetToLoad->CameraSettings.FilmbackWidth;
			CineCamera->Filmback.SensorHeight = PresetToLoad->CameraSettings.FilmbackHeight;
		}
	}
	
	LoadFinished();
}

void AVirtualCameraPawnBase::SaveHomeWaypoint(const FString& NewHomeWaypointName)
{
	if (NewHomeWaypointName.IsEmpty() || Waypoints.Contains(NewHomeWaypointName))
	{
		// Remove home waypoint mark if one exists 
		if (Waypoints.Contains(HomeWaypointName))
		{
			Waypoints[HomeWaypointName].bIsHomeWaypoint = false;
		}
		
		HomeWaypointName = NewHomeWaypointName;

		// If updating to new home waypoint name, set to true
		if (!NewHomeWaypointName.IsEmpty()) 
		{
			Waypoints[HomeWaypointName].bIsHomeWaypoint = true;
		}
	}
}

bool AVirtualCameraPawnBase::TeleportToWaypoint(const FString& WaypointName)
{
	// Make sure waypoint index is valid
	if (!Waypoints.Contains(WaypointName))
	{
		return false;
	}

	MovementComponent->Teleport(Waypoints[WaypointName].WaypointTransform);
	return true;
}

bool AVirtualCameraPawnBase::TeleportToHomeWaypoint()
{
	return TeleportToWaypoint(HomeWaypointName);
}

FString AVirtualCameraPawnBase::TakeScreenshot()
{
	if (!CineCamera)
	{
		return FString();
	}

	// Convert index to string with leading zeros
	FString ScreenshotNum = LeftPadWithZeros(ScreenshotIndex, MinimumIntegralDigits);
	// Track that another screenshot has been added
	ScreenshotIndex++;
	FVirtualCameraScreenshot::NextIndex++;
	
	APlayerController* PC = Cast<APlayerController>(Controller);

	if (PC)
	{
		UWorld* World = GetWorld();
		// Apply aspect ratio restraints to image
		if (CineCamera && World)
		{
			CineCamera->Filmback = CineCamera->DesiredFilmbackSettings;
			CineCamera->StopCameraViewUpdates();

			FScreenshotRequest::RequestScreenshot(false);
		}
	}

	// Store all the data for this screenshot
	FVirtualCameraScreenshot NewScreenshot;
	NewScreenshot.Waypoint.DateCreated = FDateTime::UtcNow();
	
	// Screenshots are named with the "Screenshot-" prefix and their index.
	// Name and transform are saved on the screenshot's internal waypoint.
	NewScreenshot.Waypoint.Name = "Screenshot-" + ScreenshotNum;
	NewScreenshot.Waypoint.WaypointTransform = CineCamera->GetComponentTransform();

	NewScreenshot.CameraSettings.Aperture = CineCamera->GetCurrentAperture();
	NewScreenshot.CameraSettings.FocalLength = CineCamera->GetCurrentFocalLength();

	Screenshots.Add(NewScreenshot.Waypoint.Name, NewScreenshot);
	return NewScreenshot.Waypoint.Name;
}

bool AVirtualCameraPawnBase::LoadScreenshotView(const FString& ScreenshotName)
{
	// Make sure waypoint index is valid
	if (!CineCamera|| !Screenshots.Contains(ScreenshotName))
	{
		return false;
	}

	// Load the camera settings
	FVirtualCameraScreenshot &ScreenshotToLoad = Screenshots[ScreenshotName];
	int32 Index;
	
	// If the aperture is in the preset list, set it to that, otherwise do nothing
	if (CineCamera->ApertureOptions.Find(ScreenshotToLoad.CameraSettings.Aperture, Index))
	{
		CineCamera->CurrentAperture = CineCamera->ApertureOptions[Index];
	}
	
	if (CineCamera->FocalLengthOptions.Find(ScreenshotToLoad.CameraSettings.FocalLength, Index))
	{
		CineCamera->CurrentFocalLength = CineCamera->FocalLengthOptions[Index];
	}
	
	// If locked on to an actor, break out of that lock
	if (GetAttachParentActor())
	{
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	}

	MovementComponent->Teleport(ScreenshotToLoad.Waypoint.WaypointTransform);
	return true;
}

bool AVirtualCameraPawnBase::RenameScreenshotLocation(const FString& TargetScreenshot, const FString& NewScreenshotName)
{
	if (Screenshots.Contains(TargetScreenshot))
	{
		Screenshots.FindOrAdd(NewScreenshotName) = Screenshots[TargetScreenshot];
		Screenshots[NewScreenshotName].Waypoint.Name = NewScreenshotName;
		Screenshots.Remove(TargetScreenshot);
		return true;
	}

	return false;
}

void AVirtualCameraPawnBase::SetDesiredDistanceUnits(const EUnit DesiredUnits)
{
	if (FUnitConversion::IsUnitOfType(DesiredUnits, EUnitType::Distance))
	{
		DesiredDistanceUnits = DesiredUnits;
	}
}

void AVirtualCameraPawnBase::SetPresetFavoriteStatus(const FString& PresetName, const bool bIsFavorite)
{
	if (SettingsPresets.Contains(PresetName))
	{
		SettingsPresets[PresetName].bIsFavorited = bIsFavorite;
	}
}

void AVirtualCameraPawnBase::SetScreenshotFavoriteStatus(const FString& ScreenshotName, const bool bIsFavorite)
{
	if (Screenshots.Contains(ScreenshotName))
	{
		Screenshots[ScreenshotName].Waypoint.bIsFavorited = bIsFavorite;
	}
}

void AVirtualCameraPawnBase::SetWaypointFavoriteStatus(const FString& WaypointName, const bool bIsFavorite)
{
	if (Waypoints.Contains(WaypointName))
	{
		Waypoints[WaypointName].bIsFavorited = bIsFavorite;
	}
}

void AVirtualCameraPawnBase::SaveSettings()
{
	if (!CineCamera)
	{
		return;
	}

	UVirtualCameraSaveGame* SaveGameInstance = Cast<UVirtualCameraSaveGame>(UGameplayStatics::CreateSaveGameObject(UVirtualCameraSaveGame::StaticClass()));

	// Save waypoints
	SaveGameInstance->Waypoints = Waypoints;
	SaveGameInstance->HomeWaypointName = HomeWaypointName;

	// Save screenshots
	SaveGameInstance->Screenshots = Screenshots;

	// Save focal length and aperture
	SaveGameInstance->CameraSettings.FocalLength = CineCamera->GetCurrentFocalLength();
	SaveGameInstance->CameraSettings.Aperture = CineCamera->GetCurrentAperture();
	SaveGameInstance->CameraSettings.bAllowFocusVisualization = bAllowFocusVisualization;
#if WITH_EDITORONLY_DATA
	SaveGameInstance->CameraSettings.DebugFocusPlaneColor = CineCamera->FocusSettings.DebugFocusPlaneColor;
#else
	SaveGameInstance->CameraSettings.DebugFocusPlaneColor = FColor();
#endif

	// Save filmback settings
	SaveGameInstance->CameraSettings.FilmbackName = CineCamera->GetCurrentFilmbackName();
	SaveGameInstance->CameraSettings.FilmbackWidth = CineCamera->Filmback.SensorWidth;
	SaveGameInstance->CameraSettings.FilmbackHeight = CineCamera->Filmback.SensorHeight;
	SaveGameInstance->CameraSettings.MatteOpacity = CineCamera->MatteOpacity;

	// Save axis settings
	SaveGameInstance->CameraSettings.AxisSettings = MovementComponent->AxisSettings;

	// Save settings presets
	SaveGameInstance->SettingsPresets = SettingsPresets;

	// Save indices for naming
	SaveGameInstance->WaypointIndex = FVirtualCameraWaypoint::NextIndex;
	SaveGameInstance->ScreenshotIndex = FVirtualCameraScreenshot::NextIndex;
	SaveGameInstance->PresetIndex = FVirtualCameraSettingsPreset::NextIndex;

	SaveGameInstance->CameraSettings.DesiredDistanceUnits = DesiredDistanceUnits;

	// Write save file to disk
	UGameplayStatics::SaveGameToSlot(SaveGameInstance, SavedSettingsSlotName, 0);
}

void AVirtualCameraPawnBase::LoadSettings()
{
	if (!CineCamera)
	{
		return;
	}

	UVirtualCameraSaveGame* SaveGameInstance = Cast<UVirtualCameraSaveGame>(UGameplayStatics::CreateSaveGameObject(UVirtualCameraSaveGame::StaticClass()));
	SaveGameInstance = Cast<UVirtualCameraSaveGame>(UGameplayStatics::LoadGameFromSlot(SavedSettingsSlotName, 0));

	if (!SaveGameInstance)
	{
		UE_LOG(LogActor, Warning, TEXT("Virtual camera pawn could not find save game to load, using default settings."))
		return;
	}
	
	// Load waypoints
	Waypoints = SaveGameInstance->Waypoints;
	HomeWaypointName = SaveGameInstance->HomeWaypointName;

	FVirtualCameraWaypoint::NextIndex = SaveGameInstance->WaypointIndex;
	if (Waypoints.Num() > FVirtualCameraWaypoint::NextIndex)
	{
		FVirtualCameraWaypoint::NextIndex = Waypoints.Num();
	}

	// Load screenshots
	Screenshots = SaveGameInstance->Screenshots;

	FVirtualCameraScreenshot::NextIndex = SaveGameInstance->ScreenshotIndex;
	if (Screenshots.Num() > FVirtualCameraScreenshot::NextIndex)
	{
		FVirtualCameraScreenshot::NextIndex = Screenshots.Num();
	}

	bAllowFocusVisualization = SaveGameInstance->CameraSettings.bAllowFocusVisualization;

#if WITH_EDITORONLY_DATA
	if (SaveGameInstance->CameraSettings.DebugFocusPlaneColor != FColor())
	{
		CineCamera->FocusSettings.DebugFocusPlaneColor = SaveGameInstance->CameraSettings.DebugFocusPlaneColor;
	}
#endif

	// Load focal length
	if (CineCamera->FocalLengthOptions.Contains(SaveGameInstance->CameraSettings.FocalLength))
	{
		CineCamera->SetCurrentFocalLength(SaveGameInstance->CameraSettings.FocalLength);
	}

	// Load aperture
	if (CineCamera->ApertureOptions.Contains(SaveGameInstance->CameraSettings.Aperture))
	{
		CineCamera->SetCurrentAperture(SaveGameInstance->CameraSettings.Aperture);
	}

	// Load filmback settings
	if (!CineCamera->SetFilmbackPresetOption(SaveGameInstance->CameraSettings.FilmbackName))
	{
		// If name isn't found, use backup settings
		CineCamera->Filmback.SensorWidth = SaveGameInstance->CameraSettings.FilmbackWidth;
		CineCamera->Filmback.SensorHeight = SaveGameInstance->CameraSettings.FilmbackHeight;
	}
	CineCamera->MatteOpacity = SaveGameInstance->CameraSettings.MatteOpacity;

	MovementComponent->AxisSettings = SaveGameInstance->CameraSettings.AxisSettings;
	MovementComponent->ResetCameraOffsetsToTracker();

	// load presets, but don't overwrite existing ones
	SettingsPresets.Append(SaveGameInstance->SettingsPresets);

	// If the saved preset index is smaller than total presets, set it so that it won't overwrite existing presets.
	FVirtualCameraSettingsPreset::NextIndex = SaveGameInstance->PresetIndex;
	if (SettingsPresets.Num() > FVirtualCameraSettingsPreset::NextIndex)
	{
		FVirtualCameraSettingsPreset::NextIndex = SettingsPresets.Num();
	}

	// Load values of indices for naming
	PresetIndex = FVirtualCameraSettingsPreset::NextIndex;
	ScreenshotIndex = FVirtualCameraScreenshot::NextIndex;
	WaypointIndex = FVirtualCameraWaypoint::NextIndex;

	DesiredDistanceUnits = SaveGameInstance->CameraSettings.DesiredDistanceUnits;
}

FString AVirtualCameraPawnBase::LeftPadWithZeros(int32 InNumber, int32 MinNumberOfCharacters) const
{
	FString ReturnString = FString::FromInt(InNumber);

	while (ReturnString.Len() < MinNumberOfCharacters)
	{
		ReturnString = FString("0") + ReturnString;
	}

	return ReturnString;
}

//~ Begin IVirtualCameraPresetContainer Interface

FString AVirtualCameraPawnBase::SavePreset_Implementation(const bool bSaveCameraSettings, const bool bSaveStabilization, const bool bSaveAxisLocking, const bool bSaveMotionScale)
{
	return SavePreset(bSaveCameraSettings, bSaveStabilization, bSaveAxisLocking, bSaveMotionScale);
}

bool AVirtualCameraPawnBase::LoadPreset_Implementation(const FString& PresetName)
{
	return LoadPreset(PresetName);
}

int32 AVirtualCameraPawnBase::DeletePreset_Implementation(const FString& PresetName)
{
	return DeletePreset(PresetName);
}

TMap<FString, FVirtualCameraSettingsPreset> AVirtualCameraPawnBase::GetSettingsPresets_Implementation()
{
	return GetSettingsPresets();
}

//~ End IVirtualCameraPresetContainer Interface

//~ Begin IVirtualCameraOptions Interface

void AVirtualCameraPawnBase::SetDesiredDistanceUnits_Implementation(const EUnit DesiredUnits)
{
	SetDesiredDistanceUnits(DesiredUnits);
}

EUnit AVirtualCameraPawnBase::GetDesiredDistanceUnits_Implementation()
{
	return GetDesiredDistanceUnits();
}

bool AVirtualCameraPawnBase::IsFocusVisualizationAllowed_Implementation()
{
	return IsFocusVisualizationAllowed();
}

//~ End IVirtualCameraOptions Interface
