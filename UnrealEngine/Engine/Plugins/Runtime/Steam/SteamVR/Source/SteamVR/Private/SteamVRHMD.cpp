// Copyright Epic Games, Inc. All Rights Reserved.

#include "SteamVRHMD.h"
#include "SteamVRPrivate.h"

#include "Misc/App.h"
#include "Misc/CoreDelegates.h"
#include "Misc/EngineVersion.h"
#include "HardwareInfo.h"
#include "RendererPrivate.h"
#include "Slate/SceneViewport.h"
#include "PostProcess/PostProcessHMD.h"
#include "SteamVRFunctionLibrary.h"
#include "Engine/GameEngine.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "IHeadMountedDisplayVulkanExtensions.h"

#include "EngineAnalytics.h"
#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"

#include "Engine/Canvas.h"
#include "CanvasItem.h"

#if WITH_EDITOR
#include "Editor/UnrealEd/Classes/Editor/EditorEngine.h"
#endif

#ifndef ARRAYSIZE
#define ARRAYSIZE( a ) ( sizeof( ( a ) ) / sizeof( ( a )[ 0 ] ) )
#endif

#if PLATFORM_MAC
// For FResourceBulkDataInterface
#include "Containers/ResourceArray.h"
#include <Metal/Metal.h>
#endif

DEFINE_LOG_CATEGORY(LogSteamVR);

PRAGMA_DISABLE_DEPRECATION_WARNINGS

// Visibility mesh
static TAutoConsoleVariable<int32> CUseSteamVRVisibleAreaMesh(TEXT("vr.SteamVR.UseVisibleAreaMesh"), 1, TEXT("If non-zero, SteamVR will use the visible area mesh in addition to the hidden area mesh optimization.  This may be problematic on beta versions of platforms."));

/** Helper function for acquiring the appropriate FSceneViewport */
FSceneViewport* FindSceneViewport()
{
	if (!GIsEditor)
	{
		UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
		return GameEngine->SceneViewport.Get();
	}
#if WITH_EDITOR
	else
	{
		UEditorEngine* EditorEngine = CastChecked<UEditorEngine>( GEngine );
		FSceneViewport* PIEViewport = (FSceneViewport*)EditorEngine->GetPIEViewport();
		if( PIEViewport != nullptr && PIEViewport->IsStereoRenderingAllowed() )
		{
			// PIE is setup for stereo rendering
			return PIEViewport;
		}
		else
		{
			// Check to see if the active editor viewport is drawing in stereo mode
			// @todo vreditor: Should work with even non-active viewport!
			FSceneViewport* EditorViewport = (FSceneViewport*)EditorEngine->GetActiveViewport();
			if( EditorViewport != nullptr && EditorViewport->IsStereoRenderingAllowed() )
			{
				return EditorViewport;
			}
		}
	}
#endif
	return nullptr;
}

#if STEAMVR_SUPPORTED_PLATFORMS
// Wrapper around vr::IVRSystem::GetStringTrackedDeviceProperty
static FString GetFStringTrackedDeviceProperty(vr::IVRSystem* VRSystem, uint32 DeviceIndex, vr::ETrackedDeviceProperty Property, vr::TrackedPropertyError* ErrorPtr = nullptr)
{
	check(VRSystem != nullptr);

	vr::TrackedPropertyError Error;
	TArray<char> Buffer;
	Buffer.AddUninitialized(vr::k_unMaxPropertyStringSize);

	int Size = VRSystem->GetStringTrackedDeviceProperty(DeviceIndex, Property, Buffer.GetData(), Buffer.Num(), &Error);
	if (Error == vr::TrackedProp_BufferTooSmall)
	{
		Buffer.AddUninitialized(Size - Buffer.Num());
		Size = VRSystem->GetStringTrackedDeviceProperty(DeviceIndex, Property, Buffer.GetData(), Buffer.Num(), &Error);
	}

	if (ErrorPtr)
	{
		*ErrorPtr = Error;
	}

	if (Error == vr::TrackedProp_Success)
	{
		return UTF8_TO_TCHAR(Buffer.GetData());
	}
	else
	{
		return UTF8_TO_TCHAR(VRSystem->GetPropErrorNameFromEnum(Error));
	}
}
#endif //STEAMVR_SUPPORTED_PLATFORMS

//---------------------------------------------------
// SteamVR Plugin Implementation
//---------------------------------------------------

class FSteamVRPlugin : public ISteamVRPlugin
{
	/** IHeadMountedDisplayModule implementation */
	virtual TSharedPtr< class IXRTrackingSystem, ESPMode::ThreadSafe > CreateTrackingSystem() override;

	FString GetModuleKeyName() const
	{
		return FString(TEXT("SteamVR"));
	}

public:
	FSteamVRPlugin()
#if !STEAMVR_SUPPORTED_PLATFORMS
	{
	}
#else //STEAMVR_SUPPORTED_PLATFORMS
		: VRSystem(nullptr)
		, VRCompositor(nullptr)
	{
		LoadOpenVRModule();
	}

	virtual void StartupModule() override
	{
		IHeadMountedDisplayModule::StartupModule();
	
		LoadOpenVRModule();
	}
	
	virtual void ShutdownModule() override
	{
		IHeadMountedDisplayModule::ShutdownModule();
		
		UnloadOpenVRModule();
	}

	virtual vr::IVRSystem* GetVRSystem() const override
	{
		return VRSystem;
	}
	
	virtual vr::IVRCompositor* GetVRCompositor() const override
	{
		return VRCompositor;
	}

	bool LoadOpenVRModule()
	{
#if PLATFORM_WINDOWS
#if PLATFORM_64BITS
		FString RootOpenVRPath;
		FString VROverridePath = FPlatformMisc::GetEnvironmentVariable(TEXT("VR_OVERRIDE"));
		
		if (VROverridePath.Len() > 0)
		{
			RootOpenVRPath = FString::Printf(TEXT("%s\\bin\\win64\\"), *VROverridePath);
		}
		else
		{
			RootOpenVRPath = FPaths::EngineDir() / FString::Printf(TEXT("Binaries/ThirdParty/OpenVR/%s/Win64/"), OPENVR_SDK_VER);
		}
		
		FPlatformProcess::PushDllDirectory(*RootOpenVRPath);
		OpenVRDLLHandle = FPlatformProcess::GetDllHandle(*(RootOpenVRPath + "openvr_api.dll"));
		FPlatformProcess::PopDllDirectory(*RootOpenVRPath);
#else
		FString RootOpenVRPath = FPaths::EngineDir() / FString::Printf(TEXT("Binaries/ThirdParty/OpenVR/%s/Win32/"), OPENVR_SDK_VER);
		FPlatformProcess::PushDllDirectory(*RootOpenVRPath);
		OpenVRDLLHandle = FPlatformProcess::GetDllHandle(*(RootOpenVRPath + "openvr_api.dll"));
		FPlatformProcess::PopDllDirectory(*RootOpenVRPath);
#endif
#elif PLATFORM_MAC
		FString RootOpenVRPath = FPaths::EngineDir() / FString::Printf(TEXT("Binaries/ThirdParty/OpenVR/%s/osx32/"), OPENVR_SDK_VER);
		UE_LOG(LogHMD, Log, TEXT("Tried to load %s"), *(RootOpenVRPath + "libopenvr_api.dylib"));
		OpenVRDLLHandle = FPlatformProcess::GetDllHandle(*(RootOpenVRPath + "libopenvr_api.dylib"));
#elif PLATFORM_LINUX
		FString RootOpenVRPath = FPaths::EngineDir() / FString::Printf(TEXT("Binaries/ThirdParty/OpenVR/%s/linux64/"), OPENVR_SDK_VER);
		OpenVRDLLHandle = FPlatformProcess::GetDllHandle(*(RootOpenVRPath + "libopenvr_api.so"));
#else
		#error "SteamVRHMD is not supported for this platform."
#endif	//PLATFORM_WINDOWS
		
		if (!OpenVRDLLHandle)
		{
			UE_LOG(LogHMD, Log, TEXT("Failed to load OpenVR library."));
			return false;
		}
		
		//@todo steamvr: Remove GetProcAddress() workaround once we update to Steamworks 1.33 or higher
		VRIsHmdPresentFn = (pVRIsHmdPresent)FPlatformProcess::GetDllExport(OpenVRDLLHandle, TEXT("VR_IsHmdPresent"));
		VRGetGenericInterfaceFn = (pVRGetGenericInterface)FPlatformProcess::GetDllExport(OpenVRDLLHandle, TEXT("VR_GetGenericInterface"));
		
		// Note:  If this fails to compile, it's because you merged a new OpenVR version, and didn't put in the module hacks marked with @epic in openvr.h
		vr::VR_SetGenericInterfaceCallback(VRGetGenericInterfaceFn);

		// Verify that we've bound correctly to the DLL functions
		if (!VRIsHmdPresentFn || !VRGetGenericInterfaceFn)
		{
			UE_LOG(LogHMD, Log, TEXT("Failed to GetProcAddress() on openvr_api.dll"));
			UnloadOpenVRModule();
			
			return false;
		}
		
		return true;
	}

