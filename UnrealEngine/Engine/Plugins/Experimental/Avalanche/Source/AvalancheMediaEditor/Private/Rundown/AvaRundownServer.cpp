// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rundown/AvaRundownServer.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Async/Async.h"
#include "AvaMediaMessageUtils.h"
#include "AvaMediaRenderTargetUtils.h"
#include "AvaRemoteControlUtils.h"
#include "Broadcast/AvaBroadcast.h"
#include "Broadcast/OutputDevices/AvaBroadcastOutputClassItem.h"
#include "Broadcast/OutputDevices/AvaBroadcastOutputDeviceItem.h"
#include "Broadcast/OutputDevices/AvaBroadcastOutputRootItem.h"
#include "Broadcast/OutputDevices/AvaBroadcastOutputTreeItem.h"
#include "Broadcast/OutputDevices/AvaBroadcastRenderTargetMediaUtils.h"
#include "IAvaMediaModule.h"
#include "IRemoteControlModule.h"
#include "ImageUtils.h"
#include "MediaOutput.h"
#include "MediaOutputEditorUtils/AvaRundownOutputEditorUtils.h"
#include "MessageEndpointBuilder.h"
#include "Misc/FileHelper.h"
#include "Playback/AvaPlaybackManager.h"
#include "Playback/AvaPlaybackUtils.h"
#include "RenderingThread.h"
#include "Rundown/AvaRundown.h"
#include "Rundown/AvaRundownManagedInstanceCache.h"
#include "Rundown/AvaRundownPagePlayer.h"
#include "Rundown/AvaRundownPlaybackUtils.h"
#include "ScopedTransaction.h"
#include "TextureResource.h"

DEFINE_LOG_CATEGORY_STATIC(LogAvaRundownServer, Log, All);

#define LOCTEXT_NAMESPACE "AvaRundownServer"

namespace UE::AvaRundownServer::Private
{
	inline FAvaRundownPageInfo GetPageInfo(const UAvaRundown* InRundown, const FAvaRundownPage& InPage)
	{
		FAvaRundownPageInfo PageInfo;
		PageInfo.PageId = InPage.GetPageId();
		PageInfo.PageName = InPage.GetPageName();
		PageInfo.IsTemplate = InPage.IsTemplate();
		PageInfo.TemplateId = InPage.GetTemplateId();
		PageInfo.CombinedTemplateIds = InPage.GetCombinedTemplateIds();
		PageInfo.AssetPath = InPage.GetAssetPath(InRundown);	// Todo: combo templates
		PageInfo.Statuses = InPage.GetPageStatuses(InRundown);
		PageInfo.TransitionLayerName = InPage.GetTransitionLayer(InRundown).ToString(); 	// Todo: combo templates
		PageInfo.OutputChannel = InPage.GetChannelName().ToString();
		PageInfo.bIsEnabled = InPage.IsEnabled();
		PageInfo.bIsPlaying = InRundown->IsPagePlaying(InPage);
		return PageInfo;
	}

	/**
	 * Utility function load a rundown asset in memory.
	 */
	static TStrongObjectPtr<UAvaRundown> LoadRundown(const FSoftObjectPath& InRundownPath)
	{
		UObject* Object = InRundownPath.ResolveObject();
		if (!Object)
		{
			Object = InRundownPath.TryLoad();
		}
		return TStrongObjectPtr<UAvaRundown>(Cast<UAvaRundown>(Object));
	}

	static FAvaRundownChannel SerializeChannel(const FAvaBroadcastOutputChannel& InChannel)
	{
		FAvaRundownChannel Channel;
		Channel.Name = InChannel.GetChannelName().ToString();
		const TArray<UMediaOutput*>& MediaOutputs = InChannel.GetMediaOutputs();
		for (const UMediaOutput* MediaOutput : MediaOutputs)
		{
			FAvaRundownOutputDeviceItem DeviceItem;
			DeviceItem.Name = MediaOutput->GetFName().ToString();
			DeviceItem.Data = FAvaRundownOutputEditorUtils::SerializeMediaOutput(MediaOutput);
			Channel.Devices.Push(MoveTemp(DeviceItem));
		}

		return Channel;
	}
	
	// recursively search device and children
	static FAvaOutputTreeItemPtr RecursiveFindOutputTreeItem(const FAvaOutputTreeItemPtr& InOutputTreeItem, const FString& InDeviceName)
	{
		if (!InOutputTreeItem->IsA<FAvaBroadcastOutputRootItem>() && InDeviceName == InOutputTreeItem->GetDisplayName().ToString())
		{
			return InOutputTreeItem;
		}
		
		for (const TSharedPtr<IAvaBroadcastOutputTreeItem>& Child : InOutputTreeItem->GetChildren())
		{
			if (FAvaOutputTreeItemPtr TreeItem = RecursiveFindOutputTreeItem(Child, InDeviceName))
			{
				return TreeItem;
			}
		}
		
		return nullptr;
	}

	static UMediaOutput* FindChannelMediaOutput(const FAvaBroadcastOutputChannel& InOutputChannel, const FString& InOutputMediaName)
	{
		const TArray<UMediaOutput*>& MediaOutputs = InOutputChannel.GetMediaOutputs();
		for (UMediaOutput* MediaOutput : MediaOutputs)
		{
			if (MediaOutput->GetName() == InOutputMediaName)
			{
				return MediaOutput;
			}
		}
		
		return nullptr;
	}
	
	inline TArray<int32> GetPlayingPages(const UAvaRundown* InRundown, bool bInIsPreview, FName InChannelName)
	{
		return bInIsPreview ? InRundown->GetPreviewingPageIds(InChannelName) : InRundown->GetPlayingPageIds(InChannelName);
	}

	static bool ContinuePages(UAvaRundown* InRundown, const TArray<int32>& InPageIds, bool bInIsPreview, FName InPreviewChannelName, FString& OutFailureReason)
	{
		bool bSuccess = false;
		for (const int32 PageId : InPageIds)
		{
			if (InRundown->CanContinuePage(PageId, bInIsPreview, InPreviewChannelName))
			{
				bSuccess |= InRundown->ContinuePage(PageId, bInIsPreview, InPreviewChannelName);
			}
			else if (bInIsPreview)
			{
				OutFailureReason.Appendf(TEXT("PageId %d was not previewing on channel \"%s\". "), PageId, *InPreviewChannelName.ToString());
			}
			else
			{
				OutFailureReason.Appendf(TEXT("PageId %d was not playing. "), PageId);
			}
		}
		return bSuccess;
	}

	static bool UpdatePagesValues(const UAvaRundown* InRundown, const TArray<int32>& InPageIds, bool bInIsPreview, FName InPreviewChannelName)
	{
		bool bSuccess = false;
		for (const int32 PageId : InPageIds)
		{
			bSuccess |= InRundown->PushRuntimeRemoteControlValues(PageId, bInIsPreview, InPreviewChannelName);
		}
		return bSuccess;
	}
}

/**
 * Holds the render target for copying the channel image.
 * The render target needs to be held for many frames until it is done.
 */
struct FAvaRundownServer::FChannelImage
{
	// Optional temporary render target for converting pixel format.
	TStrongObjectPtr<UTextureRenderTarget2D> RenderTarget;

	// Pixels readback from the render target. Format is PF_B8G8R8A8 (for now).
	TArray<FColor> RawPixels;
	int32 SizeX = 0;
	int32 SizeY = 0;

	void UpdateRenderTarget(int32 InSizeX, int32 InSizeY, EPixelFormat InFormat, const FLinearColor& InClearColor)
	{
		if (!RenderTarget.IsValid())
		{
			static const FName ChannelImageRenderTargetBaseName = TEXT("AvaRundownServer_ChannelImageRenderTarget");
			RenderTarget.Reset(UE::AvaMediaRenderTargetUtils::CreateDefaultRenderTarget(ChannelImageRenderTargetBaseName));
		}
	
		UE::AvaMediaRenderTargetUtils::UpdateRenderTarget(RenderTarget.Get(), FIntPoint(InSizeX, InSizeY), InFormat, InClearColor);
	}

	void UpdateRawPixels(int32 InSizeX, int32 InSizeY)
	{
		SizeX = InSizeX;
		SizeY = InSizeY;
		RawPixels.SetNum(InSizeX * InSizeY);
	}
};

FAvaRundownServer::FAvaRundownServer()
{
	
}

FAvaRundownServer::~FAvaRundownServer()
{
	FMessageEndpoint::SafeRelease(MessageEndpoint);
	
	for (IConsoleObject* ConsoleCommand : ConsoleCommands)
	{
		IConsoleManager::Get().UnregisterConsoleObject(ConsoleCommand);
	}
	ConsoleCommands.Empty();
}

const FMessageAddress& FAvaRundownServer::GetMessageAddress() const
{
	if (MessageEndpoint.IsValid())
	{
		return MessageEndpoint->GetAddress();
	}
	static FMessageAddress InvalidMessageAddress;
	return InvalidMessageAddress;
}

