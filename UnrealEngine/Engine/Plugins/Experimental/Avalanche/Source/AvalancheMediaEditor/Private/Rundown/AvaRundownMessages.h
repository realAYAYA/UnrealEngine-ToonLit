// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Rundown/AvaRundownPage.h"
#include "AvaRundownMessages.generated.h"

namespace EAvaRundownApiVersion
{
	/**
	 * Defines the protocol version of the Rundown Server API.
	 *
	 * API versioning is used to provide legacy support either on
	 * the client side or server side for non compatible changes.
	 * Clients can request a version of the API that they where implemented against,
	 * if the server can still honor the request it will accept.
	 */
	enum Type
	{
		Initial = 1,

		// -----<new versions can be added before this line>-------------------------------------------------
		// - this needs to be the last line (see note below)
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};
}


USTRUCT()
struct FAvaRundownMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	int32 RequestId = -1;
};

USTRUCT()
struct FAvaRundownServerMsg : public FAvaRundownMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FString Verbosity;

	UPROPERTY()
	FString Text;
};

/** Request published by client to discover servers. */
USTRUCT()
struct FAvaRundownPing : public FAvaRundownMsgBase
{
	GENERATED_BODY()
public:
	/** True if the request originates from an automatic timer. False if requests originates from user interaction. */
	UPROPERTY()
	bool bAuto = true;

	/**
	 * API Version the client has been implemented against.
	 * If none (-1) the server will consider the initial version is requested.
	 */
	UPROPERTY()
	int32 RequestedApiVersion = -1;
};

/** Response sent by server to client to be discovered. */
USTRUCT()
struct FAvaRundownPong : public FAvaRundownMsgBase
{
	GENERATED_BODY()

public:
	/** True if it is a reply to an auto ping. Mirrors the bAuto flag from Ping message. */
	UPROPERTY()
	bool bAuto = true;

	/**
	 * API Version the server will communicate with for this client.
	 * The server may honor the requested version if possible.
	 * Versions newer than server implementation will obviously not be honored either.
	 * Clients should expect an older server to reply with an older version.
	 */
	UPROPERTY()
	int32 ApiVersion = -1;

	/** Minimum API Version the server implements. */
	UPROPERTY()
	int32 MinimumApiVersion = -1;

	/** Latest API Version the server support. */
	UPROPERTY()
	int32 LatestApiVersion = -1;

	UPROPERTY()
	FString HostName;
};

/**
 *	Request list of rundown that can be opened on the current server.
 */
USTRUCT()
struct FAvaRundownGetRundowns : public FAvaRundownMsgBase
{
	GENERATED_BODY()
};

/**
 *	List of all rundowns.
 *	Expected Response from FAvaRundownGetRundowns.
 */
USTRUCT()
struct FAvaRundownRundowns : public FAvaRundownMsgBase
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<FString> Rundowns;
};

/**
 *	Request that the given rundown be opened.
 *	Only one rundown can be opened at a time. If another rundown
 *	is opened, it will be closed and all currently playing pages stopped.
 *	If the path is empty, nothing will be done and the server will reply with
 *	a FAvaRundownLoadedRundown message indicating which rundown is currently loaded.
 */
USTRUCT()
struct FAvaRundownLoadRundown : public FAvaRundownMsgBase
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FString Rundown;
};

/**
 * Request the list of pages from the given rundown.
 */
USTRUCT()
struct FAvaRundownGetPages : public FAvaRundownMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FString Rundown;
};

USTRUCT()
struct FAvaRundownCreatePage : public FAvaRundownMsgBase
{
	GENERATED_BODY()

	UPROPERTY()
	FString Rundown;

	UPROPERTY()
	int32 TemplateId = FAvaRundownPage::InvalidPageId;
};

USTRUCT()
struct FAvaRundownDeletePage : public FAvaRundownMsgBase
{
	GENERATED_BODY()

	UPROPERTY()
	FString Rundown;

	UPROPERTY()
	int32 PageId = FAvaRundownPage::InvalidPageId;
};

USTRUCT()
struct FAvaRundownCreateTemplate : public FAvaRundownMsgBase
{
	GENERATED_BODY()

	UPROPERTY()
	FString Rundown;
};

USTRUCT()
struct FAvaRundownDeleteTemplate : public FAvaRundownMsgBase
{
	GENERATED_BODY()

	UPROPERTY()
	FString Rundown;

	UPROPERTY()
	int32 PageId = FAvaRundownPage::InvalidPageId;
};

USTRUCT()
struct FAvaRundownChangeTemplateBP : public FAvaRundownMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FString Rundown;

	UPROPERTY()
	int32 TemplateId = FAvaRundownPage::InvalidPageId;

	UPROPERTY()
	FString AssetPath;
};

