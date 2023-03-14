// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/DisplayClusterRenderManager.h"
#include "Config/IPDisplayClusterConfigManager.h"

#include "Engine/GameViewportClient.h"
#include "Engine/GameEngine.h"

#include "Misc/DisplayClusterGlobals.h"
#include "Misc/DisplayClusterHelpers.h"
#include "Misc/DisplayClusterLog.h"
#include "Misc/DisplayClusterStrings.h"

#include "DisplayClusterConfigurationStrings.h"

#include "Components/DisplayClusterCameraComponent.h"
#include "DisplayClusterViewportClient.h"

#include "Config/IPDisplayClusterConfigManager.h"
#include "DisplayClusterConfigurationTypes.h"

#include "Game/IPDisplayClusterGameManager.h"

#include "Render/Device/DisplayClusterRenderDeviceFactoryInternal.h"

#include "Render/Device/IDisplayClusterRenderDeviceFactory.h"
#include "Render/PostProcess/IDisplayClusterPostProcess.h"
#include "Render/Projection/IDisplayClusterProjectionPolicyFactory.h"
#include "Render/Projection/IDisplayClusterProjectionPolicy.h"
#include "Render/Synchronization/IDisplayClusterRenderSyncPolicyFactory.h"

#include "Render/Containers/DisplayClusterRender_MeshComponent.h"

#include "Render/Presentation/DisplayClusterPresentationNative.h"

#include "Render/Synchronization/DisplayClusterRenderSyncPolicyFactoryInternal.h"
#include "Render/Synchronization/DisplayClusterRenderSyncPolicyNone.h"

#include "Framework/Application/SlateApplication.h"

#include "UnrealClient.h"
#include "Kismet/GameplayStatics.h"

#include "CineCameraComponent.h"
#include "Engine/Scene.h"

#include "DisplayClusterRootActor.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows.h"
#include "Windows/HideWindowsPlatformTypes.h"
#endif


FDisplayClusterRenderManager::FDisplayClusterRenderManager()
{
	// Instantiate and register internal render device factory
	TSharedPtr<IDisplayClusterRenderDeviceFactory> NewRenderDeviceFactory(new FDisplayClusterRenderDeviceFactoryInternal);
	RegisterRenderDeviceFactory(DisplayClusterStrings::args::dev::Mono, NewRenderDeviceFactory);
	RegisterRenderDeviceFactory(DisplayClusterStrings::args::dev::QBS,  NewRenderDeviceFactory);
	RegisterRenderDeviceFactory(DisplayClusterStrings::args::dev::SbS,  NewRenderDeviceFactory);
	RegisterRenderDeviceFactory(DisplayClusterStrings::args::dev::TB,   NewRenderDeviceFactory);

	// Instantiate and register internal sync policy factory
	TSharedPtr<IDisplayClusterRenderSyncPolicyFactory> NewSyncPolicyFactory(new FDisplayClusterRenderSyncPolicyFactoryInternal);
	RegisterSynchronizationPolicyFactory(DisplayClusterConfigurationStrings::config::cluster::render_sync::None,            NewSyncPolicyFactory); // None
	RegisterSynchronizationPolicyFactory(DisplayClusterConfigurationStrings::config::cluster::render_sync::Ethernet,        NewSyncPolicyFactory); // Ethernet
	RegisterSynchronizationPolicyFactory(DisplayClusterConfigurationStrings::config::cluster::render_sync::EthernetBarrier, NewSyncPolicyFactory); // Ethernet_Simple
	RegisterSynchronizationPolicyFactory(DisplayClusterConfigurationStrings::config::cluster::render_sync::Nvidia,          NewSyncPolicyFactory); // NVIDIA
}

