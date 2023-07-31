// Copyright Epic Games, Inc. All Rights Reserved.

#include "GoogleARCoreDevice.h"
#include "CoreMinimal.h"
#include "Misc/ScopeLock.h"
#include "HAL/ThreadSafeCounter64.h"
#include "GameFramework/WorldSettings.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/Engine.h"
#include "GeneralProjectSettings.h"
#include "GoogleARCoreXRTrackingSystem.h"
#include "GoogleARCoreAndroidHelper.h"
#include "GoogleARCoreBaseLogCategory.h"
#include "GoogleARCoreTexture.h"

#if PLATFORM_ANDROID
#include "Android/AndroidApplication.h"
#endif

#include "GoogleARCorePermissionHandler.h"

DECLARE_CYCLE_STAT(TEXT("UpdateGameFrame"), STAT_UpdateGameFrame, STATGROUP_ARCore);

#define NUM_CAMERA_TEXTURES 4

int32 GARCoreEnableVulkanSupport = 0;
static FAutoConsoleVariableRef CVarARCoreEnableVulkanSupport(
	TEXT("r.ARCore.EnableVulkanSupport"),
	GARCoreEnableVulkanSupport,
	TEXT("Whether to support Vulkan in the ARCore plugin.\n")
	TEXT(" 0: Disabled (default)\n")
	TEXT(" 1: Enabled"),
	ECVF_Default);

namespace
{
	EGoogleARCoreFunctionStatus ToARCoreFunctionStatus(EGoogleARCoreAPIStatus Status)
	{
		switch (Status)
		{
		case EGoogleARCoreAPIStatus::AR_SUCCESS:
			return EGoogleARCoreFunctionStatus::Success;
		case EGoogleARCoreAPIStatus::AR_ERROR_NOT_TRACKING:
			return EGoogleARCoreFunctionStatus::NotTracking;
		case EGoogleARCoreAPIStatus::AR_ERROR_SESSION_PAUSED:
			return EGoogleARCoreFunctionStatus::SessionPaused;
		case EGoogleARCoreAPIStatus::AR_ERROR_RESOURCE_EXHAUSTED:
			return EGoogleARCoreFunctionStatus::ResourceExhausted;
		case EGoogleARCoreAPIStatus::AR_ERROR_NOT_YET_AVAILABLE:
			return EGoogleARCoreFunctionStatus::NotAvailable;
		case EGoogleARCoreAPIStatus::AR_ERROR_ILLEGAL_STATE:
			return EGoogleARCoreFunctionStatus::IllegalState;
		default:
			ensureMsgf(false, TEXT("Unknown conversion from EGoogleARCoreAPIStatus %d to EGoogleARCoreFunctionStatus."), static_cast<int>(Status));
			return EGoogleARCoreFunctionStatus::Unknown;
		}
	}
}

static bool ShouldUseVulkan()
{
#if PLATFORM_ANDROID
	return FAndroidMisc::ShouldUseVulkan();
#else
	return false;
#endif
}

FGoogleARCoreDelegates::FGoogleARCoreOnConfigCameraDelegate FGoogleARCoreDelegates::OnCameraConfig;

FGoogleARCoreDevice* FGoogleARCoreDevice::GetInstance()
{
	static FGoogleARCoreDevice Inst;
	return &Inst;
}

FGoogleARCoreDevice::FGoogleARCoreDevice()
	: bIsARCoreSessionRunning(false)
	, bStartSessionRequested(false)
	, bShouldSessionRestart(false)
	, bARCoreInstallRequested(false)
	, bARCoreInstalled(false)
	, WorldToMeterScale(100.0f)
	, PermissionHandler(nullptr)
	, bDisplayOrientationChanged(false)
	, CurrentSessionStatus(EARSessionStatus::NotStarted, TEXT("ARCore Session is uninitialized."))
{
}

EGoogleARCoreAvailability FGoogleARCoreDevice::CheckARCoreAPKAvailability()
{
	return FGoogleARCoreAPKManager::CheckARCoreAPKAvailability();
}

EGoogleARCoreAPIStatus FGoogleARCoreDevice::RequestInstall(bool bUserRequestedInstall, EGoogleARCoreInstallStatus& OutInstallStatus)
{
	return FGoogleARCoreAPKManager::RequestInstall(bUserRequestedInstall, OutInstallStatus);
}

bool FGoogleARCoreDevice::GetIsTrackingTypeSupported(EARSessionType SessionType)
{
	if (SessionType == EARSessionType::World || SessionType == EARSessionType::Face)
	{
		return true;
	}
	else
	{
		return false;
	}
}

void FGoogleARCoreDevice::OnModuleLoaded()
{
	// Init display orientation.
	OnDisplayOrientationChanged();
}

void FGoogleARCoreDevice::OnModuleUnloaded()
{
	// clear the shared ptr.
	ARCoreSession.Reset();
	FrontCameraARCoreSession.Reset();
	BackCameraARCoreSession.Reset();
}

bool FGoogleARCoreDevice::GetIsARCoreSessionRunning()
{
	return bIsARCoreSessionRunning;
}

FARSessionStatus FGoogleARCoreDevice::GetSessionStatus()
{
	return CurrentSessionStatus;
}

