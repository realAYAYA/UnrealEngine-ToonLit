// Copyright Epic Games, Inc. All Rights Reserved.

#include "RHI.h"
#include "Modules/ModuleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/MessageDialog.h"
#include "Containers/StaticArray.h"

#if WINDOWS_USE_FEATURE_DYNAMIC_RHI

#include "Windows/WindowsPlatformApplicationMisc.h"

bool GDynamicRHIFailedToInitializeAdvancedPlatform = false;

static const TCHAR* GLoadedRHIModuleName;

enum class EWindowsRHI
{
	D3D11,
	D3D12,
	Vulkan,
	OpenGL,
	count
};
static constexpr int32 EWindowsRHICount = static_cast<int32>(EWindowsRHI::count);

static TAutoConsoleVariable<bool> CVarIgnorePerformanceModeCheck(
	TEXT("r.IgnorePerformanceModeCheck"),
	false,
	TEXT("Ignore performance mode check"),
	ECVF_RenderThreadSafe
	);

static const TCHAR* ModuleNameFromWindowsRHI(EWindowsRHI InWindowsRHI)
{
	switch (InWindowsRHI)
	{
	default: check(false);
	case EWindowsRHI::D3D11:  return TEXT("D3D11RHI");
	case EWindowsRHI::D3D12:  return TEXT("D3D12RHI");
	case EWindowsRHI::Vulkan: return TEXT("VulkanRHI");
	case EWindowsRHI::OpenGL: return TEXT("OpenGLDrv");
	}
}

static FString GetRHINameFromWindowsRHI(EWindowsRHI InWindowsRHI, ERHIFeatureLevel::Type InFeatureLevel)
{
	switch (InWindowsRHI)
	{
	default: check(false);
	case EWindowsRHI::D3D11:  return TEXT("DirectX 11");
	case EWindowsRHI::D3D12:
	{
		FString FeatureLevelName;
		GetFeatureLevelName(InFeatureLevel, FeatureLevelName);
		return FString::Printf(TEXT("DirectX 12 (%s)"), *FeatureLevelName);
	}
	case EWindowsRHI::Vulkan: return TEXT("Vulkan");
	case EWindowsRHI::OpenGL: return TEXT("OpenGL");
	}
}

static ERHIFeatureLevel::Type GetDefaultFeatureLevelForRHI(EWindowsRHI InWindowsRHI)
{
	switch (InWindowsRHI)
	{
	case EWindowsRHI::D3D11:  return ERHIFeatureLevel::SM5;
	case EWindowsRHI::D3D12:  return ERHIFeatureLevel::SM5;
	case EWindowsRHI::Vulkan: return ERHIFeatureLevel::SM5;
	case EWindowsRHI::OpenGL: return ERHIFeatureLevel::ES3_1;
	default: check(false);    return ERHIFeatureLevel::SM5;
	}
}

struct FWindowsRHIConfig
{
	TArray<EShaderPlatform>        ShaderPlatforms;
	TArray<ERHIFeatureLevel::Type> FeatureLevels;
};

struct FParsedWindowsDynamicRHIConfig
{
	TOptional<EWindowsRHI> DefaultRHI{};

	TStaticArray<FWindowsRHIConfig, EWindowsRHICount> RHIConfigs;

	bool IsEmpty() const;

	bool IsRHISupported(EWindowsRHI InWindowsRHI) const;
	TOptional<ERHIFeatureLevel::Type> GetHighestSupportedFeatureLevel(EWindowsRHI InWindowsRHI) const;
	TOptional<ERHIFeatureLevel::Type> GetNextHighestTargetedFeatureLevel(EWindowsRHI InWindowsRHI, ERHIFeatureLevel::Type InFeatureLevel) const;

	bool IsFeatureLevelTargeted(EWindowsRHI InWindowsRHI, ERHIFeatureLevel::Type InFeatureLevel) const;

	void MergeConfig(EWindowsRHI InWindowsRHI, const FWindowsRHIConfig& Other);
};

bool FParsedWindowsDynamicRHIConfig::IsEmpty() const
{
	for (const FWindowsRHIConfig& Config : RHIConfigs)
	{
		if (!Config.ShaderPlatforms.IsEmpty())
		{
			return false;
		}
	}

	return true;
}

bool FParsedWindowsDynamicRHIConfig::IsRHISupported(EWindowsRHI InWindowsRHI) const
{
	// If we don't require cooked data, then we should be able to support this RHI at any feature level.
	if (!FPlatformProperties::RequiresCookedData())
	{
		return true;
	}

	return !RHIConfigs[(int32)InWindowsRHI].ShaderPlatforms.IsEmpty();
}

