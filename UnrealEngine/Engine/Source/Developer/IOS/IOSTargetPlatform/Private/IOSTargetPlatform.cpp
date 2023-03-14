// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	IOSTargetPlatform.cpp: Implements the FIOSTargetPlatform class.
=============================================================================*/

#include "IOSTargetPlatform.h"
#include "Interfaces/IProjectManager.h"
#include "InstalledPlatformInfo.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include "Misc/MonitoredProcess.h"
#include "Logging/MessageLog.h"
#include "AnalyticsEventAttribute.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif
#if WITH_ENGINE
#include "Engine/TextureCube.h"
#include "Sound/SoundWave.h"
#include "TextureResource.h"
#include "AudioCompressionSettings.h"
#endif

/* FIOSTargetPlatform structors
 *****************************************************************************/

FIOSTargetPlatform::FIOSTargetPlatform(bool bInIsTVOS, bool bIsClientOnly)
	// override the ini name up in the base classes, which will go into the FTargetPlatformInfo
	: TNonDesktopTargetPlatformBase(bIsClientOnly, nullptr, bInIsTVOS ? TEXT("TVOS") : nullptr)
	, bIsTVOS(bInIsTVOS)
	, MobileShadingPath(0)
	, bDistanceField(false)
	, bMobileForwardEnableClusteredReflections(false)
{
#if WITH_ENGINE
	TextureLODSettings = nullptr; // TextureLODSettings are registered by the device profile.
	StaticMeshLODSettings.Initialize(this);
	GetConfigSystem()->GetBool(TEXT("/Script/Engine.RendererSettings"), TEXT("r.DistanceFields"), bDistanceField, GEngineIni);
	GetConfigSystem()->GetInt(TEXT("/Script/Engine.RendererSettings"), TEXT("r.Mobile.ShadingPath"), MobileShadingPath, GEngineIni);
	GetConfigSystem()->GetBool(TEXT("/Script/Engine.RendererSettings"), TEXT("r.Mobile.Forward.EnableClusteredReflections"), bMobileForwardEnableClusteredReflections, GEngineIni);
#endif // #if WITH_ENGINE

	// initialize the connected device detector
	DeviceHelper.OnDeviceConnected().AddRaw(this, &FIOSTargetPlatform::HandleDeviceConnected);
	DeviceHelper.OnDeviceDisconnected().AddRaw(this, &FIOSTargetPlatform::HandleDeviceDisconnected);
	DeviceHelper.Initialize(bIsTVOS);
}


FIOSTargetPlatform::~FIOSTargetPlatform()
{
}


/* ITargetPlatform interface
 *****************************************************************************/

void FIOSTargetPlatform::EnableDeviceCheck(bool OnOff)
{
	FIOSDeviceHelper::EnableDeviceCheck(OnOff);
}

void FIOSTargetPlatform::GetAllDevices( TArray<ITargetDevicePtr>& OutDevices ) const
{
	OutDevices.Reset();

	for (auto Iter = Devices.CreateConstIterator(); Iter; ++Iter)
	{
		OutDevices.Add(Iter.Value());
	}
}


ITargetDevicePtr FIOSTargetPlatform::GetDefaultDevice() const
{
	if (Devices.Num() > 0)
	{
		// first device is the default
		auto Iter = Devices.CreateConstIterator();
		if(Iter)
		{
			return Iter.Value();
		}
	}

	return NULL;
}


ITargetDevicePtr FIOSTargetPlatform::GetDevice( const FTargetDeviceId& DeviceId )
{
	return Devices.FindRef(DeviceId);
}

static FString OutputMessage;
static void OnOutput(FString Message)
{
    OutputMessage += Message;
    UE_LOG(LogTemp, Display, TEXT("%s\n"), *Message);
}