	bool Initialize()
	{
		vr::EVRInitError VRInitErr = vr::VRInitError_None;
		// Attempt to initialize the VRSystem device
		// If the commandline -xrtrackingonly is passed, then start the application in _Other mode instead of _Scene mode
		// This is used when we only want to get tracking information and don't need to render anything to the XR device
		if (FParse::Param(FCommandLine::Get(), TEXT("xrtrackingonly")))
		{
			VRSystem = vr::VR_Init(&VRInitErr, vr::VRApplication_Other);
			UE_LOG(LogHMD, Log, TEXT("Starting OpenVR in VRApplication_Other mode for tracking only"));
		}
		else
		{
			VRSystem = vr::VR_Init(&VRInitErr, vr::VRApplication_Scene);
		}
		if ((VRSystem == nullptr) || (VRInitErr != vr::VRInitError_None))
		{
			UE_LOG(LogHMD, Log, TEXT("Failed to initialize OpenVR with code %d"), (int32)VRInitErr);
			return false;
		}

		// Make sure that the version of the HMD we're compiled against is correct.  This will fill out the proper vtable!
		VRSystem = (vr::IVRSystem*)(*VRGetGenericInterfaceFn)(vr::IVRSystem_Version, &VRInitErr);
		if ((VRSystem == nullptr) || (VRInitErr != vr::VRInitError_None))
		{
			UE_LOG(LogHMD, Log, TEXT("Failed to initialize OpenVR (version mismatch) with code %d"), (int32)VRInitErr);
			Reset();
			return false;
		}

		// attach to the compositor	
		int CompositorConnectAttempts = 10;
		do
		{
			VRCompositor = (vr::IVRCompositor*)(*VRGetGenericInterfaceFn)(vr::IVRCompositor_Version, &VRInitErr);

			// If SteamVR was not running when VR_Init was called above, the system may take a few seconds to initialize.
			// retry a few times before giving up if we get a Compositor connection error.
			// This is a temporary workaround an issue that will be solved in a future version of SteamVR, where VR_Init will block until everything is ready,
			// It's only triggered in cases where SteamVR is available, but was not running prior to calling VR_Init above.
			if ((--CompositorConnectAttempts > 0) && (VRInitErr == vr::VRInitError_IPC_CompositorConnectFailed || VRInitErr == vr::VRInitError_IPC_CompositorInvalidConnectResponse))
			{
				UE_LOG(LogHMD, Warning, TEXT("Failed to get Compositor connnection (%d) retrying... (%d attempt(s) left)"), (int32)VRInitErr, CompositorConnectAttempts);
				FPlatformProcess::Sleep(1);
			}
			else
			{
				break;
			}
		} while (true);

		if (VRInitErr != vr::VRInitError_None)
		{
			UE_LOG(LogHMD, Log, TEXT("SteamVR failed to initialize.  Error: %d"), (int32)VRInitErr);
			Reset();
			return false;
		}

		return true;
	}
	
	void UnloadOpenVRModule()
	{
		if (OpenVRDLLHandle != nullptr)
		{
			UE_LOG(LogHMD, Log, TEXT("Freeing OpenVRDLLHandle."));
			FPlatformProcess::FreeDllHandle(OpenVRDLLHandle);
			OpenVRDLLHandle = nullptr;
		}
	}

	virtual bool IsHMDConnected() override
	{
		return VRIsHmdPresentFn ? (bool)(*VRIsHmdPresentFn)() : false;
	}

	virtual void Reset() override
	{
		VRSystem = nullptr;
		VRCompositor = nullptr;
		vr::VR_Shutdown();
	}

#if PLATFORM_WINDOWS
	enum class D3DApiLevel
	{
		Undefined,
		Direct3D11,
		Direct3D12
	};

	static inline D3DApiLevel GetD3DApiLevel()
	{
		if (GDynamicRHI == nullptr)
		{
			// RHI might not be up yet. Let's check the command-line and see if DX12 was specified.
			// This will get hit on startup since we don't have RHI details during stereo device bringup. 
			// This is not a final fix; we should probably move the stereo device init to later on in startup.
			bool bForceD3D12 = FParse::Param(FCommandLine::Get(), TEXT("d3d12")) || FParse::Param(FCommandLine::Get(), TEXT("dx12"));
			return bForceD3D12 ? D3DApiLevel::Direct3D12 : D3DApiLevel::Direct3D11;
		}

		const ERHIInterfaceType RHIType = RHIGetInterfaceType();

		if (RHIType == ERHIInterfaceType::D3D11)
		{
			return D3DApiLevel::Direct3D11;
		}

		if (RHIType == ERHIInterfaceType::D3D12)
		{
			return D3DApiLevel::Direct3D12;
		}

		return D3DApiLevel::Undefined;
	}

#endif
	
	uint64 GetGraphicsAdapterLuid() override
	{
#if PLATFORM_MAC	
		// @TODO  currently, for mac, GetGraphicsAdapterLuid() is used to return a device index (how the function 
		//        "GetGraphicsAdapter" used to work), not a ID... eventually we want the HMD module to return the 
		//        MTLDevice's registryID, but we cannot fully handle that until we drop support for 10.12
		//  NOTE: this is why we  use -1 as a sentinel value representing "no device" (instead of 0, which is used in the LUID case)

		const uint32 NoDevice = (uint32)-1;
		id<MTLDevice> SelectedDevice = nil;
#else
		const uint32 NoDevice = 0;
		uint64 SelectedDevice = NoDevice;
#endif
		
		{
			if (!VRSystem && !Initialize())
			{
				return NoDevice;
			}

			vr::ETextureType TextureType = vr::ETextureType::TextureType_OpenGL;
#if PLATFORM_MAC
            TextureType = vr::ETextureType::TextureType_IOSurface;
#else
			if (IsPCPlatform(GMaxRHIShaderPlatform))
			{
				if (IsVulkanPlatform(GMaxRHIShaderPlatform))
				{
					TextureType = vr::ETextureType::TextureType_Vulkan;
				}
				else if (IsOpenGLPlatform(GMaxRHIShaderPlatform))
				{
					TextureType = vr::ETextureType::TextureType_OpenGL;
				}
#if PLATFORM_WINDOWS
				else
				{
					D3DApiLevel level = GetD3DApiLevel();

					if (level == D3DApiLevel::Direct3D11)
					{
						TextureType = vr::ETextureType::TextureType_DirectX;
					}
					else if (level == D3DApiLevel::Direct3D12)
					{
						TextureType = vr::ETextureType::TextureType_DirectX12;
					}
					else
					{
						return NoDevice;
					}
				}
#endif
			}
#endif

			VRSystem->GetOutputDevice((uint64_t*)((void*)&SelectedDevice), TextureType);
		}


#if PLATFORM_MAC
		if(SelectedDevice == nil)
		{
			return NoDevice;
		}

		TArray<FMacPlatformMisc::FGPUDescriptor> const& GPUs = FPlatformMisc::GetGPUDescriptors();
		check(GPUs.Num() > 0);

		int32 DeviceIndex = -1;
		TArray<FString> NameComponents;
		bool bFoundDefault = false;
		for (uint32 i = 0; i < GPUs.Num(); i++)
		{
			FMacPlatformMisc::FGPUDescriptor const& GPU = GPUs[i];
			if (([SelectedDevice.name rangeOfString : @"Nvidia" options : NSCaseInsensitiveSearch].location != NSNotFound && GPU.GPUVendorId == 0x10DE)
				|| ([SelectedDevice.name rangeOfString : @"AMD" options : NSCaseInsensitiveSearch].location != NSNotFound && GPU.GPUVendorId == 0x1002)
				|| ([SelectedDevice.name rangeOfString : @"Intel" options : NSCaseInsensitiveSearch].location != NSNotFound && GPU.GPUVendorId == 0x8086))
			{
				NameComponents.Empty();
				bool bMatchesName = FString(GPU.GPUName).TrimStartAndEnd().ParseIntoArray(NameComponents, TEXT(" ")) > 0;
				for (FString& Component : NameComponents)
				{
					bMatchesName &= FString(SelectedDevice.name).Contains(Component);
				}
				if ((SelectedDevice.headless == GPU.GPUHeadless || GPU.GPUVendorId != 0x1002) && bMatchesName)
				{
					DeviceIndex = i;
					bFoundDefault = true;
					break;
				}
			}
		}
		if (!bFoundDefault)
		{
			UE_LOG(LogHMD, Warning, TEXT("Couldn't find Metal device %s in GPU descriptors from IORegistry - VR device selection may be wrong."), *FString(SelectedDevice.name));
		}
		return (uint64)DeviceIndex;
#else
		return SelectedDevice;
#endif //PLATFORM_MAC
	}

	virtual TSharedPtr< IHeadMountedDisplayVulkanExtensions, ESPMode::ThreadSafe > GetVulkanExtensions() override
	{
#if STEAMVR_SUPPORTED_PLATFORMS && !PLATFORM_MAC
		if (Initialize())
		{
			if (!VulkanExtensions.IsValid())
			{
				VulkanExtensions = MakeShareable(new FSteamVRHMD::FVulkanExtensions(VRCompositor));
			}
			return VulkanExtensions;
		}
#endif//STEAMVR_SUPPORTED_PLATFORMS
		return nullptr;
	}

private:
	vr::IVRSystem* VRSystem;
	vr::IVRCompositor* VRCompositor;

	pVRIsHmdPresent VRIsHmdPresentFn = nullptr;
	pVRGetGenericInterface VRGetGenericInterfaceFn = nullptr;

	TSharedPtr< IHeadMountedDisplayVulkanExtensions, ESPMode::ThreadSafe > VulkanExtensions;
	void* OpenVRDLLHandle;
#endif // STEAMVR_SUPPORTED_PLATFORMS
};

IMPLEMENT_MODULE( FSteamVRPlugin, SteamVR )

TSharedPtr< class IXRTrackingSystem, ESPMode::ThreadSafe > FSteamVRPlugin::CreateTrackingSystem()
{
#if STEAMVR_SUPPORTED_PLATFORMS
	if (!VRSystem && !Initialize())
	{
		return nullptr;
	}

	TSharedPtr< FSteamVRHMD, ESPMode::ThreadSafe > SteamVRHMD = FSceneViewExtensions::NewExtension<FSteamVRHMD>(this);
	if( SteamVRHMD->IsInitialized() )
	{
		return SteamVRHMD;
	}
#endif//STEAMVR_SUPPORTED_PLATFORMS
	return nullptr;
}


//---------------------------------------------------
// SteamVR IHeadMountedDisplay Implementation
//---------------------------------------------------

#if STEAMVR_SUPPORTED_PLATFORMS

#if !PLATFORM_MAC

FSteamVRHMD::FVulkanExtensions::FVulkanExtensions(vr::IVRCompositor* InVRCompositor)
	: VRCompositor(InVRCompositor)
{
}