void FAvaRundownServer::Init(const FString& InAssignedHostName)
{
	HostName = InAssignedHostName.IsEmpty() ? FPlatformProcess::ComputerName() : InAssignedHostName;
	
	MessageEndpoint = FMessageEndpoint::Builder("MotionDesignRundownServer")
	.Handling<FAvaRundownPing>(this, &FAvaRundownServer::HandleRundownPing)
	.Handling<FAvaRundownGetRundowns>(this, &FAvaRundownServer::HandleGetRundowns)
	.Handling<FAvaRundownLoadRundown>(this, &FAvaRundownServer::HandleLoadRundown)
	.Handling<FAvaRundownCreatePage>(this, &FAvaRundownServer::HandleCreatePage)
	.Handling<FAvaRundownDeletePage>(this, &FAvaRundownServer::HandleDeletePage)
	.Handling<FAvaRundownCreateTemplate>(this, &FAvaRundownServer::HandleCreateTemplate)
	.Handling<FAvaRundownDeleteTemplate>(this, &FAvaRundownServer::HandleDeleteTemplate)
	.Handling<FAvaRundownChangeTemplateBP>(this, &FAvaRundownServer::HandleChangeTemplateBP)
	.Handling<FAvaRundownGetPages>(this, &FAvaRundownServer::HandleGetPages)
	.Handling<FAvaRundownGetPageDetails>(this, &FAvaRundownServer::HandleGetPageDetails)
	.Handling<FAvaRundownPageChangeChannel>(this, &FAvaRundownServer::HandleChangePageChannel)
	.Handling<FAvaRundownUpdatePageFromRCP>(this, &FAvaRundownServer::HandleUpdatePageFromRCP)
	.Handling<FAvaRundownPageAction>(this, &FAvaRundownServer::HandlePageAction)
	.Handling<FAvaRundownPagePreviewAction>(this, &FAvaRundownServer::HandlePagePreviewAction)
	.Handling<FAvaRundownPageActions>(this, &FAvaRundownServer::HandlePageActions)
	.Handling<FAvaRundownPagePreviewActions>(this, &FAvaRundownServer::HandlePagePreviewActions)
	.Handling<FAvaRundownGetChannel>(this, &FAvaRundownServer::HandleGetChannel)
	.Handling<FAvaRundownGetChannels>(this, &FAvaRundownServer::HandleGetChannels)
	.Handling<FAvaRundownChannelAction>(this, &FAvaRundownServer::HandleChannelAction)
	.Handling<FAvaRundownGetDevices>(this, &FAvaRundownServer::HandleGetDevices)
	.Handling<FAvaRundownAddChannelDevice>(this, &FAvaRundownServer::HandleAddChannelDevice)
	.Handling<FAvaRundownEditChannelDevice>(this, &FAvaRundownServer::HandleEditChannelDevice)
	.Handling<FAvaRundownRemoveChannelDevice>(this, &FAvaRundownServer::HandleRemoveChannelDevice)
	.Handling<FAvaRundownGetChannelImage>(this, &FAvaRundownServer::HandleGetChannelImage)
	.NotificationHandling(FOnBusNotification::CreateRaw(this, &FAvaRundownServer::OnMessageBusNotification));
	
	if (MessageEndpoint.IsValid())
	{
		// Subscribe to the server listing requests
		MessageEndpoint->Subscribe<FAvaRundownPing>();

		UE_LOG(LogAvaRundownServer, Log, TEXT("Motion Design Rundown Server \"%s\" Started."), *HostName);
	}
}

void FAvaRundownServer::SetupBroadcastDelegates(UAvaBroadcast* InBroadcast)
{
	RemoveBroadcastDelegates(InBroadcast);
	InBroadcast->GetOnChannelsListChanged().AddRaw(this, &FAvaRundownServer::OnBroadcastChannelListChanged);
}

void FAvaRundownServer::SetupEditorDelegates()
{
	RemoveEditorDelegates();
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	AssetRegistryModule.Get().OnAssetAdded().AddRaw(this, &FAvaRundownServer::OnAssetAddedOrRemoved);
	AssetRegistryModule.Get().OnAssetRemoved().AddRaw(this, &FAvaRundownServer::OnAssetAddedOrRemoved);
}

void FAvaRundownServer::RemoveBroadcastDelegates(UAvaBroadcast* InBroadcast) const
{
	InBroadcast->GetOnChannelsListChanged().RemoveAll(this);
}

void FAvaRundownServer::RemoveEditorDelegates() const
{
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	AssetRegistryModule.Get().OnAssetAdded().RemoveAll(this);
	AssetRegistryModule.Get().OnAssetRemoved().RemoveAll(this);
}

void FAvaRundownServer::OnPageListChanged(const FAvaRundownPageListChangeParams& InParams) const
{
	FAvaRundownPageListChanged* ReplyMessage = FMessageEndpoint::MakeMessage<FAvaRundownPageListChanged>();
	ReplyMessage->Rundown = FSoftObjectPath(InParams.Rundown).ToString();
	ReplyMessage->ChangeType = static_cast<uint8>(InParams.ChangeType);
	ReplyMessage->AffectedPages = InParams.AffectedPages;
	
	SendResponse(ReplyMessage, ClientAddresses);
}

void FAvaRundownServer::OnPagesChanged(const UAvaRundown* InRundown, const FAvaRundownPage& InPage, EAvaRundownPageChanges InChange) const
{
	switch(InChange)
	{
	case EAvaRundownPageChanges::AnimationSettings:
		{
			PageAnimSettingsChanged(InRundown, InPage);
			break;
		}
	case EAvaRundownPageChanges::Blueprint:
		{
			PageBlueprintChanged(InRundown, InPage, InPage.GetAssetPath(InRundown).ToString());
			break;
		}
	case EAvaRundownPageChanges::Status:
		{
			PageStatusChanged(InRundown, InPage);
			break;
		}
	case EAvaRundownPageChanges::Channel:
		{
			PageChannelChanged(InRundown, InPage, InPage.GetChannelName().ToString());
			break;
		}
	case EAvaRundownPageChanges::RemoteControlValues:
		{
			break;
		}
	case EAvaRundownPageChanges::All:
		{
			break;
		}
	case EAvaRundownPageChanges::None:
		{
			break;
		}
	default:
		{
			break;
		}
	}
	
}
void FAvaRundownServer::PageStatusChanged(const UAvaRundown* InRundown, const FAvaRundownPage& InPage) const
{
	FAvaRundownPagesStatuses* ReplyMessage = FMessageEndpoint::MakeMessage<FAvaRundownPagesStatuses>();
	ReplyMessage->Rundown = FSoftObjectPath(InRundown).ToString();
	ReplyMessage->PageInfo = UE::AvaRundownServer::Private::GetPageInfo(InRundown, InPage);
	SendResponse(ReplyMessage, ClientAddresses);
}

void FAvaRundownServer::PageBlueprintChanged(const UAvaRundown* InRundown, const FAvaRundownPage& InPage, const FString& InBlueprintPath) const
{
	FAvaRundownPageBlueprintChanged* ReplyMessage = FMessageEndpoint::MakeMessage<FAvaRundownPageBlueprintChanged>();
	ReplyMessage->Rundown = FSoftObjectPath(InRundown).ToString();
	ReplyMessage->PageId = InPage.GetPageId();
	ReplyMessage->BlueprintPath = InBlueprintPath;
	SendResponse(ReplyMessage, ClientAddresses);
}

void FAvaRundownServer::PageChannelChanged(const UAvaRundown* InRundown, const FAvaRundownPage& InPage, const FString& InChannelName) const
{
	FAvaRundownPageChannelChanged* ReplyMessage = FMessageEndpoint::MakeMessage<FAvaRundownPageChannelChanged>();
	ReplyMessage->Rundown = FSoftObjectPath(InRundown).ToString();
	ReplyMessage->PageId = InPage.GetPageId();
	ReplyMessage->ChannelName = InChannelName;
	SendResponse(ReplyMessage, ClientAddresses);
}

void FAvaRundownServer::PageAnimSettingsChanged(const UAvaRundown* InRundown, const FAvaRundownPage& InPage) const
{
	FAvaRundownPageAnimSettingsChanged* ReplyMessage = FMessageEndpoint::MakeMessage<FAvaRundownPageAnimSettingsChanged>();
	ReplyMessage->Rundown = FSoftObjectPath(InRundown).ToString();
	ReplyMessage->PageId = InPage.GetPageId();
	SendResponse(ReplyMessage, ClientAddresses);
}

void FAvaRundownServer::OnBroadcastChannelListChanged(const FAvaBroadcastProfile& InProfile) const
{
	FAvaRundownChannelListChanged* ReplyMessage = FMessageEndpoint::MakeMessage<FAvaRundownChannelListChanged>();

	const TArray<FAvaBroadcastOutputChannel*>& OutputChannels = InProfile.GetChannels();
	
	ReplyMessage->Channels.Reserve(OutputChannels.Num());

	for (const FAvaBroadcastOutputChannel* OutputChannel : OutputChannels)
	{
		FAvaRundownChannel Channel = UE::AvaRundownServer::Private::SerializeChannel(*OutputChannel);
		ReplyMessage->Channels.Push(MoveTemp(Channel));
	}
	SendResponse(ReplyMessage, ClientAddresses);
}

void FAvaRundownServer::OnAssetAddedOrRemoved(const FAssetData& InAssetData) const
{
	using namespace UE::AvaRundownServer::Private;
	if (InAssetData.GetClass() == UAvaRundown::StaticClass() || FAvaPlaybackUtils::IsPlayableAsset(InAssetData))
	{
		FAvaRundownAssetsChanged* ReplyMessage = FMessageEndpoint::MakeMessage<FAvaRundownAssetsChanged>();
		ReplyMessage->AssetName = InAssetData.AssetName.ToString();
		SendResponse(ReplyMessage, ClientAddresses);
	}
}

void FAvaRundownServer::HandleRundownPing(const FAvaRundownPing& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext)
{
	if (!InMessage.bAuto)
	{
		UE_LOG(LogAvaRundownServer, Log, TEXT("Received Ping request from %s"), *InContext->GetSender().ToString());
	}

	SetupEditorDelegates();
	
	FAvaRundownPong* ReplyMessage = FMessageEndpoint::MakeMessage<FAvaRundownPong>();
	ReplyMessage->RequestId = InMessage.RequestId;
	ReplyMessage->bAuto = InMessage.bAuto;

	// We still support the initial version.
	constexpr int32 CurrentMinimumApiVersion = EAvaRundownApiVersion::Initial;

	// Consider clients that didn't request a version to be the "initial" version.
	const int32 RequestedApiVersion = InMessage.RequestedApiVersion != -1 ? InMessage.RequestedApiVersion : EAvaRundownApiVersion::Initial;

	// Determine the version we will communicate with this client.
	int32 HonoredApiVersion = EAvaRundownApiVersion::LatestVersion;
	
	if (RequestedApiVersion >= CurrentMinimumApiVersion && RequestedApiVersion <= EAvaRundownApiVersion::LatestVersion)
	{
		HonoredApiVersion = RequestedApiVersion;
	}
	
	ReplyMessage->ApiVersion = HonoredApiVersion;
	ReplyMessage->MinimumApiVersion = CurrentMinimumApiVersion;
	ReplyMessage->LatestApiVersion = EAvaRundownApiVersion::LatestVersion;
	ReplyMessage->HostName = HostName;

	FClientInfo& ClientInfo = GetOrAddClientInfo(InContext->GetSender());
	ClientInfo.ApiVersion = HonoredApiVersion;

	SendResponse(ReplyMessage, InContext->GetSender());
}