TOptional<ERHIFeatureLevel::Type> FParsedWindowsDynamicRHIConfig::GetHighestSupportedFeatureLevel(EWindowsRHI InWindowsRHI) const
{
	const TArray<ERHIFeatureLevel::Type>& FeatureLevels = RHIConfigs[(int32)InWindowsRHI].FeatureLevels;
	if (FeatureLevels.Num() == 0)
	{
		return TOptional<ERHIFeatureLevel::Type>();
	}

	ERHIFeatureLevel::Type MaxFeatureLevel = (ERHIFeatureLevel::Type)0;
	for (ERHIFeatureLevel::Type SupportedFeatureLevel : FeatureLevels)
	{
		MaxFeatureLevel = std::max(MaxFeatureLevel, SupportedFeatureLevel);
	}
	return MaxFeatureLevel;
}

TOptional<ERHIFeatureLevel::Type> FParsedWindowsDynamicRHIConfig::GetNextHighestTargetedFeatureLevel(EWindowsRHI InWindowsRHI, ERHIFeatureLevel::Type InFeatureLevel) const
{
	TArray<ERHIFeatureLevel::Type> LowerFeatureLevels(RHIConfigs[(int32)InWindowsRHI].FeatureLevels);
	LowerFeatureLevels.RemoveAll([InFeatureLevel](ERHIFeatureLevel::Type OtherFeatureLevel) { return OtherFeatureLevel >= InFeatureLevel; });

	if (LowerFeatureLevels.Num())
	{
		ERHIFeatureLevel::Type MaxFeatureLevel = (ERHIFeatureLevel::Type)0;
		for (ERHIFeatureLevel::Type SupportedFeatureLevel : LowerFeatureLevels)
		{
			MaxFeatureLevel = std::max(MaxFeatureLevel, SupportedFeatureLevel);
		}
		return MaxFeatureLevel;
	}

	return TOptional<ERHIFeatureLevel::Type>();
}

bool FParsedWindowsDynamicRHIConfig::IsFeatureLevelTargeted(EWindowsRHI InWindowsRHI, ERHIFeatureLevel::Type InFeatureLevel) const
{
	for (ERHIFeatureLevel::Type SupportedFeatureLevel : RHIConfigs[(int32)InWindowsRHI].FeatureLevels)
	{
		if (SupportedFeatureLevel == InFeatureLevel)
		{
			return true;
		}
	}
	return false;
}

void FParsedWindowsDynamicRHIConfig::MergeConfig(EWindowsRHI InWindowsRHI, const FWindowsRHIConfig& Other)
{
	FWindowsRHIConfig& ExistingConfig = RHIConfigs[(int32)InWindowsRHI];

	for (EShaderPlatform ShaderPlatform : Other.ShaderPlatforms)
	{
		ExistingConfig.ShaderPlatforms.AddUnique(ShaderPlatform);
	}

	for (ERHIFeatureLevel::Type FeatureLevel : Other.FeatureLevels)
	{
		ExistingConfig.FeatureLevels.AddUnique(FeatureLevel);
	}
}

TOptional<EWindowsRHI> ParseDefaultWindowsRHI()
{
	TOptional<EWindowsRHI> DefaultRHI{};

	FString DefaultGraphicsRHI;
	if (GConfig->GetString(TEXT("/Script/WindowsTargetPlatform.WindowsTargetSettings"), TEXT("DefaultGraphicsRHI"), DefaultGraphicsRHI, GEngineIni))
	{
		const FString NAME_DX11(TEXT("DefaultGraphicsRHI_DX11"));
		const FString NAME_DX12(TEXT("DefaultGraphicsRHI_DX12"));
		const FString NAME_VULKAN(TEXT("DefaultGraphicsRHI_Vulkan"));

		DefaultRHI = EWindowsRHI::D3D11;

		if (DefaultGraphicsRHI == NAME_DX11)
		{
			DefaultRHI = EWindowsRHI::D3D11;
		}
		else if (DefaultGraphicsRHI == NAME_DX12)
		{
			DefaultRHI = EWindowsRHI::D3D12;
		}
		else if (DefaultGraphicsRHI == NAME_VULKAN)
		{
			DefaultRHI = EWindowsRHI::Vulkan;
		}
		else if (DefaultGraphicsRHI != TEXT("DefaultGraphicsRHI_Default"))
		{
			UE_LOG(LogRHI, Error, TEXT("Unrecognized setting '%s' for DefaultGraphicsRHI"), *DefaultGraphicsRHI);
		}
	}

	return DefaultRHI;
}