bool FSteamVRHMD::FVulkanExtensions::GetVulkanInstanceExtensionsRequired(TArray<const ANSICHAR*>& Out)
{
	if (VRCompositor == nullptr)
	{
		UE_LOG(LogHMD, Warning, TEXT("VRCompositor is null"));
		return false;
	}
 
	static ANSICHAR* InstanceExtensionsBuf = nullptr;
 
	uint32_t BufSize = VRCompositor->GetVulkanInstanceExtensionsRequired(nullptr, 0);
	if (BufSize == 0)
	{
		return true; // No particular extensions required
	}
	if (InstanceExtensionsBuf != nullptr)
	{
		FMemory::Free(InstanceExtensionsBuf);
	}
	InstanceExtensionsBuf = (ANSICHAR*)FMemory::Malloc(BufSize);
	VRCompositor->GetVulkanInstanceExtensionsRequired(InstanceExtensionsBuf, BufSize);
 
	ANSICHAR * Context = nullptr;
	ANSICHAR * Tok = FCStringAnsi::Strtok(InstanceExtensionsBuf, " ", &Context);
	while (Tok != nullptr)
	{
		Out.Add(Tok);
		Tok = FCStringAnsi::Strtok(nullptr, " ", &Context);
	}

	return true;
}

bool FSteamVRHMD::FVulkanExtensions::GetVulkanDeviceExtensionsRequired(VkPhysicalDevice_T *pPhysicalDevice, TArray<const ANSICHAR*>& Out)
{
	if ( VRCompositor == nullptr )
	{
		UE_LOG(LogHMD, Warning, TEXT("VRCompositor is null"));
		return false;
	}
 
	static ANSICHAR* DeviceExtensionsBuf = nullptr;
 
	uint32_t BufSize = VRCompositor->GetVulkanDeviceExtensionsRequired(pPhysicalDevice, nullptr, 0);
	if (BufSize == 0)
	{
		return true; // No particular extensions required
	}
	if (DeviceExtensionsBuf != nullptr)
	{
		FMemory::Free(DeviceExtensionsBuf);
	}
	DeviceExtensionsBuf = (ANSICHAR*)FMemory::Malloc(BufSize);
	VRCompositor->GetVulkanDeviceExtensionsRequired(pPhysicalDevice, DeviceExtensionsBuf, BufSize);
 
	ANSICHAR * Context = nullptr;
	ANSICHAR * Tok = FCStringAnsi::Strtok(DeviceExtensionsBuf, " ", &Context);
	while (Tok != nullptr)
	{
		Out.Add(Tok);
		Tok = FCStringAnsi::Strtok(nullptr, " ", &Context);
	}

	return true;
}

#endif

bool FSteamVRHMD::IsHMDConnected()
{
	return SteamVRPlugin->IsHMDConnected();
}

bool FSteamVRHMD::IsHMDEnabled() const
{
	return bHmdEnabled;
}

EHMDWornState::Type FSteamVRHMD::GetHMDWornState()
{
	//HmdWornState is set in OnStartGameFrame's event loop
	return HmdWornState;
}

void FSteamVRHMD::EnableHMD(bool enable)
{
	bHmdEnabled = enable;

	if (!bHmdEnabled)
	{
		EnableStereo(false);
	}
}

bool FSteamVRHMD::GetHMDMonitorInfo(MonitorInfo& MonitorDesc) 
{
	if (IsInitialized())
	{
		int32 X, Y;
		uint32 Width, Height;
		GetWindowBounds(&X, &Y, &Width, &Height);

		MonitorDesc.MonitorName = DisplayId;
		MonitorDesc.MonitorId	= 0;
		MonitorDesc.DesktopX	= X;
		MonitorDesc.DesktopY	= Y;
		MonitorDesc.ResolutionX = Width;
		MonitorDesc.ResolutionY = Height;

		return true;
	}
	else
	{
		MonitorDesc.MonitorName = "";
		MonitorDesc.MonitorId = 0;
		MonitorDesc.DesktopX = MonitorDesc.DesktopY = MonitorDesc.ResolutionX = MonitorDesc.ResolutionY = 0;
	}

	return false;
}

void FSteamVRHMD::GetFieldOfView(float& OutHFOVInDegrees, float& OutVFOVInDegrees) const
{
	float HFOV, VFOV, Left, Right, Top, Bottom;
	VRSystem->GetProjectionRaw(vr::Eye_Left, &Right, &Left, &Top, &Bottom);
	HFOV = FMath::Atan(-Left);
	VFOV = FMath::Atan(Top - Bottom);
	VRSystem->GetProjectionRaw(vr::Eye_Right, &Right, &Left, &Top, &Bottom);
	HFOV += FMath::Atan(Right);
	VFOV = FMath::Max(VFOV, FMath::Atan(Top - Bottom));

	OutHFOVInDegrees = FMath::RadiansToDegrees(HFOV);
	OutVFOVInDegrees = FMath::RadiansToDegrees(VFOV);
}

bool FSteamVRHMD::DoesSupportPositionalTracking() const
{
	return true;
}

bool FSteamVRHMD::HasValidTrackingPosition()
{
	return GetTrackingFrame().bHaveVisionTracking;
}

bool FSteamVRHMD::GetTrackingSensorProperties(int32 SensorId, FQuat& OutOrientation, FVector& OutOrigin, FXRSensorProperties& OutSensorProperties)
{
	OutOrigin = FVector::ZeroVector;
	OutOrientation = FQuat::Identity;
	OutSensorProperties = FXRSensorProperties();

	if (VRSystem == nullptr)
	{
		return false;
	}

	uint32 SteamDeviceID = static_cast<uint32>(SensorId);
	if (SteamDeviceID >= vr::k_unMaxTrackedDeviceCount)
	{
		return false;
	}
	
	const FTrackingFrame& TrackingFrame = GetTrackingFrame();

	if (!TrackingFrame.bPoseIsValid[SteamDeviceID])
	{
		return false;
	}

	OutOrigin = TrackingFrame.DevicePosition[SteamDeviceID];
	OutOrientation = TrackingFrame.DeviceOrientation[SteamDeviceID];

	OutSensorProperties.LeftFOV = VRSystem->GetFloatTrackedDeviceProperty(SteamDeviceID, vr::Prop_FieldOfViewLeftDegrees_Float);
	OutSensorProperties.RightFOV = VRSystem->GetFloatTrackedDeviceProperty(SteamDeviceID, vr::Prop_FieldOfViewRightDegrees_Float);
	OutSensorProperties.TopFOV = VRSystem->GetFloatTrackedDeviceProperty(SteamDeviceID, vr::Prop_FieldOfViewTopDegrees_Float);
	OutSensorProperties.BottomFOV = VRSystem->GetFloatTrackedDeviceProperty(SteamDeviceID, vr::Prop_FieldOfViewBottomDegrees_Float);

	const float WorldToMetersScale = TrackingFrame.WorldToMetersScale;

	OutSensorProperties.NearPlane = VRSystem->GetFloatTrackedDeviceProperty(SteamDeviceID, vr::Prop_TrackingRangeMinimumMeters_Float) * WorldToMetersScale;
	OutSensorProperties.FarPlane = VRSystem->GetFloatTrackedDeviceProperty(SteamDeviceID, vr::Prop_TrackingRangeMaximumMeters_Float) * WorldToMetersScale;

	OutSensorProperties.CameraDistance = FVector::Dist(FVector::ZeroVector, OutOrigin);
	return true;
}

FString FSteamVRHMD::GetTrackedDevicePropertySerialNumber(int32 DeviceId)
{
	return GetFStringTrackedDeviceProperty(VRSystem, DeviceId, vr::Prop_SerialNumber_String);
}

void FSteamVRHMD::SetInterpupillaryDistance(float NewInterpupillaryDistance)
{
}

float FSteamVRHMD::GetInterpupillaryDistance() const
{
	return 0.064f;
}

bool FSteamVRHMD::GetCurrentPose(int32 DeviceId, FQuat& CurrentOrientation, FVector& CurrentPosition)
{
	uint32 SteamDeviceID = static_cast<uint32>(DeviceId);
	bool bHasValidPose = false;

	const FTrackingFrame& TrackingFrame = GetTrackingFrame();
	if (SteamDeviceID < vr::k_unMaxTrackedDeviceCount)
	{
		CurrentOrientation = TrackingFrame.DeviceOrientation[SteamDeviceID];
		CurrentPosition = TrackingFrame.DevicePosition[SteamDeviceID];

		bHasValidPose = TrackingFrame.bPoseIsValid[SteamDeviceID] && TrackingFrame.bDeviceIsConnected[SteamDeviceID];
	}
	else
	{
		CurrentOrientation = FQuat::Identity;
		CurrentPosition = FVector::ZeroVector;
	}

	return bHasValidPose;
}

void FSteamVRHMD::UpdatePoses()
{
	if (!VRSystem || !VRCompositor)
	{
		return;
	}

	FTrackingFrame& TrackingFrame = const_cast<FTrackingFrame&>(GetTrackingFrame());
	TrackingFrame.FrameNumber = GFrameNumberRenderThread;

	vr::TrackedDevicePose_t Poses[vr::k_unMaxTrackedDeviceCount];
	if (IsInRenderingThread())
	{
		vr::EVRCompositorError PoseError = VRCompositor->WaitGetPoses(Poses, ARRAYSIZE(Poses) , NULL, 0);
	}
	else
	{
		check(IsInGameThread());
		vr::EVRCompositorError PoseError = VRCompositor->GetLastPoses(Poses, ARRAYSIZE(Poses), NULL, 0);
	}

	TrackingFrame.bHaveVisionTracking = false;
	TrackingFrame.WorldToMetersScale = GameWorldToMetersScale;
	for (uint32 i = 0; i < vr::k_unMaxTrackedDeviceCount; ++i)
	{
		TrackingFrame.bHaveVisionTracking |= Poses[i].eTrackingResult == vr::ETrackingResult::TrackingResult_Running_OK;

		TrackingFrame.bDeviceIsConnected[i] = Poses[i].bDeviceIsConnected;
		TrackingFrame.bPoseIsValid[i] = Poses[i].bPoseIsValid;
		TrackingFrame.RawPoses[i] = Poses[i].mDeviceToAbsoluteTracking;

	}
	ConvertRawPoses(TrackingFrame);
}