void FAvaRundownServer::HandleGetRundowns(const FAvaRundownGetRundowns& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext)
{
	FAvaRundownRundowns* ReplyMessage = FMessageEndpoint::MakeMessage<FAvaRundownRundowns>();

	ReplyMessage->RequestId = InMessage.RequestId;
	
	// List all the play list.
	if (IAssetRegistry* AssetRegistry = IAssetRegistry::Get())
	{
		TArray<FAssetData> Assets;
		if (AssetRegistry->GetAssetsByClass(UAvaRundown::StaticClass()->GetClassPathName(), Assets))
		{
			ReplyMessage->Rundowns.Reserve(Assets.Num());		
			for (const FAssetData& Data : Assets)
			{
				ReplyMessage->Rundowns.Add(Data.ToSoftObjectPath().ToString());
			}
		}
	}

	SendResponse(ReplyMessage, InContext->GetSender());
}

void FAvaRundownServer::HandleLoadRundown(const FAvaRundownLoadRundown& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext)
{
	// If the requested path is empty, we assume this is a request for information only.
	if (!InMessage.Rundown.IsEmpty())
	{
		const FSoftObjectPath NewRundownPath(InMessage.Rundown);
		const UAvaRundown* Rundown = RundownPlaybackCommandData.GetOrLoadRundown(NewRundownPath,
			[this](UAvaRundown* InRundown)
			{
				RundownPlaybackCommandData.ClosePlaybackContext();
				RundownPlaybackCommandData.RemoveRundownDelegates(this, InRundown);
			},
			[this](UAvaRundown* InRundown)
			{
				RundownPlaybackCommandData.SetupRundownDelegates(this, InRundown);
				if (InRundown)
				{
					InRundown->InitializePlaybackContext();
				}
			});
		
		if (!Rundown)
		{
			SendMessage(InContext->GetSender(), InMessage.RequestId, ELogVerbosity::Error,
				TEXT("Rundown \"%s\" not loaded."), *InMessage.Rundown);
			return;
		}
	}

	SendMessage(InContext->GetSender(), InMessage.RequestId, ELogVerbosity::Log,
		TEXT("Rundown \"%s\" loaded."), *RundownPlaybackCommandData.CurrentRundownPath.ToString());
}

void FAvaRundownServer::HandleGetPages(const FAvaRundownGetPages& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext)
{
	UAvaRundown* Rundown = GetOrLoadRundownForEdit(InContext->GetSender(), InMessage.RequestId, InMessage.Rundown);
	if (!Rundown)
	{
		return;
	}
	
	FAvaRundownPages* ReplyMessage = FMessageEndpoint::MakeMessage<FAvaRundownPages>();
	ReplyMessage->RequestId = InMessage.RequestId;
	ReplyMessage->Pages.Reserve(Rundown->GetInstancedPages().Pages.Num() + Rundown->GetTemplatePages().Pages.Num());
	for (const FAvaRundownPage& Page : Rundown->GetInstancedPages().Pages)
	{
		FAvaRundownPageInfo PageInfo = UE::AvaRundownServer::Private::GetPageInfo(Rundown, Page);
		ReplyMessage->Pages.Add(MoveTemp(PageInfo));
	}
	
	for (const FAvaRundownPage& Page : Rundown->GetTemplatePages().Pages)
	{
		FAvaRundownPageInfo PageInfo = UE::AvaRundownServer::Private::GetPageInfo(Rundown, Page);
		ReplyMessage->Pages.Add(MoveTemp(PageInfo));
	}
	
	SendResponse(ReplyMessage, InContext->GetSender());
}

void FAvaRundownServer::HandleCreatePage(const FAvaRundownCreatePage& InMessage,
	const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext)
{
	UAvaRundown* Rundown = GetOrLoadRundownForEdit(InContext->GetSender(), InMessage.RequestId, InMessage.Rundown);
	if (!Rundown)
	{
		return;
	}

	const FAvaRundownPage& Template = Rundown->GetPage(InMessage.TemplateId);
    if (!Template.IsValidPage() || !Template.IsTemplate())
    {
    	LogAndSendMessage(InContext->GetSender(), InMessage.RequestId, ELogVerbosity::Error, TEXT("Template %d is not valid or is not a template"), InMessage.TemplateId);
    	return;
    }
	
	const int32 PageId = Rundown->AddPageFromTemplate(InMessage.TemplateId);
	if (PageId != FAvaRundownPage::InvalidPageId)
	{
		SendMessage(InContext->GetSender(), InMessage.RequestId, ELogVerbosity::Log, TEXT("Page %d Created from Template %d"), PageId, InMessage.TemplateId);
	}
	else
	{
		SendMessage(InContext->GetSender(), InMessage.RequestId, ELogVerbosity::Error, TEXT("Failed to create a page from Template %d"), InMessage.TemplateId);
	}
}

void FAvaRundownServer::HandleDeletePage(const FAvaRundownDeletePage& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext)
{
	UAvaRundown* Rundown = GetOrLoadRundownForEdit(InContext->GetSender(), InMessage.RequestId, InMessage.Rundown);
	if (!Rundown)
	{
		return;
	}

	const FAvaRundownPage& Page = Rundown->GetPage(InMessage.PageId);
	if (!Page.IsValidPage())
	{
		LogAndSendMessage(InContext->GetSender(), InMessage.RequestId, ELogVerbosity::Error, TEXT("page %d is not valid"), InMessage.PageId);
		return;
	}

	Rundown->RemovePage(InMessage.PageId);
	SendMessage(InContext->GetSender(), InMessage.RequestId, ELogVerbosity::Log, TEXT("Page %d deleted"), InMessage.PageId);
}

void FAvaRundownServer::HandleDeleteTemplate(const FAvaRundownDeleteTemplate& InMessage,
	const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext)
{
	UAvaRundown* Rundown = GetOrLoadRundownForEdit(InContext->GetSender(), InMessage.RequestId, InMessage.Rundown);
	if (!Rundown)
	{
		return;
	}

	const FAvaRundownPage& Page = Rundown->GetPage(InMessage.PageId);
	if (!Page.IsValidPage())
	{
		LogAndSendMessage(InContext->GetSender(), InMessage.RequestId, ELogVerbosity::Error, TEXT("page %d is not valid"), InMessage.PageId);
		return;
	}

	if (!Page.IsTemplate())
	{
		LogAndSendMessage(InContext->GetSender(), InMessage.RequestId, ELogVerbosity::Error, TEXT("page %d is not a template"), InMessage.PageId);
		return;
	}

	if (!Page.GetInstancedIds().IsEmpty())
	{
		LogAndSendMessage(InContext->GetSender(), InMessage.RequestId, ELogVerbosity::Error, TEXT("Template has instanced pages"), InMessage.PageId);
		return;
	}

	Rundown->RemovePage(InMessage.PageId);
	SendMessage(InContext->GetSender(), InMessage.RequestId, ELogVerbosity::Log, TEXT("Page template %d deleted"), InMessage.PageId);
}

void FAvaRundownServer::HandleCreateTemplate(const FAvaRundownCreateTemplate& InMessage,
	const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext)
{
	UAvaRundown* Rundown = GetOrLoadRundownForEdit(InContext->GetSender(), InMessage.RequestId, InMessage.Rundown);
	if (!Rundown)
	{
		return;
	}
	
	Rundown->AddTemplate();
	SendMessage(InContext->GetSender(), InMessage.RequestId, ELogVerbosity::Log, TEXT("Template Created"));
}


void FAvaRundownServer::HandleChangeTemplateBP(const FAvaRundownChangeTemplateBP& InMessage,
	const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext)
{
	UAvaRundown* Rundown = GetOrLoadRundownForEdit(InContext->GetSender(), InMessage.RequestId, InMessage.Rundown);
	if (!Rundown)
	{
		return;
	}

	FAvaRundownPage& Page = Rundown->GetPage(InMessage.TemplateId);
	if (Page.IsValidPage() && Page.IsTemplate())
	{
		if (Page.UpdateAsset(InMessage.AssetPath))
		{
			SendMessage(InContext->GetSender(), InMessage.RequestId, ELogVerbosity::Log, TEXT("Blueprint change of template: %d to %s"), InMessage.TemplateId, *InMessage.AssetPath);
			Rundown->GetOnPagesChanged().Broadcast(Rundown, Page, EAvaRundownPageChanges::Blueprint);
			return;
		}
	}
	LogAndSendMessage(InContext->GetSender(), InMessage.RequestId, ELogVerbosity::Error, TEXT("Blueprint change of template: %d to %s failed."), InMessage.TemplateId, *InMessage.AssetPath);
}

