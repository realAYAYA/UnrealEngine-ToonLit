// Copyright Epic Games, Inc. All Rights Reserved.

#include "EOSSettings.h"
#include "OnlineSubsystemEOS.h"
#include "OnlineSubsystemEOSPrivate.h"

#include "Algo/Transform.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EOSSettings)

#if WITH_EDITOR
	#include "Misc/MessageDialog.h"
#endif

#define LOCTEXT_NAMESPACE "EOS"

#define INI_SECTION TEXT("/Script/OnlineSubsystemEOS.EOSSettings")

inline bool IsAnsi(const FString& Source)
{
	for (const TCHAR& IterChar : Source)
	{
		if (!FChar::IsPrint(IterChar))
		{
			return false;
		}
	}
	return true;
}

inline bool IsHex(const FString& Source)
{
	for (const TCHAR& IterChar : Source)
	{
		if (!FChar::IsHexDigit(IterChar))
		{
			return false;
		}
	}
	return true;
}

inline bool ContainsWhitespace(const FString& Source)
{
	for (const TCHAR& IterChar : Source)
	{
		if (FChar::IsWhitespace(IterChar))
		{
			return true;
		}
	}
	return false;
}

FEOSArtifactSettings FArtifactSettings::ToNative() const
{
	FEOSArtifactSettings Native;

	Native.ArtifactName = ArtifactName;
	Native.ClientId = ClientId;
	Native.ClientSecret = ClientSecret;
	Native.DeploymentId = DeploymentId;
	Native.EncryptionKey = ClientEncryptionKey;
	Native.ProductId = ProductId;
	Native.SandboxId = SandboxId;

	return Native;
}

inline FString StripQuotes(const FString& Source)
{
	if (Source.StartsWith(TEXT("\"")))
	{
		return Source.Mid(1, Source.Len() - 2);
	}
	return Source;
}

FEOSArtifactSettings ParseArtifactSettingsFromConfigString(const FString& RawLine)
{
	FEOSArtifactSettings Result;

	const TCHAR* Delims[4] = { TEXT("("), TEXT(")"), TEXT("="), TEXT(",") };
	TArray<FString> Values;
	RawLine.ParseIntoArray(Values, Delims, 4, false);
	for (int32 ValueIndex = 0; ValueIndex < Values.Num(); ValueIndex++)
	{
		if (Values[ValueIndex].IsEmpty())
		{
			continue;
		}

		// Parse which struct field
		if (Values[ValueIndex] == TEXT("ArtifactName"))
		{
			Result.ArtifactName = StripQuotes(Values[ValueIndex + 1]);
		}
		else if (Values[ValueIndex] == TEXT("ClientId"))
		{
			Result.ClientId = StripQuotes(Values[ValueIndex + 1]);
		}
		else if (Values[ValueIndex] == TEXT("ClientSecret"))
		{
			Result.ClientSecret = StripQuotes(Values[ValueIndex + 1]);
		}
		else if (Values[ValueIndex] == TEXT("ProductId"))
		{
			Result.ProductId = StripQuotes(Values[ValueIndex + 1]);
		}
		else if (Values[ValueIndex] == TEXT("SandboxId"))
		{
			Result.SandboxId = StripQuotes(Values[ValueIndex + 1]);
		}
		else if (Values[ValueIndex] == TEXT("DeploymentId"))
		{
			Result.DeploymentId = StripQuotes(Values[ValueIndex + 1]);
		}
		// EncryptionKey is problematic as a key name as it gets removed by IniKeyDenyList, so lots of EOS config has moved to ClientEncryptionKey instead.
		// That specific issue doesn't affect this specific case as it's part of a config _value_, but supporting both names for consistency and back-compat.
		else if (Values[ValueIndex] == TEXT("EncryptionKey") || Values[ValueIndex] == TEXT("ClientEncryptionKey"))
		{
			Result.EncryptionKey = StripQuotes(Values[ValueIndex + 1]);
		}
		ValueIndex++;
	}

	return Result;
}

FEOSSettings::FEOSSettings()
	: SteamTokenType(TEXT("Session"))
	, RTCBackgroundMode(EOS_ERTCBackgroundMode::EOS_RTCBM_KeepRoomsAlive)
	, TickBudgetInMilliseconds(0)
	, TitleStorageReadChunkLength(0)
	, bEnableOverlay(false)
	, bEnableSocialOverlay(false)
	, bEnableEditorOverlay(false)
	, bPreferPersistentAuth(false)
	, bUseEAS(false)
	, bUseEOSConnect(false)
	, bUseEOSSessions(false)
	, bMirrorStatsToEOS(false)
	, bMirrorAchievementsToEOS(false)
	, bMirrorPresenceToEAS(false)
{

}