void FSteamVRHMD::GetWindowBounds(int32* X, int32* Y, uint32* Width, uint32* Height)
{
	if (vr::IVRExtendedDisplay *VRExtDisplay = vr::VRExtendedDisplay())
	{
		VRExtDisplay->GetWindowBounds(X, Y, Width, Height);
	}
	else
	{
		*X = 0;
		*Y = 0;
		*Width = WindowMirrorBoundsWidth;
		*Height = WindowMirrorBoundsHeight;
	}
}

bool FSteamVRHMD::IsInsideBounds()
{
	if (VRChaperone)
	{
		const FTrackingFrame& TrackingFrame = GetTrackingFrame();
		vr::HmdMatrix34_t VRPose = TrackingFrame.RawPoses[vr::k_unTrackedDeviceIndex_Hmd];
		FMatrix Pose = ToFMatrix(VRPose);
		
		const FVector HMDLocation(Pose.M[3][0], 0.f, Pose.M[3][2]);

		bool bLastWasNegative = false;

		// Since the order of the soft bounds are points on a plane going clockwise, wind around the sides, checking the crossproduct of the affine side to the affine HMD position.  If they're all on the same side, we're in the bounds
		for (uint8 i = 0; i < 4; ++i)
		{
			const FVector PointA = ChaperoneBounds.Bounds.Corners[i];
			const FVector PointB = ChaperoneBounds.Bounds.Corners[(i + 1) % 4];

			const FVector AffineSegment = PointB - PointA;
			const FVector AffinePoint = HMDLocation - PointA;
			const FVector CrossProduct = FVector::CrossProduct(AffineSegment, AffinePoint);

			const bool bIsNegative = (CrossProduct.Y < 0);

			// If the cross between the point and the side has flipped, that means we're not consistent, and therefore outside the bounds
			if ((i > 0) && (bLastWasNegative != bIsNegative))
			{
				return false;
			}

			bLastWasNegative = bIsNegative;
		}

		return true;
	}

	return false;
}

/** Helper function to convert bounds from SteamVR space to scaled Unreal space*/
TArray<FVector> ConvertBoundsToUnrealSpace(const FBoundingQuad& InBounds, const float WorldToMetersScale)
{
	TArray<FVector> Bounds;

	for (int32 i = 0; i < ARRAYSIZE(InBounds.Corners); ++i)
	{
		const FVector SteamVRCorner = InBounds.Corners[i];
		const FVector UnrealVRCorner(-SteamVRCorner.Z, SteamVRCorner.X, SteamVRCorner.Y);
		Bounds.Add(UnrealVRCorner * WorldToMetersScale);
	}

	return Bounds;
}

TArray<FVector> FSteamVRHMD::GetBounds() const
{
	return ConvertBoundsToUnrealSpace(ChaperoneBounds.Bounds, GetWorldToMetersScale());
}

void FSteamVRHMD::SetTrackingOrigin(EHMDTrackingOrigin::Type NewOrigin)
{
	if(VRCompositor)
	{
		vr::TrackingUniverseOrigin NewSteamOrigin;

		switch (NewOrigin)
		{
			case EHMDTrackingOrigin::Eye:
				NewSteamOrigin = vr::TrackingUniverseOrigin::TrackingUniverseSeated;
				break;
			case EHMDTrackingOrigin::Floor:
			default:
				NewSteamOrigin = vr::TrackingUniverseOrigin::TrackingUniverseStanding;
				break;
		}

		VRCompositor->SetTrackingSpace(NewSteamOrigin);

		OnTrackingOriginChanged();
	}
}

EHMDTrackingOrigin::Type FSteamVRHMD::GetTrackingOrigin() const
{
	if(VRCompositor)
	{
		const vr::TrackingUniverseOrigin CurrentOrigin = VRCompositor->GetTrackingSpace();

		switch(CurrentOrigin)
		{
		case vr::TrackingUniverseOrigin::TrackingUniverseSeated:
			return EHMDTrackingOrigin::Eye;
		case vr::TrackingUniverseOrigin::TrackingUniverseStanding:
		default:
			return EHMDTrackingOrigin::Floor;
		}
	}

	// By default, assume standing
	return EHMDTrackingOrigin::Floor;
}

bool FSteamVRHMD::GetFloorToEyeTrackingTransform(FTransform& OutStandingToSeatedTransform) const
{
	bool bSuccess = false;
	if (VRSystem && ensure(IsInGameThread()))
	{
		vr::TrackedDevicePose_t SeatedPoses[vr::k_unMaxTrackedDeviceCount];
		VRSystem->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseOrigin::TrackingUniverseSeated, 0.0f, SeatedPoses, ARRAYSIZE(SeatedPoses));
		vr::TrackedDevicePose_t StandingPoses[vr::k_unMaxTrackedDeviceCount];
		VRSystem->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseOrigin::TrackingUniverseStanding, 0.0f, StandingPoses, ARRAYSIZE(StandingPoses));

		const vr::TrackedDevicePose_t& SeatedHmdPose = SeatedPoses[vr::k_unTrackedDeviceIndex_Hmd];
		const vr::TrackedDevicePose_t& StandingHmdPose = StandingPoses[vr::k_unTrackedDeviceIndex_Hmd];
		if (SeatedHmdPose.bPoseIsValid && StandingHmdPose.bPoseIsValid)
		{
			const float WorldToMeters = GetWorldToMetersScale();

			FVector SeatedHmdPosition = FVector::ZeroVector;
			FQuat SeatedHmdOrientation = FQuat::Identity;
			PoseToOrientationAndPosition(SeatedHmdPose.mDeviceToAbsoluteTracking, WorldToMeters, SeatedHmdOrientation, SeatedHmdPosition);

			FVector StandingHmdPosition = FVector::ZeroVector;
			FQuat StandingHmdOrientation = FQuat::Identity;
			PoseToOrientationAndPosition(StandingHmdPose.mDeviceToAbsoluteTracking, WorldToMeters, StandingHmdOrientation, StandingHmdPosition);

			const FVector SeatedHmdFwd   = SeatedHmdOrientation.GetForwardVector();
			const FVector SeatedHmdRight = SeatedHmdOrientation.GetRightVector();
			const FQuat StandingToSeatedRot = FRotationMatrix::MakeFromXY(SeatedHmdFwd, SeatedHmdRight).ToQuat() * StandingHmdOrientation.Inverse();

			const FVector StandingToSeatedOffset = SeatedHmdPosition - StandingToSeatedRot.RotateVector(StandingHmdPosition);
			OutStandingToSeatedTransform = FTransform(StandingToSeatedRot, StandingToSeatedOffset);
			bSuccess = true;
		}
	}
	return bSuccess;
}

FVector2D FSteamVRHMD::GetPlayAreaBounds(EHMDTrackingOrigin::Type Origin) const
{
	FVector2f Bounds;// LWC_TODO: Precision loss
	if (Origin == EHMDTrackingOrigin::Stage && VRChaperone->GetPlayAreaSize(&Bounds.X, &Bounds.Y))
	{
		return FVector2D(Bounds);
	}
	return FVector2D::ZeroVector;
}

void FSteamVRHMD::RecordAnalytics()
{
	if (FEngineAnalytics::IsAvailable())
	{
		// prepare and send analytics data
		TArray<FAnalyticsEventAttribute> EventAttributes;

		IHeadMountedDisplay::MonitorInfo MonitorInfo;
		GetHMDMonitorInfo(MonitorInfo);

		uint64 MonitorId = MonitorInfo.MonitorId;

		char Buf[128];
		vr::TrackedPropertyError Error;
		FString DeviceName = "SteamVR - Default Device Name";
		VRSystem->GetStringTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_ModelNumber_String, Buf, sizeof(Buf), &Error);
		if (Error == vr::TrackedProp_Success)
		{
			DeviceName = FString(UTF8_TO_TCHAR(Buf));
		}
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("DeviceName"), DeviceName));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("DisplayDeviceName"), *MonitorInfo.MonitorName));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("DisplayId"), MonitorId));
		FString MonResolution(FString::Printf(TEXT("(%d, %d)"), MonitorInfo.ResolutionX, MonitorInfo.ResolutionY));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("Resolution"), MonResolution));

		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("InterpupillaryDistance"), GetInterpupillaryDistance()));


		FString OutStr(TEXT("Editor.VR.DeviceInitialised"));
		FEngineAnalytics::GetProvider().RecordEvent(OutStr, EventAttributes);
	}
}

#if PLATFORM_MAC
class FIOSurfaceResourceWrapper : public FResourceBulkDataInterface
{
public:
	FIOSurfaceResourceWrapper(CFTypeRef InSurface)
	: Surface(InSurface)
	{
		check(InSurface);
		CFRetain(Surface);
	}

	virtual const void* GetResourceBulkData() const override
	{
		return Surface;
	}
	
	virtual uint32 GetResourceBulkDataSize() const override
	{
		return 0;
	}
	
	virtual void Discard() override
	{
		delete this;
	}
	
	virtual EBulkDataType GetResourceType() const override
	{
		return EBulkDataType::VREyeBuffer;
	}
	
	virtual ~FIOSurfaceResourceWrapper()
	{
		CFRelease(Surface);
		Surface = nullptr;
	}

private:
	CFTypeRef Surface;
};
#endif