void FAvaRundownServer::HandleGetPageDetails(const FAvaRundownGetPageDetails& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext)
{	
	UAvaRundown* Rundown = GetOrLoadRundownForEdit(InContext->GetSender(), InMessage.RequestId, InMessage.Rundown);
	if (!Rundown)
	{
		return;
	}

	const FAvaRundownPage& Page = Rundown->GetPage(InMessage.PageId);
	if (!Page.IsValidPage())
	{
		LogAndSendMessage(InContext->GetSender(), InMessage.RequestId, ELogVerbosity::Error, TEXT("\"PageDetails\" not available: PageId %d is invalid."), InMessage.PageId);
		return;
	}

	if (Page.GetAssetPath(Rundown).IsNull())
	{
		LogAndSendMessage(InContext->GetSender(), InMessage.RequestId, ELogVerbosity::Error, TEXT("Page has no asset selected"), InMessage.PageId);
		return;
	}
	
	if (InMessage.bLoadRemoteControlPreset)
	{
		FAvaRundownManagedInstanceCache& ManagedInstanceCache = IAvaMediaModule::Get().GetManagedInstanceCache();
		const TSharedPtr<FAvaRundownManagedInstance> ManagedInstance
			= ManagedInstanceCache.GetOrLoadInstance(Page.GetAssetPath(Rundown));
		
		if (ManagedInstance.IsValid())
		{
			RundownEditCommandData.SaveCurrentRemoteControlPresetToPage(true);

			// Applying the controller values can break the WYSIWYG of the editor,
			// in case multiple controllers set the same exposed entity with different values.
			// There is no guaranty that the controller actions are self consistent.
			// To avoid this issue, we apply the controllers first, and then
			// restore the entity values in a second pass.
			
			Page.GetRemoteControlValues().ApplyControllerValuesToRemoteControlPreset(ManagedInstance->GetRemoteControlPreset(), true);
			Page.GetRemoteControlValues().ApplyEntityValuesToRemoteControlPreset(ManagedInstance->GetRemoteControlPreset());

			// Register the RC Preset to Remote Control module to make it available through WebRC.
			FAvaRemoteControlUtils::RegisterRemoteControlPreset(ManagedInstance->GetRemoteControlPreset(), /*bInEnsureUniqueId*/ true);

			// Keep track of what is currently registered.
			RundownEditCommandData.ManagedPageId = InMessage.PageId;
			RundownEditCommandData.ManagedInstance = ManagedInstance;
		}
	}
	
	FAvaRundownPageDetails* ReplyMessage = FMessageEndpoint::MakeMessage<FAvaRundownPageDetails>();
	ReplyMessage->RequestId = InMessage.RequestId;
	ReplyMessage->Rundown = InMessage.Rundown;
	ReplyMessage->PageInfo = UE::AvaRundownServer::Private::GetPageInfo(Rundown, Page);
	ReplyMessage->RemoteControlValues = Page.GetRemoteControlValues();
	if (InMessage.bLoadRemoteControlPreset && RundownEditCommandData.ManagedInstance.IsValid() && RundownEditCommandData.ManagedInstance->GetRemoteControlPreset())
	{
		ReplyMessage->RemoteControlPresetName = RundownEditCommandData.ManagedInstance->GetRemoteControlPreset()->GetPresetName().ToString();
		ReplyMessage->RemoteControlPresetId = RundownEditCommandData.ManagedInstance->GetRemoteControlPreset()->GetPresetId().ToString();
	}
	SendResponse(ReplyMessage, InContext->GetSender());
}

void FAvaRundownServer::HandleChangePageChannel(const FAvaRundownPageChangeChannel& InMessage,
	const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext)
{
	UAvaBroadcast& Broadcast = UAvaBroadcast::Get();
	if (InMessage.ChannelName.IsEmpty())
	{
		LogAndSendMessage(InContext->GetSender(), InMessage.RequestId, ELogVerbosity::Error, TEXT("No Channel Name Provided"));
		return;
	}

	const FName ChannelName(InMessage.ChannelName);
	const FAvaBroadcastOutputChannel& Channel = Broadcast.GetCurrentProfile().GetChannel(ChannelName);
	if (!Channel.IsValidChannel())
	{
		LogAndSendMessage(InContext->GetSender(), InMessage.RequestId, ELogVerbosity::Error, TEXT("%s is not a valid channel"), *ChannelName.ToString());
		return;
	}

	UAvaRundown* Rundown = GetOrLoadRundownForEdit(InContext->GetSender(), InMessage.RequestId, InMessage.Rundown);
	if (!Rundown)
	{
		return;
	}

	FAvaRundownPage& Page = Rundown->GetPage(InMessage.PageId);
	if (!Page.IsValidPage())
	{
		LogAndSendMessage(InContext->GetSender(), InMessage.RequestId, ELogVerbosity::Error, TEXT("PageId %d is invalid."), InMessage.PageId);
		return;
	}

	if (Page.GetChannelName() == ChannelName)
	{
		SendMessage(InContext->GetSender(), InMessage.RequestId, ELogVerbosity::Log, TEXT("Same Channel Selected"), InMessage.PageId);
		return;
	}

	Page.SetChannelName(ChannelName);
	Rundown->GetOnPagesChanged().Broadcast(Rundown, Page, EAvaRundownPageChanges::Channel);
	SendMessage(InContext->GetSender(), InMessage.RequestId, ELogVerbosity::Log, TEXT("Channel changed"));
}

void FAvaRundownServer::HandleUpdatePageFromRCP(const FAvaRundownUpdatePageFromRCP& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext)
{
	// Note that this doesn't save the rundown.
	RundownEditCommandData.SaveCurrentRemoteControlPresetToPage(InMessage.bUnregister);
	if (InMessage.bUnregister)
	{
		RundownEditCommandData.ManagedPageId = FAvaRundownPage::InvalidPageId;
		RundownEditCommandData.ManagedInstance.Reset();
	}
	SendMessage(InContext->GetSender(), InMessage.RequestId, ELogVerbosity::Log, TEXT("\"UpdatePageFromRCP\" Ok."));
}

void FAvaRundownServer::HandlePageAction(const FAvaRundownPageAction& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext)
{
	const FRequestInfo RequestInfo = {InMessage.RequestId, InContext->GetSender()};
	HandlePageActions(RequestInfo, {InMessage.PageId}, false, FName(), InMessage.Action);
}

void FAvaRundownServer::HandlePagePreviewAction(const FAvaRundownPagePreviewAction& InMessage,
	const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext)
{
	const FRequestInfo RequestInfo = {InMessage.RequestId, InContext->GetSender()};
	HandlePageActions(RequestInfo, {InMessage.PageId}, true, FName(InMessage.PreviewChannelName), InMessage.Action);
}

void FAvaRundownServer::HandlePageActions(const FAvaRundownPageActions& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext)
{
	const FRequestInfo RequestInfo = {InMessage.RequestId, InContext->GetSender()};
	HandlePageActions(RequestInfo, InMessage.PageIds, false, FName(), InMessage.Action);
}

void FAvaRundownServer::HandlePagePreviewActions(const FAvaRundownPagePreviewActions& InMessage,
	const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext)
{
	const FRequestInfo RequestInfo = {InMessage.RequestId, InContext->GetSender()};
	HandlePageActions(RequestInfo, InMessage.PageIds, true, FName(InMessage.PreviewChannelName), InMessage.Action);
}

void FAvaRundownServer::HandleGetChannel(const FAvaRundownGetChannel& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext)
{
	const FName ChannelName(InMessage.ChannelName);
	
	const UAvaBroadcast& Broadcast = UAvaBroadcast::Get();
	const FAvaBroadcastOutputChannel& Channel = Broadcast.GetCurrentProfile().GetChannel(ChannelName);

	if (!Channel.IsValidChannel())
	{
		LogAndSendMessage(InContext->GetSender(), InMessage.RequestId, ELogVerbosity::Error, TEXT("\"GetChannel\" Channel \"%s\" not found."), *InMessage.ChannelName);
		return;
	}
	
	FAvaRundownChannelResponse* ReplyMessage = FMessageEndpoint::MakeMessage<FAvaRundownChannelResponse>();
	ReplyMessage->RequestId = InMessage.RequestId;
	ReplyMessage->Channel = UE::AvaRundownServer::Private::SerializeChannel(Channel);
	SendResponse(ReplyMessage, InContext->GetSender());
}

void FAvaRundownServer::HandleGetChannels(const FAvaRundownGetChannels& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext)
{
	UAvaBroadcast& Broadcast = UAvaBroadcast::Get();
	FAvaRundownChannels* ReplyMessage = FMessageEndpoint::MakeMessage<FAvaRundownChannels>();
	ReplyMessage->RequestId = InMessage.RequestId;
	
	const TArray<FAvaBroadcastOutputChannel*>& Channels = Broadcast.GetCurrentProfile().GetChannels();
	ReplyMessage->Channels.Reserve(Channels.Num());

	for (const FAvaBroadcastOutputChannel* OutputChannel : Channels)
	{
		FAvaRundownChannel Channel = UE::AvaRundownServer::Private::SerializeChannel(*OutputChannel);
		ReplyMessage->Channels.Push(MoveTemp(Channel));
	}
	
	SetupBroadcastDelegates(&Broadcast);
	SendResponse(ReplyMessage, InContext->GetSender());
}

