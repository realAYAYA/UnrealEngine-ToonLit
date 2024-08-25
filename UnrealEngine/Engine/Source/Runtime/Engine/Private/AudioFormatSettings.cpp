// Copyright Epic Games, Inc. All Rights Reserved.

#include "Sound/AudioFormatSettings.h"
#include "Features/IModularFeatures.h"
#include "Sound/SoundWave.h"
#include "Misc/ConfigCacheIni.h"
#include "Audio.h"
#include "ISoundWaveCloudStreaming.h"

namespace Audio
{
	bool ShouldAllowPlatformSpecificFormats()
	{
		static bool IsAudioLinkEnabled = []() -> bool
		{
			bool bAvailable = IModularFeatures::Get().IsModularFeatureAvailable(TEXT("AudioLink Factory"));
			UE_CLOG(bAvailable,LogAudio, Display, TEXT("AudioLink is enabled, disabling platform specific AudioFormats."));
			return bAvailable;
		}();
		return !IsAudioLinkEnabled;
	}

	FName GetCloudStreamingFormatOverride(const FName& InCurrentFormat, const USoundWave* InWave)
	{
#if WITH_EDITORONLY_DATA
		// Is a cloud streaming feature available?
		if (InWave->IsCloudStreamingEnabled())
		{
			IModularFeatures::FScopedLockModularFeatureList ScopedLockModularFeatureList;
			TArray<Audio::ISoundWaveCloudStreamingFeature*> Features = IModularFeatures::Get().GetModularFeatureImplementations<Audio::ISoundWaveCloudStreamingFeature>(Audio::ISoundWaveCloudStreamingFeature::GetModularFeatureName());
			// If there is more than one cloud streaming feature it will be ambiguous which one to use.
			check(Features.Num() <= 1);
			for(int32 i=0; i<Features.Num(); ++i)
			{
				if (Features[i]->CanOverrideFormat(InWave))
				{
					FName NewFormatName = Features[i]->GetOverrideFormatName(InWave);
					if (NewFormatName.GetStringLength())
					{
						return NewFormatName;
					}
				}
			}
		}
#endif // WITH_EDITORONLY_DATA
		return InCurrentFormat;
	}

	FAudioFormatSettings::FAudioFormatSettings(FConfigCacheIni* InConfigSystem, const FString& InConfigFilename, const FString& InPlatformIdentifierForLogging)
	{
		ReadConfiguration(InConfigSystem, InConfigFilename, InPlatformIdentifierForLogging);
	}

	FName FAudioFormatSettings::GetWaveFormat(const USoundWave* Wave) const
	{
		FName FormatName = Audio::ToName(Wave->GetSoundAssetCompressionType());
		if (FormatName == Audio::NAME_PLATFORM_SPECIFIC)
		{
			if (ShouldAllowPlatformSpecificFormats())
			{
				if (Wave->IsStreaming())
				{
					FormatName = PlatformStreamingFormat;
				}
				else
				{
					FormatName = PlatformFormat;
				}
			}
			else
			{
				FormatName = FallbackFormat;
			}
		}
		FormatName = GetCloudStreamingFormatOverride(FormatName, Wave);
		return FormatName;
	}

	void FAudioFormatSettings::GetAllWaveFormats(TArray<FName>& OutFormats) const
	{
		OutFormats = AllWaveFormats;
	}

	void FAudioFormatSettings::GetWaveFormatModuleHints(TArray<FName>& OutHints) const
	{
		OutHints = WaveFormatModuleHints;
	}

