// Copyright Epic Games, Inc. All Rights Reserved.

#include "OculusHMDModule.h"
#include "OculusHMD.h"
#include "OculusHMDPrivateRHI.h"
#include "OculusHMDRuntimeSettings.h"
#include "Containers/StringConv.h"
#include "Misc/EngineVersion.h"
#include "Misc/Paths.h"
#if PLATFORM_ANDROID
#include "Android/AndroidApplication.h"
#include "Android/AndroidPlatformMisc.h"
#endif
#include "Interfaces/IPluginManager.h"
#include "ShaderCore.h"


//-------------------------------------------------------------------------------------------------
// FOculusHMDModule
//-------------------------------------------------------------------------------------------------

OculusPluginWrapper FOculusHMDModule::PluginWrapper;

PRAGMA_DISABLE_DEPRECATION_WARNINGS
FOculusHMDModule::FOculusHMDModule()
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	bPreInit = false;
	bPreInitCalled = false;
	OVRPluginHandle = nullptr;
	GraphicsAdapterLuid = 0;
#endif
}

void FOculusHMDModule::StartupModule()
{
	IHeadMountedDisplayModule::StartupModule();
}

void FOculusHMDModule::ShutdownModule()
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	if (PluginWrapper.Initialized)
	{
		DestroyOculusPluginWrapper(&PluginWrapper);
	}

	if (OVRPluginHandle)
	{
		FPlatformProcess::FreeDllHandle(OVRPluginHandle);
		OVRPluginHandle = nullptr;
	}
#endif
}

#if PLATFORM_ANDROID
extern bool AndroidThunkCpp_IsOculusMobileApplication();
#endif

FString FOculusHMDModule::GetModuleKeyName() const
{
	return FString(TEXT("OculusHMD"));
}

void FOculusHMDModule::GetModuleAliases(TArray<FString>& AliasesOut) const
{
	// old name for this module (was renamed in 4.17)
	AliasesOut.Add(TEXT("OculusRift"));

	AliasesOut.Add(TEXT("Oculus"));
	AliasesOut.Add(TEXT("Rift"));
}

bool FOculusHMDModule::PreInit()
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	if (!bPreInitCalled)
	{
		bPreInit = false;
		bPreInitCalled = true;

#if PLATFORM_ANDROID
		if (!AndroidThunkCpp_IsOculusMobileApplication())
		{
			UE_LOG(LogHMD, Log, TEXT("App is not packaged for Oculus Mobile"));
			return false;
		}
#endif

		// Init module if app can render
		if (FApp::CanEverRender())
		{
			// Load OVRPlugin
			OVRPluginHandle = GetOVRPluginHandle();

			if (!OVRPluginHandle)
			{
				UE_LOG(LogHMD, Log, TEXT("Failed loading OVRPlugin %s"), TEXT(OVRP_VERSION_STR));
				return false;
			}

			if (!InitializeOculusPluginWrapper(&PluginWrapper))
			{
				UE_LOG(LogHMD, Log, TEXT("Failed InitializeOculusPluginWrapper"));
				return false;
			}

			// Initialize OVRPlugin
			ovrpRenderAPIType PreinitApiType = ovrpRenderAPI_None;
#if PLATFORM_ANDROID
			void* Activity = (void*) FAndroidApplication::GetGameActivityThis();

			FConfigFile AndroidEngineSettings;
			FConfigCacheIni::LoadLocalIniFile(AndroidEngineSettings, TEXT("Engine"), true, TEXT("Android"));

			bool bPackagedForVulkan = false;
			AndroidEngineSettings.GetBool(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("bSupportsVulkan"), bPackagedForVulkan);
			UE_LOG(LogHMD, Log, TEXT("Oculus game packaged for Vulkan : %d"), bPackagedForVulkan);
			PreinitApiType = bPackagedForVulkan ? ovrpRenderAPI_Vulkan : ovrpRenderAPI_OpenGL;
#else
			void* Activity = nullptr;
#endif

			if (OVRP_FAILURE(PluginWrapper.PreInitialize4(Activity, PreinitApiType)))
			{
				UE_LOG(LogHMD, Log, TEXT("Failed initializing OVRPlugin %s"), TEXT(OVRP_VERSION_STR));
				return false;
			}

#if PLATFORM_WINDOWS
			const LUID* DisplayAdapterId;
			if (OVRP_SUCCESS(PluginWrapper.GetDisplayAdapterId2((const void**) &DisplayAdapterId)) && DisplayAdapterId)
			{
				SetGraphicsAdapterLuid(*(const uint64*) DisplayAdapterId);
			}
			else
			{
				UE_LOG(LogHMD, Log, TEXT("Could not determine HMD display adapter"));
			}

			const WCHAR* AudioInDeviceId;
			if (OVRP_SUCCESS(PluginWrapper.GetAudioInDeviceId2((const void**) &AudioInDeviceId)) && AudioInDeviceId)
			{
				GConfig->SetString(TEXT("Oculus.Settings"), TEXT("AudioInputDevice"), AudioInDeviceId, GEngineIni);
			}
			else
			{
				UE_LOG(LogHMD, Log, TEXT("Could not determine HMD audio input device"));
			}

			const WCHAR* AudioOutDeviceId;
			if (OVRP_SUCCESS(PluginWrapper.GetAudioOutDeviceId2((const void**) &AudioOutDeviceId)) && AudioOutDeviceId)
			{
				GConfig->SetString(TEXT("Oculus.Settings"), TEXT("AudioOutputDevice"), AudioOutDeviceId, GEngineIni);
			}
			else
			{
				UE_LOG(LogHMD, Log, TEXT("Could not determine HMD audio output device"));
			}
#endif

			UE_LOG(LogHMD, Log, TEXT("FOculusHMDModule PreInit successfully"));

			bPreInit = true;
		}
	}

	return bPreInit;
