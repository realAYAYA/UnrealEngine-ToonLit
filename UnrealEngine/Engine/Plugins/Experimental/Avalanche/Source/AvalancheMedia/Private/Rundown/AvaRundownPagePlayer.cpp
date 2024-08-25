// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rundown/AvaRundownPagePlayer.h"

#include "AvaMediaSettings.h"
#include "Playable/AvaPlayable.h"
#include "Playback/AvaPlaybackManager.h"
#include "Playback/AvaPlaybackUtils.h"
#include "Rundown/AvaRundown.h"

namespace UE::AvaMedia::Rundown::PagePlayer::Private
{
	static void PushAllAnimations(UAvaPlaybackGraph* InPlaybackObject, const UAvaRundown* InRundown, const FAvaRundownPage& InPage
		, const FString& InChannelName, EAvaPlaybackAnimAction InAnimAction)
	{
		const int32 NumTemplates = InPage.GetNumTemplates(InRundown);
		for (int32 TemplateIndex = 0; TemplateIndex < NumTemplates; ++TemplateIndex)
		{
			// Note: default FAvaPlaybackAnimPlaySettings (with NAME_None) will play all animations.
			InPlaybackObject->PushAnimationCommand(InPage.GetAssetPath(InRundown, TemplateIndex), InChannelName,
				InAnimAction, FAvaPlaybackAnimPlaySettings());
		}
	}

	static void PushCameraCut(UAvaPlaybackGraph* InPlaybackObject, const UAvaRundown* InRundown, const FAvaRundownPage& InPage
		, const FString& InChannelName)
	{
		InPlaybackObject->PushAnimationCommand(InPage.GetAssetPath(InRundown), InChannelName,
			EAvaPlaybackAnimAction::CameraCut, FAvaPlaybackAnimPlaySettings());
	}
}

UAvaRundownPlaybackInstancePlayer::UAvaRundownPlaybackInstancePlayer() = default;
UAvaRundownPlaybackInstancePlayer::~UAvaRundownPlaybackInstancePlayer() = default;

bool UAvaRundownPlaybackInstancePlayer::Load(const UAvaRundownPagePlayer& InPagePlayer, const UAvaRundown* InRundown, const FAvaRundownPage& InPage, int32 InSubPageIndex, const FGuid& InInstanceId)
{
	SourceAssetPath = InPage.GetAssetPath(InRundown, InSubPageIndex);
	TransitionLayer = InPage.GetTransitionLayer(InRundown, InSubPageIndex);
	
	FAvaPlaybackManager& Manager = InRundown->GetPlaybackManager();
	PlaybackInstance = Manager.AcquireOrLoadPlaybackInstance(SourceAssetPath, InPagePlayer.ChannelName);
	Playback = PlaybackInstance ? PlaybackInstance->GetPlayback() : nullptr;

	// If restoring from a remote instance.
	if (InInstanceId.IsValid())
	{
		PlaybackInstance->SetInstanceId(InInstanceId);
	}

	// Setup user instance data to be able to track this page.
	if (PlaybackInstance)
	{
		UAvaRundownPagePlayer::SetInstanceUserDataFromPage(*PlaybackInstance, InPage);
	}

	if (Playback && InPagePlayer.bIsPreview)
	{
		Playback->SetPreviewChannelName(InPagePlayer.ChannelFName);
	}

	return IsLoaded();
}

bool UAvaRundownPlaybackInstancePlayer::IsLoaded() const
{
	return Playback != nullptr;
}

void UAvaRundownPlaybackInstancePlayer::Play(const UAvaRundownPagePlayer& InPagePlayer, const UAvaRundown* InRundown, EAvaRundownPagePlayType InPlayType, bool bInIsUsingTransitionLogic)
{
	using namespace UE::AvaMedia::Rundown::PagePlayer::Private;
	
	if (!Playback || !InRundown)
	{
		return;
	}

	const bool bPlaybackObjectWasPlaying = Playback->IsPlaying();
	
	if (!Playback->IsPlaying())
	{
		Playback->Play();
	}

	const FAvaRundownPage& Page = InRundown->GetPage(InPagePlayer.PageId);

	// When not using transition logic, we start all animations manually.
	if (!bInIsUsingTransitionLogic)
	{
		Playback->PushRemoteControlValues(SourceAssetPath, InPagePlayer.ChannelName, MakeShared<FAvaPlayableRemoteControlValues>(Page.GetRemoteControlValues()));

		// TODO: play type should be transmitted to the transition behavior.
		const EAvaPlaybackAnimAction AnimAction = (InPlayType == EAvaRundownPagePlayType::PreviewFromFrame)
			? EAvaPlaybackAnimAction::PreviewFrame
			: EAvaPlaybackAnimAction::Play;
	
		// Play all the animations with the page's animation settings.
		PushAllAnimations(Playback, InRundown, Page, InPagePlayer.ChannelName, AnimAction);
	}
	
	if (bPlaybackObjectWasPlaying)
	{
		PushCameraCut(Playback, InRundown, Page, InPagePlayer.ChannelName);
	}
}

