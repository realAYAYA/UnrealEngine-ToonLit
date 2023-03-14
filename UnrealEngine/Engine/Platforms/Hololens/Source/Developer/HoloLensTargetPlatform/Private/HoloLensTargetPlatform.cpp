// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HoloLensTargetPlatform.cpp: Implements the FHoloLensTargetPlatform class.
=============================================================================*/

#include "HoloLensTargetPlatform.h"
#include "HoloLensTargetTypes.h"
#include "HoloLensTargetDevice.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/ScopeLock.h"
#include "HttpModule.h"
#include "PlatformHttp.h"
#include "Interfaces/IHttpResponse.h"
#include "GeneralProjectSettings.h"

DEFINE_LOG_CATEGORY(LogHoloLensTargetPlatform);

TArray<FHoloLensSDKVersion> FHoloLensSDKVersion::GetSDKVersions()
{
	bool bFound18362 = false;

	static TArray<FHoloLensSDKVersion> SDKVersions;
	if (SDKVersions.Num() == 0)
	{
		HKEY hRoots[] = { HKEY_LOCAL_MACHINE , HKEY_CURRENT_USER };

		for (DWORD dwRootIdx = 0; dwRootIdx < _countof(hRoots); ++dwRootIdx)
		{
			HKEY hKey;
			if (RegOpenKeyExW(hRoots[dwRootIdx], L"Software\\Microsoft\\Windows Kits\\Installed Roots", 0, KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS)
			{
				DWORD dwIndex = 0;
				WCHAR sSubKeyName[256];
				DWORD dwSubKeyNameLen = _countof(sSubKeyName);
				FString SDKPath;

				if (ERROR_SUCCESS == RegQueryValueExW(hKey, L"KitsRoot10", 0, NULL, (LPBYTE)sSubKeyName, &dwSubKeyNameLen))
				{
					SDKPath = FString(dwSubKeyNameLen, sSubKeyName);
				}
				dwSubKeyNameLen = _countof(sSubKeyName);

				while (RegEnumKeyExW(hKey, dwIndex, sSubKeyName, &dwSubKeyNameLen, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
				{
					int version[4];
					if (swscanf_s(sSubKeyName, L"%d.%d.%d.%d", &version[0], &version[1], &version[2], &version[3]) == 4)
					{
						if (FPaths::FileExists(FPaths::Combine(SDKPath, TEXT("Include"), FString(dwSubKeyNameLen, sSubKeyName), TEXT("um"), TEXT("Windows.h"))))
						{
							// HoloLens min version is 17763 for remoting and 18317 for hand tracking
							// @todo JoeG - update this version when the official os/sdk are released
							if (version[0] >= 10 && version[2] >= 17763)
							{
								new(SDKVersions) FHoloLensSDKVersion(FString(dwSubKeyNameLen, sSubKeyName), version[0], version[1], version[2], version[3]);
							}
							if (version[2] == 18362)
							{
								bFound18362 = true;
							}
						}
					}
					dwSubKeyNameLen = _countof(sSubKeyName);
					dwIndex++;
				}
				RegCloseKey(hKey);
			}
		}

		// 18362 is required as the official minimum SDK released by Microsoft for all needed HL2 features to work
		// If it's not found, we still need it as a hidden SDK version so that we can set MinimumPlatformVersion correctly to
		// allow the package to run on lower installed firmware revisions
		if (!bFound18362)
		{
			new(SDKVersions) FHoloLensSDKVersion(FString(L"10.0.18362.0"), 10, 0, 18362, 0);
		}
	}

	return SDKVersions;
}

FHoloLensTargetPlatform::FHoloLensTargetPlatform(bool bIsClientOnly)
	: TNonDesktopTargetPlatformBase(bIsClientOnly)
{
#if WITH_ENGINE
	TextureLODSettings = nullptr; // These are registered by the device profile system.
	StaticMeshLODSettings.Initialize(this);
#endif

	DeviceDetector = IHoloLensDeviceDetector::Create();

	DeviceDetectedRegistration = DeviceDetector->OnDeviceDetected().AddRaw(this, &FHoloLensTargetPlatform::OnDeviceDetected);

#if WITH_EDITOR
	// Don't automatically try to detect a device if running editor with -server.
	if (!IsRunningDedicatedServer() && !IsRunningCommandlet())
#endif
	{
		DeviceDetector->StartDeviceDetection();
	}
}

FHoloLensTargetPlatform::~FHoloLensTargetPlatform()
{
	DeviceDetector->OnDeviceDetected().Remove(DeviceDetectedRegistration);
}

void FHoloLensTargetPlatform::GetAllDevices(TArray<ITargetDevicePtr>& OutDevices) const
{
	DeviceDetector->StartDeviceDetection();

	OutDevices.Reset();
	FScopeLock Lock(&DevicesLock);
	OutDevices = Devices;
}

ITargetDevicePtr FHoloLensTargetPlatform::GetDevice(const FTargetDeviceId& DeviceId)
{
	if (PlatformName() == DeviceId.GetPlatformName())
	{
		DeviceDetector->StartDeviceDetection();

		FScopeLock Lock(&DevicesLock);
		for (ITargetDevicePtr Device : Devices)
		{
			if (DeviceId == Device->GetId())
			{
				return Device;
			}
		}
	}


	return nullptr;
}

ITargetDevicePtr FHoloLensTargetPlatform::GetDefaultDevice() const
{
	DeviceDetector->StartDeviceDetection();

	FScopeLock Lock(&DevicesLock);
	for (ITargetDevicePtr RemoteDevice : Devices)
	{
		if (RemoteDevice->IsDefault())
		{
			return RemoteDevice;
		}
	}

	return nullptr;
}

bool FHoloLensTargetPlatform::SupportsFeature(ETargetPlatformFeatures Feature) const
{
	switch (Feature)
	{
	case ETargetPlatformFeatures::Packaging:
	case ETargetPlatformFeatures::UserCredentials:
	case ETargetPlatformFeatures::DeviceOutputLog:
		return true;
	default:
		return TTargetPlatformBase<FHoloLensPlatformProperties>::SupportsFeature(Feature);
	}
}

// Hololens previously used PCD3D_ES3_1 as a shader format/platform until we
// had to split to properly support DataDrivenPlatformInfo
static FName NAME_D3D_ES3_1_HOLOLENS(TEXT("D3D_ES3_1_HOLOLENS"));
static FName NAME_PCD3D_SM5(TEXT("PCD3D_SM5"));

void FHoloLensTargetPlatform::GetAllPossibleShaderFormats(TArray<FName>& OutFormats) const
{
	OutFormats.AddUnique(NAME_D3D_ES3_1_HOLOLENS);
	OutFormats.AddUnique(NAME_PCD3D_SM5);
}

void FHoloLensTargetPlatform::GetAllTargetedShaderFormats(TArray<FName>& OutFormats) const
{
	OutFormats.AddUnique(NAME_D3D_ES3_1_HOLOLENS);
	OutFormats.AddUnique(NAME_PCD3D_SM5);
}

void FHoloLensTargetPlatform::GetPlatformSpecificProjectAnalytics( TArray<FAnalyticsEventAttribute>& AnalyticsParamArray ) const
{
	TNonDesktopTargetPlatformBase<FHoloLensPlatformProperties>::GetPlatformSpecificProjectAnalytics(AnalyticsParamArray);

	// NB. HololensDynamicRHI reads the Windows key for the default RHI
	AppendAnalyticsEventConfigString(AnalyticsParamArray, TEXT("/Script/WindowsTargetPlatform.WindowsTargetSettings"), TEXT("DefaultGraphicsRHI"), GEngineIni);
}


#if WITH_ENGINE

void FHoloLensTargetPlatform::GetReflectionCaptureFormats(TArray<FName>& OutFormats) const
{
	OutFormats.Add(FName(TEXT("FullHDR")));
	OutFormats.Add(FName(TEXT("EncodedHDR")));
}

const static FName NameBC6H(TEXT("BC6H"));
const static FName NameBC7(TEXT("BC7"));
const static FName NameRGBA16F(TEXT("RGBA16F"));
const static FName NameDXT5(TEXT("DXT5"));
const static FName HoloLensRemap[][2] =
{
	{ NameBC6H,			NameRGBA16F		},
	{ NameBC7,			NameDXT5		},
};

void FHoloLensTargetPlatform::GetTextureFormats(const UTexture* InTexture, TArray< TArray<FName> >& OutFormats) const
{
	const bool bSupportFilteredFloat32Textures = false;
	const bool bSupportCompressedVolumeTexture = false;
	GetDefaultTextureFormatNamePerLayer(OutFormats.AddDefaulted_GetRef(), this, InTexture, bSupportCompressedVolumeTexture, 4, bSupportFilteredFloat32Textures);

	// perform any remapping away from defaults
	TArray<FName>& LayerFormats = OutFormats.Last();
	for (FName& TextureFormatName : LayerFormats)
	{
		for (int32 RemapIndex = 0; RemapIndex < UE_ARRAY_COUNT(HoloLensRemap); ++RemapIndex)
		{
			if (TextureFormatName == HoloLensRemap[RemapIndex][0])
			{
				TextureFormatName = HoloLensRemap[RemapIndex][1];
				break;
			}
		}
	}
}

void FHoloLensTargetPlatform::GetAllTextureFormats(TArray<FName>& OutFormats) const
{
	GetAllDefaultTextureFormats(this, OutFormats);

	// not supported
	for (int32 RemapIndex = 0; RemapIndex < UE_ARRAY_COUNT(HoloLensRemap); ++RemapIndex)
	{
		OutFormats.Remove(HoloLensRemap[RemapIndex][0]);
	}
}

#endif

void FHoloLensTargetPlatform::OnDeviceDetected(const FHoloLensDeviceInfo& Info)
{
	FHoloLensDevicePtr NewDevice = MakeShared<FHoloLensTargetDevice, ESPMode::ThreadSafe>(*this, Info);
	{
		FScopeLock Lock(&DevicesLock);
		Devices.Add(NewDevice);
	}
	OnDeviceDiscovered().Broadcast(NewDevice.ToSharedRef());
}

bool FHoloLensTargetPlatform::SupportsBuildTarget(EBuildTargetType BuildTarget) const
{
	return BuildTarget == EBuildTargetType::Game;
}

bool FHoloLensTargetPlatform::IsSdkInstalled(bool bProjectHasCode, FString& OutDocumentationPath) const
{
	OutDocumentationPath = TEXT("Platforms/HoloLens/GettingStarted");

	const TArray<FHoloLensSDKVersion>& SDKVersions = FHoloLensSDKVersion::GetSDKVersions();
	return SDKVersions.Num() > 0;
}

int32 FHoloLensTargetPlatform::CheckRequirements(bool bProjectHasCode, EBuildConfiguration Configuration, bool bRequiresAssetNativization, FString& OutTutorialPath, FString& OutDocumentationPath, FText& CustomizedLogMessage) const
{
	OutDocumentationPath = TEXT("Platforms/HoloLens/GettingStarted");
	FString LocalErrors;

	int32 BuildStatus = ETargetPlatformReadyStatus::Ready;
	if (!IsSdkInstalled(bProjectHasCode, OutTutorialPath))
	{
		BuildStatus |= ETargetPlatformReadyStatus::SDKNotFound;
	}
	FString PublisherIdentityName = GetDefault<UGeneralProjectSettings>()->CompanyDistinguishedName;
	if (PublisherIdentityName.IsEmpty())
	{
		LocalErrors += TEXT("Missing Company Distinguished Name (See Project Settings).");
		BuildStatus |= ETargetPlatformReadyStatus::SigningKeyNotFound;
	}
	else
	{
		if (PublisherIdentityName.Contains(TEXT("CN=")) && PublisherIdentityName.Len() == 3)
		{
			LocalErrors += TEXT(" Malformed Company Distinguished Name (See Project Settings).");
			BuildStatus |= ETargetPlatformReadyStatus::SigningKeyNotFound;
		}
	}
	FString ProjectName = GetDefault<UGeneralProjectSettings>()->ProjectName;
	if (ProjectName.IsEmpty())
	{
		LocalErrors += TEXT(" Missing Project Name (See Project Settings).");
		BuildStatus |= ETargetPlatformReadyStatus::SigningKeyNotFound;
	}

	// Set the path if missing any of the bits needed for signing
	if (BuildStatus & ETargetPlatformReadyStatus::SigningKeyNotFound)
	{
		OutDocumentationPath = TEXT("Platforms/HoloLens/Signing");
	}

	if (BuildStatus != ETargetPlatformReadyStatus::Ready)
	{
		UE_LOG(LogHoloLensTargetPlatform, Warning, TEXT("FHoloLensTargetPlatform::CheckRequirements found these problems: %s"), *LocalErrors);
	}

	return BuildStatus;
}

bool FHoloLensTargetPlatform::AddDevice(const FString& DeviceId, const FString& DeviceUserFriendlyName, const FString& Username, const FString& Password, bool bDefault)
{
	DeviceDetector->TryAddDevice(DeviceId, DeviceUserFriendlyName, Username, Password);

	return true;
}