bool FIOSTargetPlatform::IsSdkInstalled(bool bProjectHasCode, FString& OutTutorialPath) const
{
#if PLATFORM_MAC
	OutTutorialPath = FString("Shared/Tutorials/InstallingXCodeTutorial");

	// run xcode-select and get the location of Xcode
	FString CmdExe = TEXT("/usr/bin/xcode-select");
	FString CommandLine = FString::Printf(TEXT("--print-path"));
	TSharedPtr<FMonitoredProcess> IPPProcess = MakeShareable(new FMonitoredProcess(CmdExe, CommandLine, true));
	OutputMessage = TEXT("");
	IPPProcess->OnOutput().BindStatic(&OnOutput);
	IPPProcess->Launch();
	while (IPPProcess->Update())
	{
		FPlatformProcess::Sleep(0.01f);
	}
	int RetCode = IPPProcess->GetReturnCode();
//	UE_LOG(LogTemp, Display, TEXT("%s"), *OutputMessage);

	bool biOSSDKInstalled = IFileManager::Get().DirectoryExists(*OutputMessage);
#else
	OutTutorialPath = FString("/Engine/Tutorial/Mobile/InstallingiTunesTutorial.InstallingiTunesTutorial");

	// On windows we check if itunes is installed - Perhaps someday make this its own check instead of piggy packing on the SDK check which will create a unintuitive error message when it fails

	// The logic here is to assume the correct Apple dll does not exist and then check the various locations it could be in, setting this to true when it is found
	// Code is structured for clarity not performance
	// See Engine\Source\Programs\IOS\MobileDeviceInterface\MobileDevice.cs for reference
	bool biOSSDKInstalled = false; 

	HKEY hKey;
	TCHAR dllPath[256];
	unsigned long pathSize = 256;
	
	// Add future version checks here

	// Check for iTunes 12
	if (!biOSSDKInstalled && RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\Wow6432Node\\Apple Inc.\\Apple Mobile Device Support\\Shared"), 0, KEY_READ, &hKey) == ERROR_SUCCESS)
	{
		if (RegQueryValueEx(hKey, TEXT("MobileDeviceDLL"), 0, NULL, (BYTE*)dllPath, &pathSize) == ERROR_SUCCESS
			&& IFileManager::Get().FileSize(*FString(dllPath)) != INDEX_NONE)
		{
			biOSSDKInstalled = true;
		}
		else
		{
			// iTunes >= 12.7 doesn't have a key specifying the 32-bit DLL but it does have a ASMapiInterfaceDLL key and MobileDevice.dll is in usually in the same directory
			if (RegQueryValueEx(hKey, TEXT("ASMapiInterfaceDLL"), 0, NULL, (BYTE*)dllPath, &pathSize) == ERROR_SUCCESS)
			{
				FString MobileDeviceDLLPath = FString(dllPath);
				int32 Index = MobileDeviceDLLPath.Find(TEXT("\\"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
				if(Index != INDEX_NONE)
				{
					MobileDeviceDLLPath = MobileDeviceDLLPath.Left(Index+1) + TEXT("MobileDevice.dll");
					if (IFileManager::Get().FileSize(*MobileDeviceDLLPath) != INDEX_NONE)
					{
						biOSSDKInstalled = true;
					}
				}
			}
		}
	}

	// Check for iTunes 12, Windows Store version
	if (!biOSSDKInstalled)
	{
		HKEY hPackagesKey;
		FString PackagesKeyName(TEXT("Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppModel\\PackageRepository\\Packages"));
		if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, *PackagesKeyName, 0, KEY_READ, &hPackagesKey) == ERROR_SUCCESS)
		{
			DWORD NumSubKeys = 0;
			DWORD LongestSubKeyLength;
			if (RegQueryInfoKey(hPackagesKey, NULL, NULL, NULL, &NumSubKeys, &LongestSubKeyLength, NULL, NULL, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
			{
				static const uint32 MAX_SUBKEY_LENGTH = 512;
				check(LongestSubKeyLength < MAX_SUBKEY_LENGTH);

				for (uint32 i = 0; i < NumSubKeys; i++)
				{
					TCHAR SubKeyName[MAX_SUBKEY_LENGTH];
					DWORD SubKeyLength = MAX_SUBKEY_LENGTH;
					if (RegEnumKeyEx(hPackagesKey, i, SubKeyName, &SubKeyLength, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
					{
						check(SubKeyLength < MAX_SUBKEY_LENGTH);

						if (FString(SubKeyName).Contains(TEXT("AppleInc.iTunes")) && (FString(SubKeyName).Contains(TEXT("_x64")) || FString(SubKeyName).Contains(TEXT("_x86"))))
						{
							HKEY iTunesKey;
							TCHAR FullPackageSubKeyName[MAX_SUBKEY_LENGTH + MAX_SUBKEY_LENGTH + 1];
							_stprintf_s(FullPackageSubKeyName, MAX_SUBKEY_LENGTH + MAX_SUBKEY_LENGTH + 1, TEXT("%s\\%s"), *PackagesKeyName, SubKeyName);
							if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, FullPackageSubKeyName, 0, KEY_READ, &iTunesKey) == ERROR_SUCCESS)
							{
								if (RegQueryValueEx(iTunesKey, TEXT("Path"), NULL, NULL, (BYTE*)dllPath, &pathSize) == ERROR_SUCCESS)
								{
									TCHAR dllFullPath[MAX_SUBKEY_LENGTH + MAX_SUBKEY_LENGTH + 32];
									_stprintf_s(dllFullPath, MAX_SUBKEY_LENGTH + MAX_SUBKEY_LENGTH + 32, TEXT("%s\\AMDS32\\MobileDevice.dll"), dllPath);

									if (IFileManager::Get().FileSize(*FString(dllFullPath)) != INDEX_NONE)
									{
										biOSSDKInstalled = true;
										break;
									}
								}
							}
						}
					}
				}
			}
		}
	}

	// Check for iTunes 11
	if(!biOSSDKInstalled
		&& RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\Wow6432Node\\Apple Inc.\\Apple Mobile Device Support\\Shared"), 0, KEY_READ, &hKey) == ERROR_SUCCESS
		&& RegQueryValueEx(hKey, TEXT("iTunesMobileDeviceDLL"), 0, NULL, (BYTE*)dllPath, &pathSize) == ERROR_SUCCESS
		&&  IFileManager::Get().FileSize(*FString(dllPath)) != INDEX_NONE)
			{
		biOSSDKInstalled = true;
	}

#endif
	return biOSSDKInstalled;
}

int32 FIOSTargetPlatform::CheckRequirements(bool bProjectHasCode, EBuildConfiguration Configuration, bool bRequiresAssetNativization, FString& OutTutorialPath, FString& OutDocumentationPath, FText& CustomizedLogMessage) const
{
	OutDocumentationPath = TEXT("Platforms/iOS/QuickStart/6");

	int32 bReadyToBuild = ETargetPlatformReadyStatus::Ready; // @todo How do we check that the iOS SDK is installed when building from Windows? Is that even possible?
	if (!IsSdkInstalled(bProjectHasCode, OutTutorialPath))
	{
		bReadyToBuild |= ETargetPlatformReadyStatus::SDKNotFound;
	}
#if PLATFORM_MAC
	OutTutorialPath = FString("/Engine/Tutorial/Installation/InstallingXCodeTutorial.InstallingXCodeTutorial");
    // shell to certtool
#else
	if (!FInstalledPlatformInfo::Get().IsValidPlatform(GetPlatformInfo().UBTPlatformString, EProjectType::Code))
	{
		if (bProjectHasCode)
		{
			OutTutorialPath = FString("/Engine/Tutorial/Mobile/iOSonPCRestrictions.iOSonPCRestrictions");
			bReadyToBuild |= ETargetPlatformReadyStatus::CodeUnsupported;
		}

		FText Reason;
		if (RequiresTempTarget(bProjectHasCode, Configuration, bRequiresAssetNativization, Reason))
		{
			OutTutorialPath = FString("/Engine/Tutorial/Mobile/iOSonPCValidPlugins.iOSonPCValidPlugins");
			bReadyToBuild |= ETargetPlatformReadyStatus::PluginsUnsupported;
		}
	}
#endif

	// shell to IPP and get the status of the provision and cert

	bool bForDistribtion = false;
	GConfig->GetBool(TEXT("/Script/UnrealEd.ProjectPackagingSettings"), TEXT("ForDistribution"), bForDistribtion, GGameIni);

	FString BundleIdentifier;
	GConfig->GetString(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("BundleIdentifier"), BundleIdentifier, GEngineIni);
	BundleIdentifier = BundleIdentifier.Replace(TEXT("[PROJECT_NAME]"), FApp::GetProjectName());
	BundleIdentifier = BundleIdentifier.Replace(TEXT("_"), TEXT(""));

	bool bAutomaticSigning = false;
	GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bAutomaticSigning"), bAutomaticSigning, GEngineIni);

	FString TeamID;
	GConfig->GetString(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("IOSTeamID"), TeamID, GEngineIni);

	FString ProjectPath = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
#if PLATFORM_MAC
    FString CmdExe = TEXT("/bin/sh");
    FString ScriptPath = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Build/BatchFiles/Mac/RunMono.sh"));
    FString IPPPath = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Binaries/DotNET/IOS/IPhonePackager.exe"));
	FString CommandLine = FString::Printf(TEXT("\"%s\" \"%s\" Validate Engine -project \"%s\" -bundlename \"%s\" -teamID \"%s\" %s %s"), *ScriptPath, *IPPPath, *ProjectPath, *(BundleIdentifier), *(TeamID), (bForDistribtion ? TEXT("-distribution") : TEXT("")), bAutomaticSigning ? TEXT("-autosigning") : TEXT(""));
#else
	FString CmdExe = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Binaries/DotNET/IOS/IPhonePackager.exe"));
	FString CommandLine = FString::Printf(TEXT("Validate Engine -project \"%s\" -bundlename \"%s\" %s"), *ProjectPath, *(BundleIdentifier), (bForDistribtion ? TEXT("-distribution") : TEXT("")) );
	FString RemoteServerName;
	FString RSyncUsername;
	GConfig->GetString(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("RemoteServerName"), RemoteServerName, GEngineIni);
	GConfig->GetString(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("RSyncUsername"), RSyncUsername, GEngineIni);
	if (RemoteServerName.Len() == 0 || RSyncUsername.Len() == 0)
	{
		bReadyToBuild |= ETargetPlatformReadyStatus::RemoveServerNameEmpty;
	}

#endif
	if (bIsTVOS)
	{
		CommandLine += " -tvos";
	}
	TSharedPtr<FMonitoredProcess> IPPProcess = MakeShareable(new FMonitoredProcess(CmdExe, CommandLine, true));
	OutputMessage = TEXT("");
	IPPProcess->OnOutput().BindStatic(&OnOutput);
	IPPProcess->Launch();
	while(IPPProcess->Update())
	{
		FPlatformProcess::Sleep(0.01f);
	}
	int RetCode = IPPProcess->GetReturnCode();
//	UE_LOG(LogTemp, Display, TEXT("%s"), *OutputMessage);
	if (RetCode == 14)
	{
		OutTutorialPath = FString("/Engine/Tutorial/Mobile/CreatingInfoPlist.CreatingInfoPlist");
		bReadyToBuild |= ETargetPlatformReadyStatus::ManifestNotFound;
	}
	else if (RetCode == 13)
	{
		OutTutorialPath = FString("/Engine/Tutorial/Mobile/CreatingSigningCertAndProvisionTutorial.CreatingSigningCertAndProvisionTutorial");
		bReadyToBuild |= ETargetPlatformReadyStatus::SigningKeyNotFound;
		bReadyToBuild |= ETargetPlatformReadyStatus::ProvisionNotFound;
	}
	else if (RetCode == 12)
	{
		OutTutorialPath = FString("/Engine/Tutorial/Mobile/CreatingSigningCertAndProvisionTutorial.CreatingSigningCertAndProvisionTutorial");
		bReadyToBuild |= ETargetPlatformReadyStatus::SigningKeyNotFound;
	}
	else if (RetCode == 11)
	{
		OutTutorialPath = FString("/Engine/Tutorial/Mobile/CreatingSigningCertAndProvisionTutorial.CreatingSigningCertAndProvisionTutorial");
		bReadyToBuild |= ETargetPlatformReadyStatus::ProvisionNotFound;
	}

	{
		TArray<FString> FoundIconFiles;
		FString Wildcard = FPaths::Combine(FPaths::ProjectDir(), TEXT("Build"), TEXT("IOS"), TEXT("Resources"), TEXT("Graphics"), TEXT("Icon*.png"));
		IFileManager::Get().FindFiles(FoundIconFiles, *Wildcard, true, false);
		if (FoundIconFiles.Num() > 0)
		{
			bReadyToBuild |= ETargetPlatformReadyStatus::CodeBuildRequired;
		}
	}

	return bReadyToBuild;
}



/* FIOSTargetPlatform callbacks
 *****************************************************************************/

void FIOSTargetPlatform::HandlePongMessage( const FIOSLaunchDaemonPong& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context )
{
	FTargetDeviceId DeviceId;
	FTargetDeviceId::Parse(Message.DeviceID, DeviceId);

	FIOSTargetDevicePtr& Device = Devices.FindOrAdd(DeviceId);

	if (!Device.IsValid())
	{
		Device = MakeShareable(new FIOSTargetDevice(*this));

		Device->SetFeature(ETargetDeviceFeatures::Reboot, Message.bCanReboot);
		Device->SetFeature(ETargetDeviceFeatures::PowerOn, Message.bCanPowerOn);
		Device->SetFeature(ETargetDeviceFeatures::PowerOff, Message.bCanPowerOff);
		Device->SetDeviceId(DeviceId);
		Device->SetDeviceName(Message.DeviceName);
		Device->SetDeviceType(Message.DeviceType);
		Device->SetDeviceEndpoint(Context->GetSender());
		Device->SetIsSimulated(Message.DeviceID.Contains(TEXT("Simulator")));

		OnDeviceDiscovered().Broadcast(Device.ToSharedRef());
	}

	Device->LastPinged = FDateTime::UtcNow();
}

void FIOSTargetPlatform::HandleDeviceConnected(const FIOSLaunchDaemonPong& Message)
{
	FTargetDeviceId DeviceId;
	FTargetDeviceId::Parse(Message.DeviceID, DeviceId);
	
	FIOSTargetDevicePtr& Device = Devices.FindOrAdd(DeviceId);
	
	if (!Device.IsValid())
	{
	if ((Message.DeviceType.Contains(TEXT("AppleTV")) && bIsTVOS) || (!Message.DeviceType.Contains(TEXT("AppleTV")) && !bIsTVOS))
		{
			Device = MakeShareable(new FIOSTargetDevice(*this));

			Device->SetFeature(ETargetDeviceFeatures::Reboot, Message.bCanReboot);
			Device->SetFeature(ETargetDeviceFeatures::PowerOn, Message.bCanPowerOn);
			Device->SetFeature(ETargetDeviceFeatures::PowerOff, Message.bCanPowerOff);
			Device->SetDeviceId(DeviceId);
			Device->SetDeviceName(Message.DeviceName);
			Device->SetAuthorized(Message.bIsAuthorized);
			Device->SetDeviceType(Message.DeviceType);
			Device->SetIsSimulated(Message.DeviceID.Contains(TEXT("Simulator")));

			OnDeviceDiscovered().Broadcast(Device.ToSharedRef());
		}
		else
		{
			return;
		}
	}
	
	// Add a very long time period to prevent the devices from getting disconnected due to a lack of pong messages
	Device->LastPinged = FDateTime::UtcNow() + FTimespan::FromDays(100.0);
}


void FIOSTargetPlatform::HandleDeviceDisconnected(const FIOSLaunchDaemonPong& Message)
{
	FTargetDeviceId DeviceId;
	FTargetDeviceId::Parse(Message.DeviceID, DeviceId);
	
	FIOSTargetDevicePtr& Device = Devices.FindOrAdd(DeviceId);
	
	if (Device.IsValid())
	{
		OnDeviceLost().Broadcast(Device.ToSharedRef());
		Devices.Remove(DeviceId);
	}
}


/* ITargetPlatform interface
 *****************************************************************************/
static bool UsesVirtualTextures()
{
	static auto* CVarMobileVirtualTextures = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.VirtualTextures"));
	return CVarMobileVirtualTextures->GetValueOnAnyThread() != 0;
}

static bool SupportsMetal()
{
	// default to NOT supporting metal
	bool bSupportsMetal = false;
	GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportsMetal"), bSupportsMetal, GEngineIni);
	return bSupportsMetal;
}

static bool SupportsMetalMRT()
{
	// default to NOT supporting metal MRT
	bool bSupportsMetalMRT = false;
	GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportsMetalMRT"), bSupportsMetalMRT, GEngineIni);
	return bSupportsMetalMRT;
}

static bool SupportsA8Devices()
{
    // default to NOT supporting A8 devices
    bool bSupportAppleA8 = false;
    GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportAppleA8"), bSupportAppleA8, GEngineIni);
    return bSupportAppleA8;
}

bool FIOSTargetPlatform::CanSupportRemoteShaderCompile() const
{
	// for 4.22 we are disabling support for XGE Shader compile on IOS
	bool bRemoteCompilingEnabled = false;
	GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("EnableRemoteShaderCompile"), bRemoteCompilingEnabled, GEngineIni);
	return false; // !bRemoteCompilingEnabled;
}

bool FIOSTargetPlatform::SupportsFeature( ETargetPlatformFeatures Feature ) const
{
	switch (Feature)
	{
		case ETargetPlatformFeatures::Packaging:
		case ETargetPlatformFeatures::DeviceOutputLog:
			return true;

		case ETargetPlatformFeatures::MobileRendering:
		case ETargetPlatformFeatures::LowQualityLightmaps:
			return SupportsMetal();
			
		case ETargetPlatformFeatures::DeferredRendering:
		case ETargetPlatformFeatures::HighQualityLightmaps:
			return SupportsMetalMRT();

		case ETargetPlatformFeatures::VirtualTextureStreaming:
			return UsesVirtualTextures();

		case ETargetPlatformFeatures::DistanceFieldAO:
			return UsesDistanceFields();
		
		default:
			break;
	}
	
	return TTargetPlatformBase<FIOSPlatformProperties>::SupportsFeature(Feature);
}


void FIOSTargetPlatform::GetAllPossibleShaderFormats( TArray<FName>& OutFormats ) const
{
	static FName NAME_SF_METAL(TEXT("SF_METAL"));
	static FName NAME_SF_METAL_MRT(TEXT("SF_METAL_MRT"));
	static FName NAME_SF_METAL_TVOS(TEXT("SF_METAL_TVOS"));
	static FName NAME_SF_METAL_MRT_TVOS(TEXT("SF_METAL_MRT_TVOS"));

	if (bIsTVOS)
	{
		if (SupportsMetalMRT())
		{
			OutFormats.AddUnique(NAME_SF_METAL_MRT_TVOS);
		}

		// because we are currently using IOS settings, we will always use metal, even if Metal isn't listed as being supported
		// however, if MetalMRT is specific and Metal is set to false, then we will just use MetalMRT
		if (SupportsMetal() || !SupportsMetalMRT())
		{
			OutFormats.AddUnique(NAME_SF_METAL_TVOS);
		}
	}
	else
	{
		if (SupportsMetal())
		{
			OutFormats.AddUnique(NAME_SF_METAL);
		}

		if (SupportsMetalMRT())
		{
			OutFormats.AddUnique(NAME_SF_METAL_MRT);
		}
	}
}

void FIOSTargetPlatform::GetAllTargetedShaderFormats( TArray<FName>& OutFormats ) const
{
	GetAllPossibleShaderFormats(OutFormats);
}

void FIOSTargetPlatform::GetPlatformSpecificProjectAnalytics( TArray<FAnalyticsEventAttribute>& AnalyticsParamArray ) const
{
	TNonDesktopTargetPlatformBase<FIOSPlatformProperties>::GetPlatformSpecificProjectAnalytics( AnalyticsParamArray );

	AppendAnalyticsEventAttributeArray(AnalyticsParamArray,
		TEXT("SupportsMetalMRT"), SupportsMetalMRT()
	);
}

#if WITH_ENGINE

void FIOSTargetPlatform::GetReflectionCaptureFormats( TArray<FName>& OutFormats ) const
{
	const bool bMobileDeferredShading = (MobileShadingPath == 1);

	if (SupportsMetalMRT() || bMobileDeferredShading || bMobileForwardEnableClusteredReflections)
	{
		OutFormats.Add(FName(TEXT("FullHDR")));
	}

	OutFormats.Add(FName(TEXT("EncodedHDR")));
}

static const FName NameASTC_RGB_HDR(TEXT("ASTC_RGB_HDR"));

// we remap some of the defaults
static const FName FormatRemap[] =
{
	// original				ASTC
	FName(TEXT("AutoDXT")),	FName(TEXT("ASTC_RGBAuto")),
	FName(TEXT("DXT1")),	FName(TEXT("ASTC_RGB")),
	FName(TEXT("DXT5")),	FName(TEXT("ASTC_RGBA")),
	FName(TEXT("DXT5n")),	FName(TEXT("ASTC_NormalAG")),
	FName(TEXT("BC5")),		FName(TEXT("ASTC_NormalRG")),
	FName(TEXT("BC4")),		FName(TEXT("ETC2_R11")),
	FName(TEXT("BC6H")),	NameASTC_RGB_HDR,
	FName(TEXT("BC7")),		FName(TEXT("ASTC_RGBA_HQ"))
};
static const FName NameG8(TEXT("G8"));
static const FName NameRGBA16F(TEXT("RGBA16F"));

void FIOSTargetPlatform::GetTextureFormats( const UTexture* Texture, TArray< TArray<FName> >& OutFormats) const
{
	check(Texture);

	const int32 NumLayers = Texture->Source.GetNumLayers();
	
	TArray<FName>& TextureFormatNames = OutFormats.AddDefaulted_GetRef();
	TextureFormatNames.Reserve(NumLayers);

	// optionaly compress landscape weightmaps for a mobile rendering
	static const auto CompressLandscapeWeightMapsVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.CompressLandscapeWeightMaps"));
	static const bool bCompressLandscapeWeightMaps = (CompressLandscapeWeightMapsVar && CompressLandscapeWeightMapsVar->GetValueOnAnyThread() != 0);
	if (Texture->LODGroup == TEXTUREGROUP_Terrain_Weightmap && bCompressLandscapeWeightMaps)
	{
		TextureFormatNames.Init(FName(TEXT("AutoDXT")), NumLayers);
	}

	// if we didn't assign anything specially, then use the defaults
	if (TextureFormatNames.Num() == 0)
	{
        int32 BlockSize = 1;
		// Compressed volume textures require MTLGPUFamilyApple3 or later
		// min spec for TVOS is AppleTV HD which is MTLGPUFamilyApple2 (A8)
		bool bSupportCompressedVolumeTexture = !bIsTVOS && !SupportsA8Devices();
		bool bSupportFilteredFloat32Textures = false;
		GetDefaultTextureFormatNamePerLayer(TextureFormatNames, this, Texture, bSupportCompressedVolumeTexture, BlockSize, bSupportFilteredFloat32Textures);
	}

	// include the formats we want
	for (FName& TextureFormatName : TextureFormatNames)
	{
		for (int32 RemapIndex = 0; RemapIndex < UE_ARRAY_COUNT(FormatRemap); RemapIndex += 2)
		{
			if (TextureFormatName == FormatRemap[RemapIndex])
			{
				TextureFormatName = FormatRemap[RemapIndex + 1];
				break;
			}
		}
	}
	
	bool bSupportASTCHDR = UsesASTCHDR();

	if ( ! bSupportASTCHDR )
	{
		for (FName& TextureFormatName : TextureFormatNames)
		{
			if ( TextureFormatName == NameASTC_RGB_HDR )
			{
				TextureFormatName = NameRGBA16F;
			}
		}
	}

	for (FName& TextureFormatName : OutFormats.Last())
	{
		if (Texture->IsA(UTextureCube::StaticClass()))
		{
			const UTextureCube* Cube = CastChecked<UTextureCube>(Texture);
			if (Cube != nullptr)
			{
				FTextureFormatSettings FormatSettings;
				Cube->GetDefaultFormatSettings(FormatSettings);
				if (FormatSettings.CompressionSettings == TC_EncodedReflectionCapture && !FormatSettings.CompressionNone)
				{
					TextureFormatName = FName(TEXT("ETC2_RGBA"));
				}
			}
		}
	}
}

void FIOSTargetPlatform::GetAllTextureFormats(TArray<FName>& OutFormats) const 
{
	bool bFoundRemap = false;

	GetAllDefaultTextureFormats(this, OutFormats);

	for (int32 RemapIndex = 0; RemapIndex < UE_ARRAY_COUNT(FormatRemap); RemapIndex += 2)
	{
		OutFormats.Remove(FormatRemap[RemapIndex+0]);
	}

	// include the formats we want
	for (int32 RemapIndex = 0; RemapIndex < UE_ARRAY_COUNT(FormatRemap); RemapIndex += 2)
	{
		OutFormats.AddUnique(FormatRemap[RemapIndex + 1]);
	}
}

FName FIOSTargetPlatform::FinalizeVirtualTextureLayerFormat(FName Format) const
{
#if WITH_EDITOR

	// VirtualTexture Format was already run through the ordinary texture remaps to change AutoDXT to ASTC or ETC
	// this then runs again
	// currently it forces all ASTC to ETC
	// this is needed because the runtime virtual texture encoder only supports ETC
	
	// code dupe with AndroidTargetPlatform

	const static FName NameETC2_RGB(TEXT("ETC2_RGB"));
	const static FName NameETC2_RGBA(TEXT("ETC2_RGBA"));
	const static FName NameAutoETC2(TEXT("AutoETC2"));

	// Remap non-ETC variants to ETC
	const static FName ETCRemap[][2] =
	{
		{ { FName(TEXT("ASTC_RGB")) },			{ NameETC2_RGB } },
		{ { FName(TEXT("ASTC_RGBA")) },			{ NameETC2_RGBA } },
		{ { FName(TEXT("ASTC_RGBAuto")) },		{ NameAutoETC2 } },
		{ { FName(TEXT("ASTC_RGBA_HQ")) },		{ NameETC2_RGBA } },
//		{ { FName(TEXT("ASTC_RGB_HDR")) },		{ NameRGBA16F } }, // ?
		{ { FName(TEXT("ASTC_NormalAG")) },		{ NameETC2_RGB } },
		{ { FName(TEXT("ASTC_NormalRG")) },		{ NameETC2_RGB } },
	};

	for (int32 RemapIndex = 0; RemapIndex < UE_ARRAY_COUNT(ETCRemap); RemapIndex++)
	{
		if (ETCRemap[RemapIndex][0] == Format)
		{
			return ETCRemap[RemapIndex][1];
		}
	}
#endif
	return Format;
}

const UTextureLODSettings& FIOSTargetPlatform::GetTextureLODSettings() const
{
	return *TextureLODSettings;
}

#endif // WITH_ENGINE