void FAvaRundownServer::HandleChannelAction(const FAvaRundownChannelAction& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext)
{
	UAvaBroadcast& Broadcast = UAvaBroadcast::Get();
	if (InMessage.Action == EAvaRundownChannelActions::Start)
	{
		if (InMessage.ChannelName.IsEmpty())
		{
			Broadcast.StartBroadcast();
			SendMessage(InContext->GetSender(), InMessage.RequestId, ELogVerbosity::Log, TEXT("\"ChannelAction\" Ok."));
		}
		else
		{
			const FName ChannelName(InMessage.ChannelName);
			FAvaBroadcastOutputChannel& Channel = Broadcast.GetCurrentProfile().GetChannelMutable(ChannelName);
			if (Channel.IsValidChannel())
			{
				Channel.StartChannelBroadcast();
				SendMessage(InContext->GetSender(), InMessage.RequestId, ELogVerbosity::Log, TEXT("\"ChannelAction\" Ok."));
			}
			else
			{
				LogAndSendMessage(InContext->GetSender(), InMessage.RequestId, ELogVerbosity::Error, TEXT("\"ChannelAction\" Failed. Reason: Invalid Channel \"%s\"."), *InMessage.ChannelName);
			}
		}
	}
	else if (InMessage.Action == EAvaRundownChannelActions::Stop)
	{
		if (InMessage.ChannelName.IsEmpty())
		{
			Broadcast.StopBroadcast();
			SendMessage(InContext->GetSender(), InMessage.RequestId, ELogVerbosity::Log, TEXT("\"ChannelAction\" Ok."));
		}
		else
		{
			const FName ChannelName(InMessage.ChannelName);
			FAvaBroadcastOutputChannel& Channel = Broadcast.GetCurrentProfile().GetChannelMutable(ChannelName);
			if (Channel.IsValidChannel())
			{
				Channel.StopChannelBroadcast();
				SendMessage(InContext->GetSender(), InMessage.RequestId, ELogVerbosity::Log, TEXT("\"ChannelAction\" Ok."));
			}
			else
			{
				LogAndSendMessage(InContext->GetSender(), InMessage.RequestId, ELogVerbosity::Error, TEXT("\"ChannelAction\" Failed. Reason: Invalid Channel \"%s\"."), *InMessage.ChannelName);
			}
		}
	}
	else
	{
		LogAndSendMessage(InContext->GetSender(), InMessage.RequestId, ELogVerbosity::Error, TEXT("\"ChannelAction\" Failed. Reason: Invalid Action (must be \"Start\" or \"Stop\"."));
	}
}

void FAvaRundownServer::HandleGetChannelImage(const FAvaRundownGetChannelImage& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext)
{
	if (InMessage.ChannelName.IsEmpty())
	{
		SendMessage(InContext->GetSender(), InMessage.RequestId, ELogVerbosity::Error, TEXT("\"GetChannelImage\" Failed. Reason: Invalid ChannelName."));
		return;
	}
	
	const FName ChannelName(InMessage.ChannelName);
	const FAvaBroadcastOutputChannel& Channel = UAvaBroadcast::Get().GetCurrentProfile().GetChannel(ChannelName);
	
	if (!Channel.IsValidChannel())
	{
		SendMessage(InContext->GetSender(), InMessage.RequestId, ELogVerbosity::Error, TEXT("\"GetChannelImage\" Failed. Reason: Invalid Channel \"%s\"."), *InMessage.ChannelName);
		return;
	}

	const FRequestInfo RequestInfo = {InMessage.RequestId, InContext->GetSender()};
	
	UTextureRenderTarget2D* ChannelRenderTarget = Channel.GetCurrentRenderTarget(true);
	
	// If the channel's render target is not the desired format, we will need to convert it.
	TSharedPtr<FChannelImage> ChannelImage;
	if (AvailableChannelImages.Num() > 0)
	{
		ChannelImage = AvailableChannelImages.Pop();
	}
	else
	{
		ChannelImage = MakeShared<FChannelImage>();
	}

	if (ChannelRenderTarget->GetFormat() != PF_B8G8R8A8)
	{
		ChannelImage->UpdateRenderTarget(ChannelRenderTarget->SizeX, ChannelRenderTarget->SizeY, PF_B8G8R8A8, ChannelRenderTarget->ClearColor);
	}
	else
	{
		ChannelImage->RenderTarget.Reset(); // No need for conversion.
	}
	
	TWeakPtr<FAvaRundownServer> WeakRundownServer(SharedThis(this));

	// The conversion is done by the GPU in the render thread.
	ENQUEUE_RENDER_COMMAND(FAvaConvertChannelImage)(
		[ChannelRenderTarget, ChannelImage, RequestInfo, WeakRundownServer](FRHICommandListImmediate& RHICmdList)
		{
			FRHITexture* SourceRHI = ChannelRenderTarget->GetResource()->GetTexture2DRHI();
			FRHITexture* ReadbackRHI = SourceRHI;

			// Convert if needed.
			if (ChannelImage->RenderTarget.IsValid())
			{
				FRHITexture* DestinationRHI = ChannelImage->RenderTarget->GetResource()->GetTexture2DRHI();
				UE::AvaBroadcastRenderTargetMediaUtils::CopyTexture(RHICmdList, SourceRHI, DestinationRHI);
				ReadbackRHI = DestinationRHI;
			}

			// Reading Render Target pixels in the render thread to avoid a flush render commands.
			const FReadSurfaceDataFlags ReadDataFlags(RCM_UNorm, CubeFace_MAX);
			const FIntRect SourceRect = FIntRect(0, 0, ChannelRenderTarget->SizeX, ChannelRenderTarget->SizeY);

			ChannelImage->UpdateRawPixels(SourceRect.Width(), SourceRect.Height());
			
			RHICmdList.ReadSurfaceData(ReadbackRHI, SourceRect, ChannelImage->RawPixels, ReadDataFlags);

			// When the converted render target is ready, we resume the work in the game thread.
			AsyncTask(ENamedThreads::GameThread, [WeakRundownServer, RequestInfo, ChannelImage]()
			{
				if (const TSharedPtr<FAvaRundownServer> RundownServer = WeakRundownServer.Pin())
				{
					RundownServer->FinishGetChannelImage(RequestInfo, ChannelImage);
				}
			});
		});
}

void FAvaRundownServer::HandleAddChannelDevice(const FAvaRundownAddChannelDevice& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext)
{
	UAvaBroadcast& Broadcast = UAvaBroadcast::Get();
	if (InMessage.ChannelName.IsEmpty() || InMessage.MediaOutputName.IsEmpty())
	{
		LogAndSendMessage(InContext->GetSender(), InMessage.RequestId, ELogVerbosity::Error, TEXT("\"AddChannelDevice\" Failed. Reason: One or more Empty Parameters."));
		return;
	}

	const FName ChannelName(InMessage.ChannelName);
	const FAvaBroadcastOutputChannel& OutputChannel = Broadcast.GetCurrentProfile().GetChannel(ChannelName);
	if (!OutputChannel.IsValidChannel())
	{
		LogAndSendMessage(InContext->GetSender(), InMessage.RequestId, ELogVerbosity::Error, TEXT("\"AddChannelDevice\" Failed. Reason: Invalid Channel \"%s\"."), *InMessage.ChannelName);
		return;    
	}

	/*
		We're essentially replicating the UI editor here. The editor:
		1. Builds an output tree
		2. Allows drag-and-drop of output/devices to a channel
		3. AddMediaOutputToChannel() is called

		We don't have immediate drag-and-drop information here, since this is called externally, so we'll rebuild a tree,
		and recursively search for a match, and then issue the same AddMediaOutputToChannel call the editor UI would've called.

		This won't be called frequently, so it's equivalent to an end-user opening up and adding a device to a channel via
		the broadcast window (tree rebuild -> drag and drop item)
	*/

	const FAvaOutputTreeItemPtr OutputDevices = MakeShared<FAvaBroadcastOutputRootItem>();
	FAvaBroadcastOutputTreeItem::RefreshTree(OutputDevices);
	FAvaOutputTreeItemPtr TreeItem = UE::AvaRundownServer::Private::RecursiveFindOutputTreeItem(OutputDevices, InMessage.MediaOutputName);
	
	if (!TreeItem.IsValid())
	{
		LogAndSendMessage(InContext->GetSender(), InMessage.RequestId, ELogVerbosity::Error, TEXT("\"AddChannelDevice\" Failed. Reason: Invalid Device \"%s\"."), *InMessage.MediaOutputName);
		return;
	}

	const FAvaBroadcastMediaOutputInfo OutputInfo;
	const UMediaOutput* OutputDevice = TreeItem->AddMediaOutputToChannel(OutputChannel.GetChannelName(), OutputInfo);
	Broadcast.SaveBroadcast();

	LogAndSendMessage(InContext->GetSender(), InMessage.RequestId, ELogVerbosity::Log, TEXT("\"AddChannelDevice\" successfully added device \"%s\""), *OutputDevice->GetFName().ToString());
}

void FAvaRundownServer::HandleEditChannelDevice(const FAvaRundownEditChannelDevice& InMessage,
	const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext)
{
	UAvaBroadcast& Broadcast = UAvaBroadcast::Get();
	if (InMessage.ChannelName.IsEmpty() || InMessage.MediaOutputName.IsEmpty())
	{
		LogAndSendMessage(InContext->GetSender(), InMessage.RequestId, ELogVerbosity::Error, TEXT("\"EditChannelDevice\" Failed. Reason: One or more Empty Parameters."));
		return;
	}

	const FName ChannelName(InMessage.ChannelName);
	const FAvaBroadcastOutputChannel& OutputChannel = Broadcast.GetCurrentProfile().GetChannel(ChannelName);
	if (!OutputChannel.IsValidChannel())
	{
		LogAndSendMessage(InContext->GetSender(), InMessage.RequestId, ELogVerbosity::Error, TEXT("\"EditChannelDevice\" Failed. Reason: Invalid Channel \"%s\"."), *InMessage.ChannelName);
		return;    
	}

	UMediaOutput* MediaOutput = UE::AvaRundownServer::Private::FindChannelMediaOutput(OutputChannel, InMessage.MediaOutputName);
	
	if (!MediaOutput)
	{
		LogAndSendMessage(InContext->GetSender(), InMessage.RequestId, ELogVerbosity::Error, TEXT("\"EditChannelDevice\" Failed. Reason: Invalid Device \"%s\"."), *InMessage.MediaOutputName);
		return;
	}
	
	FAvaRundownOutputDeviceItem DeviceItem;
	DeviceItem.Name = InMessage.MediaOutputName;
	DeviceItem.Data = InMessage.Data;

	FAvaRundownOutputEditorUtils::EditMediaOutput(MediaOutput, DeviceItem.Data);

	Broadcast.SaveBroadcast();
	
	SendMessage(InContext->GetSender(), InMessage.RequestId, ELogVerbosity::Log, TEXT("\"EditChannelDevice\". Successfully edited device \"%s\" on \"%s\""), *InMessage.MediaOutputName, *InMessage.ChannelName); 
}