float FGoogleARCoreDevice::GetWorldToMetersScale()
{
	return WorldToMeterScale;
}

// This function will be called by public function to start AR core session request.
void FGoogleARCoreDevice::StartARCoreSessionRequest(UARSessionConfig* SessionConfig)
{
	UE_LOG(LogGoogleARCore, Log, TEXT("Start ARCore session requested."));
	
	if (!GCObject)
	{
		GCObject = MakeShared<FInternalGCObject, ESPMode::ThreadSafe>(this);
	}
	
#if PLATFORM_ANDROID
	// Vulkan support is currently disabled, until it's revisited and fixed
	if (ShouldUseVulkan()) // !GARCoreEnableVulkanSupport
	{
		UE_LOG(LogGoogleARCore, Error, TEXT("StartARCoreSessionRequest ignored - ARCore requires an OpenGL context, but we're using Vulkan!"));
		return;
	}
#endif
	
	// The new SessionConfig should be set to the session already at this point. We can check if it is front camera session here.
	bool bShouldUseFrontCamera = GetIsFrontCameraSession();
	bool bShouldSwitchCamera = bShouldUseFrontCamera ? ARCoreSession == BackCameraARCoreSession : ARCoreSession == FrontCameraARCoreSession;

	if (bIsARCoreSessionRunning && !bShouldSwitchCamera)
	{
		UE_LOG(LogGoogleARCore, Log, TEXT("ARCore session is already running, set it to use the new session config."));
		EGoogleARCoreAPIStatus Status = ARCoreSession->ConfigSession(*SessionConfig);
		ensureMsgf(Status == EGoogleARCoreAPIStatus::AR_SUCCESS, TEXT("Failed to set ARCore session to new configuration while it is running."));

		return;
	}

	if (bStartSessionRequested)
	{
		UE_LOG(LogGoogleARCore, Warning, TEXT("ARCore session is already starting. This will overriding the previous session config with the new one."))
	}
	else 
	{
		PauseARCoreSession();
	}

	bStartSessionRequested = true;
	bARCoreInstallRequested = false;

	// Try recreating the ARCoreSession to fix the fatal error.
	if (CurrentSessionStatus.Status == EARSessionStatus::FatalError)
	{
		UE_LOG(LogGoogleARCore, Warning, TEXT("Reset ARCore session due to fatal error detected."));
		ResetARCoreSession();
	}
}

bool FGoogleARCoreDevice::SetARCameraConfig(FGoogleARCoreCameraConfig CameraConfig)
{
	EGoogleARCoreAPIStatus APIStatus = ARCoreSession->SetCameraConfig(CameraConfig);
	if (APIStatus == EGoogleARCoreAPIStatus::AR_SUCCESS)
	{
		return true;
	}
	else if (APIStatus == EGoogleARCoreAPIStatus::AR_ERROR_SESSION_NOT_PAUSED)
	{
		UE_LOG(LogGoogleARCore, Error, TEXT("Failed to set ARCamera configuration due to the arcore session isn't paused."));
	}
	else
	{
		UE_LOG(LogGoogleARCore, Error, TEXT("Failed to set ARCamera configuration with provided CameraConfig."));
	}

	UE_LOG(LogGoogleARCore, Error, TEXT("You should only call the ConfigARCoreCamera function when the OnConfigCamera delegate get called, and the provided CameraConfig must be from the array that is passed by the delegate."));
	return false;
}

bool FGoogleARCoreDevice::GetARCameraConfig(FGoogleARCoreCameraConfig& OutCurrentCameraConfig)
{
	if (ARCoreSession.IsValid())
	{
		ARCoreSession->GetARCameraConfig(OutCurrentCameraConfig);
		return true;
	}
	else
	{
		return false;
	}
}

bool FGoogleARCoreDevice::GetIsFrontCameraSession()
{
	UARSessionConfig* CurrentConfig = AccessSessionConfig();

	if (CurrentConfig->GetSessionType() == EARSessionType::Face)
	{
		return true;
	}

	UGoogleARCoreSessionConfig* ARCoreSessionConfig = Cast<UGoogleARCoreSessionConfig>(CurrentConfig);
	if (ARCoreSessionConfig != nullptr && ARCoreSessionConfig->CameraFacing == EGoogleARCoreCameraFacing::Front)
	{
		return true;
	}

	return false;
}

bool FGoogleARCoreDevice::GetShouldInvertCulling()
{
	UARSessionConfig* CurrentConfig = AccessSessionConfig();
	UGoogleARCoreSessionConfig* ARCoreSessionConfig = Cast<UGoogleARCoreSessionConfig>(CurrentConfig);

	if(!CurrentConfig->ShouldEnableCameraTracking())
	{
		return false;
	}

	if (CurrentConfig->GetSessionType() == EARSessionType::Face)
	{
		return true;
	}

	if (ARCoreSessionConfig != nullptr && ARCoreSessionConfig->CameraFacing == EGoogleARCoreCameraFacing::Front)
	{
		return true;
	}

	return false;
}