static TArray<EShaderPlatform> ParseShaderPlatformsConfig(const TCHAR* InSettingName)
{
	TArray<FString> TargetedShaderFormats;
	GConfig->GetArray(TEXT("/Script/WindowsTargetPlatform.WindowsTargetSettings"), InSettingName, TargetedShaderFormats, GEngineIni);

	TArray<EShaderPlatform> ShaderPlatforms;
	ShaderPlatforms.Reserve(TargetedShaderFormats.Num());

	for (const FString& ShaderFormat : TargetedShaderFormats)
	{
		ShaderPlatforms.AddUnique(ShaderFormatToLegacyShaderPlatform(FName(*ShaderFormat)));
	}

	return ShaderPlatforms;
}

static TArray<ERHIFeatureLevel::Type> FeatureLevelsFromShaderPlatforms(const TArray<EShaderPlatform>& InShaderPlatforms)
{
	TArray<ERHIFeatureLevel::Type> FeatureLevels;
	FeatureLevels.Reserve(InShaderPlatforms.Num());

	for (EShaderPlatform ShaderPlatform : InShaderPlatforms)
	{
		FeatureLevels.AddUnique(FDataDrivenShaderPlatformInfo::GetMaxFeatureLevel(ShaderPlatform));
	}

	return FeatureLevels;
}

static FWindowsRHIConfig ParseWindowsRHIConfig(const TCHAR* ShaderFormatsSettingName)
{
	FWindowsRHIConfig Config;

	Config.ShaderPlatforms = ParseShaderPlatformsConfig(ShaderFormatsSettingName);
	Config.FeatureLevels = FeatureLevelsFromShaderPlatforms(Config.ShaderPlatforms);

	return Config;
}

FParsedWindowsDynamicRHIConfig ParseWindowsDynamicRHIConfig()
{
	FParsedWindowsDynamicRHIConfig Config;

	Config.DefaultRHI = ParseDefaultWindowsRHI();

	Config.RHIConfigs[(int32)EWindowsRHI::D3D11]  = ParseWindowsRHIConfig(TEXT("D3D11TargetedShaderFormats"));
	Config.RHIConfigs[(int32)EWindowsRHI::D3D12]  = ParseWindowsRHIConfig(TEXT("D3D12TargetedShaderFormats"));
	Config.RHIConfigs[(int32)EWindowsRHI::Vulkan] = ParseWindowsRHIConfig(TEXT("VulkanTargetedShaderFormats"));

	// Only add OpenGL support to non-client programs.
	if (!FPlatformProperties::RequiresCookedData())
	{
		Config.RHIConfigs[(int32)EWindowsRHI::OpenGL].ShaderPlatforms.Add(SP_OPENGL_PCES3_1);
		Config.RHIConfigs[(int32)EWindowsRHI::OpenGL].FeatureLevels.Add(ERHIFeatureLevel::ES3_1);
	}

	if (FWindowsRHIConfig DeprecatedConfig = ParseWindowsRHIConfig(TEXT("TargetedRHIs")); !DeprecatedConfig.ShaderPlatforms.IsEmpty())
	{
		// Since we don't have context here, we have to add this old config setting to every potential RHI.
		Config.MergeConfig(EWindowsRHI::D3D11,  DeprecatedConfig);
		Config.MergeConfig(EWindowsRHI::D3D12,  DeprecatedConfig);
		Config.MergeConfig(EWindowsRHI::Vulkan, DeprecatedConfig);
		Config.MergeConfig(EWindowsRHI::OpenGL, DeprecatedConfig);
	}

	return Config;
}