#else
	return false;
#endif // OCULUS_HMD_SUPPORTED_PLATFORMS
}


bool FOculusHMDModule::IsHMDConnected()
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	UDEPRECATED_UOculusHMDRuntimeSettings* HMDSettings = GetMutableDefault<UDEPRECATED_UOculusHMDRuntimeSettings>();
	if (FApp::CanEverRender() && OculusHMD::IsOculusHMDConnected() && HMDSettings->XrApi != EOculusXrApi::NativeOpenXR)
	{
		return true;
	}
#endif // OCULUS_HMD_SUPPORTED_PLATFORMS
	return false;
}


uint64 FOculusHMDModule::GetGraphicsAdapterLuid()
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS_D3D11 || OCULUS_HMD_SUPPORTED_PLATFORMS_D3D12
	if (!GraphicsAdapterLuid)
	{
		int GraphicsAdapter;

		if (GConfig->GetInt(TEXT("Oculus.Settings"), TEXT("GraphicsAdapter"), GraphicsAdapter, GEngineIni) &&
			GraphicsAdapter >= 0)
		{
			TRefCountPtr<IDXGIFactory> DXGIFactory;
			TRefCountPtr<IDXGIAdapter> DXGIAdapter;
			DXGI_ADAPTER_DESC DXGIAdapterDesc;

			if (SUCCEEDED(CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)DXGIFactory.GetInitReference())) &&
				SUCCEEDED(DXGIFactory->EnumAdapters(GraphicsAdapter, DXGIAdapter.GetInitReference())) &&
				SUCCEEDED(DXGIAdapter->GetDesc(&DXGIAdapterDesc)))
			{
				FMemory::Memcpy(&GraphicsAdapterLuid, &DXGIAdapterDesc.AdapterLuid, sizeof(GraphicsAdapterLuid));
			}
		}
	}
#endif

#if OCULUS_HMD_SUPPORTED_PLATFORMS
	return GraphicsAdapterLuid;
#else
	return 0;
#endif
}


FString FOculusHMDModule::GetAudioInputDevice()
{
	FString AudioInputDevice;
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	GConfig->GetString(TEXT("Oculus.Settings"), TEXT("AudioInputDevice"), AudioInputDevice, GEngineIni);
#endif
	return AudioInputDevice;
}


FString FOculusHMDModule::GetAudioOutputDevice()
{
	FString AudioOutputDevice;
#if OCULUS_HMD_SUPPORTED_PLATFORMS
#if PLATFORM_WINDOWS
	if (bPreInit)
	{
		if (FApp::CanEverRender() && OculusHMD::IsOculusServiceRunning())
		{
			const WCHAR* audioOutDeviceId;
			if (OVRP_SUCCESS(PluginWrapper.GetAudioOutDeviceId2((const void**)&audioOutDeviceId)) && audioOutDeviceId)
			{
				AudioOutputDevice = audioOutDeviceId;
			}
		}
	}
#else
	GConfig->GetString(TEXT("Oculus.Settings"), TEXT("AudioOutputDevice"), AudioOutputDevice, GEngineIni);
#endif
#endif
	return AudioOutputDevice;
}


TSharedPtr< class IXRTrackingSystem, ESPMode::ThreadSafe > FOculusHMDModule::CreateTrackingSystem()
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	if (PreInit())
	{
		OculusHMD::FOculusHMDPtr OculusHMD = FSceneViewExtensions::NewExtension<OculusHMD::FOculusHMD>();

		if (OculusHMD->Startup())
		{
			HeadMountedDisplay = OculusHMD;
			return OculusHMD;
		}
	}
	HeadMountedDisplay = nullptr;
#endif
	return nullptr;
}


TSharedPtr< IHeadMountedDisplayVulkanExtensions, ESPMode::ThreadSafe >  FOculusHMDModule::GetVulkanExtensions()
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	if (PreInit())
	{
		if (!VulkanExtensions.IsValid())
		{
			VulkanExtensions = MakeShareable(new OculusHMD::FVulkanExtensions);
		}
	}
	return VulkanExtensions;
#endif
	return nullptr;
}

FString FOculusHMDModule::GetDeviceSystemName()
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	ovrpSystemHeadset SystemHeadset;
	if (OVRP_SUCCESS(PluginWrapper.GetSystemHeadsetType2(&SystemHeadset)))
	{
		switch (SystemHeadset)
		{
		case ovrpSystemHeadset_Oculus_Quest:
			return FString("Oculus Quest");

		case ovrpSystemHeadset_Oculus_Quest_2:
			return FString("Oculus Quest2");

		default:
			return FString("Oculus_Quest");
		}
	}
	return FString();