	void FAudioFormatSettings::ReadConfiguration(FConfigCacheIni* InConfigSystem, const FString& InConfigFilename, const FString& InPlatformIdentifierForLogging)
	{
		auto MakePrettyArrayToString = [](const TArray<FName>& InNames) -> FString 
		{
			return FString::JoinBy(InNames, TEXT(", "),[](const FName& i) -> FString { return i.GetPlainNameString(); });
		};
		
		auto ToFName = [](const FString& InName) -> FName { return { *InName }; };
			
		using namespace Audio;

		// AllWaveFormats.
		{		
			TArray<FString> FormatNames;
			if (InConfigSystem->GetArray(TEXT("Audio"), TEXT("AllWaveFormats"), FormatNames, InConfigFilename))
			{
				Algo::Transform(FormatNames, AllWaveFormats, ToFName);
			}
			else
			{
				AllWaveFormats = { NAME_BINKA, NAME_ADPCM, NAME_PCM, NAME_OPUS, NAME_RADA};
				UE_LOG(LogAudio, Warning, TEXT("Audio:AllWaveFormats is not defined, defaulting to built in formats. (%s)"), *MakePrettyArrayToString(AllWaveFormats));
			}
		}

		// FormatModuleHints
		{		
			TArray<FString> FormatModuleHints;
			if (InConfigSystem->GetArray(TEXT("Audio"), TEXT("FormatModuleHints"), FormatModuleHints, InConfigFilename))
			{
				Algo::Transform(FormatModuleHints, WaveFormatModuleHints, ToFName);
			}
		}

		// FallbackFormat
		{
			FString FallbackFormatString;
			if (InConfigSystem->GetString(TEXT("Audio"), TEXT("FallbackFormat"), FallbackFormatString, InConfigFilename))
			{
				FallbackFormat = *FallbackFormatString;
			}
			else
			{
				FallbackFormat = NAME_ADPCM;
				UE_LOG(LogAudio, Warning, TEXT("Audio:FallbackFormat is not defined, defaulting to '%s'."), *FallbackFormat.GetPlainNameString());
			}
			if (!AllWaveFormats.Contains(FallbackFormat) && AllWaveFormats.Num() > 0)
			{
				UE_LOG(LogAudio, Warning, TEXT("FallbackFormat '%s' not defined in 'AllWaveFormats'. Using first format listed '%s'"), *FallbackFormatString, *AllWaveFormats[0].ToString());
				FallbackFormat = AllWaveFormats[0];
			}
		}

		// PlatformFormat
		{
			FString PlatformFormatString;
			if (InConfigSystem->GetString(TEXT("Audio"), TEXT("PlatformFormat"), PlatformFormatString, InConfigFilename))
			{
				PlatformFormat = *PlatformFormatString;
			}
			else
			{
				PlatformFormat = NAME_ADPCM;
				UE_LOG(LogAudio, Warning, TEXT("Audio:PlatformFormat is not defined, defaulting to '%s'."), *PlatformFormat.GetPlainNameString());
			}
			if (!AllWaveFormats.Contains(PlatformFormat))
			{
				UE_LOG(LogAudio, Warning, TEXT("PlatformFormat '%s' not defined in 'AllWaveFormats'. Using fallback format '%s'"), *PlatformFormatString, *FallbackFormat.ToString());
				PlatformFormat = FallbackFormat;
			}
		}

		// PlatformStreamingFormat
		{
			FString PlatformStreamingFormatString;
			if (InConfigSystem->GetString(TEXT("Audio"), TEXT("PlatformStreamingFormat"), PlatformStreamingFormatString, InConfigFilename))
			{
				PlatformStreamingFormat = *PlatformStreamingFormatString;		
			}
			else
			{
				PlatformStreamingFormat = NAME_ADPCM;
				UE_LOG(LogAudio, Warning, TEXT("Audio:PlatformStreamingFormat is not defined, defaulting to '%s'."), *PlatformStreamingFormat.GetPlainNameString());
			}
			if (!AllWaveFormats.Contains(PlatformStreamingFormat))
			{
				UE_LOG(LogAudio, Warning, TEXT("PlatformStreamingFormat '%s' not defined in 'AllWaveFormats'. Using fallback format '%s'"), *PlatformStreamingFormatString, *FallbackFormat.ToString());
				PlatformStreamingFormat = FallbackFormat;
			}
		}

		UE_LOG(LogAudio, Verbose, TEXT("AudioFormatSettings: TargetName='%s', AllWaveFormats=(%s), Hints=(%s), PlatformFormat='%s', PlatformStreamingFormat='%s', FallbackFormat='%s'"),
			*InPlatformIdentifierForLogging, *MakePrettyArrayToString(AllWaveFormats), *MakePrettyArrayToString(WaveFormatModuleHints), *PlatformFormat.ToString(), *PlatformStreamingFormat.ToString(), *FallbackFormat.ToString());	
	}

}// namespace Audio
