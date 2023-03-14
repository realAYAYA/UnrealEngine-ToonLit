// Copyright Epic Games, Inc. All Rights Reserved.

#include "AndroidDeviceProfileSelector.h"
#include "AndroidDeviceProfileMatchingRules.h"
#include "AndroidJavaSurfaceViewDevices.h"
#include "Templates/Casts.h"
#include "Internationalization/Regex.h"
#include "Misc/CommandLine.h"
#include "Misc/SecureHash.h"
#include "Containers/StringConv.h"

#if PLATFORM_ANDROID
#include "Android/AndroidPlatformMisc.h"
#endif

UAndroidDeviceProfileMatchingRules::UAndroidDeviceProfileMatchingRules(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UAndroidJavaSurfaceViewDevices::UAndroidJavaSurfaceViewDevices(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

static UAndroidDeviceProfileMatchingRules* GetAndroidDeviceProfileMatchingRules()
{
	// We need to initialize the class early as device profiles need to be evaluated before ProcessNewlyLoadedUObjects can be called.
	extern UClass* Z_Construct_UClass_UAndroidDeviceProfileMatchingRules();
	CreatePackage(UAndroidDeviceProfileMatchingRules::StaticPackage());
	Z_Construct_UClass_UAndroidDeviceProfileMatchingRules();

	// Get the default object which will has the values from DeviceProfiles.ini
	UAndroidDeviceProfileMatchingRules* Rules = Cast<UAndroidDeviceProfileMatchingRules>(UAndroidDeviceProfileMatchingRules::StaticClass()->GetDefaultObject());
	check(Rules);
	return Rules;
}

TMap<FName, FString> FAndroidDeviceProfileSelector::SelectorProperties;

void FAndroidDeviceProfileSelector::VerifySelectorParams()
{
	check(SelectorProperties.Find(FAndroidProfileSelectorSourceProperties::SRC_GPUFamily));
	check(SelectorProperties.Find(FAndroidProfileSelectorSourceProperties::SRC_GLVersion));
	check(SelectorProperties.Find(FAndroidProfileSelectorSourceProperties::SRC_AndroidVersion));
	check(SelectorProperties.Find(FAndroidProfileSelectorSourceProperties::SRC_DeviceMake));
	check(SelectorProperties.Find(FAndroidProfileSelectorSourceProperties::SRC_DeviceModel));
	check(SelectorProperties.Find(FAndroidProfileSelectorSourceProperties::SRC_DeviceBuildNumber));
	check(SelectorProperties.Find(FAndroidProfileSelectorSourceProperties::SRC_VulkanVersion));
	check(SelectorProperties.Find(FAndroidProfileSelectorSourceProperties::SRC_UsingHoudini));
	check(SelectorProperties.Find(FAndroidProfileSelectorSourceProperties::SRC_VulkanAvailable));
	check(SelectorProperties.Find(FAndroidProfileSelectorSourceProperties::SRC_Hardware));
	check(SelectorProperties.Find(FAndroidProfileSelectorSourceProperties::SRC_Chipset));
	check(SelectorProperties.Find(FAndroidProfileSelectorSourceProperties::SRC_HMDSystemName));
	check(SelectorProperties.Find(FAndroidProfileSelectorSourceProperties::SRC_TotalPhysicalGB));
}

FString FAndroidDeviceProfileSelector::FindMatchingProfile(const FString& FallbackProfileName)
{
	FString OutProfileName = FallbackProfileName;
	FString CommandLine = FCommandLine::Get();
	
	FString GPUFamily			= SelectorProperties.FindChecked(FAndroidProfileSelectorSourceProperties::SRC_GPUFamily);
	FString GLVersion			= SelectorProperties.FindChecked(FAndroidProfileSelectorSourceProperties::SRC_GLVersion);
	FString AndroidVersion		= SelectorProperties.FindChecked(FAndroidProfileSelectorSourceProperties::SRC_AndroidVersion);
	FString DeviceMake			= SelectorProperties.FindChecked(FAndroidProfileSelectorSourceProperties::SRC_DeviceMake);
	FString DeviceModel			= SelectorProperties.FindChecked(FAndroidProfileSelectorSourceProperties::SRC_DeviceModel);
	FString DeviceBuildNumber	= SelectorProperties.FindChecked(FAndroidProfileSelectorSourceProperties::SRC_DeviceBuildNumber);
	FString VulkanVersion		= SelectorProperties.FindChecked(FAndroidProfileSelectorSourceProperties::SRC_VulkanVersion);
	FString UsingHoudini		= SelectorProperties.FindChecked(FAndroidProfileSelectorSourceProperties::SRC_UsingHoudini);
	FString VulkanAvailable		= SelectorProperties.FindChecked(FAndroidProfileSelectorSourceProperties::SRC_VulkanAvailable);
	FString Hardware			= SelectorProperties.FindChecked(FAndroidProfileSelectorSourceProperties::SRC_Hardware);
	FString Chipset				= SelectorProperties.FindChecked(FAndroidProfileSelectorSourceProperties::SRC_Chipset);
	FString TotalPhysicalGB		= SelectorProperties.FindChecked(FAndroidProfileSelectorSourceProperties::SRC_TotalPhysicalGB);
	FString HMDSystemName		= SelectorProperties.FindChecked(FAndroidProfileSelectorSourceProperties::SRC_HMDSystemName);

	for (const FProfileMatch& Profile : GetAndroidDeviceProfileMatchingRules()->MatchProfile)
	{
		FString PreviousRegexMatch;
		bool bFoundMatch = true;
		for (const FProfileMatchItem& Item : Profile.Match)
		{
			FString ConfigRuleString;
			const FString* SourceString = nullptr;
			FString MatchString = Item.MatchString;
			switch (Item.SourceType)
			{
			case ESourceType::SRC_PreviousRegexMatch:
				SourceString = &PreviousRegexMatch;
				break;
			case ESourceType::SRC_GpuFamily:
				SourceString = &GPUFamily;
				break;
			case ESourceType::SRC_GlVersion:
				SourceString = &GLVersion;
				break;
			case ESourceType::SRC_AndroidVersion:
				SourceString = &AndroidVersion;
				break;
			case ESourceType::SRC_DeviceMake:
				SourceString = &DeviceMake;
				break;
			case ESourceType::SRC_DeviceModel:
				SourceString = &DeviceModel;
				break;
			case ESourceType::SRC_DeviceBuildNumber:
				SourceString = &DeviceBuildNumber;
				break;
			case ESourceType::SRC_VulkanVersion:
				SourceString = &VulkanVersion;
				break;
			case ESourceType::SRC_UsingHoudini:
				SourceString = &UsingHoudini;
				break;
			case ESourceType::SRC_VulkanAvailable:
				SourceString = &VulkanAvailable;
				break;
			case ESourceType::SRC_CommandLine:
				SourceString = &CommandLine;
				break;
			case ESourceType::SRC_Hardware:
				SourceString = &Hardware;
				break;
			case ESourceType::SRC_Chipset:
				SourceString = &Chipset;
				break;
			case ESourceType::SRC_HMDSystemName:
				SourceString = &HMDSystemName;
				break;
			case ESourceType::SRC_ConfigRuleVar:
				{
					// expected Matchstring contents for configrulevar is "configrule_varname|matchstring"
					// sourcestring is set to the configrule variable content. 
					// sourcestring will be an empty string if configrule_varname is not found.
					FString VariableValueMatchString;
					FString ConfigRuleVarName;
					SourceString = &ConfigRuleString;
					if (MatchString.Split(TEXT("|"), &ConfigRuleVarName, &VariableValueMatchString))
					{
						MatchString = VariableValueMatchString;
#if PLATFORM_ANDROID
						// TODO: Do this when running device selection with editor builds.
						if (FString* ConfigRuleVar = FAndroidMisc::GetConfigRulesVariable(ConfigRuleVarName))
						{
							ConfigRuleString = *ConfigRuleVar;
						}
#endif
					}					
					break;
				}
			default:
				continue;
			}

			const bool bNumericOperands = SourceString->IsNumeric() && MatchString.IsNumeric();

			switch (Item.CompareType)
			{
			case CMP_Equal:
				if (Item.SourceType == SRC_CommandLine) 
				{
					if (!FParse::Param(*CommandLine, *MatchString))
					{
						bFoundMatch = false;
					}
				}
				else
				{
					if (*SourceString != MatchString)
					{
						bFoundMatch = false;
					}
				}
				break;
			case CMP_Less:
				if ((bNumericOperands && FCString::Atof(**SourceString) >= FCString::Atof(*MatchString)) || (!bNumericOperands && *SourceString >= MatchString))
				{
					bFoundMatch = false;
				}
				break;
			case CMP_LessEqual:
				if ((bNumericOperands && FCString::Atof(**SourceString) > FCString::Atof(*MatchString)) || (!bNumericOperands && *SourceString > MatchString))
				{
					bFoundMatch = false;
				}
				break;
			case CMP_Greater:
				if ((bNumericOperands && FCString::Atof(**SourceString) <= FCString::Atof(*MatchString)) || (!bNumericOperands && *SourceString <= MatchString))
				{
					bFoundMatch = false;
				}
				break;
			case CMP_GreaterEqual:
				if ((bNumericOperands && FCString::Atof(**SourceString) < FCString::Atof(*MatchString)) || (!bNumericOperands && *SourceString < MatchString))
				{
					bFoundMatch = false;
				}
				break;
			case CMP_NotEqual:
				if (Item.SourceType == SRC_CommandLine)
				{
					if (FParse::Param(*CommandLine, *MatchString))
					{
						bFoundMatch = false;
					}
				}
				else
				{
					if (*SourceString == MatchString)
					{
						bFoundMatch = false;
					}
				}
				break;
			case CMP_EqualIgnore:
				if (SourceString->ToLower() != MatchString.ToLower())
				{
					bFoundMatch = false;
				}
				break;
			case CMP_LessIgnore:
				if (SourceString->ToLower() >= MatchString.ToLower())
				{
					bFoundMatch = false;
				}
				break;
			case CMP_LessEqualIgnore:
				if (SourceString->ToLower() > MatchString.ToLower())
				{
					bFoundMatch = false;
				}
				break;
			case CMP_GreaterIgnore:
				if (SourceString->ToLower() <= MatchString.ToLower())
				{
					bFoundMatch = false;
				}
				break;
			case CMP_GreaterEqualIgnore:
				if (SourceString->ToLower() < MatchString.ToLower())
				{
					bFoundMatch = false;
				}
				break;
			case CMP_NotEqualIgnore:
				if (SourceString->ToLower() == MatchString.ToLower())
				{
					bFoundMatch = false;
				}
				break;
			case CMP_Regex:
				{
					const FRegexPattern RegexPattern(MatchString);
					FRegexMatcher RegexMatcher(RegexPattern, *SourceString);
					if (RegexMatcher.FindNext())
					{
						PreviousRegexMatch = RegexMatcher.GetCaptureGroup(1);
					}
					else
					{
						bFoundMatch = false;
					}
				}
			break;
				case CMP_Hash:
				{
					// Salt string is concatenated onto the end of the input text.
					// For example the input string "PhoneModel" with salt "Salt" and pepper "Pepper" can be computed with
					// % printf "PhoneModelSaltPepper" | openssl dgst -sha1 -hex
					// resulting in d9e5cbd6b0e4dba00edd9de92cf64ee4c3f3a2db and would be stored in the matching rules as 
					// "Salt|d9e5cbd6b0e4dba00edd9de92cf64ee4c3f3a2db". Salt is optional.
					FString MatchHashString;
					FString SaltString;
					if (!MatchString.Split(TEXT("|"), &SaltString, &MatchHashString))
					{
						MatchHashString = MatchString;
					}
					FString HashInputString = *SourceString + SaltString
#ifdef HASH_PEPPER_SECRET_GUID
						+ HASH_PEPPER_SECRET_GUID.ToString()
#endif
						;

					FSHAHash SourceHash;
					FSHA1::HashBuffer(TCHAR_TO_ANSI(*HashInputString), HashInputString.Len(), SourceHash.Hash);
					if (SourceHash.ToString() != MatchHashString.ToUpper())
					{
						bFoundMatch = false;
					}
				}
				break;
			default:
				bFoundMatch = false;
			}

			if (!bFoundMatch)
			{
				break;
			}
		}

		if (bFoundMatch)
		{
			OutProfileName = Profile.Profile;
			break;
		}
	}
	return OutProfileName;
}

int32 FAndroidDeviceProfileSelector::GetNumProfiles()
{
	return GetAndroidDeviceProfileMatchingRules()->MatchProfile.Num();
}
