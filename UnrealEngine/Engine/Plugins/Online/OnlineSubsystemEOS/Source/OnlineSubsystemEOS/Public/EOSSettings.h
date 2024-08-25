// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectMacros.h"
#include "Engine/RuntimeOptionsBase.h"
#include "Engine/DataAsset.h"
#include "EOSShared.h"
#include "EOSSettings.generated.h"

/** Native version of the UObject based config data */
struct FEOSArtifactSettings
{
	FString ArtifactName;
	FString ClientId;
	FString ClientSecret;
	FString ProductId;
	FString SandboxId;
	FString DeploymentId;
	FString EncryptionKey;
};

UCLASS(Deprecated)
class UDEPRECATED_EOSArtifactSettings :
	public UDataAsset
{
	GENERATED_BODY()

public:
	UDEPRECATED_EOSArtifactSettings()
	{
	}
};

USTRUCT(BlueprintType)
struct FArtifactSettings
{
	GENERATED_BODY()

public:
	/** This needs to match what the launcher passes in the -epicapp command line arg */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="EOS Settings")
	FString ArtifactName;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="EOS Artifact Settings")
	FString ClientId;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="EOS Artifact Settings")
	FString ClientSecret;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="EOS Artifact Settings")
	FString ProductId;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="EOS Artifact Settings")
	FString SandboxId;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="EOS Artifact Settings")
	FString DeploymentId;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="EOS Artifact Settings")
	// Config key renamed to ClientEncryptionKey as EncryptionKey gets removed from packaged builds due to IniKeyDenylist=EncryptionKey entry in BaseGame.ini.
	FString ClientEncryptionKey;

	FEOSArtifactSettings ToNative() const;
};

/** Native version of the UObject based config data */
struct FEOSSettings
{
	FEOSSettings();

	FString CacheDir;
	FString DefaultArtifactName;
	FString SteamTokenType;
	EOS_ERTCBackgroundMode RTCBackgroundMode;
	int32 TickBudgetInMilliseconds;
	int32 TitleStorageReadChunkLength;
	bool bEnableOverlay;
	bool bEnableSocialOverlay;
	bool bEnableEditorOverlay;
	bool bPreferPersistentAuth;
	bool bUseEAS;
	bool bUseEOSConnect;
	bool bUseEOSSessions;
	bool bMirrorStatsToEOS;
	bool bMirrorAchievementsToEOS;
	bool bMirrorPresenceToEAS;
	TArray<FEOSArtifactSettings> Artifacts;
	TArray<FString> TitleStorageTags;
	TArray<FString> AuthScopeFlags;
};

UCLASS(Config=Engine, DefaultConfig)
class ONLINESUBSYSTEMEOS_API UEOSSettings :
	public URuntimeOptionsBase
{
	GENERATED_BODY()

public:
	/**
	 * The directory any PDS/TDS files are cached into. This is per artifact e.g.:
	 *
	 * <UserDir>/<ArtifactId>/<CacheDir>
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="EOS Settings")
	FString CacheDir = TEXT("CacheDir");

	/** Used when launched from a store other than EGS or when the specified artifact name was not present */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="EOS Settings")
	FString DefaultArtifactName;

	/** The preferred background mode to be used by RTC services */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="EOS Settings")
	FString RTCBackgroundMode;

	/** Used to throttle how much time EOS ticking can take */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="EOS Settings")
	int32 TickBudgetInMilliseconds = 0;

	/** Set to true to enable the overlay (ecom features) */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="EOS Settings")
	bool bEnableOverlay = false;

	/** Set to true to enable the social overlay (friends, invites, etc.) */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="EOS Settings")
	bool bEnableSocialOverlay = false;

	/** Set to true to enable the overlay when running in the editor */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "EOS Settings")
	bool bEnableEditorOverlay = false;

	/** Set to true to prefer persistent auth over external authentication during Login */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="EOS Settings")
	bool bPreferPersistentAuth = false;

	/** Tag combinations for paged queries in title file enumerations, separate tags within groups using `+` */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="EOS Settings")
	TArray<FString> TitleStorageTags;

	/** Chunk size used when reading a title file */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="EOS Settings")
	int32 TitleStorageReadChunkLength = 0;

	/** Per artifact SDK settings. A game might have a FooStaging, FooQA, and public Foo artifact */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="EOS Settings")
	TArray<FArtifactSettings> Artifacts;

	/** Auth scopes to request during login */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "EOS Settings")
	TArray<FString> AuthScopeFlags;

	/** Set to true to have Epic Accounts used (friends list will be unified with the default platform) */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="EOSPlus Login Settings", DisplayName="Use Epic Account for EOS login (requires account linking)")
	bool bUseEAS = false;

	/** Set to true to have EOS Connect APIs used to link accounts for crossplay */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="EOSPlus Login Settings", DisplayName="Use EOS Connect APIs to create and link Product User IDs (PUIDs), and use EOS Game Services")
	bool bUseEOSConnect = false;

	/** Set to true to write stats to EOS as well as the default platform */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Crossplay Settings")
	bool bMirrorStatsToEOS = false;

	/** Set to true to write achievement data to EOS as well as the default platform */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Crossplay Settings")
	bool bMirrorAchievementsToEOS = false;

	/** Set to true to use EOS for session registration with data mirrored to the default platform */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Crossplay Settings", DisplayName="Use Crossplay Sessions")
	bool bUseEOSSessions = false;

	/** Set to true to have Epic Accounts presence information updated when the default platform is updated */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Crossplay Settings")
	bool bMirrorPresenceToEAS = false;

	/**
	 * When running with Steam, defines what TokenType OSSEOS will request from OSSSteam to login with.
	 * Please see EOS documentation at https://dev.epicgames.com/docs/dev-portal/identity-provider-management#steam for more information.
	 * Note the default is currently "Session" but this is deprecated. Please migrate to WebApi.
	 * Possible values:
	 *     "App" -> [DEPRECATED] Use Steam Encryption Application Tickets from ISteamUser::GetEncryptedAppTicket.
	 *     "Session" -> [DEPRECATED] Use Steam Auth Session Tickets from ISteamUser::GetAuthSessionTicket.
	 *     "WebApi" -> Use Steam Auth Tickets from ISteamUser::GetAuthTicketForWebApi, using the default remote service identity configured for OSSSteam.
	 *     "WebApi:<remoteserviceidentity>" -> Use Steam Auth Tickets from ISteamUser::GetAuthTicketForWebApi, using an explicit remote service identity.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Crossplay Settings")
	FString SteamTokenType = TEXT("Session");

	/** Get the settings for the selected artifact */
	static bool GetSelectedArtifactSettings(FEOSArtifactSettings& OutSettings);

	static FEOSSettings GetSettings();
	FEOSSettings ToNative() const;

private:
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	static FString GetDefaultArtifactName();

	static bool GetArtifactSettings(const FString& ArtifactName, FEOSArtifactSettings& OutSettings);
	static bool GetArtifactSettings(const FString& ArtifactName, const FString& SandboxId, FEOSArtifactSettings& OutSettings);
	static bool GetArtifactSettingsImpl(const FString& ArtifactName, const TOptional<FString>& SandboxId, FEOSArtifactSettings& OutSettings);

	static const TArray<FEOSArtifactSettings>& GetCachedArtifactSettings();

	static FEOSSettings AutoGetSettings();
	static const FEOSSettings& ManualGetSettings();
};