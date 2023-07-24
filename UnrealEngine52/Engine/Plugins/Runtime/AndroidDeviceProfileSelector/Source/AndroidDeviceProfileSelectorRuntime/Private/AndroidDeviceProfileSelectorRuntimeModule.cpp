// Copyright Epic Games, Inc. All Rights Reserved.AndroidDeviceProfileSelector

#include "AndroidDeviceProfileSelectorRuntimeModule.h"
#include "AndroidDeviceProfileSelectorRuntime.h"
#include "Templates/Casts.h"
#include "Internationalization/Regex.h"
#include "Modules/ModuleManager.h"
#include "AndroidDeviceProfileSelectorRuntime.h"
#include "AndroidDeviceProfileSelector.h"
#include "AndroidJavaSurfaceViewDevices.h"
#include "IHeadMountedDisplayModule.h"

IMPLEMENT_MODULE(FAndroidDeviceProfileSelectorRuntimeModule, AndroidDeviceProfileSelectorRuntime);

void FAndroidDeviceProfileSelectorRuntimeModule::StartupModule()
{
}

void FAndroidDeviceProfileSelectorRuntimeModule::ShutdownModule()
{
}

// Build the selector params for the current device.
static const TMap<FName,FString>& GetDeviceSelectorParams()
{
	static bool bInitialized = false;
	static TMap<FName, FString> AndroidParams;
	if(!bInitialized)
	{
		bInitialized = true;
		auto GetParam = [](const FString& DefaultParam, const TCHAR* ConfRuleVarName)
		{
#if PLATFORM_ANDROID
			if (FString* ConfRuleVarValue = FAndroidMisc::GetConfigRulesVariable(ConfRuleVarName))
			{
				return *ConfRuleVarValue;
			}
#endif
			return DefaultParam;
		};

#if !(PLATFORM_ANDROID_X86 || PLATFORM_ANDROID_X64)
		// Not running an Intel libUnreal.so with Houdini library present means we're emulated
		bool bUsingHoudini = (access("/system/lib/libhoudini.so", F_OK) != -1);
#else
		bool bUsingHoudini = false;
#endif

		FPlatformMemoryStats Stats = FPlatformMemory::GetStats();
		// this is used in the same way as PlatformMemoryBucket
		// which on Android has a different rounding algo. See GenericPlatformMemory::GetMemorySizeBucket.
		uint64 MemoryBucketRoundingAddition = 384;
#if PLATFORM_ANDROID
		if (FString* MemoryBucketRoundingAdditionVar = FAndroidMisc::GetConfigRulesVariable(TEXT("MemoryBucketRoundingAddition")))
		{
			MemoryBucketRoundingAddition = FCString::Atoi64(**MemoryBucketRoundingAdditionVar);
		}
#endif
		uint32 TotalPhysicalGB = (uint32)((Stats.TotalPhysical + MemoryBucketRoundingAddition * 1024 * 1024 - 1) / 1024 / 1024 / 1024);

		FString HMDRequestedProfileName;
		if (IHeadMountedDisplayModule::IsAvailable())
		{
			HMDRequestedProfileName = IHeadMountedDisplayModule::Get().GetDeviceSystemName();
		}

		AndroidParams.Add(FAndroidProfileSelectorSourceProperties::SRC_GPUFamily, GetParam(FAndroidMisc::GetGPUFamily(), TEXT("gpu")));
		AndroidParams.Add(FAndroidProfileSelectorSourceProperties::SRC_GLVersion, FAndroidMisc::GetGLVersion());
		AndroidParams.Add(FAndroidProfileSelectorSourceProperties::SRC_VulkanAvailable, FAndroidMisc::IsVulkanAvailable() ? TEXT("true") : TEXT("false"));
		AndroidParams.Add(FAndroidProfileSelectorSourceProperties::SRC_VulkanVersion, FAndroidMisc::GetVulkanVersion());
		AndroidParams.Add(FAndroidProfileSelectorSourceProperties::SRC_AndroidVersion, FAndroidMisc::GetAndroidVersion());
		AndroidParams.Add(FAndroidProfileSelectorSourceProperties::SRC_DeviceMake, FAndroidMisc::GetDeviceMake());
		AndroidParams.Add(FAndroidProfileSelectorSourceProperties::SRC_DeviceModel, FAndroidMisc::GetDeviceModel());
		AndroidParams.Add(FAndroidProfileSelectorSourceProperties::SRC_DeviceBuildNumber, FAndroidMisc::GetDeviceBuildNumber());
		AndroidParams.Add(FAndroidProfileSelectorSourceProperties::SRC_UsingHoudini, bUsingHoudini ? TEXT("true") : TEXT("false"));
		AndroidParams.Add(FAndroidProfileSelectorSourceProperties::SRC_Hardware, GetParam(FString(TEXT("unknown")), TEXT("hardware")));
		AndroidParams.Add(FAndroidProfileSelectorSourceProperties::SRC_Chipset, GetParam(FString(TEXT("unknown")), TEXT("chipset")));
		AndroidParams.Add(FAndroidProfileSelectorSourceProperties::SRC_HMDSystemName, HMDRequestedProfileName);
		AndroidParams.Add(FAndroidProfileSelectorSourceProperties::SRC_TotalPhysicalGB, FString::Printf(TEXT("%d"), TotalPhysicalGB));

#if PLATFORM_ANDROID
		// allow ConfigRules to override cvars first
		const TMap<FString, FString>& ConfigRules = FAndroidMisc::GetConfigRulesTMap();
		for (const TPair<FString, FString>& Pair : ConfigRules)
		{
			const FString& VariableName = Pair.Key;
			const FString& VariableValue = Pair.Value;
			AndroidParams.Add(FName(FString::Printf(TEXT("SRC_ConfigRuleVar[%s]"), *VariableName)), *VariableValue);
		}
#endif
	}
	return AndroidParams;
}

