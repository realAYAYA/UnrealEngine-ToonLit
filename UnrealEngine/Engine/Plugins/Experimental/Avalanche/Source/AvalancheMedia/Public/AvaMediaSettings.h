// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/EnumAsByte.h"
#include "Containers/StringFwd.h"
#include "Engine/DeveloperSettings.h"
#include "Framework/AvaInstanceSettings.h"
#include "Logging/LogVerbosity.h"
#include "Math/MathFwd.h"
#include "PixelFormat.h"
#include "UObject/SoftObjectPtr.h"
#include "AvaMediaSettings.generated.h"

class UUserWidget;

/**
 * Defines the verbosity level of the logging system.
 * This enum mirrors ELogVerbosity but can be used directly as a configuration property.
 */
UENUM()
enum class EAvaMediaLogVerbosity : uint8
{
	NoLogging = 0,
	Fatal,
	Error,
	Warning,
	Display,
	Log,
	Verbose,
	VeryVerbose
};

USTRUCT()
struct FAvaPlaybackServerLoggingEntry
{
	GENERATED_BODY()

	UPROPERTY(config, EditAnywhere, Category=Settings)
	FName Category;

	UPROPERTY(config, EditAnywhere, Category=Settings)
	EAvaMediaLogVerbosity VerbosityLevel = EAvaMediaLogVerbosity::VeryVerbose;
};

USTRUCT()
struct FAvaMediaLocalPlaybackServerSettings
{
	GENERATED_BODY()

	/** Name given to the game mode local playback server started as a separate process. */
	UPROPERTY(config, EditAnywhere, Category = Settings)
	FString ServerName = TEXT("LocalServer");

	/** Main window resolution of the local playback server. */
	UPROPERTY(config, EditAnywhere, Category = Settings)
	FIntPoint Resolution = FIntPoint(960, 540);

	/** Enable a log console for the local server process. */
	UPROPERTY(config, EditAnywhere, Category = Settings)
	bool bEnableLogConsole = false;

	/** Extra command line arguments. */
	UPROPERTY(config, EditAnywhere, Category = Settings)
	FString ExtraCommandLineArguments;

	/**
	 * Which logs to include and with which verbosity level
	*/
	UPROPERTY(Config, EditAnywhere, Category="Settings")
	TArray<FAvaPlaybackServerLoggingEntry> Logging;
};