void FAvaRundownServer::HandleRemoveChannelDevice(const FAvaRundownRemoveChannelDevice& InMessage,
	const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext)
{
	UAvaBroadcast& Broadcast = UAvaBroadcast::Get();
	if (InMessage.ChannelName.IsEmpty() || InMessage.MediaOutputName.IsEmpty())
	{
		LogAndSendMessage(InContext->GetSender(), InMessage.RequestId, ELogVerbosity::Error, TEXT("\"RemoveChannelDevice\" Failed. Reason: One or more Empty Parameters."));
		return;
	}
	const FName ChannelName(InMessage.ChannelName);
	const FAvaBroadcastOutputChannel& OutputChannel = Broadcast.GetCurrentProfile().GetChannel(ChannelName);
	if (!OutputChannel.IsValidChannel())
	{
		LogAndSendMessage(InContext->GetSender(), InMessage.RequestId, ELogVerbosity::Error, TEXT("\"RemoveChannelDevice\" Failed. Reason: Invalid Channel \"%s\"."), *InMessage.ChannelName);
		return;    
	}

	UMediaOutput* MediaOutput = UE::AvaRundownServer::Private::FindChannelMediaOutput(OutputChannel, InMessage.MediaOutputName);
	
	if (!MediaOutput)
	{
		LogAndSendMessage(InContext->GetSender(), InMessage.RequestId, ELogVerbosity::Error, TEXT("\"RemoveChannelDevice\" Failed. Reason: Invalid Device \"%s\"."), *InMessage.MediaOutputName);
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("RemoveMediaOutput", "Remove Media Output"));
	
	Broadcast.Modify();

	const int32 RemovedCount = Broadcast.GetCurrentProfile().RemoveChannelMediaOutputs(ChannelName, TArray{MediaOutput});

	if (RemovedCount == 0)
	{
		Transaction.Cancel();
		LogAndSendMessage(InContext->GetSender(), InMessage.RequestId, ELogVerbosity::Error, TEXT("\"RemoveChannelDevice\" Didn't remove device."));
		return;
	}

	Broadcast.SaveBroadcast();
	LogAndSendMessage(InContext->GetSender(), InMessage.RequestId, ELogVerbosity::Log, TEXT("\"RemoveChannelDevice\" Removed Device \"%s\""), *InMessage.MediaOutputName);
}

void FAvaRundownServer::HandleGetDevices(const FAvaRundownGetDevices& InMessage,
	const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext)
{
	FAvaRundownDevicesList* ReplyMessage = FMessageEndpoint::MakeMessage<FAvaRundownDevicesList>();
	ReplyMessage->RequestId = InMessage.RequestId;
	
	const FAvaOutputTreeItemPtr OutputDevices = MakeShared<FAvaBroadcastOutputRootItem>();
	FAvaBroadcastOutputTreeItem::RefreshTree(OutputDevices);
	// OutputDevices here aren't literally a physical device, just a construct representing
	// output. This convention was pulled from the SAvaBroadcastOutputDevices->RefreshDevices() call
	for (const TSharedPtr<IAvaBroadcastOutputTreeItem>& ClassItem : OutputDevices->GetChildren())
	{
		if (const FAvaBroadcastOutputClassItem* AvaOutputClassItem = ClassItem->CastTo<FAvaBroadcastOutputClassItem>())
		{
			FAvaRundownOutputClassItem OutputClassItem;
			OutputClassItem.Name = ClassItem->GetDisplayName().ToString();

			const TArray<FAvaOutputTreeItemPtr>& Children = AvaOutputClassItem->GetChildren();
			if (Children.Num() > 0)
			{
				for (const TSharedPtr<IAvaBroadcastOutputTreeItem>& OutputDeviceItem : Children)
				{
					if (OutputDeviceItem->IsA<FAvaBroadcastOutputDeviceItem>())
					{
						FAvaRundownOutputDeviceItem DeviceItem;
						DeviceItem.Name = OutputDeviceItem->GetDisplayName().ToString();
						// Intentionally leaving DeviceItem.Data blank, as it's not usable data by itself
						// .Data will be filled out on a GetChannels call, where it becomes usable

						OutputClassItem.Devices.Push(DeviceItem);
					}
				}
			}
			else
			{
				FAvaRundownOutputDeviceItem DeviceItem;
				DeviceItem.Name = OutputClassItem.Name;
				OutputClassItem.Devices.Push(DeviceItem);
			}

			ReplyMessage->DeviceClasses.Push(OutputClassItem);
		}
	}

	SendResponse(ReplyMessage, InContext->GetSender());
}

void FAvaRundownServer::LogAndSendMessage(const FMessageAddress& InSender, int32 InRequestId, ELogVerbosity::Type InVerbosity, const TCHAR* InFormat, ...) const
{
	TCHAR TempString[1024];
	va_list Args;
	va_start(Args, InFormat);
	FCString::GetVarArgs(TempString, UE_ARRAY_COUNT(TempString), InFormat, Args);
	va_end(Args);

	// The UE_LOG macro adds ELogVerbosity:: to the verbosity, which prevents
	// us from using it with a variable.
	switch (InVerbosity)
	{
	case ELogVerbosity::Type::Log:
		UE_LOG(LogAvaRundownServer, Log, TEXT("%s"), TempString);
		break;
	case ELogVerbosity::Type::Display:
		UE_LOG(LogAvaRundownServer, Display, TEXT("%s"), TempString);
		break;
	case ELogVerbosity::Type::Warning:
		UE_LOG(LogAvaRundownServer, Warning, TEXT("%s"), TempString);
		break;
	case ELogVerbosity::Type::Error:
		UE_LOG(LogAvaRundownServer, Error, TEXT("%s"), TempString);
		break;
	default:
		UE_LOG(LogAvaRundownServer, Log, TEXT("%s"), TempString);
		break;
	}
	
	// Send the error message to the client.
	SendMessageImpl(InSender, InRequestId, InVerbosity, TempString);
}

void FAvaRundownServer::SendMessage(const FMessageAddress& InSender, int32 InRequestId, ELogVerbosity::Type InVerbosity, const TCHAR* InFormat, ...) const
{
	TCHAR TempString[1024];
	va_list Args;
	va_start(Args, InFormat);
	FCString::GetVarArgs(TempString, UE_ARRAY_COUNT(TempString), InFormat, Args);
	va_end(Args);

	// Send the error message to the client.
	SendMessageImpl(InSender, InRequestId, InVerbosity, TempString);
}

void FAvaRundownServer::SendMessageImpl(const FMessageAddress& InSender, int32 InRequestId, ELogVerbosity::Type InVerbosity, const TCHAR* InMsg) const
{
	FAvaRundownServerMsg* ErrorMessage = FMessageEndpoint::MakeMessage<FAvaRundownServerMsg>();
	ErrorMessage->RequestId = InRequestId;
	ErrorMessage->Verbosity = ToString(InVerbosity);
	ErrorMessage->Text = InMsg;
	SendResponse(ErrorMessage, InSender);
}

void FAvaRundownServer::RegisterConsoleCommands()
{
	if (ConsoleCommands.Num() != 0)
	{
		return;
	}
		
	ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("MotionDesignRundownServer.Status"),
			TEXT("Display current status of all server info."),
			FConsoleCommandWithArgsDelegate::CreateRaw(this, &FAvaRundownServer::ShowStatusCommand),
			ECVF_Default
			));

}

void FAvaRundownServer::ShowStatusCommand(const TArray<FString>& InArgs)
{
	UE_LOG(LogAvaRundownServer, Display, TEXT("Rundown Server: \"%s\""), *HostName);
	UE_LOG(LogAvaRundownServer, Display, TEXT("- Endpoint Bus Address: \"%s\""), MessageEndpoint.IsValid() ? *MessageEndpoint->GetAddress().ToString() : TEXT("Invalid"));
	UE_LOG(LogAvaRundownServer, Display, TEXT("- Computer: \"%s\""), *HostName);

	for (const TPair<FMessageAddress, TSharedPtr<FClientInfo>>& Client : Clients)
	{
		const FClientInfo& ClientInfo = *Client.Value;
		UE_LOG(LogAvaRundownServer, Display, TEXT("Connected Client: \"%s\""), *ClientInfo.Address.ToString());
		UE_LOG(LogAvaRundownServer, Display, TEXT("   - Api Version: %d"), ClientInfo.ApiVersion);
	}
	
	UE_LOG(LogAvaRundownServer, Display, TEXT("Rundown Caches:"));
	UE_LOG(LogAvaRundownServer, Display, TEXT("- Editing Rundown: \"%s\""), *RundownEditCommandData.CurrentRundownPath.ToString());
	UE_LOG(LogAvaRundownServer, Display, TEXT("- Editing PageId: \"%d\""), RundownEditCommandData.ManagedPageId);
	UE_LOG(LogAvaRundownServer, Display, TEXT("- Playing Rundown: \"%s\""), *RundownPlaybackCommandData.CurrentRundownPath.ToString());

	if (RundownPlaybackCommandData.CurrentRundown.IsValid())
	{
		TArray<int32> PlayingPages = RundownPlaybackCommandData.CurrentRundown->GetPlayingPageIds();
		for (const int32 PlayingPageId : PlayingPages)
		{
			UE_LOG(LogAvaRundownServer, Display, TEXT("- Playing PageId: \"%d\""), PlayingPageId);
		}
		TArray<int32> PreviewingPages = RundownPlaybackCommandData.CurrentRundown->GetPreviewingPageIds();
		for (const int32 PreviewingPageId : PreviewingPages)
		{
			UE_LOG(LogAvaRundownServer, Display, TEXT("- Previewing PageId: \"%d\""), PreviewingPageId);
		}
	}
}