int FGoogleARCoreDevice::AddRuntimeAugmentedImage(UGoogleARCoreAugmentedImageDatabase* TargetImageDatabase, const TArray<uint8>& ImageGrayscalePixels,
	int ImageWidth, int ImageHeight, FString ImageName, float ImageWidthInMeter)
{
	if (!ARCoreSession.IsValid())
	{
		UE_LOG(LogGoogleARCore, Warning, TEXT("Failed to add runtime augmented image: No valid session!"));
		return -1;
	}

	return ARCoreSession->AddRuntimeAugmentedImage(TargetImageDatabase, ImageGrayscalePixels, ImageWidth, ImageHeight, ImageName, ImageWidthInMeter);
}

bool FGoogleARCoreDevice::AddRuntimeCandidateImage(UARSessionConfig* SessionConfig, const TArray<uint8>& ImageGrayscalePixels, int ImageWidth, int ImageHeight,
	FString FriendlyName, float PhysicalWidth)
{
	if (!ARCoreSession.IsValid())
	{
		UE_LOG(LogGoogleARCore, Warning, TEXT("Failed to add runtime candidate image: No valid session!"));
		return false;
	}

	return ARCoreSession->AddRuntimeCandidateImage(SessionConfig, ImageGrayscalePixels, ImageWidth, ImageHeight, FriendlyName, PhysicalWidth);
}

bool FGoogleARCoreDevice::GetStartSessionRequestFinished()
{
	return !bStartSessionRequested;
}

// Note that this function will only be registered when ARCore is supported.
void FGoogleARCoreDevice::UpdateGameFrame(UWorld* World)
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateGameFrame);
	
	WorldToMeterScale = World->GetWorldSettings()->WorldToMeters;
	TFunction<void()> Func;
	while (RunOnGameThreadQueue.Dequeue(Func))
	{
		Func();
	}

	if (!bIsARCoreSessionRunning && bStartSessionRequested)
	{
		if (!bARCoreInstalled)
		{
			EGoogleARCoreInstallStatus InstallStatus = EGoogleARCoreInstallStatus::Installed;
			EGoogleARCoreAPIStatus Status = FGoogleARCoreAPKManager::RequestInstall(!bARCoreInstallRequested, InstallStatus);

			if (Status != EGoogleARCoreAPIStatus::AR_SUCCESS)
			{
				bStartSessionRequested = false;
				CurrentSessionStatus.Status = EARSessionStatus::NotSupported;
				CurrentSessionStatus.AdditionalInfo = TEXT("ARCore APK installation failed on this device.");
			}
			else if (InstallStatus == EGoogleARCoreInstallStatus::Installed)
			{
				bARCoreInstalled = true;
			}
			else
			{
				bARCoreInstallRequested = true;
			}
		}
		else if (PermissionStatus == EARCorePermissionStatus::Denied)
		{
			CurrentSessionStatus.Status = EARSessionStatus::PermissionNotGranted;
			CurrentSessionStatus.AdditionalInfo = TEXT("Camera permission has been denied by the user.");
			bStartSessionRequested = false;
		}
		else
		{
			CheckAndRequrestPermission(*AccessSessionConfig());
			// Either we don't need to request permission or the permission request is done.
			// Queue the session start task on UiThread
			if (PermissionStatus == EARCorePermissionStatus::Granted)
			{
				StartSessionWithRequestedConfig();
			}
		}
	}

	if (bIsARCoreSessionRunning)
	{
		// Update ARFrame
		FVector2D ViewportSize(1, 1);
		if (GEngine && GEngine->GameViewport)
		{
			ViewportSize = GEngine->GameViewport->Viewport->GetSizeXY();
		}
		ARCoreSession->SetDisplayGeometry(static_cast<int>(FGoogleARCoreAndroidHelper::GetDisplayRotation()), ViewportSize.X, ViewportSize.Y);
		EGoogleARCoreAPIStatus Status = ARCoreSession->Update(WorldToMeterScale);
		if (Status == EGoogleARCoreAPIStatus::AR_ERROR_FATAL)
		{
			ARCoreSession->Pause();
			bIsARCoreSessionRunning = false;
			CurrentSessionStatus.Status = EARSessionStatus::FatalError;
			CurrentSessionStatus.AdditionalInfo = TEXT("Fatal error occurred when updating ARCore Session. Stopping and restarting ARCore Session may fix the issue.");
		}
		else
		{
			if (auto Frame = ARCoreSession->GetLatestFrame())
			{
				LastCameraTextureId = Frame->GetCameraTextureId();
				if (ARCoreSession->IsSceneDepthEnabled())
				{
					Frame->UpdateDepthTexture(DepthTexture);
				}
			}
		}
	}
}

EARCorePermissionStatus FGoogleARCoreDevice::CheckAndRequrestPermission(const UARSessionConfig& ConfigurationData)
{
	if (PermissionStatus == EARCorePermissionStatus::Unknown)
	{
		TArray<FString> RuntimePermissions;
		TArray<FString> NeededPermissions;
		GetRequiredRuntimePermissionsForConfiguration(ConfigurationData, RuntimePermissions);
		if (RuntimePermissions.Num() > 0)
		{
			for (int32 i = 0; i < RuntimePermissions.Num(); i++)
			{
				if (!UARCoreAndroidPermissionHandler::CheckRuntimePermission(RuntimePermissions[i]))
				{
					NeededPermissions.Add(RuntimePermissions[i]);
				}
			}
		}
		
		if (NeededPermissions.Num() > 0)
		{
			PermissionStatus = EARCorePermissionStatus::Requested;
			if (PermissionHandler == nullptr)
			{
				PermissionHandler = NewObject<UARCoreAndroidPermissionHandler>();
			}
			PermissionHandler->RequestRuntimePermissions(NeededPermissions);
		}
		else
		{
			PermissionStatus = EARCorePermissionStatus::Granted;
		}
	}
	
	return PermissionStatus;
}