USTRUCT()
struct FAvaRundownPageInfo
{
	GENERATED_BODY()
public:
	UPROPERTY()
	int32 PageId = FAvaRundownPage::InvalidPageId;

	UPROPERTY()
	FString PageName;

	UPROPERTY()
	bool IsTemplate = false;

	UPROPERTY()
	int32 TemplateId = FAvaRundownPage::InvalidPageId;

	UPROPERTY()
	TArray<int32> CombinedTemplateIds;

	UPROPERTY()
	FSoftObjectPath AssetPath;

	UPROPERTY()
	TArray<FAvaRundownChannelPageStatus> Statuses;

	UPROPERTY()
	FString TransitionLayerName;

	UPROPERTY()
	FString OutputChannel;

	UPROPERTY()
	bool bIsEnabled = false;

	UPROPERTY()
	bool bIsPlaying = false;
};

/*
 * List of pages from the current rundown.
 */
USTRUCT()
struct FAvaRundownPages : public FAvaRundownMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	TArray<FAvaRundownPageInfo> Pages;
};

/**
 * Request the page details from the given rundown.
 */
USTRUCT()
struct FAvaRundownGetPageDetails : public FAvaRundownMsgBase
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FString Rundown;

	UPROPERTY()
	int32 PageId = FAvaRundownPage::InvalidPageId;

	/** This will request that a managed asset instance gets loaded to be
	 * accessible through WebRC. */
	UPROPERTY()
	bool bLoadRemoteControlPreset = false;
};

/**
 *	Server response to FAvaRundownGetPageDetails request.
 */
USTRUCT()
struct FAvaRundownPageDetails : public FAvaRundownMsgBase
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FString Rundown;

	UPROPERTY()
	FAvaRundownPageInfo PageInfo;

	UPROPERTY()
	FAvaPlayableRemoteControlValues RemoteControlValues;

	/** Name of the remote control preset to resolve through WebRC API. */
	UPROPERTY()
	FString RemoteControlPresetName;

	UPROPERTY()
	FString RemoteControlPresetId;
};

USTRUCT()
struct FAvaRundownPagesStatuses : public FAvaRundownMsgBase
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FString Rundown;

	UPROPERTY()
	FAvaRundownPageInfo PageInfo;
};

USTRUCT()
struct FAvaRundownPageListChanged : public FAvaRundownMsgBase
{
	GENERATED_BODY()

	UPROPERTY()
	FString Rundown;

	/** See EAvaPageListChange flags. */
	UPROPERTY()
	uint8 ChangeType = 0;

	UPROPERTY();
	TArray<int32> AffectedPages;
};

USTRUCT()
struct FAvaRundownPageBlueprintChanged : public FAvaRundownMsgBase
{
	GENERATED_BODY()

	UPROPERTY()
	FString Rundown;

	UPROPERTY()
	int32 PageId = FAvaRundownPage::InvalidPageId;

	UPROPERTY()
	FString BlueprintPath;
};

USTRUCT()
struct FAvaRundownPageChannelChanged : public FAvaRundownMsgBase
{
	GENERATED_BODY()

	UPROPERTY()
	FString Rundown;

	UPROPERTY()
	int32 PageId = FAvaRundownPage::InvalidPageId;

	UPROPERTY()
	FString ChannelName;
};

USTRUCT()
struct FAvaRundownPageAnimSettingsChanged : public FAvaRundownMsgBase
{
	GENERATED_BODY()

	UPROPERTY()
	FString Rundown;

	UPROPERTY()
	int32 PageId = FAvaRundownPage::InvalidPageId;
};

USTRUCT()
struct FAvaRundownPageChangeChannel : public FAvaRundownMsgBase
{
	GENERATED_BODY()

	UPROPERTY()
	FString Rundown;

	UPROPERTY()
	int32 PageId = FAvaRundownPage::InvalidPageId;

	UPROPERTY()
	FString ChannelName;
};

/** This is a request to save the managed RCP back to the corresponding page. */
USTRUCT()
struct FAvaRundownUpdatePageFromRCP : public FAvaRundownMsgBase
{
	GENERATED_BODY()

public:
	/** Unregister the Remote Control Preset from the WebRC. */
	UPROPERTY()
	bool bUnregister = false;
};

/** Supported Page actions for playback. */
UENUM()
enum class EAvaRundownPageActions
{
	None,
	Load,
	Unload,
	Play,
	PlayNext,
	Stop,
	ForceStop,
	Continue,
	UpdateValues,
	TakeToProgram
};

