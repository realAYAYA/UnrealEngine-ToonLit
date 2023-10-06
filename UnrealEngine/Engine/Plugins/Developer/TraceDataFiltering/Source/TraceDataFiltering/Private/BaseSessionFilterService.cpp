// Copyright Epic Games, Inc. All Rights Reserved.

#include "BaseSessionFilterService.h"
#include "IFilterPreset.h"
#include "TraceServices/Model/Channel.h"
#include "Misc/CoreDelegates.h"

FBaseSessionFilterService::FBaseSessionFilterService(TraceServices::FSessionHandle InHandle, TSharedPtr<const TraceServices::IAnalysisSession> InSession) : Session(InSession), Handle(InHandle)
{
	FCoreDelegates::OnEndFrame.AddRaw(this, &FBaseSessionFilterService::OnApplyChannelChanges);
}

FBaseSessionFilterService::~FBaseSessionFilterService()
{
	FCoreDelegates::OnEndFrame.RemoveAll(this);
}

void FBaseSessionFilterService::GetRootObjects(TArray<FTraceObjectInfo>& OutObjects) const
{
	const TraceServices::IChannelProvider* ChannelProvider = Session->ReadProvider<TraceServices::IChannelProvider>("ChannelProvider");
	if (ChannelProvider)
	{
		const TArray<TraceServices::FChannelEntry>& Channels = ChannelProvider->GetChannels();
		for (int32 ChannelIndex = 0; ChannelIndex < Channels.Num(); ++ChannelIndex)
		{
			FTraceObjectInfo& EventInfo = OutObjects.AddDefaulted_GetRef();
			EventInfo.Name = Channels[ChannelIndex].Name;
			EventInfo.bEnabled = Channels[ChannelIndex].bIsEnabled;
			EventInfo.bReadOnly = Channels[ChannelIndex].bReadOnly;
			EventInfo.Hash = GetTypeHash(EventInfo.Name);
			EventInfo.OwnerHash = 0;
		}
	}
}

void FBaseSessionFilterService::GetChildObjects(uint32 InObjectHash, TArray<FTraceObjectInfo>& OutChildObjects) const
{
	/** TODO, parent/child relationship for Channels */
}

const FDateTime& FBaseSessionFilterService::GetTimestamp()
{
	const TraceServices::IChannelProvider* ChannelProvider = Session->ReadProvider<TraceServices::IChannelProvider>("ChannelProvider");
	if (ChannelProvider)
	{
		TimeStamp = ChannelProvider->GetTimeStamp();
	}

	return TimeStamp;
}

void FBaseSessionFilterService::SetObjectFilterState(const FString& InObjectName, const bool bFilterState)
{
	if (bFilterState)
	{
		FrameDisabledChannels.Remove(InObjectName);
		FrameEnabledChannels.Add(InObjectName);
	}
	else
	{
		FrameEnabledChannels.Remove(InObjectName);
		FrameDisabledChannels.Add(InObjectName);
	}
}

void FBaseSessionFilterService::UpdateFilterPreset(const TSharedPtr<IFilterPreset> Preset, bool IsEnabled)
{
	TArray<FString> Names;
	Preset->GetAllowlistedNames(Names);
	if (IsEnabled)
	{
		FrameEnabledChannels.Append(Names);
	}
	else
	{
		FrameDisabledChannels.Append(Names);
	}
}

void FBaseSessionFilterService::DisableAllChannels()
{
	const TraceServices::IChannelProvider* ChannelProvider = Session->ReadProvider<TraceServices::IChannelProvider>("ChannelProvider");
	if (ChannelProvider)
	{
		const TArray<TraceServices::FChannelEntry>& Channels = ChannelProvider->GetChannels();
		for (int32 ChannelIndex = 0; ChannelIndex < Channels.Num(); ++ChannelIndex)
		{
			SetObjectFilterState(Channels[ChannelIndex].Name, false);
		}
	}
}