void FGoogleARCoreDevice::HandleRuntimePermissionsGranted(const TArray<FString>& RuntimePermissions, const TArray<bool>& Granted)
{
	bool bGranted = true;
	for (int32 i = 0; i < RuntimePermissions.Num(); i++)
	{
		if (!Granted[i])
		{
			bGranted = false;
			UE_LOG(LogGoogleARCore, Warning, TEXT("Android runtime permission denied: %s"), *RuntimePermissions[i]);
		}
		else
		{
			UE_LOG(LogGoogleARCore, Log, TEXT("Android runtime permission granted: %s"), *RuntimePermissions[i]);
		}
	}
	
	PermissionStatus = bGranted ? EARCorePermissionStatus::Granted : EARCorePermissionStatus::Denied;
}

void FGoogleARCoreDevice::StartSessionWithRequestedConfig()
{
	bStartSessionRequested = false;
	
	if (ShouldUseVulkan())
	{
		GLContext = FGoogleARCoreOpenGLContext::CreateContext();
	}
	
	UARSessionConfig* RequestedConfig = AccessSessionConfig();
	UGoogleARCoreSessionConfig* ARCoreConfig = Cast<UGoogleARCoreSessionConfig>(RequestedConfig);
	bool bUseFrontCamera = (ARCoreConfig != nullptr && ARCoreConfig->CameraFacing == EGoogleARCoreCameraFacing::Front)
		|| RequestedConfig->GetSessionType() == EARSessionType::Face;

	if (bUseFrontCamera)
	{
		if (!FrontCameraARCoreSession.IsValid())
		{
			FrontCameraARCoreSession = CreateSession(true);
		}

		ARCoreSession = FrontCameraARCoreSession;
	}
	else
	{
		if (!BackCameraARCoreSession.IsValid())
		{
			BackCameraARCoreSession = CreateSession(false);
		}

		ARCoreSession = BackCameraARCoreSession;
	}

	if (!ARCoreSession.IsValid())
	{
		return;
	}
	
	// Allocate passthrough camera texture if necessary.
	if (!PassthroughCameraTextures.Num())
	{
		AllocatePassthroughCameraTextures();
	}

	StartSession();
}

TSharedPtr<FGoogleARCoreSession> FGoogleARCoreDevice::CreateSession(bool bUseFrontCamera)
{
	TSharedPtr<FGoogleARCoreSession> NewARCoreSession = FGoogleARCoreSession::CreateARCoreSession(bUseFrontCamera);
	EGoogleARCoreAPIStatus SessionCreateStatus = NewARCoreSession->GetSessionCreateStatus();
	if (SessionCreateStatus != EGoogleARCoreAPIStatus::AR_SUCCESS)
	{
		ensureMsgf(false, TEXT("Failed to create ARCore session with error status: %d"), (int)SessionCreateStatus);
		CurrentSessionStatus.AdditionalInfo =
			FString::Printf(TEXT("Failed to create ARCore session with error status: %d"), (int)SessionCreateStatus);

		if (SessionCreateStatus != EGoogleARCoreAPIStatus::AR_ERROR_FATAL)
		{
			CurrentSessionStatus.Status = EARSessionStatus::NotSupported;
		}
		else
		{
			CurrentSessionStatus.Status = EARSessionStatus::FatalError;
		}

		NewARCoreSession.Reset();
	}
	else
	{
		NewARCoreSession->SetARSystem(ARSystem.ToSharedRef());
	}

	return NewARCoreSession;
}