USTRUCT()
struct FAvaRundownPageAction : public FAvaRundownMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	int32 PageId = FAvaRundownPage::InvalidPageId;

	UPROPERTY()
	EAvaRundownPageActions Action = EAvaRundownPageActions::None;
};

USTRUCT()
struct FAvaRundownPagePreviewAction : public FAvaRundownPageAction
{
	GENERATED_BODY()
public:
	/** Specify which preview channel to use. If left empty, the rundown's default preview channel is used. */
	UPROPERTY()
	FString PreviewChannelName;
};

/**
 * Command to execute an action on multiple pages at the same time.
 * This is necessary for pages to be part of the same transition.
 */
USTRUCT()
struct FAvaRundownPageActions : public FAvaRundownMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	TArray<int32> PageIds;

	UPROPERTY()
	EAvaRundownPageActions Action = EAvaRundownPageActions::None;
};

USTRUCT()
struct FAvaRundownPagePreviewActions : public FAvaRundownPageActions
{
	GENERATED_BODY()
public:
	/** Specify which preview channel to use. If left empty, the rundown's default preview channel is used. */
	UPROPERTY()
	FString PreviewChannelName;
};

UENUM()
enum class EAvaRundownPageEvents
{
	None,
	AnimStarted,
	AnimPaused,
	AnimFinished
};

USTRUCT()
struct FAvaRundownPageEvent : public FAvaRundownMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	int32 PageId = FAvaRundownPage::InvalidPageId;

	UPROPERTY()
	EAvaRundownPageEvents Event = EAvaRundownPageEvents::None;
};

USTRUCT()
struct FAvaRundownOutputDeviceItem
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FString Name;

	/**
	 * Raw Json string representing a serialized UMediaOutput.
	 */
	UPROPERTY()
	FString Data;
};

USTRUCT()
struct FAvaRundownOutputClassItem
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FString Name;

	UPROPERTY()
	TArray<FAvaRundownOutputDeviceItem> Devices;
};

USTRUCT()
struct FAvaRundownDevicesList : public FAvaRundownMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	TArray<FAvaRundownOutputClassItem> DeviceClasses;
};

USTRUCT()
struct FAvaRundownGetChannel : public FAvaRundownMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FString ChannelName;
};

USTRUCT()
struct FAvaRundownGetChannels : public FAvaRundownMsgBase
{
	GENERATED_BODY()
};

USTRUCT()
struct FAvaRundownChannel
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FString Name;

	UPROPERTY()
	TArray<FAvaRundownOutputDeviceItem> Devices;
};

USTRUCT()
struct FAvaRundownChannelListChanged : public FAvaRundownMsgBase
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FAvaRundownChannel> Channels;
};

USTRUCT()
struct FAvaRundownChannelResponse : public FAvaRundownMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FAvaRundownChannel Channel;
};

USTRUCT()
struct FAvaRundownChannels : public FAvaRundownMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	TArray<FAvaRundownChannel> Channels;
};

USTRUCT()
struct FAvaRundownAssetsChanged : public FAvaRundownMsgBase
{
	GENERATED_BODY()

	UPROPERTY()
	FString AssetName;
};

// Channel actions
UENUM()
enum class EAvaRundownChannelActions
{
	None,
	Start,
	Stop
};

USTRUCT()
struct FAvaRundownChannelAction : public FAvaRundownMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FString ChannelName;

	UPROPERTY()
	EAvaRundownChannelActions Action = EAvaRundownChannelActions::None;
};

USTRUCT()
struct FAvaRundownGetDevices : public FAvaRundownMsgBase
{
	GENERATED_BODY()
};

USTRUCT()
struct FAvaRundownAddChannelDevice : public FAvaRundownMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FString ChannelName;

	UPROPERTY()
	FString MediaOutputName;
};

USTRUCT()
struct FAvaRundownEditChannelDevice : public FAvaRundownMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FString ChannelName;

	UPROPERTY()
	FString MediaOutputName;

	UPROPERTY()
	FString Data;
};

USTRUCT()
struct FAvaRundownRemoveChannelDevice : public FAvaRundownMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FString ChannelName;

	UPROPERTY()
	FString MediaOutputName;
};

/* No difference from FAvaRundownOutputDeviceItem except this is meant to return a single device response. */
USTRUCT()
struct FAvaRundownOutputDeviceItemResponse : public FAvaRundownMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FString Name;

	UPROPERTY()
	FString Data;
};

USTRUCT()
struct FAvaRundownGetChannelImage : public FAvaRundownMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FString ChannelName;
};

USTRUCT()
struct FAvaRundownChannelImage : public FAvaRundownMsgBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	TArray<uint8> ImageData;
};