FDisplayClusterRenderManager::~FDisplayClusterRenderManager()
{
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IPDisplayClusterManager
//////////////////////////////////////////////////////////////////////////////////////////////
bool FDisplayClusterRenderManager::Init(EDisplayClusterOperationMode OperationMode)
{
	CurrentOperationMode = OperationMode;

	return true;
}

void FDisplayClusterRenderManager::Release()
{
	//@note: No need to release our RenderDevice. It will be released in a safe way by TSharedPtr.
}

bool FDisplayClusterRenderManager::StartSession(UDisplayClusterConfigurationData* InConfigData, const FString& InNodeId)
{
	if (CurrentOperationMode == EDisplayClusterOperationMode::Disabled)
	{
		UE_LOG(LogDisplayClusterRender, Log, TEXT("Operation mode is 'Disabled' so no initialization will be performed"));
		return true;
	}

	// Set callback on viewport created. We want to make sure the DisplayClusterViewportClient is used.
	UGameViewportClient::OnViewportCreated().AddRaw(this, &FDisplayClusterRenderManager::OnViewportCreatedHandler_CheckViewportClass);

	// Create synchronization object
	UE_LOG(LogDisplayClusterRender, Log, TEXT("Instantiating synchronization policy object..."));
	SyncPolicy = CreateRenderSyncPolicy();
	if (SyncPolicy)
	{
		SyncPolicy->Initialize();
	}

	// Instantiate render device
	TSharedPtr<IDisplayClusterRenderDevice, ESPMode::ThreadSafe> NewRenderDevice;
	UE_LOG(LogDisplayClusterRender, Log, TEXT("Instantiating stereo device..."));
	NewRenderDevice = CreateRenderDevice();

	// Set new device as the engine's stereoscopic device
	if (GEngine && NewRenderDevice.IsValid())
	{
		GEngine->StereoRenderingDevice = StaticCastSharedPtr<IStereoRendering>(NewRenderDevice);
		RenderDevicePtr = NewRenderDevice.Get();
	}

	// When session is starting in Editor the device won't be initialized so we avoid nullptr access here.
	//@todo Now we always have a device, even for Editor. Change the condition working on the EditorDevice.
	return (RenderDevicePtr ? RenderDevicePtr->Initialize() : true);
}

void FDisplayClusterRenderManager::EndSession()
{
#if WITH_EDITOR
	if (GIsEditor && RenderDevicePtr)
	{
		// Since we can run multiple PIE sessions we have to clean device before the next one.
		GEngine->StereoRenderingDevice.Reset();
		RenderDevicePtr = nullptr;
	}
#endif

	SyncPolicy.Reset();
}

bool FDisplayClusterRenderManager::StartScene(UWorld* InWorld)
{
	if (RenderDevicePtr)
	{
		RenderDevicePtr->StartScene(InWorld);
	}

	return true;
}

void FDisplayClusterRenderManager::EndScene()
{
	if (RenderDevicePtr)
	{
		RenderDevicePtr->EndScene();
	}
}

#if PLATFORM_WINDOWS
static bool SimulateMouseClick(HWND WindowHandle)
{
	RECT WindowRect; // Window rect in screen coordinates

	if (!GetWindowRect(WindowHandle, &WindowRect))
	{
		return false;
	}

	const double ScreenWidth = double(::GetSystemMetrics(SM_CXSCREEN));
	const double ScreenHeight = double(::GetSystemMetrics(SM_CYSCREEN));

	check(ScreenWidth > 0.5);
	check(ScreenHeight > 0.5);

	INPUT Inputs[3] = {{ 0 }};

	// Expected coordinates are normalized to screen dimensions of 65535x65535

	const double Left   = double(WindowRect.left);
	const double Right  = double(WindowRect.right);
	const double Top    = double(WindowRect.top);
	const double Bottom = double(WindowRect.bottom);

	Inputs[0].type = INPUT_MOUSE;
	Inputs[0].mi.dx = LONG((Left + (Right  - Left)/2.0) * (65535.0 / ScreenWidth));
	Inputs[0].mi.dy = LONG((Top  + (Bottom -  Top)/2.0) * (65535.0 / ScreenHeight));
	Inputs[0].mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;

	Inputs[1].type = INPUT_MOUSE;
	Inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;

	Inputs[2].type = INPUT_MOUSE;
	Inputs[2].mi.dwFlags = MOUSEEVENTF_LEFTUP;

	return !!::SendInput(3, Inputs, sizeof(INPUT));
}
#endif //PLATFORM_WINDOWS

void FDisplayClusterRenderManager::PreTick(float DeltaSeconds)
{
	if(!bWasWindowFocused)
	{
		if (UGameViewportClient* GameViewportClient = GEngine->GameViewport)
		{
			if (TSharedPtr<SWindow> Window = GameViewportClient->GetWindow())
			{
				if (TSharedPtr<const FGenericWindow> NativeWindow = Window->GetNativeWindow())
				{
					if (const void* WindowHandle = NativeWindow->GetOSWindowHandle())
					{
#if PLATFORM_WINDOWS
						const HWND GameHWND = (HWND)WindowHandle;

						::SetWindowPos(GameHWND, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
						::SetForegroundWindow(GameHWND);
						::SetCapture(GameHWND);
						::SetFocus(GameHWND);
						::SetActiveWindow(GameHWND);

						SimulateMouseClick(GameHWND);
#endif
						FSlateApplication::Get().SetAllUserFocusToGameViewport();

						bWasWindowFocused = true;
					}
				}
			}
		}
	}

	if (RenderDevicePtr)
	{
		RenderDevicePtr->PreTick(DeltaSeconds);
	}
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterRenderManager
//////////////////////////////////////////////////////////////////////////////////////////////
IDisplayClusterRenderDevice* FDisplayClusterRenderManager::GetRenderDevice() const
{
	return RenderDevicePtr;
}

bool FDisplayClusterRenderManager::RegisterRenderDeviceFactory(const FString& InDeviceType, TSharedPtr<IDisplayClusterRenderDeviceFactory>& InFactory)
{
	UE_LOG(LogDisplayClusterRender, Log, TEXT("Registering factory for rendering device type: %s"), *InDeviceType);

	if (!InFactory.IsValid())
	{
		UE_LOG(LogDisplayClusterRender, Warning, TEXT("Invalid factory object"));
		return false;
	}

	{
		FScopeLock Lock(&CritSecInternals);

		if (RenderDeviceFactories.Contains(InDeviceType))
		{
			UE_LOG(LogDisplayClusterRender, Warning, TEXT("Setting a new factory for '%s' rendering device type"), *InDeviceType);
		}

		RenderDeviceFactories.Emplace(InDeviceType, InFactory);
	}

	UE_LOG(LogDisplayClusterRender, Log, TEXT("Registered factory for rendering device type: %s"), *InDeviceType);

	return true;
}

bool FDisplayClusterRenderManager::UnregisterRenderDeviceFactory(const FString& InDeviceType)
{
	UE_LOG(LogDisplayClusterRender, Log, TEXT("Unregistering factory for rendering device type: %s"), *InDeviceType);

	{
		FScopeLock Lock(&CritSecInternals);

		if (!RenderDeviceFactories.Contains(InDeviceType))
		{
			UE_LOG(LogDisplayClusterRender, Warning, TEXT("A factory for '%s' rendering device type not found"), *InDeviceType);
			return false;
		}

		RenderDeviceFactories.Remove(InDeviceType);
	}

	UE_LOG(LogDisplayClusterRender, Log, TEXT("Unregistered factory for rendering device type: %s"), *InDeviceType);

	return true;
}

bool FDisplayClusterRenderManager::RegisterSynchronizationPolicyFactory(const FString& InSyncPolicyType, TSharedPtr<IDisplayClusterRenderSyncPolicyFactory>& InFactory)
{
	UE_LOG(LogDisplayClusterRender, Log, TEXT("Registering factory for synchronization policy: %s"), *InSyncPolicyType);

	if (!InFactory.IsValid())
	{
		UE_LOG(LogDisplayClusterRender, Warning, TEXT("Invalid factory object"));
		return false;
	}

	{
		FScopeLock Lock(&CritSecInternals);

		if (SyncPolicyFactories.Contains(InSyncPolicyType))
		{
			UE_LOG(LogDisplayClusterRender, Warning, TEXT("A new factory for '%s' synchronization policy was set"), *InSyncPolicyType);
		}

		SyncPolicyFactories.Emplace(InSyncPolicyType, InFactory);
	}

	UE_LOG(LogDisplayClusterRender, Log, TEXT("Registered factory for synchronization policy: %s"), *InSyncPolicyType);

	return true;
}

bool FDisplayClusterRenderManager::UnregisterSynchronizationPolicyFactory(const FString& InSyncPolicyType)
{
	UE_LOG(LogDisplayClusterRender, Log, TEXT("Unregistering factory for synchronization policy: %s"), *InSyncPolicyType);

	{
		FScopeLock Lock(&CritSecInternals);

		if (!SyncPolicyFactories.Contains(InSyncPolicyType))
		{
			UE_LOG(LogDisplayClusterRender, Warning, TEXT("A factory for '%s' synchronization policy not found"), *InSyncPolicyType);
			return false;
		}

		SyncPolicyFactories.Remove(InSyncPolicyType);
	}

	UE_LOG(LogDisplayClusterRender, Log, TEXT("Unregistered factory for synchronization policy: %s"), *InSyncPolicyType);

	return true;
}

TSharedPtr<IDisplayClusterRenderSyncPolicy> FDisplayClusterRenderManager::GetCurrentSynchronizationPolicy()
{
	FScopeLock Lock(&CritSecInternals);
	return SyncPolicy;
}

bool FDisplayClusterRenderManager::RegisterProjectionPolicyFactory(const FString& InProjectionType, TSharedPtr<IDisplayClusterProjectionPolicyFactory>& InFactory)
{
	UE_LOG(LogDisplayClusterRender, Log, TEXT("Registering factory for projection type: %s"), *InProjectionType);

	if (!InFactory.IsValid())
	{
		UE_LOG(LogDisplayClusterRender, Warning, TEXT("Invalid factory object"));
		return false;
	}

	{
		FScopeLock Lock(&CritSecInternals);

		if (ProjectionPolicyFactories.Contains(InProjectionType))
		{
			UE_LOG(LogDisplayClusterRender, Warning, TEXT("A new factory for '%s' projection policy was set"), *InProjectionType);
		}

		ProjectionPolicyFactories.Emplace(InProjectionType, InFactory);
	}

	UE_LOG(LogDisplayClusterRender, Log, TEXT("Registered factory for projection type: %s"), *InProjectionType);

	return true;
}

bool FDisplayClusterRenderManager::UnregisterProjectionPolicyFactory(const FString& InProjectionType)
{
	UE_LOG(LogDisplayClusterRender, Log, TEXT("Unregistering factory for projection policy: %s"), *InProjectionType);

	{
		FScopeLock Lock(&CritSecInternals);

		if (!ProjectionPolicyFactories.Contains(InProjectionType))
		{
			UE_LOG(LogDisplayClusterRender, Warning, TEXT("A handler for '%s' projection type not found"), *InProjectionType);
			return false;
		}

		ProjectionPolicyFactories.Remove(InProjectionType);
	}

	UE_LOG(LogDisplayClusterRender, Log, TEXT("Unregistered factory for projection policy: %s"), *InProjectionType);

	return true;
}

TSharedPtr<IDisplayClusterProjectionPolicyFactory> FDisplayClusterRenderManager::GetProjectionPolicyFactory(const FString& InProjectionType)
{
	FScopeLock Lock(&CritSecInternals);

	TSharedPtr<IDisplayClusterProjectionPolicyFactory> Factory;
	if (!DisplayClusterHelpers::map::template ExtractValue(ProjectionPolicyFactories, InProjectionType, Factory))
	{
		UE_LOG(LogDisplayClusterRender, Warning, TEXT("No factory found for projection policy: %s"), *InProjectionType);
	}

	return Factory;
}

void FDisplayClusterRenderManager::GetRegisteredProjectionPolicies(TArray<FString>& OutPolicyIDs) const
{
	FScopeLock Lock(&CritSecInternals);
	ProjectionPolicyFactories.GetKeys(OutPolicyIDs);
}


bool FDisplayClusterRenderManager::RegisterPostProcessFactory(const FString& InPostProcessType, TSharedPtr<IDisplayClusterPostProcessFactory>& InFactory)
{
	UE_LOG(LogDisplayClusterRender, Log, TEXT("Registering factory for postprocess type: %s"), *InPostProcessType);

	if (!InFactory.IsValid())
	{
		UE_LOG(LogDisplayClusterRender, Warning, TEXT("Invalid factory object"));
		return false;
	}

	{
		FScopeLock Lock(&CritSecInternals);

		if (PostProcessFactories.Contains(InPostProcessType))
		{
			UE_LOG(LogDisplayClusterRender, Warning, TEXT("A new factory for '%s' postprocess was set"), *InPostProcessType);
		}

		PostProcessFactories.Emplace(InPostProcessType, InFactory);
	}

	UE_LOG(LogDisplayClusterRender, Log, TEXT("Registered factory for postprocess type: %s"), *InPostProcessType);

	return true;
}

bool FDisplayClusterRenderManager::UnregisterPostProcessFactory(const FString& InPostProcessType)
{
	UE_LOG(LogDisplayClusterRender, Log, TEXT("Unregistering factory for postprocess: %s"), *InPostProcessType);

	{
		FScopeLock Lock(&CritSecInternals);

		if (!PostProcessFactories.Contains(InPostProcessType))
		{
			UE_LOG(LogDisplayClusterRender, Warning, TEXT("A handler for '%s' postprocess type not found"), *InPostProcessType);
			return false;
		}

		PostProcessFactories.Remove(InPostProcessType);
	}

	UE_LOG(LogDisplayClusterRender, Log, TEXT("Unregistered factory for postprocess: %s"), *InPostProcessType);

	return true;
}

TSharedPtr<IDisplayClusterPostProcessFactory> FDisplayClusterRenderManager::GetPostProcessFactory(const FString& InPostProcessType)
{
	FScopeLock Lock(&CritSecInternals);

	TSharedPtr<IDisplayClusterPostProcessFactory> Factory;
	if (!DisplayClusterHelpers::map::template ExtractValue(PostProcessFactories, InPostProcessType, Factory))
	{
		UE_LOG(LogDisplayClusterRender, Warning, TEXT("No factory found for postprocess: %s"), *InPostProcessType);
	}

	return Factory;
}

void FDisplayClusterRenderManager::GetRegisteredPostProcess(TArray<FString>& OutPostProcessIDs) const
{
	FScopeLock Lock(&CritSecInternals);
	PostProcessFactories.GetKeys(OutPostProcessIDs);
}

TSharedPtr<IDisplayClusterRender_MeshComponent, ESPMode::ThreadSafe> FDisplayClusterRenderManager::CreateMeshComponent() const
{
	return MakeShared<FDisplayClusterRender_MeshComponent, ESPMode::ThreadSafe>();
}

IDisplayClusterViewportManager* FDisplayClusterRenderManager::GetViewportManager() const
{
	ADisplayClusterRootActor* RootActor = GDisplayCluster->GetGameMgr()->GetRootActor();
	if (RootActor)
	{
		return RootActor->GetViewportManager();
	}

	return nullptr;
}

//////////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterRenderManager
//////////////////////////////////////////////////////////////////////////////////////////////
TSharedPtr<IDisplayClusterRenderDevice, ESPMode::ThreadSafe> FDisplayClusterRenderManager::CreateRenderDevice() const
{
	TSharedPtr<IDisplayClusterRenderDevice, ESPMode::ThreadSafe> NewRenderDevice;

	if (CurrentOperationMode == EDisplayClusterOperationMode::Cluster)
	{
		if (GDynamicRHI == nullptr)
		{
			UE_LOG(LogDisplayClusterRender, Error, TEXT("GDynamicRHI is null. Cannot detect RHI name."));
			return nullptr;
		}

		// Monoscopic
		if (FParse::Param(FCommandLine::Get(), DisplayClusterStrings::args::dev::Mono))
		{
			NewRenderDevice = RenderDeviceFactories[DisplayClusterStrings::args::dev::Mono]->Create(DisplayClusterStrings::args::dev::Mono);
		}
		// Quad buffer stereo
		else if (FParse::Param(FCommandLine::Get(), DisplayClusterStrings::args::dev::QBS))
		{
			NewRenderDevice = RenderDeviceFactories[DisplayClusterStrings::args::dev::QBS]->Create(DisplayClusterStrings::args::dev::QBS);
		}
		// Side-by-side
		else if (FParse::Param(FCommandLine::Get(), DisplayClusterStrings::args::dev::SbS))
		{
			NewRenderDevice = RenderDeviceFactories[DisplayClusterStrings::args::dev::SbS]->Create(DisplayClusterStrings::args::dev::SbS);
		}
		// Top-bottom
		else if (FParse::Param(FCommandLine::Get(), DisplayClusterStrings::args::dev::TB))
		{
			NewRenderDevice = RenderDeviceFactories[DisplayClusterStrings::args::dev::TB]->Create(DisplayClusterStrings::args::dev::TB);
		}
		// Leave native render but inject custom present for cluster synchronization
		else
		{
			UE_LOG(LogDisplayClusterRender, Warning, TEXT("No rendering device specified! A native present handler will be instantiated when viewport is available"));
			UGameViewportClient::OnViewportCreated().AddRaw(const_cast<FDisplayClusterRenderManager*>(this), &FDisplayClusterRenderManager::OnViewportCreatedHandler_SetCustomPresent);
		}
	}
	else if (CurrentOperationMode == EDisplayClusterOperationMode::Editor)
	{
#if 0
		UE_LOG(LogDisplayClusterRender, Log, TEXT("Instantiating DX11 mono device for PIE"));
		NewRenderDevice = MakeShared<FDisplayClusterDeviceMonoscopicDX11>();
#endif
	}
	else if (CurrentOperationMode == EDisplayClusterOperationMode::Disabled)
	{
		// Stereo device is not needed
		UE_LOG(LogDisplayClusterRender, Log, TEXT("No need to instantiate stereo device"));
	}
	else
	{
		UE_LOG(LogDisplayClusterRender, Warning, TEXT("Unknown operation mode"));
	}

	if (!NewRenderDevice.IsValid())
	{
		UE_LOG(LogDisplayClusterRender, Log, TEXT("No stereo device created"));
	}

	return NewRenderDevice;
}

TSharedPtr<IDisplayClusterRenderSyncPolicy> FDisplayClusterRenderManager::CreateRenderSyncPolicy() const
{
	if (CurrentOperationMode != EDisplayClusterOperationMode::Cluster)
	{
		UE_LOG(LogDisplayClusterRender, Warning, TEXT("Synchronization policy is not available for the current operation mode"));
		return nullptr;
	}

	if (GDynamicRHI == nullptr)
	{
		UE_LOG(LogDisplayClusterRender, Error, TEXT("GDynamicRHI is null. Cannot detect RHI name."));
		return nullptr;
	}

	const UDisplayClusterConfigurationData* ConfigData = GDisplayCluster->GetPrivateConfigMgr()->GetConfig();
	if (!ConfigData)
	{
		UE_LOG(LogDisplayClusterRender, Error, TEXT("Couldn't get configuration data"));
		return nullptr;
	}

	// Create sync policy specified in a config file
	const FString SyncPolicyType = ConfigData->Cluster->Sync.RenderSyncPolicy.Type;

	TSharedPtr<IDisplayClusterRenderSyncPolicy> NewSyncPolicy;
	if (SyncPolicyFactories.Contains(SyncPolicyType))
	{
		UE_LOG(LogDisplayClusterRender, Log, TEXT("A factory for the requested synchronization policy <%s> was found"), *SyncPolicyType);
		NewSyncPolicy = SyncPolicyFactories[SyncPolicyType]->Create(SyncPolicyType, ConfigData->Cluster->Sync.RenderSyncPolicy.Parameters);
	}
	else
	{
		const FString DefaultPolicy = DisplayClusterConfigurationStrings::config::cluster::render_sync::EthernetBarrier;
		UE_LOG(LogDisplayClusterRender, Log, TEXT("No factory found for the requested synchronization policy <%s>. Default '%s' policy will be used."), *SyncPolicyType, *DefaultPolicy);
		NewSyncPolicy = SyncPolicyFactories[DefaultPolicy]->Create(DefaultPolicy, TMap<FString, FString>());
	}

	return NewSyncPolicy;
}

void FDisplayClusterRenderManager::ResizeWindow(int32 WinX, int32 WinY, int32 ResX, int32 ResY)
{
	UGameEngine* Engine = Cast<UGameEngine>(GEngine);
	TSharedPtr<SWindow> Window = Engine->GameViewportWindow.Pin();
	check(Window.IsValid());

	UE_LOG(LogDisplayClusterRender, Log, TEXT("Adjusting game window: pos [%d, %d],  size [%d x %d]"), WinX, WinY, ResX, ResY);

	// Adjust window position/size
	Window->ReshapeWindow(FVector2D(WinX, WinY), FVector2D(ResX, ResY));
}

void FDisplayClusterRenderManager::OnViewportCreatedHandler_SetCustomPresent() const
{
	if (GEngine && GEngine->GameViewport)
	{
		if (!GEngine->GameViewport->Viewport->GetViewportRHI().IsValid())
		{
			GEngine->GameViewport->OnBeginDraw().AddRaw(this, &FDisplayClusterRenderManager::OnBeginDrawHandler);
		}
	}
}

void FDisplayClusterRenderManager::OnViewportCreatedHandler_CheckViewportClass() const
{
	if (GEngine && GEngine->GameViewport)
	{
		UDisplayClusterViewportClient* const GameViewport = Cast<UDisplayClusterViewportClient>(GEngine->GameViewport);
		if (!GameViewport)
		{
			UE_LOG(LogDisplayClusterRender, Warning, TEXT("DisplayClusterViewportClient is not set as a default GameViewport class"));
		}
	}
}

void FDisplayClusterRenderManager::OnBeginDrawHandler() const
{
	static bool initialized = false;
	if (!initialized && GEngine->GameViewport->Viewport->GetViewportRHI().IsValid())
	{
		FDisplayClusterPresentationNative* const NativePresentHandler = new FDisplayClusterPresentationNative(GEngine->GameViewport->Viewport, SyncPolicy);
		check(NativePresentHandler);
		GEngine->GameViewport->Viewport->GetViewportRHI().GetReference()->SetCustomPresent(NativePresentHandler);
		initialized = true;
	}
}
