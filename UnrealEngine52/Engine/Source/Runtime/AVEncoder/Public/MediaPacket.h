// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

namespace AVEncoder
{
	struct FVideoConfig
	{
		FString Codec;
		uint32 Width;
		uint32 Height;
		uint32 Framerate;
		uint32 Bitrate;
	};

	struct FAudioConfig
	{
		FString Codec;
		uint32 Samplerate;
		uint32 NumChannels;
		uint32 Bitrate;
	};

	enum class EPacketType
	{
		Audio,
		Video,
		Invalid
	};

	struct FMediaPacket
	{
		EPacketType Type;
		FTimespan Timestamp;
		FTimespan Duration;
		TArray<uint8> Data;

		union
		{
			struct
			{
				bool bKeyFrame;
				int32 Width;
				int32 Height;
				uint32 FrameAvgQP;
				uint32 Framerate;
			} Video;

			struct
			{
			} Audio;
		};

		explicit FMediaPacket(EPacketType TypeIn)
		{
			Type = TypeIn;
			if (Type == EPacketType::Audio)
			{
			}
			else if (Type == EPacketType::Video)
			{
				FMemory::Memzero(Video);
			}
		}

		bool IsVideoKeyFrame() const
		{
			return (Type == EPacketType::Video && Video.bKeyFrame) ? true : false;
		}
	};
}