FEOSSettings UEOSSettings::GetSettings()
{
	if (UObjectInitialized())
	{
		return UEOSSettings::AutoGetSettings();
	}

	return UEOSSettings::ManualGetSettings();
}

FEOSSettings UEOSSettings::AutoGetSettings()
{
	return GetDefault<UEOSSettings>()->ToNative();
}

const FEOSSettings& UEOSSettings::ManualGetSettings()
{
	static TOptional<FEOSSettings> CachedSettings;

	if (!CachedSettings.IsSet())
	{
		CachedSettings.Emplace();

		GConfig->GetString(INI_SECTION, TEXT("CacheDir"), CachedSettings->CacheDir, GEngineIni);
		GConfig->GetString(INI_SECTION, TEXT("DefaultArtifactName"), CachedSettings->DefaultArtifactName, GEngineIni);
		GConfig->GetString(INI_SECTION, TEXT("SteamTokenType"), CachedSettings->SteamTokenType, GEngineIni);
		CachedSettings->RTCBackgroundMode = EOS_ERTCBackgroundMode::EOS_RTCBM_KeepRoomsAlive;
		FString RTCBackgroundModeStr;
		GConfig->GetString(INI_SECTION, TEXT("RTCBackgroundMode"), RTCBackgroundModeStr, GEngineIni);
		if (!RTCBackgroundModeStr.IsEmpty())
		{
			LexFromString(CachedSettings->RTCBackgroundMode, *RTCBackgroundModeStr);
		}
		GConfig->GetInt(INI_SECTION, TEXT("TickBudgetInMilliseconds"), CachedSettings->TickBudgetInMilliseconds, GEngineIni);
		GConfig->GetInt(INI_SECTION, TEXT("TitleStorageReadChunkLength"), CachedSettings->TitleStorageReadChunkLength, GEngineIni);
		GConfig->GetBool(INI_SECTION, TEXT("bEnableOverlay"), CachedSettings->bEnableOverlay, GEngineIni);
		GConfig->GetBool(INI_SECTION, TEXT("bEnableSocialOverlay"), CachedSettings->bEnableSocialOverlay, GEngineIni);
		GConfig->GetBool(INI_SECTION, TEXT("bEnableEditorOverlay"), CachedSettings->bEnableEditorOverlay, GEngineIni);
		GConfig->GetBool(INI_SECTION, TEXT("bPreferPersistentAuth"), CachedSettings->bPreferPersistentAuth, GEngineIni);
		GConfig->GetBool(INI_SECTION, TEXT("bUseEAS"), CachedSettings->bUseEAS, GEngineIni);
		GConfig->GetBool(INI_SECTION, TEXT("bUseEOSConnect"), CachedSettings->bUseEOSConnect, GEngineIni);
		GConfig->GetBool(INI_SECTION, TEXT("bUseEOSSessions"), CachedSettings->bUseEOSSessions, GEngineIni);
		GConfig->GetBool(INI_SECTION, TEXT("bMirrorStatsToEOS"), CachedSettings->bMirrorStatsToEOS, GEngineIni);
		GConfig->GetBool(INI_SECTION, TEXT("bMirrorAchievementsToEOS"), CachedSettings->bMirrorAchievementsToEOS, GEngineIni);
		GConfig->GetBool(INI_SECTION, TEXT("bMirrorPresenceToEAS"), CachedSettings->bMirrorPresenceToEAS, GEngineIni);
		// Artifacts explicitly skipped
		GConfig->GetArray(INI_SECTION, TEXT("TitleStorageTags"), CachedSettings->TitleStorageTags, GEngineIni);
		GConfig->GetArray(INI_SECTION, TEXT("AuthScopeFlags"), CachedSettings->AuthScopeFlags, GEngineIni);
	}

	return *CachedSettings;
}

FEOSSettings UEOSSettings::ToNative() const
{
	FEOSSettings Native;

	Native.CacheDir = CacheDir;
	Native.DefaultArtifactName = DefaultArtifactName;
	Native.SteamTokenType = SteamTokenType;
	Native.RTCBackgroundMode = EOS_ERTCBackgroundMode::EOS_RTCBM_KeepRoomsAlive;
	if (!RTCBackgroundMode.IsEmpty())
	{
		LexFromString(Native.RTCBackgroundMode, *RTCBackgroundMode);
	}
	Native.TickBudgetInMilliseconds = TickBudgetInMilliseconds;
	Native.TitleStorageReadChunkLength = TitleStorageReadChunkLength;
	Native.bEnableOverlay = bEnableOverlay;
	Native.bEnableSocialOverlay = bEnableSocialOverlay;
	Native.bEnableEditorOverlay = bEnableEditorOverlay;
	Native.bPreferPersistentAuth = bPreferPersistentAuth;
	Native.bUseEAS = bUseEAS;
	Native.bUseEOSConnect = bUseEOSConnect;
	Native.bUseEOSSessions = bUseEOSSessions;
	Native.bMirrorStatsToEOS = bMirrorStatsToEOS;
	Native.bMirrorAchievementsToEOS = bMirrorAchievementsToEOS;
	Native.bMirrorPresenceToEAS = bMirrorPresenceToEAS;
	Algo::Transform(Artifacts, Native.Artifacts, &FArtifactSettings::ToNative);
	Native.TitleStorageTags = TitleStorageTags;
	Native.AuthScopeFlags = AuthScopeFlags;

	return Native;
}