void FGoogleARCoreDevice::StartSession()
{
	UARSessionConfig* RequestedConfig = AccessSessionConfig();

	if (!GetIsTrackingTypeSupported(RequestedConfig->GetSessionType()))
	{
		UE_LOG(LogGoogleARCore, Warning, TEXT("Start AR failed: Unsupported AR tracking type %d for GoogleARCore"), static_cast<int>(RequestedConfig->GetSessionType()));
		CurrentSessionStatus.AdditionalInfo = TEXT("Unsupported AR tracking type. Only EARSessionType::World is supported by ARCore.");
		CurrentSessionStatus.Status = EARSessionStatus::UnsupportedConfiguration;
		return;
	}

	EGoogleARCoreAPIStatus Status = ARCoreSession->ConfigSession(*RequestedConfig);

	if (Status != EGoogleARCoreAPIStatus::AR_SUCCESS)
	{
		UE_LOG(LogGoogleARCore, Error, TEXT("ARCore Session start failed with error status %d"), static_cast<int>(Status));
		CurrentSessionStatus.AdditionalInfo = TEXT("ARCore Session start failed due to unsupported ARSessionConfig.");
		CurrentSessionStatus.Status = EARSessionStatus::UnsupportedConfiguration;
		return;
	}

	check(PassthroughCameraTextures.Num());
	TArray<uint32> TextureIds;
	PassthroughCameraTextures.GetKeys(TextureIds);
	ARCoreSession->SetCameraTextureIds(TextureIds);

	EGoogleARCoreCameraFacing CameraFacing = EGoogleARCoreCameraFacing::Back;
	const UGoogleARCoreSessionConfig *GoogleConfig = Cast<UGoogleARCoreSessionConfig>(RequestedConfig);
	if (GoogleConfig != nullptr)
	{
		CameraFacing = GoogleConfig->CameraFacing;
	}

	TArray<FGoogleARCoreCameraConfig> SupportedCameraConfigs = ARCoreSession->GetSupportedCameraConfig();
	FGoogleARCoreDelegates::OnCameraConfig.Broadcast(SupportedCameraConfigs);
	
	UE_LOG(LogGoogleARCore, Log, TEXT("Got %d supported camera config from ARCore:"), SupportedCameraConfigs.Num());
	for (int Index = 0; Index < SupportedCameraConfigs.Num(); ++Index)
	{
		UE_LOG(LogGoogleARCore, Log, TEXT("%d: %s"), Index + 1, *SupportedCameraConfigs[Index].ToLogString());
	}

	// If the session config specifies a meaningful video format, we'll try to match it here
	const FARVideoFormat DesiredVideoFormat = RequestedConfig->GetDesiredVideoFormat();
	if (DesiredVideoFormat.Width > 0 && DesiredVideoFormat.Height > 0)
	{
		if (DesiredVideoFormat.FPS == 30 || DesiredVideoFormat.FPS == 60)
		{
			const FGoogleARCoreCameraConfig* MatchCameraConfig = nullptr;
			for (const FGoogleARCoreCameraConfig& CameraConfig : SupportedCameraConfigs)
			{
				if (CameraConfig.CameraTextureResolution.X == DesiredVideoFormat.Width &&
					CameraConfig.CameraTextureResolution.Y == DesiredVideoFormat.Height &&
					CameraConfig.GetMaxFPS() == DesiredVideoFormat.FPS)
				{
					MatchCameraConfig = &CameraConfig;
					break;
				}
			}

			if (MatchCameraConfig)
			{
				SetARCameraConfig(*MatchCameraConfig);
				UE_LOG(LogGoogleARCore, Log, TEXT("Found and applied camera config matching video format (%d x %d, %d FPS): %s"),
					DesiredVideoFormat.Width, DesiredVideoFormat.Height, DesiredVideoFormat.FPS, *MatchCameraConfig->ToLogString());
			}
			else
			{
				UE_LOG(LogGoogleARCore, Warning, TEXT("Couldn't find any camera config matching desired video format (%d x %d, %d FPS)"),
					DesiredVideoFormat.Width, DesiredVideoFormat.Height, DesiredVideoFormat.FPS);
			}
		}
		else
		{
			UE_LOG(LogGoogleARCore, Warning, TEXT("ARCore camera only supports 30/60 FPS"));
		}
	}

	Status = ARCoreSession->Resume();

	if (Status != EGoogleARCoreAPIStatus::AR_SUCCESS)
	{
		UE_LOG(LogGoogleARCore, Error, TEXT("ARCore Session start failed with error status %d"), static_cast<int>(Status));

		if (Status == EGoogleARCoreAPIStatus::AR_ERROR_ILLEGAL_STATE)
		{
			CurrentSessionStatus.AdditionalInfo = TEXT("Failed to start ARCore Session due to illegal state: All camera images previously acquired must be released before resuming the session with a different camera configuration.");
			CurrentSessionStatus.Status = EARSessionStatus::Other;
		}
		else
		{
			// If we failed here, the only reason would be fatal error.
			CurrentSessionStatus.AdditionalInfo = TEXT("Fatal error occurred when starting ARCore Session. Stopping and restarting ARCore Session may fix the issue.");
			CurrentSessionStatus.Status = EARSessionStatus::FatalError;
		}

		return;
	}

	if (auto TrackingSystem = FGoogleARCoreXRTrackingSystem::GetInstance())
	{
		const bool bMatchFOV = RequestedConfig->ShouldRenderCameraOverlay();
		TrackingSystem->ConfigARCoreXRCamera(bMatchFOV, RequestedConfig->ShouldRenderCameraOverlay());
	}
	else
	{
		UE_LOG(LogGoogleARCore, Error, TEXT("ERROR: GoogleARCoreXRTrackingSystem is not available."));
	}
	
	ARCoreSession->GetARCameraConfig(SessionCameraConfig);

	bIsARCoreSessionRunning = true;
	CurrentSessionStatus.Status = EARSessionStatus::Running;
	CurrentSessionStatus.AdditionalInfo = TEXT("ARCore Session is running.");
	UE_LOG(LogGoogleARCore, Log, TEXT("ARCore session started successfully."));

	ARSystem->OnARSessionStarted.Broadcast();
}

void FGoogleARCoreDevice::SetARSystem(TSharedPtr<FARSupportInterface , ESPMode::ThreadSafe> InARSystem)
{
	check(InARSystem.IsValid());
	ARSystem = InARSystem;
}