void FSteamVRHMD::PoseToOrientationAndPosition(const vr::HmdMatrix34_t& InPose, const float WorldToMetersScale, FQuat& OutOrientation, FVector& OutPosition) const
{
	FMatrix Pose = ToFMatrix(InPose);
	if (!((FMath::Abs(1.f - Pose.GetScaledAxis(EAxis::X).SizeSquared()) <= KINDA_SMALL_NUMBER) && (FMath::Abs(1.f - Pose.GetScaledAxis(EAxis::Y).SizeSquared()) <= KINDA_SMALL_NUMBER) && (FMath::Abs(1.f - Pose.GetScaledAxis(EAxis::Z).SizeSquared()) <= KINDA_SMALL_NUMBER)))
	{
		// When running an oculus rift through steamvr the tracking reference seems to have a slightly scaled matrix, about 99%.  We need to strip that so we can build the quaternion without hitting an ensure.
		Pose.RemoveScaling(KINDA_SMALL_NUMBER);
	}
	FQuat Orientation(Pose);

	OutOrientation.X = -Orientation.Z;
	OutOrientation.Y = Orientation.X;
	OutOrientation.Z = Orientation.Y;
	OutOrientation.W = -Orientation.W;

	FVector Position = ((FVector(-Pose.M[3][2], Pose.M[3][0], Pose.M[3][1])) * WorldToMetersScale - BaseOffset);
	OutPosition = BaseOrientation.Inverse().RotateVector(Position);

	OutOrientation = BaseOrientation.Inverse() * OutOrientation;
	OutOrientation.Normalize();
}

void FSteamVRHMD::ConvertRawPoses(FSteamVRHMD::FTrackingFrame& TrackingFrame) const
{
	for (uint32 i = 0; i < vr::k_unMaxTrackedDeviceCount; ++i)
	{
		#if 0
		FMatrix Pose = ToFMatrix(TrackingFrame.RawPoses[i]);
		if (!((FMath::Abs(1.f - Pose.GetScaledAxis(EAxis::X).SizeSquared()) <= KINDA_SMALL_NUMBER) && (FMath::Abs(1.f - Pose.GetScaledAxis(EAxis::Y).SizeSquared()) <= KINDA_SMALL_NUMBER) && (FMath::Abs(1.f - Pose.GetScaledAxis(EAxis::Z).SizeSquared()) <= KINDA_SMALL_NUMBER)))
		{
			// When running an oculus rift through steamvr the tracking reference seems to have a slightly scaled matrix, about 99%.  We need to strip that so we can build the quaternion without hitting an ensure.
			Pose.RemoveScaling(KINDA_SMALL_NUMBER);
		}
		FQuat Orientation(Pose);
		Orientation = FQuat(-Orientation.Z, Orientation.X, Orientation.Y, -Orientation.W);

		FVector Position = ((FVector(-Pose.M[3][2], Pose.M[3][0], Pose.M[3][1]) - BaseOffset) * TrackingFrame.WorldToMetersScale);
		TrackingFrame.DevicePosition[i] = BaseOrientation.Inverse().RotateVector(Position);

		TrackingFrame.DeviceOrientation[i] = BaseOrientation.Inverse() * Orientation;
		TrackingFrame.DeviceOrientation[i].Normalize();
#endif
		PoseToOrientationAndPosition(TrackingFrame.RawPoses[i], TrackingFrame.WorldToMetersScale, TrackingFrame.DeviceOrientation[i], TrackingFrame.DevicePosition[i]);
	}
}

float FSteamVRHMD::GetWorldToMetersScale() const
{
	const FTrackingFrame& TrackingFrame = GetTrackingFrame();
	return TrackingFrame.bPoseIsValid[vr::k_unTrackedDeviceIndex_Hmd] ? TrackingFrame.WorldToMetersScale : 100.0f;
}

EXRTrackedDeviceType FSteamVRHMD::GetTrackedDeviceType(int32 DeviceId) const
{
	check(VRSystem != nullptr);
	vr::TrackedDeviceClass DeviceClass = VRSystem->GetTrackedDeviceClass(DeviceId);

	switch (DeviceClass)
	{
	case vr::TrackedDeviceClass_HMD:
		return EXRTrackedDeviceType::HeadMountedDisplay;
	case vr::TrackedDeviceClass_Controller:
		return EXRTrackedDeviceType::Controller;
	case vr::TrackedDeviceClass_TrackingReference:
		return EXRTrackedDeviceType::TrackingReference;
	case vr::TrackedDeviceClass_GenericTracker:
		return EXRTrackedDeviceType::Other;
	default:
		return EXRTrackedDeviceType::Invalid;
	}
}

bool FSteamVRHMD::EnumerateTrackedDevices(TArray<int32>& TrackedIds, EXRTrackedDeviceType DeviceType)
{
	TrackedIds.Empty();
	if (VRSystem == nullptr)
	{
		return false;
	}

	const FTrackingFrame& TrackingFrame = GetTrackingFrame();
	for (uint32 i = 0; i < vr::k_unMaxTrackedDeviceCount; ++i)
	{
		// Add only devices with a currently valid tracked pose
		if (TrackingFrame.bPoseIsValid[i] &&
			(DeviceType == EXRTrackedDeviceType::Any || GetTrackedDeviceType(i) == DeviceType))
		{
			TrackedIds.Add(i);
		}
	}
	return TrackedIds.Num() > 0;
}

ETrackingStatus FSteamVRHMD::GetControllerTrackingStatus(int32 DeviceId) const
{
	ETrackingStatus TrackingStatus = ETrackingStatus::NotTracked;

	const FTrackingFrame& TrackingFrame = GetTrackingFrame();
	if (DeviceId < vr::k_unMaxTrackedDeviceCount && TrackingFrame.bPoseIsValid[DeviceId] && TrackingFrame.bDeviceIsConnected[DeviceId])
	{
		TrackingStatus = ETrackingStatus::Tracked;
	}

	return TrackingStatus;
}

bool FSteamVRHMD::IsTracking(int32 DeviceId)
{
	uint32 SteamDeviceId = static_cast<uint32>(DeviceId);
	bool bHasTrackedPose = false;
	if (VRSystem != nullptr)
	{
		const FTrackingFrame& TrackingFrame = GetTrackingFrame();
		if (SteamDeviceId < vr::k_unMaxTrackedDeviceCount)
		{
			bHasTrackedPose = TrackingFrame.bPoseIsValid[SteamDeviceId];
		}
	}
	return bHasTrackedPose;
}

bool FSteamVRHMD::IsChromaAbCorrectionEnabled() const
{
	return true;
}

void FSteamVRHMD::OnEndPlay(FWorldContext& InWorldContext)
{
	if (!GEnableVREditorHacks)
	{
		EnableStereo(false);
	}
}

const FName FSteamVRHMD::SteamSystemName(TEXT("SteamVR"));

FString FSteamVRHMD::GetVersionString() const
{
	if (VRSystem == nullptr)
	{
		return FString();
	}

	const FString Manufacturer = GetFStringTrackedDeviceProperty(VRSystem, vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_ManufacturerName_String);
	const FString Model = GetFStringTrackedDeviceProperty(VRSystem, vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_ModelNumber_String);
	const FString Serial = GetFStringTrackedDeviceProperty(VRSystem, vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_SerialNumber_String);
	const FString DriverId = GetFStringTrackedDeviceProperty(VRSystem, vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_TrackingSystemName_String);
	const FString DriverVersion = GetFStringTrackedDeviceProperty(VRSystem, vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_DriverVersion_String);

	return FString::Printf(TEXT("%s, Driver: %s, Serial: %s, HMD Device: %s %s, Driver version: %s"), *FEngineVersion::Current().ToString(), *DriverId, *Serial, *Manufacturer, *Model, *DriverVersion);
}

bool FSteamVRHMD::OnStartGameFrame(FWorldContext& WorldContext)
{
	static const double kShutdownTimeout = 4.0; // How many seconds to allow the renderer to exit stereo mode before shutting down the VR subsystem
	
	if (VRSystem == nullptr)
	{
		return false;
	}

	if (bStereoEnabled != bStereoDesired)
	{
		bStereoEnabled = EnableStereo(bStereoDesired);
	}

	FQuat Orientation;
	FVector Position;
	GameWorldToMetersScale = WorldContext.World()->GetWorldSettings()->WorldToMeters;
	UpdatePoses();
	GetCurrentPose(IXRTrackingSystem::HMDDeviceId, Orientation, Position);

	RefreshTrackingToWorldTransform(WorldContext);

	bool bShouldShutdown = false;
	if (bIsQuitting)
	{
		if (QuitTimestamp < FApp::GetCurrentTime())
		{
			bShouldShutdown = true;
			bIsQuitting = false;
		}
	}

	// Poll SteamVR events
	vr::VREvent_t VREvent;
	while (VRSystem->PollNextEvent(&VREvent, sizeof(VREvent)))
	{
		switch (VREvent.eventType)
		{
		case vr::VREvent_Quit:
			if (IsStereoEnabled())
			{
				// If we're currently in stereo mode, allow a few seconds while we disable stereo rendering before shutting down the VR system
				EnableStereo(false);
				QuitTimestamp = FApp::GetCurrentTime() + kShutdownTimeout;
				bIsQuitting = true;
			}
			else if (!bIsQuitting)
			{
				// If we're not currently in stereo mode (and not already counting down, we can shut down the VR system immediately
				bShouldShutdown = true;
			}
			break;
		case vr::VREvent_InputFocusCaptured:
			FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Broadcast();
			break;
		case vr::VREvent_InputFocusReleased:
			FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Broadcast();
			break;
		case vr::VREvent_TrackedDeviceUserInteractionStarted:
			// if the event was sent by the HMD
			if (VREvent.trackedDeviceIndex == vr::k_unTrackedDeviceIndex_Hmd)
			{
				// Save the position we are currently at and put us in the state where we could move to a worn state
				bShouldCheckHMDPosition = true;
				HMDStartLocation = Position;
			}
			break;
		case vr::VREvent_TrackedDeviceUserInteractionEnded:
			// if the event was sent by the HMD. 
			if (VREvent.trackedDeviceIndex == vr::k_unTrackedDeviceIndex_Hmd) 
			{
				// Don't check to see if we might be wearing the HMD anymore.
				bShouldCheckHMDPosition = false;
				// Don't change our state to "not worn" unless we are currently wearing it.
				if (HmdWornState == EHMDWornState::Worn)
				{
					HmdWornState = EHMDWornState::NotWorn;
					FCoreDelegates::VRHeadsetRemovedFromHead.Broadcast();
				}
			}
			break;
		case vr::VREvent_ChaperoneDataHasChanged:
		case vr::VREvent_ChaperoneUniverseHasChanged:
		case vr::VREvent_ChaperoneTempDataHasChanged:
		case vr::VREvent_ChaperoneSettingsHaveChanged:
			// if the event was sent by the HMD. 
			if ((VREvent.trackedDeviceIndex == vr::k_unTrackedDeviceIndex_Hmd) && (VRChaperone != nullptr))
			{
				ChaperoneBounds = FChaperoneBounds(VRChaperone);
			}
			break;
		}
	}


	// SteamVR gives 5 seconds from VREvent_Quit till its process is killed
	if (bShouldShutdown)
	{
		bShouldCheckHMDPosition = false;
		Shutdown();

#if WITH_EDITOR
		if (GIsEditor)
		{
			FSceneViewport* SceneVP = FindSceneViewport();
			if (SceneVP && SceneVP->IsStereoRenderingAllowed())
			{
				TSharedPtr<SWindow> Window = SceneVP->FindWindow();
				Window->RequestDestroyWindow();
			}
		}
		else
#endif//WITH_EDITOR
		{
			// ApplicationWillTerminateDelegate will fire from inside of the RequestExit
			FPlatformMisc::RequestExit(false);
		}
	}

	// If the HMD is being interacted with, but we haven't decided the HMD is worn yet.  
	if (bShouldCheckHMDPosition)
	{
		if (FVector::Dist(HMDStartLocation, Position) > HMDWornMovementThreshold)
		{
			HmdWornState = EHMDWornState::Worn;
			FCoreDelegates::VRHeadsetPutOnHead.Broadcast();
			bShouldCheckHMDPosition = false;
		}
	}


	return true;
}

