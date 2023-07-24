// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IAudioCodec.h"
#include "UObject/NameTypes.h"
#include "AudioDecompress.h"

class ICompressedAudioInfo; 	// Forward declares.

namespace Audio
{
	struct FBackCompatInput : public IDecoderInput
	{
		FName OldFormatName;
		FSoundWaveProxyPtr Wave;
		mutable FFormatDescriptorSection Desc;
		mutable TUniquePtr<ICompressedAudioInfo> OldInfoObject;

		FBackCompatInput(
			FName InOldFormatName,
			const FSoundWaveProxyPtr& InWave)
			: OldFormatName(InOldFormatName)
			, Wave(InWave)
		{
		}

		bool HasError() const override;
		bool IsEndOfStream() const override;

		ICompressedAudioInfo* GetInfo(
			FFormatDescriptorSection* OutDescriptor = nullptr) const;

		bool FindSection(FEncodedSectionBase& OutSection) override;
		int64 Tell() const override;

		bool SeekToTime(const float InSeconds) override;
		
		TArrayView<const uint8> PeekNextPacket(
			int32 InMaxPacketLength) const override;

		TArrayView<const uint8> PopNextPacket(
			int32 InPacketSize) override;
	};
}