// Default to Performance Mode on low-end machines
static bool DefaultFeatureLevelES31()
{
	static TOptional<bool> ForceES31;
	if (ForceES31.IsSet())
	{
		return ForceES31.GetValue();
	}

	// Force Performance mode for machines with too few cores including hyperthreads
	int MinCoreCount = 0;
	if (GConfig->GetInt(TEXT("PerformanceMode"), TEXT("MinCoreCount"), MinCoreCount, GEngineIni) && FPlatformMisc::NumberOfCoresIncludingHyperthreads() < MinCoreCount)
	{
		ForceES31 = true;
		return true;
	}

	FWindowsPlatformApplicationMisc::FGPUInfo BestGPUInfo = FWindowsPlatformApplicationMisc::GetBestGPUInfo();

	FString MinMemorySizeBucketString;
	FString MinIntegratedMemorySizeBucketString;
	if (GConfig->GetString(TEXT("PerformanceMode"), TEXT("MinMemorySizeBucket"), MinMemorySizeBucketString, GEngineIni) && GConfig->GetString(TEXT("PerformanceMode"), TEXT("MinIntegratedMemorySizeBucket"), MinIntegratedMemorySizeBucketString, GEngineIni))
	{
		for (int EnumIndex = int(EPlatformMemorySizeBucket::Largest); EnumIndex <= int(EPlatformMemorySizeBucket::Tiniest); EnumIndex++)
		{
			const TCHAR* BucketString = LexToString(EPlatformMemorySizeBucket(EnumIndex));
			// Force Performance mode for machines with too little memory
			if (MinMemorySizeBucketString == BucketString)
			{
				if (FPlatformMemory::GetMemorySizeBucket() >= EPlatformMemorySizeBucket(EnumIndex))
				{
					ForceES31 = true;
					return true;
				}
			}

			// Force Performance mode for machines with too little memory when shared with the GPU
			if (MinIntegratedMemorySizeBucketString == BucketString)
			{
				const int MIN_GPU_MEMORY = 512 * 1024 * 1024;
				if (FPlatformMemory::GetMemorySizeBucket() >= EPlatformMemorySizeBucket(EnumIndex) && BestGPUInfo.DedicatedVideoMemory < MIN_GPU_MEMORY)
				{
					ForceES31 = true;

					return true;
				}
			}
		}
	}

	TArray<FString> DeviceDefaultRHIList;
	GConfig->GetArray(TEXT("Devices"), TEXT("DeviceDefaultRHIList"), DeviceDefaultRHIList, GHardwareIni);

	FString GPUBrand = FPlatformMisc::GetPrimaryGPUBrand();
	for (const FString& DeviceDefaultRHIString : DeviceDefaultRHIList)
	{
		const TCHAR* Line = *DeviceDefaultRHIString;

		ensure(Line[0] == TCHAR('('));

		FString RHIName;
		FParse::Value(Line+1, TEXT("RHI="), RHIName);

		FString DeviceName;
		FParse::Value(Line+1, TEXT("DeviceName="), DeviceName);

		if (RHIName.Compare("D3D11_ES31", ESearchCase::IgnoreCase) == 0 && GPUBrand.Compare(DeviceName, ESearchCase::IgnoreCase) == 0)
		{
			ForceES31 = true;

			return true;
		}

		FString VendorId;
		FParse::Value(Line + 1, TEXT("VendorId="), VendorId);
		uint32 VendorIdInt = FParse::HexNumber(*VendorId);

		FString DeviceId;
		FParse::Value(Line + 1, TEXT("DeviceId="), DeviceId);
		uint32 DeviceIdInt = FParse::HexNumber(*DeviceId);

		if (BestGPUInfo.VendorId && BestGPUInfo.DeviceId &&
			BestGPUInfo.VendorId == VendorIdInt && BestGPUInfo.DeviceId == DeviceIdInt &&
			RHIName.Compare("D3D11_ES31", ESearchCase::IgnoreCase) == 0)
		{
			ForceES31 = true;

			return true;
		}
	}

	ForceES31 = false;
	return false;
}

static bool PreferFeatureLevelES31()
{
	if (!GIsEditor)
	{
		bool bPreferFeatureLevelES31 = false;
		bool bFoundPreference = GConfig->GetBool(TEXT("D3DRHIPreference"), TEXT("bPreferFeatureLevelES31"), bPreferFeatureLevelES31, GGameUserSettingsIni);

		// Force low-spec users into performance mode but respect their choice once they have set a preference
		bool bDefaultES31 = false;
		if (!bFoundPreference && !CVarIgnorePerformanceModeCheck.GetValueOnAnyThread())
		{
			bDefaultES31 = DefaultFeatureLevelES31();
		}

		if (bPreferFeatureLevelES31 || bDefaultES31)
		{
			if (!bFoundPreference)
			{
				GConfig->SetBool(TEXT("D3DRHIPreference"), TEXT("bPreferFeatureLevelES31"), true, GGameUserSettingsIni);
			}
			return true;
		}
	}
	return false;
}

static bool IsES31D3DOnly()
{
	bool bES31DXOnly = false;
#if !WITH_EDITOR
	if (!GIsEditor)
	{
		GConfig->GetBool(TEXT("PerformanceMode"), TEXT("bES31DXOnly"), bES31DXOnly, GEngineIni);
	}
#endif
	return bES31DXOnly;
}

