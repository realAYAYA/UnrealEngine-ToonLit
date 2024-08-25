// Copyright Epic Games, Inc. All Rights Reserved.

#include "AudioInsightsTraceChannelHandle.h"

#include "Misc/ScopeLock.h"
#include "Trace/Trace.h"


namespace UE::Audio::Insights
{
	FTraceChannelHandle::FTraceChannelHandle(FString&& InName, TSharedRef<FTraceChannelManager> InManager)
		: ChannelName(MoveTemp(InName))
		, Manager(InManager)
	{
		Init();
	}

	FTraceChannelHandle::FTraceChannelHandle(const FTraceChannelHandle& Other)
		: ChannelName(Other.ChannelName)
		, Manager(Other.Manager)
	{
		Init();
	}

	FTraceChannelHandle::FTraceChannelHandle(FTraceChannelHandle&& Other)
		: ChannelName(MoveTemp(Other.ChannelName))
		, Manager(MoveTemp(Other.Manager))
	{
	}

	FTraceChannelHandle::~FTraceChannelHandle()
	{
		Reset();
	}

	bool FTraceChannelHandle::operator==(const FTraceChannelHandle& Other) const
	{
		return ChannelName.Compare(Other.ChannelName, ESearchCase::IgnoreCase) == 0 && Manager == Other.Manager;
	}

	bool FTraceChannelHandle::operator!=(const FTraceChannelHandle& Other) const
	{
		return ChannelName.Compare(Other.ChannelName, ESearchCase::IgnoreCase) != 0 || Manager != Other.Manager;
	}

	FTraceChannelHandle& FTraceChannelHandle::operator=(const FTraceChannelHandle& Other)
	{
		if (*this != Other)
		{
			ChannelName = Other.ChannelName;
			Manager = Other.Manager;
			Init();
		}
		return *this;
	}

	FTraceChannelHandle& FTraceChannelHandle::operator=(FTraceChannelHandle&& Other)
	{
		if (*this == Other)
		{
			Reset();
		}

		ChannelName = MoveTemp(Other.ChannelName);
		Manager = Other.Manager;
		return *this;
	}

	void FTraceChannelHandle::Init()
	{
		if (IsValid())
		{
			FScopeLock Lock(&Manager->CritSect);

			if (uint32* RefCount = Manager->ActiveChannels.Find(ChannelName))
			{
				(*RefCount)++;
			}
			else
			{
				Manager->ActiveChannels.Add(ChannelName, 1);
				constexpr bool bIsEnabled = true;
				Trace::ToggleChannel(*ChannelName, bIsEnabled);
			}
		}
	}

	void FTraceChannelHandle::Reset()
	{
		if (IsValid())
		{
			FScopeLock Lock(&Manager->CritSect);

			uint32& RefCount = Manager->ActiveChannels.FindChecked(ChannelName);
			RefCount--;
			if (RefCount == 0)
			{
				constexpr bool bIsEnabled = false;
				Trace::ToggleChannel(*ChannelName, bIsEnabled);
				Manager->ActiveChannels.Remove(ChannelName);
			}
		}
	}

	bool FTraceChannelHandle::IsValid() const
	{
		return !ChannelName.IsEmpty();
	}

	FTraceChannelHandle FTraceChannelManager::CreateHandle(FString&& InName)
	{
		return FTraceChannelHandle(MoveTemp(InName), AsShared());
	}
} // namespace UE::Audio::Insights