void* FGoogleARCoreDevice::GetARSessionRawPointer()
{
#if PLATFORM_ANDROID
	return reinterpret_cast<void*>(ARCoreSession->GetHandle());
#endif
	return nullptr;
}

void* FGoogleARCoreDevice::GetGameThreadARFrameRawPointer()
{
#if PLATFORM_ANDROID
	return reinterpret_cast<void*>(ARCoreSession->GetLatestFrameRawPointer());
#endif
	return nullptr;
}

TSharedPtr<FARSupportInterface , ESPMode::ThreadSafe> FGoogleARCoreDevice::GetARSystem()
{
	return ARSystem;
}

void FGoogleARCoreDevice::PauseARCoreSession()
{
	UE_LOG(LogGoogleARCore, Log, TEXT("Pausing ARCore session."));
	if (!bIsARCoreSessionRunning)
	{
		if(bStartSessionRequested)
		{
			bStartSessionRequested = false;
		}
		else
		{
			UE_LOG(LogGoogleARCore, Log, TEXT("Could not stop ARCore tracking session because there is no running tracking session!"));
		}
		return;
	}

	EGoogleARCoreAPIStatus Status = ARCoreSession->Pause();

	if (Status == EGoogleARCoreAPIStatus::AR_ERROR_FATAL)
	{
		CurrentSessionStatus.Status = EARSessionStatus::FatalError;
		CurrentSessionStatus.AdditionalInfo = TEXT("Fatal error occurred when starting ARCore Session. Stopping and restarting ARCore Session may fix the issue.");
	}
	else
	{
		CurrentSessionStatus.Status = EARSessionStatus::NotStarted;
		CurrentSessionStatus.AdditionalInfo = TEXT("ARCore Session is paused.");
	}
	bIsARCoreSessionRunning = false;
	UE_LOG(LogGoogleARCore, Log, TEXT("ARCore session paused"));
}

void FGoogleARCoreDevice::ResetARCoreSession()
{
	ARCoreSession.Reset();
	FrontCameraARCoreSession.Reset();
	BackCameraARCoreSession.Reset();
	CurrentSessionStatus.Status = EARSessionStatus::NotStarted;
	CurrentSessionStatus.AdditionalInfo = TEXT("ARCore Session is uninitialized.");
	if (GLContext)
	{
		GLContext = nullptr;
	}
	
	PassthroughCameraTextures = {};
	LastCameraTextureId = 0;
}

void FGoogleARCoreDevice::AllocatePassthroughCameraTextures()
{
	FGoogleARCoreCameraConfig CameraConfig;
	GetARCameraConfig(CameraConfig);
	
	TArray<UARCoreCameraTexture*> Textures;
	for (auto Index = 0; Index < NUM_CAMERA_TEXTURES; ++Index)
	{
		if (auto Texture = UARTexture::CreateARTexture<UARCoreCameraTexture>(EARTextureType::CameraImage))
		{
			Texture->Size = { (float)CameraConfig.CameraTextureResolution.X, (float)CameraConfig.CameraTextureResolution.Y };
			Texture->UpdateResource();
			Textures.Add(Texture);
		}
	}
	
	FlushRenderingCommands();
	
	for (auto Texture : Textures)
	{
		auto TextureId = Texture->GetTextureId();
		check(TextureId);
		PassthroughCameraTextures.Add(TextureId, Texture);
		UE_LOG(LogGoogleARCore, Log, TEXT("Created external camera texture of size (%.0f x %.0f) with texture Id (%d) and external texture Guid (%s)"),
			   Texture->Size.X, Texture->Size.Y, TextureId, *Texture->ExternalTextureGuid.ToString());
	}
}

FMatrix FGoogleARCoreDevice::GetPassthroughCameraProjectionMatrix(FIntPoint ViewRectSize) const
{
	if (!ARCoreSession.IsValid() || ARCoreSession->GetLatestFrame() == nullptr)
	{
		return FMatrix::Identity;
	}
	return ARCoreSession->GetLatestFrame()->GetProjectionMatrix();
}

void FGoogleARCoreDevice::GetPassthroughCameraImageUVs(const TArray<float>& InUvs, TArray<float>& OutUVs) const
{
	if (!ARCoreSession.IsValid() || ARCoreSession->GetLatestFrame() == nullptr)
	{
		return;
	}
	ARCoreSession->GetLatestFrame()->TransformDisplayUvCoords(InUvs, OutUVs);
}

int64 FGoogleARCoreDevice::GetPassthroughCameraTimestamp() const
{
	if (!ARCoreSession.IsValid() || ARCoreSession->GetLatestFrame() == nullptr)
	{
		return 0;
	}
	return ARCoreSession->GetLatestFrame()->GetCameraTimestamp();
}

EGoogleARCoreTrackingState FGoogleARCoreDevice::GetTrackingState() const
{
	if (!ARCoreSession.IsValid() || ARCoreSession->GetLatestFrame() == nullptr)
	{
		return EGoogleARCoreTrackingState::StoppedTracking;
	}
	else if (!bIsARCoreSessionRunning)
	{
		return EGoogleARCoreTrackingState::NotTracking;
	}

	return ARCoreSession->GetLatestFrame()->GetCameraTrackingState();
}