static bool AllowD3D12FeatureLevelES31(const FParsedWindowsDynamicRHIConfig& Config)
{
	if (!GIsEditor)
	{
		return Config.IsFeatureLevelTargeted(EWindowsRHI::D3D12, ERHIFeatureLevel::ES3_1);
	}
	return true;
}

// Choose the default from DefaultGraphicsRHI or TargetedRHIs. DefaultGraphicsRHI has precedence.
static EWindowsRHI ChooseDefaultRHI(const FParsedWindowsDynamicRHIConfig& Config)
{
	// Default graphics RHI is the main project setting that governs the choice, so it takes the priority
	if (TOptional<EWindowsRHI> ConfigDefault = Config.DefaultRHI)
	{
		return ConfigDefault.GetValue();
	}

	const EWindowsRHI DefaultRHIOrder[] =
	{
		EWindowsRHI::D3D12,
		EWindowsRHI::D3D11,
		EWindowsRHI::Vulkan,
	};

	// Find the first RHI with configured support based on the order above
	for (EWindowsRHI DefaultRHI : DefaultRHIOrder)
	{
		if (TOptional<ERHIFeatureLevel::Type> HighestFL = Config.GetHighestSupportedFeatureLevel(DefaultRHI))
		{
			return DefaultRHI;
		}
	}

	return EWindowsRHI::D3D11;
}

static TOptional<EWindowsRHI> ChoosePreferredRHI(EWindowsRHI InDefaultRHI)
{
	TOptional<EWindowsRHI> RHIPreference{};

	// If we are in game, there is a separate setting that can make it prefer D3D12 over D3D11 (but not over other RHIs).
	if (!GIsEditor && (InDefaultRHI == EWindowsRHI::D3D11 || InDefaultRHI == EWindowsRHI::D3D12))
	{
		bool bUseD3D12InGame = false;
		if (GConfig->GetBool(TEXT("D3DRHIPreference"), TEXT("bUseD3D12InGame"), bUseD3D12InGame, GGameUserSettingsIni) && bUseD3D12InGame)
		{
			RHIPreference = EWindowsRHI::D3D12;
		}
	}

	return RHIPreference;
}

static TOptional<EWindowsRHI> ChooseForcedRHI(const FParsedWindowsDynamicRHIConfig& Config)
{
	TOptional<EWindowsRHI> ForcedRHI = {};

	// Command line overrides
	uint32 Sum = 0;
	if (FParse::Param(FCommandLine::Get(), TEXT("vulkan")))
	{
		ForcedRHI = EWindowsRHI::Vulkan;
		Sum++;
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("opengl")))
	{
		ForcedRHI = EWindowsRHI::OpenGL;
		Sum++;
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("d3d11")) || FParse::Param(FCommandLine::Get(), TEXT("dx11")))
	{
		ForcedRHI = EWindowsRHI::D3D11;
		Sum++;
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("d3d12")) || FParse::Param(FCommandLine::Get(), TEXT("dx12")))
	{
		ForcedRHI = EWindowsRHI::D3D12;
		Sum++;
	}

	if (Sum > 1)
	{
		FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("WindowsDynamicRHI", "RHIOptionsError", "-d3d12/dx12, -d3d11/dx11, -vulkan, and -opengl are mutually exclusive options, but more than one was specified on the command-line."));
		UE_LOG(LogRHI, Fatal, TEXT("-d3d12, -d3d11, -vulkan, and -opengl are mutually exclusive options, but more than one was specified on the command-line."));
	}

#if	!WITH_EDITOR && UE_BUILD_SHIPPING
	// In Shipping builds we can limit ES31 on Windows to only DX11. All RHIs are allowed by default.

	// FeatureLevelES31 is also a command line override, so it will determine the underlying RHI unless one is specified
	if (IsES31D3DOnly() && (FParse::Param(FCommandLine::Get(), TEXT("FeatureLevelES31")) || FParse::Param(FCommandLine::Get(), TEXT("FeatureLevelES3_1"))))
	{
		if (ForcedRHI == EWindowsRHI::OpenGL)
		{
			FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("WindowsDynamicRHI", "RHIPerformanceOpenGL", "OpenGL is not supported for Performance Mode."));
			UE_LOG(LogRHI, Fatal, TEXT("OpenGL is not supported for Performance Mode."));
		}
		else if (ForcedRHI == EWindowsRHI::Vulkan)
		{
			FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("WindowsDynamicRHI", "RHIPerformanceVulkan", "Vulkan is not supported for Performance Mode."));
			UE_LOG(LogRHI, Fatal, TEXT("Vulkan is not supported for Performance Mode."));
		}
		else if (ForcedRHI == EWindowsRHI::D3D12)
		{
			if (!AllowD3D12FeatureLevelES31(Config))
			{
				FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("WindowsDynamicRHI", "RHIPerformanceDX12", "DirectX 12 is not supported for Performance Mode."));
				UE_LOG(LogRHI, Fatal, TEXT("DirectX 12 is not supported for Performance Mode."));
			}
		}
		else
		{
			ForcedRHI = EWindowsRHI::D3D11;
		}
	}