bool UEOSSettings::GetSelectedArtifactSettings(FEOSArtifactSettings& OutSettings)
{
	// Get default artifact name from config.
	FString ArtifactName = GetDefaultArtifactName();
	// Prefer -epicapp over config. This generally comes from EGS.
	FParse::Value(FCommandLine::Get(), TEXT("EpicApp="), ArtifactName);
	// Prefer -EOSArtifactNameOverride over previous.
	FParse::Value(FCommandLine::Get(), TEXT("EOSArtifactNameOverride="), ArtifactName);

	FString SandboxId;
	// Get the -epicsandboxid argument. This generally comes from EGS.
	bool bHasSandboxId = FParse::Value(FCommandLine::Get(), TEXT("EpicSandboxId="), SandboxId);
	// Prefer -EpicSandboxIdOverride over previous.
	bHasSandboxId |= FParse::Value(FCommandLine::Get(), TEXT("EpicSandboxIdOverride="), SandboxId);
	// If present, grab the settings where both match
	if (bHasSandboxId)
	{
		if (GetArtifactSettings(ArtifactName, SandboxId, OutSettings))
		{
			return true;
		}

		UE_LOG_ONLINE(Log, TEXT("UEOSSettings::GetSelectedArtifactSettings() ArtifactName=[%s] SandboxId=[%s] no settings found for pair, falling back on just ArtifactName."), *ArtifactName, *SandboxId);
	}

	// Fall back on just matching the Artifact name. This assumes non-EGS and only one settings entry per ArtifactName in config.
	const bool bSuccess = GetArtifactSettings(ArtifactName, OutSettings);
	UE_CLOG_ONLINE(!bSuccess, Error, TEXT("UEOSSettings::GetSelectedArtifactSettings() ArtifactName=[%s] no settings found."), *ArtifactName);
	return bSuccess;
}

FString UEOSSettings::GetDefaultArtifactName()
{
	if (UObjectInitialized())
	{
		return GetDefault<UEOSSettings>()->DefaultArtifactName;
	}

	FString DefaultArtifactName;
	GConfig->GetString(INI_SECTION, TEXT("DefaultArtifactName"), DefaultArtifactName, GEngineIni);
	return DefaultArtifactName;
}

bool UEOSSettings::GetArtifactSettings(const FString& ArtifactName, FEOSArtifactSettings& OutSettings)
{
	return GetArtifactSettingsImpl(ArtifactName, TOptional<FString>(), OutSettings);
}

bool UEOSSettings::GetArtifactSettings(const FString& ArtifactName, const FString& SandboxId, FEOSArtifactSettings& OutSettings)
{
	return GetArtifactSettingsImpl(ArtifactName, SandboxId, OutSettings);
}

bool UEOSSettings::GetArtifactSettingsImpl(const FString& ArtifactName, const TOptional<FString>& SandboxId, FEOSArtifactSettings& OutSettings)
{
	if (UObjectInitialized())
	{
		const UEOSSettings* This = GetDefault<UEOSSettings>();
		const FArtifactSettings* Found = This->Artifacts.FindByPredicate([&ArtifactName, &SandboxId](const FArtifactSettings& Element)
		{
			return Element.ArtifactName == ArtifactName
				&& (!SandboxId.IsSet() || Element.SandboxId == SandboxId);
		});
		if (Found)
		{
			OutSettings = Found->ToNative();
			return true;
		}
		return false;
	}
	else
	{
		const TArray<FEOSArtifactSettings>& CachedSettings = GetCachedArtifactSettings();
		const FEOSArtifactSettings* Found = CachedSettings.FindByPredicate([&ArtifactName, &SandboxId](const FEOSArtifactSettings& Element)
		{
			return Element.ArtifactName == ArtifactName
				&& (!SandboxId.IsSet() || Element.SandboxId == SandboxId);
		});
		if (Found)
		{
			OutSettings = *Found;
			return true;
		}
		return false;
	}
}