#else
	return FString();
#endif
}

bool FOculusHMDModule::IsStandaloneStereoOnlyDevice()
{
#if PLATFORM_ANDROID
	return FAndroidMisc::GetDeviceMake() == FString("Oculus");
#else
	return false;
#endif
}


#if OCULUS_HMD_SUPPORTED_PLATFORMS
void* FOculusHMDModule::GetOVRPluginHandle()
{
	void* OVRPluginHandle = nullptr;

#if PLATFORM_WINDOWS
#if PLATFORM_64BITS
	FString BinariesPath = FPaths::EngineDir() / FString(TEXT("Binaries/ThirdParty/Oculus/OVRPlugin/OVRPlugin/Win64"));
#else
	FString BinariesPath = FPaths::EngineDir() / FString(TEXT("Binaries/ThirdParty/Oculus/OVRPlugin/OVRPlugin/Win32"));
#endif

	FPlatformProcess::PushDllDirectory(*BinariesPath);

	FString XrApi;
	// Treat no XrApi value as LegacyOVRPlugin so that older games upgrading to newer engine versions don't change XrApis
	if (!GConfig->GetString(TEXT("/Script/OculusHMD.OculusHMDRuntimeSettings"), TEXT("XrApi"), XrApi, GEngineIni) || XrApi.Equals(FString("LegacyOVRPlugin")))
	{
	    OVRPluginHandle = FPlatformProcess::GetDllHandle(*(BinariesPath / "OVRPlugin.dll"));
	}
	else if (XrApi.Equals(FString("OVRPluginOpenXR")))
	{
#if PLATFORM_64BITS
		OVRPluginHandle = FPlatformProcess::GetDllHandle(*(BinariesPath / "OpenXR/OVRPlugin.dll"));
#else
		UE_LOG(LogHMD, Error, TEXT("OVRPlugin OpenXR only supported on 64-bit Windows. Please switch to Legacy OVRPlugin instead."));
#endif
	}
	FPlatformProcess::PopDllDirectory(*BinariesPath);
#elif PLATFORM_ANDROID
	OVRPluginHandle = FPlatformProcess::GetDllHandle(TEXT("libOVRPlugin.so"));
#endif // PLATFORM_ANDROID

	return OVRPluginHandle;
}


bool FOculusHMDModule::PoseToOrientationAndPosition(const FQuat& InOrientation, const FVector& InPosition, FQuat& OutOrientation, FVector& OutPosition) const
{
	OculusHMD::CheckInGameThread();

	OculusHMD::FOculusHMD* OculusHMD = static_cast<OculusHMD::FOculusHMD*>(HeadMountedDisplay.Pin().Get());

	if (OculusHMD)
	{
		ovrpPosef InPose;
		InPose.Orientation = OculusHMD::ToOvrpQuatf(InOrientation);
		InPose.Position = OculusHMD::ToOvrpVector3f(InPosition);
		OculusHMD::FPose OutPose;

		if (OculusHMD->ConvertPose(InPose, OutPose))
		{
			OutOrientation = OutPose.Orientation;
			OutPosition = OutPose.Position;
			return true;
		}
	}

	return false;
}


void FOculusHMDModule::SetGraphicsAdapterLuid(uint64 InLuid)
{
	GraphicsAdapterLuid = InLuid;

#if OCULUS_HMD_SUPPORTED_PLATFORMS_D3D11 || OCULUS_HMD_SUPPORTED_PLATFORMS_D3D12
	TRefCountPtr<IDXGIFactory> DXGIFactory;

	if (SUCCEEDED(CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)DXGIFactory.GetInitReference())))
	{
		for (int32 adapterIndex = 0;; adapterIndex++)
		{
			TRefCountPtr<IDXGIAdapter> DXGIAdapter;
			DXGI_ADAPTER_DESC DXGIAdapterDesc;

			if (FAILED(DXGIFactory->EnumAdapters(adapterIndex, DXGIAdapter.GetInitReference())) ||
				FAILED(DXGIAdapter->GetDesc(&DXGIAdapterDesc)))
			{
				break;
			}

			if (!FMemory::Memcmp(&GraphicsAdapterLuid, &DXGIAdapterDesc.AdapterLuid, sizeof(GraphicsAdapterLuid)))
			{
				// Remember this adapterIndex so we use the right adapter, even when we startup without HMD connected
				GConfig->SetInt(TEXT("Oculus.Settings"), TEXT("GraphicsAdapter"), adapterIndex, GEngineIni);
				break;
			}
		}
	}
#endif // OCULUS_HMD_SUPPORTED_PLATFORMS_D3D11 || OCULUS_HMD_SUPPORTED_PLATFORMS_D3D12
}
#endif // OCULUS_HMD_SUPPORTED_PLATFORMS

PRAGMA_ENABLE_DEPRECATION_WARNINGS

IMPLEMENT_MODULE(FOculusHMDModule, OculusHMD)