#endif //!WITH_EDITOR && UE_BUILD_SHIPPING

	return ForcedRHI;
}

static TOptional<ERHIFeatureLevel::Type> GetForcedFeatureLevel()
{
	TOptional<ERHIFeatureLevel::Type> ForcedFeatureLevel{};

	if (FParse::Param(FCommandLine::Get(), TEXT("es31")) || FParse::Param(FCommandLine::Get(), TEXT("FeatureLevelES31")) || FParse::Param(FCommandLine::Get(), TEXT("FeatureLevelES3_1")))
	{
		ForcedFeatureLevel = ERHIFeatureLevel::ES3_1;
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("sm5")))
	{
		ForcedFeatureLevel = ERHIFeatureLevel::SM5;
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("sm6")))
	{
		ForcedFeatureLevel = ERHIFeatureLevel::SM6;
	}

	return ForcedFeatureLevel;
}

static ERHIFeatureLevel::Type ChooseFeatureLevel(EWindowsRHI ChosenRHI, const TOptional<EWindowsRHI> ForcedRHI, const TOptional<ERHIFeatureLevel::Type> ForcedFeatureLevel, const FParsedWindowsDynamicRHIConfig& Config)
{
	if (ForcedFeatureLevel)
	{
		// Allow the forced feature level if we're in a position to compile its shaders
		if (!FPlatformProperties::RequiresCookedData())
		{
			return ForcedFeatureLevel.GetValue();
		}

		// Make sure the feature level is supported by the runtime, otherwise fall back to the default
		if (Config.IsFeatureLevelTargeted(ChosenRHI, ForcedFeatureLevel.GetValue()))
		{
			return ForcedFeatureLevel.GetValue();
		}
	}

	TOptional<ERHIFeatureLevel::Type> FeatureLevel{};

	if ((ChosenRHI == EWindowsRHI::D3D11 || ChosenRHI == EWindowsRHI::D3D12) && Config.IsFeatureLevelTargeted(ChosenRHI, ERHIFeatureLevel::ES3_1) && PreferFeatureLevelES31())
	{
		FeatureLevel = TOptional<ERHIFeatureLevel::Type>(ERHIFeatureLevel::ES3_1);
	}
	else
	{
		FeatureLevel = Config.GetHighestSupportedFeatureLevel(ChosenRHI);

		// If we were forced to a specific RHI while not forced to a specific feature level and the project isn't configured for it, find the default Feature Level for that RHI
		if (!FeatureLevel)
		{
			FeatureLevel = GetDefaultFeatureLevelForRHI(ChosenRHI);

			if (FPlatformProperties::RequiresCookedData())
			{
				const TCHAR* RHIName = ModuleNameFromWindowsRHI(ChosenRHI);
				FMessageDialog::Open(EAppMsgType::Ok, FText::Format(NSLOCTEXT("WindowsDynamicRHI", "MissingTargetedShaderFormats", "Unable to launch with RHI '{0}' since the project is not configured to support it."), FText::AsCultureInvariant(RHIName)));
				UE_LOG(LogRHI, Fatal, TEXT("Unable to launch with RHI '%s' since the project is not configured to support it."), RHIName);
			}
			else
			{
				const TCHAR* RHIName = ModuleNameFromWindowsRHI(ChosenRHI);
				const FString FeatureLevelName = LexToString(FeatureLevel.GetValue());
				UE_LOG(LogRHI, Warning, TEXT("User requested RHI '%s' but that is not supported by this project's data. Defaulting to Feature Level '%s'."), RHIName, *FeatureLevelName);
			}
		}
	}

	// If the user wanted to force a feature level and we couldn't set it, log out why and what we're actually running with
	if (ForcedFeatureLevel)
	{
		const FString ForcedName = LexToString(ForcedFeatureLevel.GetValue());
		const FString UsedName = LexToString(FeatureLevel.GetValue());
		UE_LOG(LogRHI, Warning, TEXT("User requested Feature Level '%s' but that is not supported by this project. Falling back to Feature Level '%s'."), *ForcedName, *UsedName);
	}

	return FeatureLevel.GetValue();
}