void FSteamVRHMD::ResetOrientationAndPosition(float yaw)
{
	ResetOrientation(yaw);
	ResetPosition();
}

void FSteamVRHMD::ResetOrientation(float Yaw)
{
	const FTrackingFrame& TrackingFrame = GetTrackingFrame();

	FRotator ViewRotation;
	ViewRotation = FRotator(TrackingFrame.DeviceOrientation[vr::k_unTrackedDeviceIndex_Hmd]);
	ViewRotation.Pitch = 0;
	ViewRotation.Roll = 0;
	ViewRotation.Yaw += BaseOrientation.Rotator().Yaw;

	if (Yaw != 0.f)
	{
		// apply optional yaw offset
		ViewRotation.Yaw -= Yaw;
		ViewRotation.Normalize();
	}

	BaseOrientation = ViewRotation.Quaternion();
}
void FSteamVRHMD::ResetPosition()
{
	const FTrackingFrame& TrackingFrame = GetTrackingFrame();
	FMatrix Pose = ToFMatrix(TrackingFrame.RawPoses[vr::k_unTrackedDeviceIndex_Hmd]);
	BaseOffset = FVector(-Pose.M[3][2], Pose.M[3][0], Pose.M[3][1]) * TrackingFrame.WorldToMetersScale;
}

void FSteamVRHMD::SetBaseRotation(const FRotator& BaseRot)
{
	BaseOrientation = BaseRot.Quaternion();
}
FRotator FSteamVRHMD::GetBaseRotation() const
{
	return FRotator::ZeroRotator;
}

void FSteamVRHMD::SetBaseOrientation(const FQuat& BaseOrient)
{
	BaseOrientation = BaseOrient;
}

FQuat FSteamVRHMD::GetBaseOrientation() const
{
	return BaseOrientation;
}

void FSteamVRHMD::SetBasePosition(const FVector& BasePosition)
{
	BaseOffset = BasePosition;
}

FVector FSteamVRHMD::GetBasePosition() const
{
	return BaseOffset;
}

bool FSteamVRHMD::IsStereoEnabled() const
{
	return VRSystem && bStereoEnabled && bHmdEnabled;
}

bool FSteamVRHMD::EnableStereo(bool bStereo)
{
	if( bStereoEnabled == bStereo )
	{
		return false;
	}

	if (bStereo && bIsQuitting)
	{
		// Cancel shutting down the vr subsystem if re-enabling stereo before we're done counting down
		bIsQuitting = false;
	}

	if (VRSystem == nullptr && (!bStereo || !Startup()))
	{
		return false;
	}

	bStereoDesired = (IsHMDEnabled()) ? bStereo : false;
	if (bStereoDesired && !bOcclusionMeshesBuilt)
	{
		SetupOcclusionMeshes();
		bOcclusionMeshesBuilt = true;
	}

	// Set the viewport to match that of the HMD display
	FSceneViewport* SceneVP = FindSceneViewport();
	if (SceneVP)
	{
		TSharedPtr<SWindow> Window = SceneVP->FindWindow();
		if (Window.IsValid() && SceneVP->GetViewportWidget().IsValid())
		{
			// Set MirrorWindow state on the Window
			Window->SetMirrorWindow(bStereo);

			if( bStereo )
			{
				uint32 Width, Height;
				if (VRSystem)
				{
					VRSystem->GetRecommendedRenderTargetSize(&Width, &Height);
					//Width is one eye so double it for window bounds
					Width += Width;
				}
				else
				{
					Width = WindowMirrorBoundsWidth;
					Height = WindowMirrorBoundsHeight;
				}

				bStereoEnabled = bStereoDesired;
				SceneVP->SetViewportSize(Width, Height);
			}
			else
			{
				//flush all commands that might call GetStereoProjectionMatrix or other functions that rely on bStereoEnabled 
				FlushRenderingCommands();

				// Note: Setting before resize to ensure we don't try to allocate a new vr rt.
				bStereoEnabled = bStereoDesired;

				FRHIViewport* const ViewportRHI = SceneVP->GetViewportRHI();
				if (ViewportRHI != nullptr)
				{
					ViewportRHI->SetCustomPresent(nullptr);
				}

				FVector2D size = SceneVP->FindWindow()->GetSizeInScreen();
				SceneVP->SetViewportSize( size.X, size.Y );
				Window->SetViewportSizeDrivenByWindow( true );
			}
		}
	}

	// Uncap fps to enable FPS higher than 62
	GEngine->bForceDisableFrameRateSmoothing = bStereoEnabled;
	
	return bStereoEnabled;
}

void FSteamVRHMD::AdjustViewRect(int32 ViewIndex, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const
{
	const float PixelDensity = GetPixelDenity();

	SizeX = FMath::CeilToInt(IdealRenderTargetSize.X * PixelDensity);
	SizeY = FMath::CeilToInt(IdealRenderTargetSize.Y * PixelDensity);

	SizeX = SizeX / 2;
	X += SizeX * ViewIndex;
}

bool FSteamVRHMD::GetRelativeEyePose(int32 DeviceId, int32 ViewIndex, FQuat& OutOrientation, FVector& OutPosition)
{
	if (DeviceId != IXRTrackingSystem::HMDDeviceId || !(ViewIndex == EStereoscopicEye::eSSE_LEFT_EYE || ViewIndex == EStereoscopicEye::eSSE_RIGHT_EYE))
	{
		return false;
	}
	auto Frame = GetTrackingFrame();

	vr::Hmd_Eye HmdEye = (ViewIndex == EStereoscopicEye::eSSE_LEFT_EYE) ? vr::Eye_Left : vr::Eye_Right;
	vr::HmdMatrix34_t HeadFromEye = VRSystem->GetEyeToHeadTransform(HmdEye);

		// grab the eye position, currently ignoring the rotation supplied by GetHeadFromEyePose()
	OutPosition = FVector(-HeadFromEye.m[2][3], HeadFromEye.m[0][3], HeadFromEye.m[1][3]) * Frame.WorldToMetersScale;
	FQuat Orientation(ToFMatrix(HeadFromEye));

	OutOrientation.X = -Orientation.Z;
	OutOrientation.Y = Orientation.X;
	OutOrientation.Z = Orientation.Y;
	OutOrientation.W = -Orientation.W;

	return true;
}

void FSteamVRHMD::CalculateStereoViewOffset(const int32 StereoViewIndex, FRotator& ViewRotation, const float WorldToMeters, FVector& ViewLocation)
{
	// Needed to transform world locked stereo layers
	PlayerLocation = ViewLocation;

	// Forward to the base implementation (that in turn will call the DefaultXRCamera implementation)
	FHeadMountedDisplayBase::CalculateStereoViewOffset(StereoViewIndex, ViewRotation, WorldToMeters, ViewLocation);
}

FMatrix FSteamVRHMD::GetStereoProjectionMatrix(const int32 StereoViewIndex) const
{
	check(IsStereoEnabled() || IsHeadTrackingEnforced());

	vr::Hmd_Eye HmdEye = (StereoViewIndex == EStereoscopicEye::eSSE_RIGHT_EYE) ? vr::Eye_Right : vr::Eye_Left;
	float Left, Right, Top, Bottom;

	VRSystem->GetProjectionRaw(HmdEye, &Right, &Left, &Top, &Bottom);
	Bottom *= -1.0f;
	Top *= -1.0f;
	Right *= -1.0f;
	Left *= -1.0f;

	if (StereoViewIndex == EStereoscopicEye::eSSE_MONOSCOPIC)
	{
		float DummyLeft, DummyTop, DummyBottom;
		VRSystem->GetProjectionRaw(vr::Eye_Right, &Right, &DummyLeft, &DummyTop, &DummyBottom);
		Right *= -1.0f;
	}

	float ZNear = GNearClippingPlane;

	float SumRL = (Right + Left);
	float SumTB = (Top + Bottom);
	float InvRL = (1.0f / (Right - Left));
	float InvTB = (1.0f / (Top - Bottom));

#if 1
	FMatrix Mat = FMatrix(
		FPlane((2.0f * InvRL), 0.0f, 0.0f, 0.0f),
		FPlane(0.0f, (2.0f * InvTB), 0.0f, 0.0f),
		FPlane((SumRL * InvRL), (SumTB * InvTB), 0.0f, 1.0f),
		FPlane(0.0f, 0.0f, ZNear, 0.0f)
		);
#else
	vr::HmdMatrix44_t SteamMat = VRSystem->GetProjectionMatrix(HmdEye, ZNear, 10000.0f, vr::TextureType_DirectX);
	FMatrix Mat = ToFMatrix(SteamMat);

	Mat.M[3][3] = 0.0f;
	Mat.M[2][3] = 1.0f;
	Mat.M[2][2] = 0.0f;
	Mat.M[3][2] = ZNear;
#endif

	return Mat;
}

void FSteamVRHMD::GetEyeRenderParams_RenderThread(const FHeadMountedDisplayPassContext& Context, FVector2D& EyeToSrcUVScaleValue, FVector2D& EyeToSrcUVOffsetValue) const
{
	if (Context.View.StereoViewIndex == EStereoscopicEye::eSSE_LEFT_EYE)
	{
		EyeToSrcUVOffsetValue.X = 0.0f;
		EyeToSrcUVOffsetValue.Y = 0.0f;

		EyeToSrcUVScaleValue.X = 0.5f;
		EyeToSrcUVScaleValue.Y = 1.0f;
	}
	else
	{
		EyeToSrcUVOffsetValue.X = 0.5f;
		EyeToSrcUVOffsetValue.Y = 0.0f;

		EyeToSrcUVScaleValue.X = 0.5f;
		EyeToSrcUVScaleValue.Y = 1.0f;
	}
}

bool FSteamVRHMD::GetHMDDistortionEnabled(EShadingPath /* ShadingPath */) const
{
	return false;
}

float FSteamVRHMD::GetPixelDenity() const
{
	const FTrackingFrame& TrackingFrame = GetTrackingFrame();
	return TrackingFrame.PixelDensity;
}

void FSteamVRHMD::SetPixelDensity(const float NewDensity)
{
	check(IsInGameThread());
	GameTrackingFrame.PixelDensity = NewDensity;

	ENQUEUE_RENDER_COMMAND(UpdatePixelDensity)(
		[this, PixelDensity = GameTrackingFrame.PixelDensity](FRHICommandListImmediate&)
		{
			RenderTrackingFrame.PixelDensity = PixelDensity;
		});
}

void FSteamVRHMD::OnBeginRendering_GameThread()
{
	check(IsInGameThread());
	SpectatorScreenController->BeginRenderViewFamily();
}

void FSteamVRHMD::OnBeginRendering_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& ViewFamily)
{
	check(IsInRenderingThread());
	UpdatePoses();

	check(pBridge);
	pBridge->BeginRendering_RenderThread(RHICmdList);

	check(SpectatorScreenController);
	SpectatorScreenController->UpdateSpectatorScreenMode_RenderThread();

	// Update PlayerOrientation used by StereoLayers positioning
	const FSceneView* MainView = ViewFamily.Views[0];
	const FQuat ViewOrientation = MainView->ViewRotation.Quaternion();
	PlayerOrientation = ViewOrientation * MainView->BaseHmdOrientation.Inverse();
}