bool UAvaRundownPlaybackInstancePlayer::IsPlaying() const
{
	return Playback != nullptr && Playback->IsPlaying();
}

bool UAvaRundownPlaybackInstancePlayer::Continue(const FString& InChannelName)
{
	if (Playback && Playback->IsPlaying())
	{
		// Animation command, within this playback, needs channel for now.
		const FAvaPlaybackAnimPlaySettings AnimSettings;	// Note: Leaving the name to None means the action apply to all animations.
		Playback->PushAnimationCommand(SourceAssetPath, InChannelName, EAvaPlaybackAnimAction::Continue, AnimSettings);
		return true;
	}
	return false;
}

bool UAvaRundownPlaybackInstancePlayer::Stop()
{
	if (!Playback)
	{
		return false;
	}
	
	const bool bUnload = !UAvaMediaSettings::Get().bKeepPagesLoaded;
	
	if (Playback->IsPlaying())
	{
		// Propagate the unload options in case this object is playing remote.
		const EAvaPlaybackStopOptions PlaybackStopOptions = bUnload ?
			EAvaPlaybackStopOptions::Default | EAvaPlaybackStopOptions::Unload : EAvaPlaybackStopOptions::Default;
		Playback->Stop(PlaybackStopOptions);
	}

	if (PlaybackInstance)
	{
		// Unload the local object as well.
		if (bUnload)
		{
			PlaybackInstance->Unload();
		}
		else
		{
			PlaybackInstance->Recycle();
		}
	}

	Playback = nullptr;
	PlaybackInstance.Reset();
	return true;
}

bool UAvaRundownPlaybackInstancePlayer::HasPlayable(const UAvaPlayable* InPlayable) const
{
	return Playback && Playback->HasPlayable(InPlayable);
}

UAvaPlayable* UAvaRundownPlaybackInstancePlayer::GetFirstPlayable() const
{
	return Playback ? Playback->GetFirstPlayable() : nullptr;
}

UAvaRundownPagePlayer* UAvaRundownPlaybackInstancePlayer::GetPagePlayer() const
{
	return Cast<UAvaRundownPagePlayer>(GetOuter());
}

void UAvaRundownPlaybackInstancePlayer::SetPagePlayer(UAvaRundownPagePlayer* InPagePlayer)
{
	LowLevelRename(GetFName(), InPagePlayer);
}

UAvaRundownPagePlayer::UAvaRundownPagePlayer()
{
	UAvaPlayable::OnSequenceEvent().AddUObject(this, &UAvaRundownPagePlayer::HandleOnPlayableSequenceEvent);
}

UAvaRundownPagePlayer::~UAvaRundownPagePlayer()
{
	UAvaPlayable::OnSequenceEvent().RemoveAll(this);
}

bool UAvaRundownPagePlayer::Initialize(UAvaRundown* InRundown, const FAvaRundownPage& InPage, bool bInIsPreview, const FName& InPreviewChannel)
{
	if (!InRundown)
	{
		UE_LOG(LogAvaRundown, Error, TEXT("UAvaRundownPagePlayer::Initialize: Invalid rundown."));
		return false;
	}

	if (!InPage.IsValidPage())
	{
		UE_LOG(LogAvaRundown, Error, TEXT("UAvaRundownPagePlayer::Initialize: Invalid page."));
		return false;
	}
	
	checkf(InstancePlayers.IsEmpty(), TEXT("Can't initialize a page player if already loaded or playing."));

	RundownWeak = InRundown;
	PageId = InPage.GetPageId();
	bIsPreview = bInIsPreview;
	ChannelFName = bIsPreview ? InPreviewChannel : InPage.GetChannelName();
	ChannelName = ChannelFName.ToString();
	return true;
}