static bool HandleUnsupportedFeatureLevel(EWindowsRHI& WindowsRHI, ERHIFeatureLevel::Type& FeatureLevel, TOptional<ERHIFeatureLevel::Type> ForcedFeatureLevel, const FParsedWindowsDynamicRHIConfig& Config)
{
	if (ForcedFeatureLevel)
	{
		if (WindowsRHI == EWindowsRHI::D3D12 && FeatureLevel == ERHIFeatureLevel::SM6)
		{
			FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("WindowsDynamicRHI", "RequiredDX12SM6", "DX12 SM6 is not supported on your system. Try running without the -sm6 command line argument."));
			FPlatformMisc::RequestExit(1);
		}

		return false;
	}

	if (FeatureLevel == ERHIFeatureLevel::SM6)
	{
		GDynamicRHIFailedToInitializeAdvancedPlatform = true;
	}

	if (TOptional<ERHIFeatureLevel::Type> FallbackFeatureLevel = Config.GetNextHighestTargetedFeatureLevel(WindowsRHI, FeatureLevel))
	{
		UE_LOG(LogRHI, Log, TEXT("RHI %s with Feature Level %s not supported, falling back to Feature Level %s"), ModuleNameFromWindowsRHI(WindowsRHI), *LexToString(FeatureLevel), *LexToString(FallbackFeatureLevel.GetValue()));

		FeatureLevel = FallbackFeatureLevel.GetValue();
		return true;
	}

	return false;
}

static bool HandleUnsupportedRHI(EWindowsRHI& WindowsRHI, ERHIFeatureLevel::Type& FeatureLevel, TOptional<EWindowsRHI> ForcedRHI, const FParsedWindowsDynamicRHIConfig& Config)
{
	if (ForcedRHI)
	{
		if (ForcedRHI == EWindowsRHI::D3D12)
		{
			FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("WindowsDynamicRHI", "RequiredDX12", "DX12 is not supported on your system. Try running without the -dx12 or -d3d12 command line argument."));
			FPlatformMisc::RequestExit(1);
		}
	}

	if (WindowsRHI == EWindowsRHI::D3D12)
	{
		if (TOptional<ERHIFeatureLevel::Type> D3D11FeatureLevel = Config.GetHighestSupportedFeatureLevel(EWindowsRHI::D3D11))
		{
			UE_LOG(LogRHI, Log, TEXT("D3D12 is not supported, falling back to D3D11 with Feature Level %s"), *LexToString(D3D11FeatureLevel.GetValue()));

			WindowsRHI = EWindowsRHI::D3D11;
			FeatureLevel = D3D11FeatureLevel.GetValue();
			return true;
		}

		FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("WindowsDynamicRHI", "RequiredDX12", "DX12 is not supported on your system. Try running without the -dx12 or -d3d12 command line argument."));
		FPlatformMisc::RequestExit(1);
	}

	if (WindowsRHI == EWindowsRHI::D3D11)
	{
		FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("WindowsDynamicRHI", "RequiredDX11Feature_11_SM5", "A D3D11-compatible GPU (Feature Level 11.0, Shader Model 5.0) is required to run the engine."));
		FPlatformMisc::RequestExit(1);
	}

	if (WindowsRHI == EWindowsRHI::Vulkan)
	{
		FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("WindowsDynamicRHI", "RequiredVulkan", "Vulkan Driver is required to run the engine."));
		FPlatformMisc::RequestExit(1);
	}

	if (WindowsRHI == EWindowsRHI::OpenGL)
	{
		FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("WindowsDynamicRHI", "RequiredOpenGL", "OpenGL 4.3 is required to run the engine."));
		FPlatformMisc::RequestExit(1);
	}

	return false;
}