void FAvaRundownServer::FinishGetChannelImage(const FRequestInfo& InRequestInfo, const TSharedPtr<FChannelImage>& InChannelImage)
{
	FAvaRundownChannelImage* ReplyMessage = FMessageEndpoint::MakeMessage<FAvaRundownChannelImage>();
	ReplyMessage->RequestId = InRequestInfo.RequestId;
	FImage Image;
	bool bSuccess = false;

	// Note: replacing FImageUtils::GetRenderTargetImage since we already have the raw pixels.
	{
		constexpr EPixelFormat Format = PF_B8G8R8A8;
		const int32 ImageBytes = CalculateImageBytes(InChannelImage->SizeX, InChannelImage->SizeY, 0, Format);
		Image.RawData.AddUninitialized(ImageBytes);
		FMemory::Memcpy( Image.RawData.GetData(), InChannelImage->RawPixels.GetData(), InChannelImage->RawPixels.Num() * sizeof(FColor) );
		Image.SizeX = InChannelImage->SizeX;
		Image.SizeY = InChannelImage->SizeY;
		Image.NumSlices = 1;
		Image.Format = ERawImageFormat::BGRA8;
		Image.GammaSpace = EGammaSpace::sRGB;
	}

	// TODO: profile this.
	// Options: resize the render target on the gpu prior to reading pixels.
	{
		FImage ResizedImage;
		Image.ResizeTo(ResizedImage, Image.GetWidth() * .25f, Image.GetHeight() * .25f, Image.Format, EGammaSpace::Linear);

		TArray64<uint8> CompressedData;
		if (FImageUtils::CompressImage(CompressedData,TEXT("JPEG"), ResizedImage, 95))
		{
			const uint32 SafeMessageSizeLimit = UE::AvaMediaMessageUtils::GetSafeMessageSizeLimit();
			if (CompressedData.Num() > SafeMessageSizeLimit)
			{
				LogAndSendMessage(InRequestInfo.Sender, InRequestInfo.RequestId, ELogVerbosity::Error,
					TEXT("\"GetChannelImage\" Failed. Reason: (DataSize: %d) is larger that the safe size limit for udp segmenter (%d)."),
					CompressedData.Num(), SafeMessageSizeLimit);
				return;
			}

			ReplyMessage->ImageData.Append(CompressedData.GetData(), CompressedData.GetAllocatedSize());
			bSuccess = true;
		}
	}
	
	if (!bSuccess)
	{
		LogAndSendMessage(InRequestInfo.Sender, InRequestInfo.RequestId, ELogVerbosity::Error,
			TEXT("\"GetChannelImage\" Failed. Reason: Unable to retrieve Channel Image."));
		return;
	}

	SendResponse(ReplyMessage, InRequestInfo.Sender);
	
	// Put the image back in the pool of available images for next request. (or we could abandon it)
	AvailableChannelImages.Add(InChannelImage);
}

void FAvaRundownServer::HandlePageActions(const FRequestInfo& InRequestInfo, const TArray<int32>& InPageIds,
	bool bInIsPreview, FName InPreviewChannelName, EAvaRundownPageActions InAction) const
{
	using namespace UE::AvaRundownServer::Private;
	
	if (!RundownPlaybackCommandData.CurrentRundown.IsValid())
	{
		LogAndSendMessage(InRequestInfo.Sender, InRequestInfo.RequestId, ELogVerbosity::Error,
			TEXT("\"PageAction\" Failed. Reason: no play list currently loaded."));
		return;
	}

	UAvaRundown* Rundown = RundownPlaybackCommandData.CurrentRundown.Get();

	{
		// Validate the pages - the command will be considered a failure (as a whole) if it contains invalid pages.
		FString InvalidPages;
		for (const int32 PageId : InPageIds)
		{
			const FAvaRundownPage& Page = Rundown->GetPage(PageId);
			if (!Page.IsValidPage())
			{
				InvalidPages.Appendf(TEXT("%s%d"), InvalidPages.IsEmpty() ? TEXT("") : TEXT(", "), PageId);
			}
		}

		if (!InvalidPages.IsEmpty())
		{
			LogAndSendMessage(InRequestInfo.Sender, InRequestInfo.RequestId, ELogVerbosity::Error,
				TEXT("\"PageAction\" Failed. Reason: PageIds {%s} are invalid."), *InvalidPages);
			return;
		}
	}

	const FName PreviewChannelName = !InPreviewChannelName.IsNone() ? InPreviewChannelName : UAvaRundown::GetDefaultPreviewChannelName();
	// Todo: support program channel name in command.
	const FName CommandChannelName = bInIsPreview ? InPreviewChannelName : NAME_None; 
	
	bool bSuccess = false;
	FString FailureReason;
	switch (InAction)
	{
		case EAvaRundownPageActions::Load:
			for (const int32 PageId : InPageIds)
			{
				bSuccess |= Rundown->GetPageLoadingManager().RequestLoadPage(PageId, bInIsPreview, PreviewChannelName);
			}
			break;
		case EAvaRundownPageActions::Unload:
			for (const int32 PageId : InPageIds)
			{
				const FAvaRundownPage& Page =  Rundown->GetPage(PageId);
				if (Page.IsValidPage())
				{
					bSuccess |= Rundown->UnloadPage(PageId, (bInIsPreview ? PreviewChannelName : Page.GetChannelName()).ToString());
				}
			}
			break;
		case EAvaRundownPageActions::Play:
			bSuccess = !Rundown->PlayPages(InPageIds, bInIsPreview ? EAvaRundownPagePlayType::PreviewFromStart : EAvaRundownPagePlayType::PlayFromStart, PreviewChannelName).IsEmpty();
			break;
		case EAvaRundownPageActions::PlayNext:
			{
				const int32 NextPageId = FAvaRundownPlaybackUtils::GetPageIdToPlayNext(Rundown, UAvaRundown::InstancePageList, bInIsPreview, PreviewChannelName);
				if (FAvaRundownPlaybackUtils::IsPageIdValid(NextPageId))
				{
					bSuccess = Rundown->PlayPage(NextPageId, bInIsPreview ? EAvaRundownPagePlayType::PreviewFromFrame : EAvaRundownPagePlayType::PlayFromStart);
				}
				break;
			}
		case EAvaRundownPageActions::Stop:
			if (InPageIds.IsEmpty())
			{
				// If the list of pages is empty, we will stop all the playing pages.
				const TArray<int32> PageIds = GetPlayingPages(Rundown, bInIsPreview, CommandChannelName);
				bSuccess = !Rundown->StopPages(PageIds, EAvaRundownPageStopOptions::Default, bInIsPreview).IsEmpty();
			}
			else
			{
				bSuccess = !Rundown->StopPages(InPageIds, EAvaRundownPageStopOptions::Default, bInIsPreview).IsEmpty();
			}
			break;
		case EAvaRundownPageActions::ForceStop:
			if (InPageIds.IsEmpty())
			{
				// If the list of pages is empty, we will stop all the playing pages.
				const TArray<int32> PageIds = GetPlayingPages(Rundown, bInIsPreview, CommandChannelName);
				bSuccess = !Rundown->StopPages(PageIds, EAvaRundownPageStopOptions::ForceNoTransition, bInIsPreview).IsEmpty();
			}
			else
			{
				bSuccess = !Rundown->StopPages(InPageIds, EAvaRundownPageStopOptions::ForceNoTransition, bInIsPreview).IsEmpty();
			}
			break;
		case EAvaRundownPageActions::Continue:
			if (InPageIds.IsEmpty())
			{
				// If the list of pages is empty, we will continue all the playing pages.
				const TArray<int32> PageIds = GetPlayingPages(Rundown, bInIsPreview, CommandChannelName);
				bSuccess = ContinuePages(Rundown, PageIds, bInIsPreview, PreviewChannelName, FailureReason);
			}
			else
			{
				bSuccess = ContinuePages(Rundown, InPageIds, bInIsPreview, PreviewChannelName, FailureReason);
			}
			break;
		case EAvaRundownPageActions::UpdateValues:
			if (InPageIds.IsEmpty())
			{
				// If the list of pages is empty, we will continue all the playing pages.
				const TArray<int32> PageIds = GetPlayingPages(Rundown, bInIsPreview, CommandChannelName);
				bSuccess = UpdatePagesValues(Rundown, PageIds, bInIsPreview, PreviewChannelName);
			}
			else
			{
				bSuccess = UpdatePagesValues(Rundown, InPageIds, bInIsPreview, PreviewChannelName);
			}
			break;
		case EAvaRundownPageActions::TakeToProgram:
			{
				const TArray<int32> PageIds = FAvaRundownPlaybackUtils::GetPagesToTakeToProgram(Rundown, InPageIds, PreviewChannelName);
				Rundown->PlayPages(PageIds, EAvaRundownPagePlayType::PlayFromStart);
			}
			break;
		default:
			FailureReason.Appendf(TEXT("Invalid action. "));
			break;
	}

	const TCHAR* CommandName = bInIsPreview ? TEXT("PagePreviewAction") : TEXT("PageAction");

	// For multi-page commands, we consider a partial success as success.
	// Remote applications are notified of the page status with FAvaRundownPagesStatuses.
	//
	// Todo:
	// For pages that failed to execute the command, the failure reason is not sent
	// to remote applications. Given the more complex status information, we would
	// probably need a response message for this command with additional error information.
	
	if (bSuccess)
	{
		SendMessage(InRequestInfo.Sender, InRequestInfo.RequestId, ELogVerbosity::Log,
			TEXT("\"%s\" Ok."), CommandName);
	}
	else if (!FailureReason.IsEmpty())
	{
		LogAndSendMessage(InRequestInfo.Sender, InRequestInfo.RequestId, ELogVerbosity::Error,
			TEXT("\"%s\" Failed. Reason: %s"), CommandName, *FailureReason);
	}
	else
	{
		LogAndSendMessage(InRequestInfo.Sender, InRequestInfo.RequestId, ELogVerbosity::Error,
			TEXT("\"%s\" Failed."), CommandName);
	}
}