bool UAvaRundownPagePlayer::InitializeAndLoad(UAvaRundown* InRundown, const FAvaRundownPage& InPage, bool bInIsPreview, const FName& InPreviewChannel)
{
	if (!Initialize(InRundown, InPage, bInIsPreview, InPreviewChannel))
	{
		return false;
	}
	
	const int32 NumTemplates = InPage.GetNumTemplates(InRundown);
	for (int32 SubPageIndex = 0; SubPageIndex < NumTemplates; ++SubPageIndex)
	{
		CreateAndLoadInstancePlayer(InRundown, InPage, SubPageIndex, FGuid());
	}
	return InstancePlayers.Num() > 0;
}

UAvaRundownPlaybackInstancePlayer* UAvaRundownPagePlayer::LoadInstancePlayer(int32 InSubPageIndex, const FGuid& InInstanceId)
{
	const UAvaRundown* Rundown = RundownWeak.Get();
	if (!Rundown)
	{
		UE_LOG(LogAvaRundown, Error, TEXT("UAvaRundownPagePlayer::LoadSubPage: Rundown is no longuer valid."));
		return nullptr;
	}

	const FAvaRundownPage& Page = Rundown->GetPage(PageId);
	if (!Page.IsValidPage())
	{
		UE_LOG(LogAvaRundown, Error, TEXT("UAvaRundownPagePlayer::LoadSubPage: Invalid pageId %d."), PageId);
		return nullptr;
	}

	return CreateAndLoadInstancePlayer(Rundown, Page, InSubPageIndex, InInstanceId);
}

void UAvaRundownPagePlayer::AddInstancePlayer(UAvaRundownPlaybackInstancePlayer* InExistingInstancePlayer)
{
	// Remove from previous player.
	if (UAvaRundownPagePlayer* PreviousPagePlayer = InExistingInstancePlayer->GetPagePlayer())
	{
		PreviousPagePlayer->RemoveInstancePlayer(InExistingInstancePlayer);
	}

	InstancePlayers.Add(InExistingInstancePlayer);
	InExistingInstancePlayer->SetPagePlayer(this);
}

bool UAvaRundownPagePlayer::IsLoaded() const
{
	for (const TObjectPtr<UAvaRundownPlaybackInstancePlayer>& InstancePlayer : InstancePlayers)
	{
		if (InstancePlayer && InstancePlayer->IsLoaded())
		{
			return true;
		}
	}
	return false;
}

bool UAvaRundownPagePlayer::Play(EAvaRundownPagePlayType InPlayType, bool bInIsUsingTransitionLogic)
{
	const UAvaRundown* Rundown = RundownWeak.Get();
	if (!Rundown)
	{
		return false;
	}

	bool bIsPlaying = false;
	for (const TObjectPtr<UAvaRundownPlaybackInstancePlayer>& InstancePlayer : InstancePlayers)
	{
		InstancePlayer->Play(*this, Rundown, InPlayType, bInIsUsingTransitionLogic);
		bIsPlaying |= InstancePlayer->IsPlaying();
	}

	return bIsPlaying;
}

bool UAvaRundownPagePlayer::IsPlaying() const
{
	for (const TObjectPtr<UAvaRundownPlaybackInstancePlayer>& InstancePlayer : InstancePlayers)
	{
		if (InstancePlayer && InstancePlayer->IsPlaying())
		{
			return true;
		}
	}
	return false;
}

bool UAvaRundownPagePlayer::Continue()
{
	bool bSuccess = false;
	for (const TObjectPtr<UAvaRundownPlaybackInstancePlayer>& InstancePlayer : InstancePlayers)
	{
		bSuccess |= InstancePlayer->Continue(ChannelName);
	}
	return bSuccess;
}

bool UAvaRundownPagePlayer::Stop()
{
	bool bSuccess = false;
	for (const TObjectPtr<UAvaRundownPlaybackInstancePlayer>& InstancePlayer : InstancePlayers)
	{
		bSuccess |= InstancePlayer->Stop();
	}
	if (RundownWeak.IsValid())
	{
		RundownWeak->NotifyPageStopped(PageId);
	}
	return bSuccess;
}