static IDynamicRHIModule* LoadDynamicRHIModule(ERHIFeatureLevel::Type& DesiredFeatureLevel, const TCHAR*& LoadedRHIModuleName)
{
	// Make sure the DDSPI is initialized before we try and read from it
	FGenericDataDrivenShaderPlatformInfo::Initialize();

	bool bUseGPUCrashDebugging = false;
	if (!GIsEditor && GConfig->GetBool(TEXT("D3DRHIPreference"), TEXT("bUseGPUCrashDebugging"), bUseGPUCrashDebugging, GGameUserSettingsIni))
	{
		auto GPUCrashDebuggingCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.GPUCrashDebugging"));
		*GPUCrashDebuggingCVar = bUseGPUCrashDebugging;
	}

	const FParsedWindowsDynamicRHIConfig Config = ParseWindowsDynamicRHIConfig();

	// RHI is chosen by the project settings (first DefaultGraphicsRHI, then TargetedRHIs are consulted, "Default" maps to D3D12). 
	// After this, a separate game-only setting (does not affect editor) bPreferD3D12InGame selects between D3D12 or D3D11 (but will not have any effect if Vulkan or OpenGL are chosen).
	// Commandline switches apply after this and can force an arbitrary RHIs. If RHI isn't supported, the game will refuse to start.

	EWindowsRHI DefaultRHI = ChooseDefaultRHI(Config);
	const TOptional<EWindowsRHI> PreferredRHI = ChoosePreferredRHI(DefaultRHI);
	const TOptional<EWindowsRHI> ForcedRHI = ChooseForcedRHI(Config);

	EWindowsRHI ChosenRHI = DefaultRHI;
	if (ForcedRHI)
	{
		ChosenRHI = ForcedRHI.GetValue();
	}
	else if (PreferredRHI)
	{
		ChosenRHI = PreferredRHI.GetValue();
	}

	const TOptional<ERHIFeatureLevel::Type> ForcedFeatureLevel = GetForcedFeatureLevel();

	DesiredFeatureLevel = ChooseFeatureLevel(ChosenRHI, ForcedRHI, ForcedFeatureLevel, Config);

	// Load the dynamic RHI module.

	bool bTryWithNewConfig = false;
	do
	{
		const FString RHIName = GetRHINameFromWindowsRHI(ChosenRHI, DesiredFeatureLevel);
		FApp::SetGraphicsRHI(RHIName);

		const TCHAR* ModuleName = ModuleNameFromWindowsRHI(ChosenRHI);
		IDynamicRHIModule* DynamicRHIModule = FModuleManager::LoadModulePtr<IDynamicRHIModule>(ModuleName);

		if (DynamicRHIModule && DynamicRHIModule->IsSupported(DesiredFeatureLevel))
		{
			LoadedRHIModuleName = ModuleName;
			return DynamicRHIModule;
		}

		bTryWithNewConfig = HandleUnsupportedFeatureLevel(ChosenRHI, DesiredFeatureLevel, ForcedFeatureLevel, Config);

		if (!bTryWithNewConfig)
		{
			bTryWithNewConfig = HandleUnsupportedRHI(ChosenRHI, DesiredFeatureLevel, ForcedRHI, Config);
		}

	} while (bTryWithNewConfig);

	return nullptr;
}

FDynamicRHI* PlatformCreateDynamicRHI()
{
	FDynamicRHI* DynamicRHI = nullptr;

	ERHIFeatureLevel::Type RequestedFeatureLevel;
	const TCHAR* LoadedRHIModuleName;
	IDynamicRHIModule* DynamicRHIModule = LoadDynamicRHIModule(RequestedFeatureLevel, LoadedRHIModuleName);

	if (DynamicRHIModule)
	{
		// Create the dynamic RHI.
		DynamicRHI = DynamicRHIModule->CreateRHI(RequestedFeatureLevel);
		GLoadedRHIModuleName = LoadedRHIModuleName;
	}

	return DynamicRHI;
}

const TCHAR* GetSelectedDynamicRHIModuleName(bool bCleanup)
{
	check(FApp::CanEverRender());

	if (GDynamicRHI)
	{
		check(!!GLoadedRHIModuleName);
		return GMaxRHIFeatureLevel == ERHIFeatureLevel::ES3_1 ? TEXT("ES31") : GLoadedRHIModuleName;
	}
	else
	{
		ERHIFeatureLevel::Type DesiredFeatureLevel;
		const TCHAR* RHIModuleName;
		IDynamicRHIModule* DynamicRHIModule = LoadDynamicRHIModule(DesiredFeatureLevel, RHIModuleName);
		check(DynamicRHIModule);
		check(RHIModuleName);
		if (bCleanup)
		{
			FModuleManager::Get().UnloadModule(RHIModuleName);
		}

		return DesiredFeatureLevel == ERHIFeatureLevel::ES3_1 ? TEXT("ES31") : RHIModuleName;
	}
}

#endif //WINDOWS_USE_FEATURE_DYNAMIC_RHI
