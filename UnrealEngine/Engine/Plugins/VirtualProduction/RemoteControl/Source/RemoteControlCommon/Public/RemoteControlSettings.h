// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "RemoteControlSettings.generated.h"

/**
 * Passphrase Struct
 */
USTRUCT(BlueprintType)
struct FRCPassphrase
{
	GENERATED_BODY()
	
	FRCPassphrase(){}

	UPROPERTY(EditAnywhere, Category="Passphrase")
	FString Identifier;
	
	UPROPERTY(EditAnywhere, Category="Passphrase")
	FString Passphrase;
};

/**
 * Global remote control settings
 */
UCLASS(config = RemoteControl)
class REMOTECONTROLCOMMON_API URemoteControlSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Gets the settings container name for the settings, either Project or Editor */
	virtual FName GetContainerName() const
	{
		return "Project";
	}
	
	/** Gets the category for the settings, some high level grouping like, Editor, Engine, Game...etc. */
	virtual FName GetCategoryName() const
	{
		return "Plugins";
	}
	
	/** The unique name for your section of settings, uses the class's FName. */
	virtual FName GetSectionName() const
	{
		return "Remote Control";
	}

	virtual FText GetSectionText() const
	{
		return NSLOCTEXT("RemoteControlSettings", "RemoteControlSettingsSection", "Remote Control");
	}

	virtual TArray<FString> GetHashedPassphrases() const
	{
		TArray<FString> OutArray;

		for (const FRCPassphrase& Passphrase : Passphrases)
		{
			OutArray.Add(Passphrase.Passphrase);
		}
		
		return OutArray;
	}

	/**
	 * Should transactions be generated for events received through protocols (ie. MIDI, DMX etc.)
	 * Disabling transactions improves performance but will prevent events from being transacted to Multi-User
	 * unless using the Remote Control Interception feature.
	 */
	UPROPERTY(config, EditAnywhere, Category = RemoteControl)
	bool bProtocolsGenerateTransactions = true;

	/** The remote control web app http port. */
	UPROPERTY(config, EditAnywhere, Category = "Remote Control Web Interface", DisplayName = "Remote Control Web Interface http Port")
	uint32 RemoteControlWebInterfacePort = 30000;

	/** Should force a build of the WebApp at startup. */
	UPROPERTY(config, EditAnywhere, Category = "Remote Control Web Interface", DisplayName = "Force WebApp build at startup")
	bool bForceWebAppBuildAtStartup = false;

	/** Should WebApp log timing. */
	UPROPERTY(config, EditAnywhere, Category = "Remote Control Web Interface", DisplayName = "Log WebApp requests handle duration")
	bool bWebAppLogRequestDuration = false;

	/** Whether communication with the Web Interface should only be allowed with an Passphrase */
	UPROPERTY(Config, EditAnywhere, Category = "Remote Control Web Interface", DisplayName = "Use Passphrase to block Access")
	bool bUseRemoteControlPassphrase = false;

	/** Whether the User should be warned that Passphrase usage is disabled or now. Initially activated */
	UPROPERTY(Config, EditAnywhere, Category = "Remote Control Web Interface", DisplayName = "Warn that Passphrase might be disabled ")
	bool bShowPassphraseDisabledWarning = true;

	/** Whether web server is started automatically. */
	UPROPERTY(config, EditAnywhere, Category = "Remote Control Web Server")
	bool bAutoStartWebServer = true;

	/** Whether web socket server is started automatically. */
	UPROPERTY(config, EditAnywhere, Category = "Remote Control Web Server")
	bool bAutoStartWebSocketServer = true;

	/** The web remote control HTTP server's port. */
	UPROPERTY(config, EditAnywhere, Category = "Remote Control Web Server", DisplayName = "Remote Control HTTP Server Port")
	uint32 RemoteControlHttpServerPort = 30010;

	/** The web remote control WebSocket server's port. */
	UPROPERTY(config, EditAnywhere, Category = "Remote Control Web Server", DisplayName = "Remote Control WebSocket Server Port")
	uint32 RemoteControlWebSocketServerPort = 30020;

	/** Show a warning icon for exposed editor-only fields. */
	UPROPERTY(config, EditAnywhere, Category = "Remote Control Preset", DisplayName = "Show a warning when exposing editor-only entities.")
	bool bDisplayInEditorOnlyWarnings = false;

	/** The split widget control ratio between entity tree and details/protocol binding list. */
	UPROPERTY(config)
	float TreeBindingSplitRatio = 0.7;

	UPROPERTY(config)
	bool bUseRebindingContext = true;

	UPROPERTY(config, EditAnywhere, Category = "Remote Control Web Interface", DisplayName = "Remote Control Web Interface Passphrase")
	TArray<FRCPassphrase> Passphrases = {};
};