UAvaRundown* FAvaRundownServer::GetOrLoadRundownForEdit(const FMessageAddress& InSender, int32 InRequestId, const FString& InRundownPath)
{
	UAvaRundown* Rundown;
	
	if (!InRundownPath.IsEmpty())
	{
		// If a path is specified, the rundown gets reloaded
		// unless it was already loaded from a previous editing command.
		// This will not affect the currently loaded rundown for playback.
		const FSoftObjectPath NewRundownPath(InRundownPath);
		
		Rundown = RundownEditCommandData.GetOrLoadRundown(NewRundownPath,
			[this](UAvaRundown* InRundown)
			{
				RundownEditCommandData.SaveCurrentRemoteControlPresetToPage(true);
				RundownEditCommandData.RemoveRundownDelegates(this, InRundown);
			},
			[this](UAvaRundown* InRundown)
			{
				RundownEditCommandData.SetupRundownDelegates(this, InRundown);
			});

		if (!Rundown)
		{
			LogAndSendMessage(InSender, InRequestId, ELogVerbosity::Error, TEXT("Failed to load Rundown \"%s\"."), *InRundownPath);
		}
	}
	else
	{
		// If the path is not specified, we assume it is using the previously loaded rundown.
		Rundown = RundownEditCommandData.CurrentRundown.Get();

		// Note: for backward compatibility with QA python script, we allow this command to use the current "playback" rundown as fallback.
		if (!Rundown)
		{
			Rundown = RundownPlaybackCommandData.CurrentRundown.Get();

			// Update the edit data accordingly.
			RundownEditCommandData.CurrentRundown.Reset(Rundown);
			RundownEditCommandData.CurrentRundownPath = RundownPlaybackCommandData.CurrentRundownPath;
		}

		if (!Rundown)
		{
			LogAndSendMessage(InSender, InRequestId, ELogVerbosity::Error, TEXT("No rundown path specified and no rundown currently loaded."));
		}
	}
	return Rundown;
}

void FAvaRundownServer::OnMessageBusNotification(const FMessageBusNotification& InNotification)
{
	// This is called when the websocket client disconnects.
	if (InNotification.NotificationType == EMessageBusNotification::Unregistered)
	{
		TWeakPtr<FAvaRundownServer> ServerWeak = SharedThis(this);
		auto RemoveClient = [ServerWeak, RegistrationAddress = InNotification.RegistrationAddress]()
		{
			if (const TSharedPtr<FAvaRundownServer> Server = ServerWeak.Pin())
			{
				UE_LOG(LogAvaRundownServer, Log, TEXT("Client \"%s\" disconnected."), *RegistrationAddress.ToString());
				Server->Clients.Remove(RegistrationAddress);
				Server->RefreshClientAddresses();
			}
		};

		if (IsInGameThread())
		{
			RemoveClient();
		}
		else
		{
			Async(EAsyncExecution::TaskGraphMainThread, MoveTemp(RemoveClient));
		}
	}
}

void FAvaRundownServer::RefreshClientAddresses()
{
	ClientAddresses.Reset(Clients.Num());
	for (const TPair<FMessageAddress, TSharedPtr<FClientInfo>>& Client : Clients)
	{
		ClientAddresses.Add(Client.Key);
	}
}

UAvaRundown* FAvaRundownServer::FRundownCache::GetOrLoadRundown(const FSoftObjectPath& InRundownPath,
	const FRundownEventFunction InUnloadCurrentRundownFunction,
	const FRundownEventFunction InNewRundownLoadedFunction)
{
	if (CurrentRundownPath != InRundownPath)
	{
		const TStrongObjectPtr<UAvaRundown> NewRundown = UE::AvaRundownServer::Private::LoadRundown(InRundownPath);
		if (NewRundown.IsValid())
		{
			if (CurrentRundown.Get())
			{
				InUnloadCurrentRundownFunction(CurrentRundown.Get());
			}
			InNewRundownLoadedFunction(NewRundown.Get());
			CurrentRundown = NewRundown;
			CurrentRundownPath = InRundownPath;
		}
		else
		{
			return nullptr;
		}
	}
	return CurrentRundown.Get();
}

void FAvaRundownServer::FRundownCache::SetupRundownDelegates(FAvaRundownServer* InRundownServer, UAvaRundown* InRundown)
{
	RemoveRundownDelegates(InRundownServer, InRundown);

	if (!InRundown)
	{
		return;
	}

	FAvaPlaybackManager& Manager = IAvaMediaModule::Get().GetLocalPlaybackManager();
	TWeakObjectPtr<UAvaRundown> RundownWeak(InRundown);
	TWeakPtr<FAvaRundownServer> RundownServerWeak = InRundownServer->AsShared();
	OnPlaybackInstanceStatusChangedDelegateHandle = 
		Manager.OnPlaybackInstanceStatusChanged.AddLambda([RundownWeak, RundownServerWeak](const FAvaPlaybackInstance& InPlaybackInstance)
		{
			UAvaRundown* Rundown = RundownWeak.Get();
			const TSharedPtr<FAvaRundownServer> RundownServer = RundownServerWeak.Pin();
			if (IsValid(Rundown) && RundownServer.IsValid())
			{
				const int32 PageId = UAvaRundownPagePlayer::GetPageIdFromInstanceUserData(InPlaybackInstance.GetInstanceUserData());
				const FAvaRundownPage Page = Rundown->GetPage(PageId);
				if (Page.IsValidPage())
				{
					RundownServer->PageStatusChanged(Rundown, Page);
				}
			}
		});

	InRundown->GetOnPagesChanged().AddRaw(InRundownServer, &FAvaRundownServer::OnPagesChanged);
	InRundown->GetOnInstancedPageListChanged().AddRaw(InRundownServer, &FAvaRundownServer::OnPageListChanged);
	InRundown->GetOnTemplatePageListChanged().AddRaw(InRundownServer, &FAvaRundownServer::OnPageListChanged);
}

void FAvaRundownServer::FRundownCache::RemoveRundownDelegates(const FAvaRundownServer* InRundownServer, UAvaRundown* InRundown) const
{
	FAvaPlaybackManager& Manager = IAvaMediaModule::Get().GetLocalPlaybackManager();
	Manager.OnPlaybackInstanceStatusChanged.Remove(OnPlaybackInstanceStatusChangedDelegateHandle);

	if (InRundown)
	{
		InRundown->GetOnPagesChanged().RemoveAll(InRundownServer);
		InRundown->GetOnInstancedPageListChanged().RemoveAll(InRundownServer);
		InRundown->GetOnTemplatePageListChanged().RemoveAll(InRundownServer);
	}
}

FAvaRundownServer::FRundownEditCommandData::~FRundownEditCommandData()
{
	SaveCurrentRemoteControlPresetToPage(true);
}

void FAvaRundownServer::FRundownEditCommandData::SaveCurrentRemoteControlPresetToPage(bool bInUnregister)
{
	if (!ManagedInstance.IsValid() || !ManagedInstance->GetRemoteControlPreset())
	{
		return;
	}

	// Check if the RCP was registered.
	IRemoteControlModule& RemoteControlModule = IRemoteControlModule::Get();
	const FName CurrentPresetName = ManagedInstance->GetRemoteControlPreset()->GetPresetName();
	const URemoteControlPreset* ResolvedPreset = RemoteControlModule.ResolvePreset(CurrentPresetName);
	if (ResolvedPreset != ManagedInstance->GetRemoteControlPreset())
	{
		return;
	}

	if (bInUnregister)
	{
		// Unregister from RC module.
		RemoteControlModule.UnregisterEmbeddedPreset(CurrentPresetName);
	}

	if (!CurrentRundown.IsValid())
	{
		return;
	}

	// Save the modified values to the page.
	FAvaRundownPage& ManagedPage = CurrentRundown->GetPage(ManagedPageId);
	if (!ManagedPage.IsValidPage())
	{
		return;
	}

	constexpr bool bIsDefault = false;
	FAvaPlayableRemoteControlValues NewValues;
	NewValues.CopyFrom(ManagedInstance->GetRemoteControlPreset(), bIsDefault);

	// UpdateRemoteControlValues does half the job by ensuring that missing values are added and
	// extra values are removed. But it doesn't change existing values.
	EAvaPlayableRemoteControlChanges RemoteControlChanges = ManagedPage.UpdateRemoteControlValues(NewValues, bIsDefault);

	// Modify existing values if different.
	for (const TPair<FGuid, FAvaPlayableRemoteControlValue>& NewValue : NewValues.EntityValues)
	{
		const FAvaPlayableRemoteControlValue* ExistingValue = ManagedPage.GetRemoteControlEntityValue(NewValue.Key);
		
		if (ExistingValue && !NewValue.Value.IsSameValueAs(*ExistingValue))
		{
			ManagedPage.SetRemoteControlEntityValue(NewValue.Key, NewValue.Value);
			RemoteControlChanges |= EAvaPlayableRemoteControlChanges::EntityValues;
		}
	}
	for (const TPair<FGuid, FAvaPlayableRemoteControlValue>& NewValue : NewValues.ControllerValues)
	{
		const FAvaPlayableRemoteControlValue* ExistingValue = ManagedPage.GetRemoteControlControllerValue(NewValue.Key);

		if (ExistingValue && !NewValue.Value.IsSameValueAs(*ExistingValue))
		{
			ManagedPage.SetRemoteControlControllerValue(NewValue.Key, NewValue.Value);
			RemoteControlChanges |= EAvaPlayableRemoteControlChanges::ControllerValues;
		}
	}

	if (RemoteControlChanges != EAvaPlayableRemoteControlChanges::None)
	{
		CurrentRundown->NotifyPageRemoteControlValueChanged(ManagedPageId, RemoteControlChanges);
	}
}

FAvaRundownServer::FRundownPlaybackCommandData::~FRundownPlaybackCommandData()
{
	ClosePlaybackContext();
}

void FAvaRundownServer::FRundownPlaybackCommandData::ClosePlaybackContext()
{
	if (CurrentRundown.IsValid())
	{
		// Stop all playing pages.
		CurrentRundown->ClosePlaybackContext(true);
		CurrentRundown.Reset();
		CurrentRundownPath.Reset();
	}
}

#undef LOCTEXT_NAMESPACE