const TArray<FEOSArtifactSettings>& UEOSSettings::GetCachedArtifactSettings()
{
	static TArray<FEOSArtifactSettings> CachedArtifactSettings = []()
	{
		TArray<FString> ConfigArray;
		GConfig->GetArray(INI_SECTION, TEXT("Artifacts"), ConfigArray, GEngineIni);

		TArray<FEOSArtifactSettings> Result;
		Algo::Transform(ConfigArray, Result, &ParseArtifactSettingsFromConfigString);

		return Result;
	}();
	return CachedArtifactSettings;
}

#if WITH_EDITOR
void UEOSSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property == nullptr)
	{
		Super::PostEditChangeProperty(PropertyChangedEvent);
		return;
	}

	// Turning off the overlay in general turns off the social overlay too
	if (PropertyChangedEvent.Property->GetFName() == FName(TEXT("bEnableOverlay")))
	{
		if (!bEnableOverlay)
		{
			bEnableSocialOverlay = false;
			bEnableEditorOverlay = false;
		}
	}

	// Turning on the social overlay requires the base overlay too
	if (PropertyChangedEvent.Property->GetFName() == FName(TEXT("bEnableSocialOverlay")))
	{
		if (bEnableSocialOverlay)
		{
			bEnableOverlay = true;
		}
	}

	if (PropertyChangedEvent.MemberProperty != nullptr &&
		PropertyChangedEvent.MemberProperty->GetFName() == FName(TEXT("Artifacts")) &&
		(PropertyChangedEvent.ChangeType & EPropertyChangeType::ValueSet))
	{
		// Loop through all entries validating them
		for (FArtifactSettings& Artifact : Artifacts)
		{
			if (!Artifact.ClientId.IsEmpty())
			{
				if (!Artifact.ClientId.StartsWith(TEXT("xyz")))
				{
					FMessageDialog::Open(EAppMsgType::Ok,
						LOCTEXT("ClientIdInvalidMsg", "Client ids created after SDK version 1.5 start with xyz. Double check that you did not use your BPT Client Id instead."));
				}
				if (!IsAnsi(Artifact.ClientId) || ContainsWhitespace(Artifact.ClientId))
				{
					FMessageDialog::Open(EAppMsgType::Ok,
						LOCTEXT("ClientIdNotAnsiMsg", "Client ids must contain ANSI printable characters only with no whitespace"));
					Artifact.ClientId.Empty();
				}
			}

			if (!Artifact.ClientSecret.IsEmpty())
			{
				if (!IsAnsi(Artifact.ClientSecret) || ContainsWhitespace(Artifact.ClientSecret))
				{
					FMessageDialog::Open(EAppMsgType::Ok,
						LOCTEXT("ClientSecretNotAnsiMsg", "ClientSecret must contain ANSI printable characters only with no whitespace"));
					Artifact.ClientSecret.Empty();
				}
			}

			if (!Artifact.ClientEncryptionKey.IsEmpty())
			{
				if (!IsHex(Artifact.ClientEncryptionKey) || Artifact.ClientEncryptionKey.Len() != 64)
				{
					FMessageDialog::Open(EAppMsgType::Ok,
						LOCTEXT("EncryptionKeyNotHexMsg", "ClientEncryptionKey must contain 64 hex characters"));
					Artifact.ClientEncryptionKey.Empty();
				}
			}
		}
	}

	// Turning off EAS disables presence mirroring too
	if (PropertyChangedEvent.Property->GetFName() == FName(TEXT("bUseEAS")))
	{
		if (!bUseEAS)
		{
			bMirrorPresenceToEAS = false;
		}
	}

	// Turning on presence requires EAS
	if (PropertyChangedEvent.Property->GetFName() == FName(TEXT("bMirrorPresenceToEAS")))
	{
		if (bMirrorPresenceToEAS)
		{
			bUseEAS = true;
		}
	}

	// Turning off EAS disables presence mirroring too
	if (PropertyChangedEvent.Property->GetFName() == FName(TEXT("bUseEOSConnect")))
	{
		if (!bUseEOSConnect)
		{
			bMirrorAchievementsToEOS = false;
			bMirrorStatsToEOS = false;
			bUseEOSSessions = false;
		}
	}

	// These all require EOS turned on if they are on
	if (PropertyChangedEvent.Property->GetFName() == FName(TEXT("bMirrorAchievementsToEOS")) ||
		PropertyChangedEvent.Property->GetFName() == FName(TEXT("bMirrorStatsToEOS")) ||
		PropertyChangedEvent.Property->GetFName() == FName(TEXT("bUseEOSSessions")))
	{
		if (bMirrorAchievementsToEOS || bMirrorStatsToEOS || bUseEOSSessions)
		{
			bUseEOSConnect = true;
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#endif

#undef LOCTEXT_NAMESPACE