UCLASS(config=Engine, meta=(DisplayName="Playback & Broadcast"))
class AVALANCHEMEDIA_API UAvaMediaSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UAvaMediaSettings();
	
	static const UAvaMediaSettings& Get() { return *GetSingletonInstance();}
	static UAvaMediaSettings& GetMutable() {return *GetSingletonInstance();}

	static ELogVerbosity::Type ToLogVerbosity(EAvaMediaLogVerbosity InAvaMediaLogVerbosity);

	/** Specifies the background clear color for the channel. */
	UPROPERTY(config, EditAnywhere, Category = "Broadcast")
	FLinearColor ChannelClearColor = FLinearColor::Black;

	/** Pixel format used if no media output has specific format requirement. */
	UPROPERTY(config, EditAnywhere, Category = "Broadcast")
	TEnumAsByte<EPixelFormat> ChannelDefaultPixelFormat = EPixelFormat::PF_B8G8R8A8;

	/** Resolution used if no media output has specific resolution requirement. */
	UPROPERTY(config, EditAnywhere, Category = "Broadcast")
	FIntPoint ChannelDefaultResolution = FIntPoint(1920, 1080);

	/**
	 * Enables drawing the placeholder widget when there is no Motion Design asset playing.
	 * If false, the channel is cleared to the background color.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Broadcast")
	bool bDrawPlaceholderWidget = false;
	
	/** Specify a place holder widget to render when no Motion Design asset is playing. */
	UPROPERTY(config, EditAnywhere, Category = "Broadcast")
	TSoftClassPtr<UUserWidget> PlaceholderWidgetClass;

	/**
	 * Default resolution for rundown preview.
	 * This resolution may be lower than the broadcast resolution to improve gpu performance.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Rundown")
	FIntPoint PreviewDefaultResolution = FIntPoint(960, 540);

	UPROPERTY(config, EditAnywhere, Category = "Rundown")
	FString PreviewChannelName;

	/**
	 * Special logic will not play any transition if the RC values are the same.
	 * This applies to combo templates.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Rundown")
	bool bEnableComboTemplateSpecialLogic = true;

	/**
	 * Special logic will not play any transition if the RC values are the same.
	 * This applies to single (non-combo) templates.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Rundown")
	bool bEnableSingleTemplateSpecialLogic = false;
	
	/** Whether playback client is started automatically. */
	UPROPERTY(config, EditAnywhere, Category = "Playback Client")
	bool bAutoStartPlaybackClient = false;

	/** Enable verbose logging for playback client. */
	UPROPERTY(config, EditAnywhere, Category = "Playback Client")
	bool bVerbosePlaybackClientLogging = false;

	/** Defines the interval in seconds in which servers are being pinged by the client. */
	UPROPERTY(config, EditAnywhere, Category = "Playback Client")
	float PingInterval = 2.5f;

	/** If servers do not respond after the ping timeout interval in seconds, they are disconnected. */
	UPROPERTY(config, EditAnywhere, Category = "Playback Client")
	float PingTimeoutInterval = 3*2.5f;

	/** Defines the timeout, in seconds, after which a pending status request is dropped and issued again. */
	UPROPERTY(config, EditAnywhere, Category = "Playback Client")
	float ClientPendingStatusRequestTimeout = 5.0f;
	
	/** Whether playback server is started automatically. */
	UPROPERTY(config, EditAnywhere, Category = "Playback Server")
	bool bAutoStartPlaybackServer = false;

	/** Name given to the playback server. If empty, the server name will be the computer name. */
	UPROPERTY(config, EditAnywhere, Category = "Playback Server")
	FString PlaybackServerName;

	/** Enable verbose logging for playback server. */
	UPROPERTY(config, EditAnywhere, Category = "Playback Server")
	bool bVerbosePlaybackServerLogging = false;

	/**
	 * Determines the verbosity level of the playback server's log replication.
	 * The server is not going to replicate any log event that is below this log level.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Playback Server")
	EAvaMediaLogVerbosity PlaybackServerLogReplicationVerbosity = EAvaMediaLogVerbosity::Error;

	/** Defines the timeout, in seconds, after which a pending status request is dropped and issued again. */
	UPROPERTY(config, EditAnywhere, Category = "Playback Server")
	float ServerPendingStatusRequestTimeout = 5.0f;

	/** Settings for the local playback server process. See "Launch Local Server" in the broadcast editor toolbar. */
	UPROPERTY(config, EditAnywhere, Category = "Playback Server")
	FAvaMediaLocalPlaybackServerSettings LocalPlaybackServerSettings;
	
	/** If true, the playback objects are kept in memory after pages are stopped. They are unloaded otherwise.*/
	UPROPERTY(Config, EditAnywhere, Category = "Playback Manager")
	bool bKeepPagesLoaded = false;

	UPROPERTY(Config, EditAnywhere, Category = "Playback Manager")
	FAvaInstanceSettings AvaInstanceSettings;

	/**
	 * Maximum cached Managed Motion Design assets used for rundown editor's page details.
	 * A value of 0 indicate the cache will grow without limit.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Managed Motion Design Instance Cache", meta = (DisplayName = "Maximum Cache Size"))
	int32 ManagedInstanceCacheMaximumSize = 20;
	
	/** Whether web server is started automatically. */
	UPROPERTY(config, EditAnywhere, Category = "Web Server")
	bool bAutoStartWebServer = true;

	/** The web remote control HTTP server's port. */
	UPROPERTY(config, EditAnywhere, Category = "Web Server")
	uint32 HttpServerPort = 10123;

private:
	static UAvaMediaSettings* GetSingletonInstance();
};
