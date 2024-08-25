// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "NNEDenoiserModelIOMappingData.h"

namespace UE::NNEDenoiser::Private
{
	
struct FResourceInfo
{
	EResourceName Name;
	int32 Channel;
	int32 Frame;
};

class FResourceMapping
{
public:
	void Add(FResourceInfo Info)
	{
		ChannelMapping.Add(MoveTemp(Info));
	}

	FResourceInfo& Add_GetRef(FResourceInfo Info)
	{
		return ChannelMapping.Add_GetRef(MoveTemp(Info));
	}

	const FResourceInfo& GetChecked(int32 Channel) const
	{
		check(Channel >= 0 && Channel < ChannelMapping.Num());

		return ChannelMapping[Channel];
	}

	int32 Num() const
	{
		return ChannelMapping.Num();
	}

	bool HasResource(EResourceName Name) const
	{
		for (const auto& Info: ChannelMapping)
		{
			if (Info.Name == Name)
			{
				return true;
			}
		}
		return false;
	}

	int32 NumFrames(EResourceName Name) const
	{
		int32 MinFrameIndex = 1;
		for (const auto& Info: ChannelMapping)
		{
			if (Info.Name == Name)
			{
				MinFrameIndex = FMath::Min(MinFrameIndex, Info.Frame);
			}
		}

		return 1 - MinFrameIndex;
	}

	TMap<int32, TArray<FIntPoint>> GetChannelMappingPerFrame(EResourceName Name) const
	{
		TMap<int32, TArray<FIntPoint>> Result;
		for (int32 I = 0; I < ChannelMapping.Num(); I++)
		{
			const FResourceInfo& Info = ChannelMapping[I];

			if (Info.Name == Name)
			{
				Result.FindOrAdd(Info.Frame).Emplace(I, Info.Channel);
			}
		}

		return Result;
	}

private:
	TArray<FResourceInfo> ChannelMapping;
};

class FResourceMappingList
{
public:
	void Add(FResourceMapping ResourceMapping)
	{
		InputMapping.Add(MoveTemp(ResourceMapping));
	}

	FResourceMapping& Add_GetRef(FResourceMapping ResourceMapping)
	{
		return InputMapping.Add_GetRef(MoveTemp(ResourceMapping));
	}

	const FResourceMapping& GetChecked(int32 InputIndex) const
	{
		check(InputIndex >= 0 && InputIndex < InputMapping.Num());

		return InputMapping[InputIndex];
	}

	int32 Num() const
	{
		return InputMapping.Num();
	}

	int32 NumChannels(int32 InputIndex) const
	{
		check(InputIndex >= 0 && InputIndex < InputMapping.Num());
		
		return InputMapping[InputIndex].Num();
	}

	int32 NumFrames(EResourceName Name) const
	{
		int32 MaxNumFrames = 0;
		for (const auto& Input : InputMapping)
		{
			const int32 NumFrames = Input.NumFrames(Name);
			if (MaxNumFrames < NumFrames)
			{
				MaxNumFrames = NumFrames;
			}
		}

		return MaxNumFrames;
	}

	bool HasResource(EResourceName Name) const
	{
		for (const auto& Input : InputMapping)
		{
			if (Input.HasResource(Name))
			{
				return true;
			}
		}

		return false;
	}

	bool HasResource(int32 InputIndex, EResourceName Name) const
	{
		check(InputIndex >= 0 && InputIndex < InputMapping.Num());

		return InputMapping[InputIndex].HasResource(Name);
	}

	TMap<int32, TArray<FIntPoint>> GetChannelMappingPerFrame(int32 InputIndex, EResourceName Name) const
	{
		check(InputIndex >= 0 && InputIndex < InputMapping.Num());

		return InputMapping[InputIndex].GetChannelMappingPerFrame(Name);
	}

private:
	TArray<FResourceMapping> InputMapping;
};

} // namespace UE::NNEDenoiser::Private