EGoogleARCoreTrackingFailureReason FGoogleARCoreDevice::GetTrackingFailureReason() const
{
	if (!ARCoreSession.IsValid() || ARCoreSession->GetLatestFrame() == nullptr)
	{
		return EGoogleARCoreTrackingFailureReason::None;
	}

	return ARCoreSession->GetLatestFrame()->GetCameraTrackingFailureReason();
}

FTransform FGoogleARCoreDevice::GetLatestPose() const
{
	if (!ARCoreSession.IsValid() || ARCoreSession->GetLatestFrame() == nullptr)
	{
		return FTransform::Identity;
	}
	return ARCoreSession->GetLatestFrame()->GetCameraPose();
}

EGoogleARCoreFunctionStatus FGoogleARCoreDevice::GetLatestPointCloud(UGoogleARCorePointCloud*& OutLatestPointCloud) const
{
	if (!ARCoreSession.IsValid() || ARCoreSession->GetLatestFrame() == nullptr)
	{
		return EGoogleARCoreFunctionStatus::NotAvailable;
	}

	return ToARCoreFunctionStatus(ARCoreSession->GetLatestFrame()->GetPointCloud(OutLatestPointCloud));
}

EGoogleARCoreFunctionStatus FGoogleARCoreDevice::AcquireLatestPointCloud(UGoogleARCorePointCloud*& OutLatestPointCloud) const
{
	if (!ARCoreSession.IsValid() || ARCoreSession->GetLatestFrame() == nullptr)
	{
		return EGoogleARCoreFunctionStatus::NotAvailable;
	}

	return ToARCoreFunctionStatus(ARCoreSession->GetLatestFrame()->AcquirePointCloud(OutLatestPointCloud));
}

#if PLATFORM_ANDROID
EGoogleARCoreFunctionStatus FGoogleARCoreDevice::GetLatestCameraMetadata(const ACameraMetadata*& OutCameraMetadata) const
{
	if (!ARCoreSession.IsValid() || ARCoreSession->GetLatestFrame() == nullptr)
	{
		return EGoogleARCoreFunctionStatus::NotAvailable;
	}

	return ToARCoreFunctionStatus(ARCoreSession->GetLatestFrame()->GetCameraMetadata(OutCameraMetadata));
}
#endif

EGoogleARCoreFunctionStatus FGoogleARCoreDevice::AcquireCameraImage(UGoogleARCoreCameraImage *&OutLatestCameraImage)
{
	if (!ARCoreSession.IsValid() || ARCoreSession->GetLatestFrame() == nullptr)
	{
		return EGoogleARCoreFunctionStatus::NotAvailable;
	}

	return ToARCoreFunctionStatus(ARCoreSession->GetLatestFrame()->AcquireCameraImage(OutLatestCameraImage));
}

void FGoogleARCoreDevice::TransformARCoordinates2D(EGoogleARCoreCoordinates2DType InputCoordinatesType, const TArray<FVector2D>& InputCoordinates, EGoogleARCoreCoordinates2DType OutputCoordinatesType, TArray<FVector2D>& OutputCoordinates) const
{
	if (!ARCoreSession.IsValid() || ARCoreSession->GetLatestFrame() == nullptr)
	{
		UE_LOG(LogGoogleARCore, Log, TEXT("Failed to transform ARCoordinate2D due to thers is no valid ARCore session."));
		return;
	}

	return ARCoreSession->GetLatestFrame()->TransformARCoordinates2D(InputCoordinatesType, InputCoordinates, OutputCoordinatesType, OutputCoordinates);
}

FGoogleARCoreLightEstimate FGoogleARCoreDevice::GetLatestLightEstimate() const
{
	if (!ARCoreSession.IsValid() || ARCoreSession->GetLatestFrame() == nullptr)
	{
		return FGoogleARCoreLightEstimate();
	}

	return ARCoreSession->GetLatestFrame()->GetLightEstimate();
}

void FGoogleARCoreDevice::ARLineTrace(const FVector2D& ScreenPosition, EGoogleARCoreLineTraceChannel TraceChannels, TArray<FARTraceResult>& OutHitResults)
{
	if (!ARCoreSession.IsValid() || ARCoreSession->GetLatestFrame() == nullptr)
	{
		return;
	}
	OutHitResults.Empty();
	ARCoreSession->GetLatestFrame()->ARLineTrace(ScreenPosition, TraceChannels, OutHitResults);
}

void FGoogleARCoreDevice::ARLineTrace(const FVector& Start, const FVector& End, EGoogleARCoreLineTraceChannel TraceChannels, TArray<FARTraceResult>& OutHitResults)
{
	if (!ARCoreSession.IsValid() || ARCoreSession->GetLatestFrame() == nullptr)
	{
		return;
	}
	OutHitResults.Empty();
	ARCoreSession->GetLatestFrame()->ARLineTrace(Start, End, TraceChannels, OutHitResults);
}