FXRRenderBridge* FSteamVRHMD::GetActiveRenderBridge_GameThread(bool /* bUseSeparateRenderTarget */)
{
	check(IsInGameThread());

	return pBridge;
}

void FSteamVRHMD::CalculateRenderTargetSize(const class FViewport& Viewport, uint32& InOutSizeX, uint32& InOutSizeY)
{
	if (!IsStereoEnabled())
	{
		return;
	}

	const float PixelDensity = GetPixelDenity();

	InOutSizeX = FMath::CeilToInt(IdealRenderTargetSize.X * PixelDensity);
	InOutSizeY = FMath::CeilToInt(IdealRenderTargetSize.Y * PixelDensity);

	check(InOutSizeX != 0 && InOutSizeY != 0);
}

bool FSteamVRHMD::NeedReAllocateViewportRenderTarget(const FViewport& Viewport)
{
	check(IsInGameThread());

	if (IsStereoEnabled())
	{
		const uint32 InSizeX = Viewport.GetSizeXY().X;
		const uint32 InSizeY = Viewport.GetSizeXY().Y;
		const FIntPoint RenderTargetSize = Viewport.GetRenderTargetTextureSizeXY();

		uint32 NewSizeX = InSizeX, NewSizeY = InSizeY;
		CalculateRenderTargetSize(Viewport, NewSizeX, NewSizeY);
		if (NewSizeX != RenderTargetSize.X || NewSizeY != RenderTargetSize.Y)
		{
			return true;
		}
	}
	return false;
}

bool FSteamVRHMD::NeedReAllocateDepthTexture(const TRefCountPtr<struct IPooledRenderTarget> & DepthTarget)
{
	check(IsInRenderingThread());

	// Check the dimensions of the currently stored depth swapchain vs the current rendering swapchain.
	if (pBridge->GetSwapChain()->GetTexture2D()->GetSizeX() != pBridge->GetDepthSwapChain()->GetTexture2D()->GetSizeX() ||
		pBridge->GetSwapChain()->GetTexture2D()->GetSizeY() != pBridge->GetDepthSwapChain()->GetTexture2D()->GetSizeY())
	{
		return true;
	}

	return false;
}

#if PLATFORM_MAC
static const uint32 SteamVRSwapChainLength = 3;
#else
static const uint32 SteamVRSwapChainLength = 1;
#endif

bool FSteamVRHMD::AllocateRenderTargetTexture(uint32 Index, uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, ETextureCreateFlags InTexFlags, ETextureCreateFlags InTargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32 NumSamples)
{
	if (!IsStereoEnabled())
	{
		return false;
	}

	TArray<FTextureRHIRef> SwapChainTextures;
	FTextureRHIRef BindingTexture;

#if PLATFORM_MAC
	MetalBridge* MetalBridgePtr = (MetalBridge*)pBridge.GetReference();
#endif

	if (pBridge != nullptr && pBridge->GetSwapChain() != nullptr && pBridge->GetSwapChain()->GetTexture2D() != nullptr && pBridge->GetSwapChain()->GetTexture2D()->GetSizeX() == SizeX && pBridge->GetSwapChain()->GetTexture2D()->GetSizeY() == SizeY)
	{
		OutTargetableTexture = (FTexture2DRHIRef&)pBridge->GetSwapChain()->GetTextureRef();
		OutShaderResourceTexture = OutTargetableTexture;
		return true;
	}

	FRHITextureCreateDesc Desc =
		FRHITextureCreateDesc::Create2D(TEXT("FSteamVRHMD"))
		.SetExtent(SizeX, SizeY)
		.SetFormat(PF_B8G8R8A8)
		.SetFlags(ETextureCreateFlags::RenderTargetable | ETextureCreateFlags::ShaderResource)
		.SetInitialState(ERHIAccess::SRVMask);

	for (uint32 SwapChainIter = 0; SwapChainIter < SteamVRSwapChainLength; ++SwapChainIter)
	{
#if PLATFORM_MAC
		IOSurfaceRef Surface = MetalBridgePtr->GetSurface(SizeX, SizeY);
		check(Surface != nil);

		Desc.BulkData = new FIOSurfaceResourceWrapper(Surface);
		CFRelease(Surface);
#endif

		FTexture2DRHIRef TargetableTexture;

		TargetableTexture = RHICreateTexture(Desc);

		SwapChainTextures.Add((FTextureRHIRef&)TargetableTexture);

		if (BindingTexture == nullptr)
		{
			BindingTexture = GDynamicRHI->RHICreateAliasedTexture((FTextureRHIRef&)TargetableTexture);
		}
	}

	pBridge->CreateSwapChain(BindingTexture, MoveTemp(SwapChainTextures));

	// These are the same.
	OutTargetableTexture = (FTexture2DRHIRef&)BindingTexture;
	OutShaderResourceTexture = (FTexture2DRHIRef&)BindingTexture;

	return true;
}

bool FSteamVRHMD::AllocateDepthTexture(uint32 Index, uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, ETextureCreateFlags Flags, ETextureCreateFlags TargetableTextureFlags,
	FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32 NumSamples /* ignored, we always use 1 */)
{
	if (!IsStereoEnabled() || pBridge == nullptr)
	{
		return false;
	}

#if PLATFORM_MAC
	// @todo: Determine if we want to manage depth on the Mac?
	return false;
#else
	auto DepthSwapChain = pBridge->GetDepthSwapChain();
	if (DepthSwapChain)
	{
		// If size is the same as requested, return the swap-chain texture.
		if (DepthSwapChain->GetTexture2D()->GetSizeX() == SizeX && DepthSwapChain->GetTexture2D()->GetSizeY() == SizeY)
		{
			// @todo: Do we need to check format, etc?
			OutTargetableTexture = OutShaderResourceTexture = DepthSwapChain->GetTexture2D();
			return true;
		}
	}

	TArray<FTextureRHIRef> SwapChainTextures;
	FTextureRHIRef BindingTexture;

	FClearValueBinding ClearValue(0.0f, 0);
	ClearValue.ColorBinding = EClearBinding::EDepthStencilBound;

	const FRHITextureCreateDesc Desc =
		FRHITextureCreateDesc::Create2D(TEXT("SteamVRDepthStencil"))
		.SetExtent(SizeX, SizeY)
		.SetFormat(PF_DepthStencil)
		.SetFlags(Flags | TargetableTextureFlags | ETextureCreateFlags::ShaderResource)
		.SetInitialState(ERHIAccess::SRVMask)
		.SetClearValue(ClearValue);

	for (uint32 SwapChainIter = 0; SwapChainIter < SteamVRSwapChainLength; ++SwapChainIter)
	{
		FTextureRHIRef TargetableTexture = RHICreateTexture(Desc);

		SwapChainTextures.Add(TargetableTexture);

		if (BindingTexture == nullptr)
		{
			BindingTexture = GDynamicRHI->RHICreateAliasedTexture((FTextureRHIRef&)TargetableTexture);
		}
	}

	OutTargetableTexture = (FTexture2DRHIRef&)BindingTexture;
	OutShaderResourceTexture = (FTexture2DRHIRef&)BindingTexture;

	// Create the bridge's depth swapchain.
	pBridge->CreateDepthSwapChain(BindingTexture, MoveTemp(SwapChainTextures));

	return true;
#endif
}