int32 UAvaRundownPagePlayer::GetPageIdFromInstanceUserData(const FString& InUserData)
{
	FString PageIdString;
	if (FParse::Value(*InUserData, TEXT("PageId="), PageIdString))
	{
		return FCString::Atoi(*PageIdString);
	}
	return FAvaRundownPage::InvalidPageId;
}

void UAvaRundownPagePlayer::SetInstanceUserDataFromPage(FAvaPlaybackInstance& InPlaybackInstance, const FAvaRundownPage& InPage)
{
	InPlaybackInstance.SetInstanceUserData(FString::Printf(TEXT("PageId=%d"), InPage.GetPageId()));
}

bool UAvaRundownPagePlayer::HasPlayable(const UAvaPlayable* InPlayable) const
{
	for (const TObjectPtr<UAvaRundownPlaybackInstancePlayer>& InstancePlayer : InstancePlayers)
	{
		if (InstancePlayer->HasPlayable(InPlayable))
		{
			return true;
		}
	}
	return false;
}

UAvaRundownPlaybackInstancePlayer* UAvaRundownPagePlayer::FindInstancePlayerForPlayable(const UAvaPlayable* InPlayable) const
{
	for (const TObjectPtr<UAvaRundownPlaybackInstancePlayer>& InstancePlayer : InstancePlayers)
	{
		if (InstancePlayer && InstancePlayer->HasPlayable(InPlayable))
		{
			return InstancePlayer;
		}
	}
	return nullptr;
}

UAvaRundownPlaybackInstancePlayer* UAvaRundownPagePlayer::FindInstancePlayerByInstanceId(const FGuid& InInstanceId) const
{
	for (const TObjectPtr<UAvaRundownPlaybackInstancePlayer>& InstancePlayer : InstancePlayers)
	{
		if (InstancePlayer && InstancePlayer->GetPlaybackInstanceId() == InInstanceId)
		{
			return InstancePlayer;
		}
	}
	return nullptr;
}

UAvaRundownPlaybackInstancePlayer* UAvaRundownPagePlayer::FindInstancePlayerByAssetPath(const FSoftObjectPath& InAssetPath) const
{
	for (const TObjectPtr<UAvaRundownPlaybackInstancePlayer>& InstancePlayer : InstancePlayers)
	{
		if (InstancePlayer && InstancePlayer->SourceAssetPath == InAssetPath)
		{
			return InstancePlayer;
		}
	}
	return nullptr;
}

UAvaRundownPlaybackInstancePlayer* UAvaRundownPagePlayer::CreateAndLoadInstancePlayer(const UAvaRundown* InRundown, const FAvaRundownPage& InPage, int32 InSubPageIndex, const FGuid& InInstanceId)
{
	UAvaRundownPlaybackInstancePlayer* InstancePlayer = NewObject<UAvaRundownPlaybackInstancePlayer>(this);
	if (InstancePlayer->Load(*this, InRundown, InPage, InSubPageIndex, InInstanceId))
	{
		InstancePlayers.Add(InstancePlayer);
		return InstancePlayer;
	}

	return nullptr;
}

void UAvaRundownPagePlayer::RemoveInstancePlayer(UAvaRundownPlaybackInstancePlayer* InInstancePlayer)
{
	InstancePlayers.Remove(InInstancePlayer);
}

void UAvaRundownPagePlayer::HandleOnPlayableSequenceEvent(UAvaPlayable* InPlayable, const FName& SequenceName, EAvaPlayableSequenceEventType InEventType)
{
	// Check that this is the playable for this page player.
	if (!HasPlayable(InPlayable))
	{
		return;
	}
	
	// Notify the rundown.
	if (RundownWeak.IsValid())
	{
		using namespace UE::AvaPlayback::Utils;
		if (InEventType == EAvaPlayableSequenceEventType::Started)
		{
			UE_LOG(LogAvaRundown, Verbose, TEXT("%s Rundown Page %d: Sequence Started \"%s\"."), *GetBriefFrameInfo(), PageId, *SequenceName.ToString());
		}

		if (InEventType == EAvaPlayableSequenceEventType::Finished)
		{
			UE_LOG(LogAvaRundown, Verbose, TEXT("%s Rundown Page %d: Sequence Finished \"%s\"."), *GetBriefFrameInfo(), PageId, *SequenceName.ToString());
			RundownWeak->NotifyPageSequenceFinished(PageId);
		}
	}
}