EGoogleARCoreFunctionStatus FGoogleARCoreDevice::CreateARPin(const FTransform& PinToWorldTransform, UARTrackedGeometry* TrackedGeometry, USceneComponent* ComponentToPin, const FName DebugName, UARPin*& OutARAnchorObject)
{
	if (!bIsARCoreSessionRunning)
	{
		return EGoogleARCoreFunctionStatus::SessionPaused;
	}

	const FTransform& TrackingToAlignedTracking = ARSystem->GetAlignmentTransform();
	const FTransform PinToTrackingTransform = PinToWorldTransform.GetRelativeTransform(ARSystem->GetXRTrackingSystem()->GetTrackingToWorldTransform()).GetRelativeTransform(TrackingToAlignedTracking);

	EGoogleARCoreFunctionStatus Status = ToARCoreFunctionStatus(ARCoreSession->CreateARAnchor(PinToTrackingTransform, TrackedGeometry, ComponentToPin, DebugName, OutARAnchorObject));

	return Status;
}

bool FGoogleARCoreDevice::TryGetOrCreatePinForNativeResource(void* InNativeResource, const FString& InPinName, UARPin*& OutPin)
{
	OutPin = nullptr;
	if (!bIsARCoreSessionRunning)
	{
		return false;
	}

	return ARCoreSession->TryGetOrCreatePinForNativeResource(InNativeResource, InPinName, OutPin);
}

void FGoogleARCoreDevice::RemoveARPin(UARPin* ARAnchorObject)
{
	if (!ARCoreSession.IsValid())
	{
		return;
	}

	ARCoreSession->DetachAnchor(ARAnchorObject);
}

void FGoogleARCoreDevice::GetAllARPins(TArray<UARPin*>& ARCoreAnchorList)
{
	if (!ARCoreSession.IsValid())
	{
		return;
	}
	ARCoreSession->GetAllAnchors(ARCoreAnchorList);
}

void FGoogleARCoreDevice::GetUpdatedARPins(TArray<UARPin*>& ARCoreAnchorList)
{
	if (!ARCoreSession.IsValid() || ARCoreSession->GetLatestFrame() == nullptr)
	{
		return;
	}
	ARCoreSession->GetLatestFrame()->GetUpdatedAnchors(ARCoreAnchorList);
}

// Functions that are called on Android lifecycle events.
void FGoogleARCoreDevice::OnApplicationCreated()
{
}

void FGoogleARCoreDevice::OnApplicationDestroyed()
{
}

void FGoogleARCoreDevice::OnApplicationPause()
{
	UE_LOG(LogGoogleARCore, Log, TEXT("OnPause Called: %d"), bIsARCoreSessionRunning);
	bShouldSessionRestart = bIsARCoreSessionRunning;
	if (bIsARCoreSessionRunning)
	{
		PauseARCoreSession();
	}
}

void FGoogleARCoreDevice::OnApplicationResume()
{
	UE_LOG(LogGoogleARCore, Log, TEXT("OnResume Called: %d"), bShouldSessionRestart);
	// Try to ask for permission if it is denied by user.
	if (bShouldSessionRestart)
	{
		bShouldSessionRestart = false;
		StartSession();
	}
}

void FGoogleARCoreDevice::OnApplicationStop()
{
}

void FGoogleARCoreDevice::OnApplicationStart()
{
}

// TODO: we probably don't need this.
void FGoogleARCoreDevice::OnDisplayOrientationChanged()
{
	FGoogleARCoreAndroidHelper::UpdateDisplayRotation();
	bDisplayOrientationChanged = true;
}

UARSessionConfig* FGoogleARCoreDevice::AccessSessionConfig() const
{
	return (ARSystem.IsValid())
		? &ARSystem->AccessSessionConfig()
		: nullptr;
}


EGoogleARCoreFunctionStatus FGoogleARCoreDevice::GetCameraImageIntrinsics(FARCameraIntrinsics& OutCameraIntrinsics)
{
	if (!ARCoreSession.IsValid() || ARCoreSession->GetLatestFrame() == nullptr)
	{
		return EGoogleARCoreFunctionStatus::NotAvailable;
	}

	return ToARCoreFunctionStatus(ARCoreSession->GetLatestFrame()->GetCameraImageIntrinsics(OutCameraIntrinsics));
}

EGoogleARCoreFunctionStatus FGoogleARCoreDevice::GetCameraTextureIntrinsics(FARCameraIntrinsics& OutCameraIntrinsics)
{
	if (!ARCoreSession.IsValid() || ARCoreSession->GetLatestFrame() == nullptr)
	{
		return EGoogleARCoreFunctionStatus::NotAvailable;
	}

	return ToARCoreFunctionStatus(ARCoreSession->GetLatestFrame()->GetCameraTextureIntrinsics(OutCameraIntrinsics));
}

UARTexture* FGoogleARCoreDevice::GetLastCameraTexture() const
{
	if (auto Record = PassthroughCameraTextures.Find(LastCameraTextureId))
	{
		return *Record;
	}
	
	return nullptr;
}

UARTexture* FGoogleARCoreDevice::GetDepthTexture() const
{
	return DepthTexture;
}

void FGoogleARCoreDevice::FInternalGCObject::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(ARCoreDevice->DepthTexture);
	Collector.AddReferencedObject(ARCoreDevice->PermissionHandler);
	Collector.AddReferencedObjects(ARCoreDevice->PassthroughCameraTextures);
}