FSteamVRHMD::FSteamVRHMD(const FAutoRegister& AutoRegister, ISteamVRPlugin* InSteamVRPlugin) :
	FHeadMountedDisplayBase(nullptr),
	FHMDSceneViewExtension(AutoRegister),
	bHmdEnabled(true),
	HmdWornState(EHMDWornState::Unknown),
	bStereoDesired(false),
	bStereoEnabled(false),
	bOcclusionMeshesBuilt(false),
	WindowMirrorBoundsWidth(2160),
	WindowMirrorBoundsHeight(1200),
	HMDWornMovementThreshold(50.0f),
	HMDStartLocation(FVector::ZeroVector),
	BaseOrientation(FQuat::Identity),
	BaseOffset(FVector::ZeroVector),
	bIsQuitting(false),
	QuitTimestamp(),
	bShouldCheckHMDPosition(false),
	RendererModule(nullptr),
	SteamVRPlugin(InSteamVRPlugin),
	VRSystem(InSteamVRPlugin->GetVRSystem()),
	VRCompositor(InSteamVRPlugin->GetVRCompositor()),
	VROverlay(vr::VROverlay()),
	VRChaperone(vr::VRChaperone())
{
	Startup();
}

FSteamVRHMD::~FSteamVRHMD()
{
	Shutdown();
}

bool FSteamVRHMD::IsInitialized() const
{
	return (VRSystem != nullptr) && (VRCompositor != nullptr);
}

bool FSteamVRHMD::Startup()
{
	// grab a pointer to the renderer module for displaying our mirror window
	static const FName RendererModuleName("Renderer");
	RendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName);

	// Re-initialize the plugin if we're canceling the shutdown
	if (!IsInitialized())
	{
		SteamVRPlugin->Initialize();
		VRSystem = SteamVRPlugin->GetVRSystem();
		VRCompositor = SteamVRPlugin->GetVRCompositor();
		VROverlay = vr::VROverlay();
		VRChaperone = vr::VRChaperone();
	}

	if (ensure(IsInitialized()))
	{
		// grab info about the attached display
		FString DriverId = GetFStringTrackedDeviceProperty(VRSystem, vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_TrackingSystemName_String);
		DisplayId = GetFStringTrackedDeviceProperty(VRSystem, vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_SerialNumber_String);

		// determine our ideal screen percentage
		uint32 RecommendedWidth, RecommendedHeight;
		VRSystem->GetRecommendedRenderTargetSize(&RecommendedWidth, &RecommendedHeight);
		RecommendedWidth *= 2;

		IdealRenderTargetSize = FIntPoint(RecommendedWidth, RecommendedHeight);

		static const auto PixelDensityCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("vr.PixelDensity"));
		if (PixelDensityCVar)
		{ 
			SetPixelDensity(FMath::Clamp(PixelDensityCVar->GetFloat(), PixelDensityMin, PixelDensityMax));
		}

		// enforce finishcurrentframe
		static IConsoleVariable* CFCFVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.finishcurrentframe"));
		CFCFVar->Set(false);

		// Grab the chaperone
		ensure(VRChaperone);
		if (VRChaperone)
		{
			ChaperoneBounds = FChaperoneBounds(VRChaperone);
		}

#if PLATFORM_MAC
		if (IsMetalPlatform(GMaxRHIShaderPlatform))
		{
			pBridge = new MetalBridge(this);
		}
#else
		if ( IsPCPlatform( GMaxRHIShaderPlatform ) )
		{
			if ( IsVulkanPlatform( GMaxRHIShaderPlatform ) )
			{
				pBridge = new VulkanBridge( this );
			}
			else if ( IsOpenGLPlatform( GMaxRHIShaderPlatform ) )
			{
				pBridge = new OpenGLBridge( this );
			}
#if PLATFORM_WINDOWS
			else
			{
				auto level = FSteamVRPlugin::GetD3DApiLevel();

				if (level == FSteamVRPlugin::D3DApiLevel::Direct3D11)
				{
					pBridge = new D3D11Bridge(this);
				}
				else if (level == FSteamVRPlugin::D3DApiLevel::Direct3D12)
				{
					pBridge = new D3D12Bridge(this);
				}
			}
#endif
			ensure( pBridge != nullptr );
		}
#endif

		if(pBridge->IsUsingExplicitTimingMode())
		{
			VRCompositor->SetExplicitTimingMode(vr::EVRCompositorTimingMode::VRCompositorTimingMode_Explicit_ApplicationPerformsPostPresentHandoff);
		}

		LoadFromIni();

		SplashTicker = MakeShareable(new FSteamSplashTicker(this));
		SplashTicker->RegisterForMapLoad();

		CreateSpectatorScreenController();

		UE_LOG(LogHMD, Log, TEXT("SteamVR initialized.  Driver: %s  Display: %s"), *DriverId, *DisplayId);
		return true;
	}

	return false;

}

void FSteamVRHMD::LoadFromIni()
{
	const TCHAR* SteamVRSettings = TEXT("SteamVR.Settings");
	int32 i;

	if (GConfig->GetInt(SteamVRSettings, TEXT("WindowMirrorBoundsWidth"), i, GEngineIni))
	{
		WindowMirrorBoundsWidth = i;
	}

	if (GConfig->GetInt(SteamVRSettings, TEXT("WindowMirrorBoundsHeight"), i, GEngineIni))
	{
		WindowMirrorBoundsHeight = i;
	}

	float ConfigFloat = 0.0f;

	if (GConfig->GetFloat(SteamVRSettings, TEXT("HMDWornMovementThreshold"), ConfigFloat, GEngineIni))
	{
		HMDWornMovementThreshold = ConfigFloat;
	}
}

void FSteamVRHMD::Shutdown()
{
	if (VRSystem != nullptr)
	{
		UE_LOG(LogHMD, Log, TEXT("SteamVR Shutting down."));

		SplashTicker->UnregisterForMapLoad();
		SplashTicker = nullptr;

		// shut down our headset
		VRSystem = nullptr;
		VRCompositor = nullptr;
		VRChaperone = nullptr;
		VROverlay = nullptr;

		SteamVRPlugin->Reset();
	}
}

static void SetupHiddenAreaMeshes(vr::IVRSystem* const VRSystem, FHMDViewMesh Result[2], const vr::EHiddenAreaMeshType MeshType)
{
	const vr::HiddenAreaMesh_t LeftEyeMesh = VRSystem->GetHiddenAreaMesh(vr::Hmd_Eye::Eye_Left, MeshType);
	const vr::HiddenAreaMesh_t RightEyeMesh = VRSystem->GetHiddenAreaMesh(vr::Hmd_Eye::Eye_Right, MeshType);

	const uint32 VertexCount = LeftEyeMesh.unTriangleCount * 3;
	check(LeftEyeMesh.unTriangleCount == RightEyeMesh.unTriangleCount);

	// Copy mesh data from SteamVR format to ours, then initialize the meshes.
	if (VertexCount > 0)
	{
		FVector2D* const LeftEyePositions = new FVector2D[VertexCount];
		FVector2D* const RightEyePositions = new FVector2D[VertexCount];

		for (uint32 VertexIndex = 0; VertexIndex < VertexCount; ++VertexIndex)
		{
			const vr::HmdVector2_t& LeftSrc = LeftEyeMesh.pVertexData[VertexIndex];
			const vr::HmdVector2_t& RightSrc = RightEyeMesh.pVertexData[VertexIndex];

			FVector2D& LeftDst = LeftEyePositions[VertexIndex];
			FVector2D& RightDst = RightEyePositions[VertexIndex];

			LeftDst.X = LeftSrc.v[0];
			LeftDst.Y = LeftSrc.v[1];

			RightDst.X = RightSrc.v[0];
			RightDst.Y = RightSrc.v[1];
		}
		
		const FHMDViewMesh::EHMDMeshType MeshTransformType = (MeshType == vr::EHiddenAreaMeshType::k_eHiddenAreaMesh_Standard) ? FHMDViewMesh::MT_HiddenArea : FHMDViewMesh::MT_VisibleArea;
		Result[0].BuildMesh(LeftEyePositions, VertexCount, MeshTransformType);
		Result[1].BuildMesh(RightEyePositions, VertexCount, MeshTransformType);

		delete[] LeftEyePositions;
		delete[] RightEyePositions;
	}
}


void FSteamVRHMD::SetupOcclusionMeshes()
{
	FSteamVRHMD* const Self = this;
	ENQUEUE_RENDER_COMMAND(SetupOcclusionMeshesCmd)([Self](FRHICommandListImmediate& RHICmdList)
	{
		SetupHiddenAreaMeshes(Self->VRSystem, Self->HiddenAreaMeshes, vr::EHiddenAreaMeshType::k_eHiddenAreaMesh_Standard);

		if (CUseSteamVRVisibleAreaMesh.GetValueOnAnyThread() > 0)
		{
			SetupHiddenAreaMeshes(Self->VRSystem, Self->VisibleAreaMeshes, vr::EHiddenAreaMeshType::k_eHiddenAreaMesh_Inverse);
		}
	});
}

const FSteamVRHMD::FTrackingFrame& FSteamVRHMD::GetTrackingFrame() const
{
	if (IsInRenderingThread())
	{
		return RenderTrackingFrame;
	}
	else
	{
		return GameTrackingFrame;
	}
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS

#endif //STEAMVR_SUPPORTED_PLATFORMS