FString const FAndroidDeviceProfileSelectorRuntimeModule::GetRuntimeDeviceProfileName()
{
	static FString ProfileName;

	if (ProfileName.IsEmpty())
	{
		// Fallback profiles in case we do not match any rules
		ProfileName = FPlatformMisc::GetDefaultDeviceProfileName();
		if (ProfileName.IsEmpty())
		{
			ProfileName = FPlatformProperties::PlatformName();
		}
		const TMap<FName, FString>& AndroidParams = GetDeviceSelectorParams();

		FAndroidDeviceProfileSelector::SetSelectorProperties(AndroidParams);

		UE_LOG(LogAndroid, Log, TEXT("Checking %d rules from DeviceProfile ini file."), FAndroidDeviceProfileSelector::GetNumProfiles() );
		UE_LOG(LogAndroid, Log, TEXT("  Default profile: %s"), * ProfileName);
		UE_LOG(LogAndroid, Log, TEXT("  Android selector params: "));
		for(auto& MapIt : AndroidParams)
		{
			UE_LOG(LogAndroid, Log, TEXT("  %s: %s"), *MapIt.Key.ToString(), *MapIt.Value);
		}

		const FString& DeviceMake = AndroidParams.FindChecked(FAndroidProfileSelectorSourceProperties::SRC_DeviceMake);
		const FString& DeviceModel = AndroidParams.FindChecked(FAndroidProfileSelectorSourceProperties::SRC_DeviceModel);

		CheckForJavaSurfaceViewWorkaround(DeviceMake, DeviceModel);

		// Use override from ConfigRules if set
		FString* ConfigProfile = FAndroidMisc::GetConfigRulesVariable(TEXT("Profile"));
		if (ConfigProfile != nullptr)
		{
			ProfileName = *ConfigProfile;
			UE_LOG(LogAndroid, Log, TEXT("Using ConfigRules Profile: [%s]"), *ProfileName);
		}
		else
		{
			// Find a match with the DeviceProfiles matching rules
			ProfileName = FAndroidDeviceProfileSelector::FindMatchingProfile(ProfileName);
			UE_LOG(LogAndroid, Log, TEXT("Selected Device Profile: [%s]"), *ProfileName);
		}
	}

	return ProfileName;
}

bool FAndroidDeviceProfileSelectorRuntimeModule::GetSelectorPropertyValue(const FName& PropertyType, FString& PropertyValueOUT)
{
	if (const FString* Found = GetDeviceSelectorParams().Find(PropertyType))
	{
		PropertyValueOUT = *Found;
		return true;
	}
	// Special case for non-existent config rule variables
	// they should return true and a value of '[null]'
	// this prevents configrule issues from throwing errors.
	if (PropertyType.ToString().StartsWith(TEXT("SRC_ConfigRuleVar[")))
	{
		PropertyValueOUT = TEXT("[null]");
		return true;
	}

	return false;
}

void FAndroidDeviceProfileSelectorRuntimeModule::CheckForJavaSurfaceViewWorkaround(const FString& DeviceMake, const FString& DeviceModel) const
{
#if USE_ANDROID_JNI
	// We need to initialize the class early as device profiles need to be evaluated before ProcessNewlyLoadedUObjects can be called.
	extern UClass* Z_Construct_UClass_UAndroidJavaSurfaceViewDevices();
	Z_Construct_UClass_UAndroidJavaSurfaceViewDevices();

	const UAndroidJavaSurfaceViewDevices *const SurfaceViewDevices = Cast<UAndroidJavaSurfaceViewDevices>(UAndroidJavaSurfaceViewDevices::StaticClass()->GetDefaultObject());
	check(SurfaceViewDevices);

	for(const FJavaSurfaceViewDevice& Device : SurfaceViewDevices->SurfaceViewDevices)
	{
		if(Device.Manufacturer == DeviceMake && Device.Model == DeviceModel)
		{
			extern void AndroidThunkCpp_UseSurfaceViewWorkaround();
			AndroidThunkCpp_UseSurfaceViewWorkaround();
			return;
		}
	}
#endif
}
