// Copyright Epic Games, Inc. All Rights Reserved.

#include "DecoderInputBackCompat.h"
#include "AudioDevice.h"
#include "AudioDeviceManager.h"
#include "Interfaces/IAudioFormat.h"
#include "AudioDecompress.h"
#include "Sound/SoundWave.h"
#include "DecoderBackCompat.h"

namespace Audio
{	
	PRAGMA_DISABLE_DEPRECATION_WARNINGS

	ICompressedAudioInfo* FBackCompatInput::GetInfo(
		FFormatDescriptorSection* OutDescriptor /*= nullptr*/) const
	{
		if (!OldInfoObject.IsValid())
		{
			TUniquePtr<ICompressedAudioInfo> InfoInstance;
			FName Format = Wave->GetRuntimeFormat();
			IAudioInfoFactory* Factory = IAudioInfoFactoryRegistry::Get().Find(Format);
			if (!ensure(Factory))
			{
				return nullptr;
			}

			InfoInstance.Reset(Factory->Create());
			if (!ensure(InfoInstance.IsValid()))
			{
				return nullptr;
			}

			FSoundQualityInfo Info;
			if (Wave->IsStreaming())
			{
				if (!InfoInstance->StreamCompressedInfo(Wave, &Info))
				{
					return nullptr;
				}
			}
			else
			{
				if (!InfoInstance->ReadCompressedInfo(Wave->GetResourceData(), Wave->GetResourceSize(), &Info))
				{
					return nullptr;
				}
			}

			// Commit the new instance only if we successfully read the info above.
			OldInfoObject.Reset(InfoInstance.Release());

			Desc.NumChannels		= Info.NumChannels;
			Desc.NumFramesPerSec	= Info.SampleRate;
			Desc.NumFrames			= (uint32)((float)Info.Duration * Info.SampleRate);
			Desc.NumBytesPerPacket	= ~0;

			Desc.CodecName			= FBackCompatCodec::GetDetailsStatic().Name;
			Desc.CodecFamilyName	= FBackCompatCodec::GetDetailsStatic().FamilyName;
			Desc.CodecVersion		= FBackCompatCodec::GetDetailsStatic().Version;				
		}

		if (OutDescriptor)
		{
			*OutDescriptor = Desc;
		}

		return OldInfoObject.Get();
	}

	bool FBackCompatInput::FindSection(FEncodedSectionBase& OutSection)
	{
		if (FFormatDescriptorSection::kSectionName == OutSection.SectionName)
		{
			FFormatDescriptorSection& Descriptor = static_cast<FFormatDescriptorSection&>(OutSection);
			return GetInfo(&Descriptor) != nullptr;
		}
		return false;
	}

	bool FBackCompatInput::HasError() const
	{
		return GetInfo() == nullptr;
	}

	int64 FBackCompatInput::Tell() const
	{
		// Not implemented.
		audio_ensure(false);
		return -1;
	}

	bool FBackCompatInput::SeekToTime(const float InSeconds)
	{
		ICompressedAudioInfo* Info = GetInfo();
		if (Info)
		{
			Info->SeekToTime(InSeconds);
			return true;
		}

		return false;
	}

	bool FBackCompatInput::IsEndOfStream() const
	{
		// Not implemented.
		audio_ensure(false);
		return false;
	}

	TArrayView<const uint8> FBackCompatInput::PeekNextPacket(int32 InMaxPacketLength) const
	{
		// Not implemented.
		audio_ensure(false);
		return MakeArrayView<const uint8>(nullptr,0);
	}

	TArrayView<const uint8> FBackCompatInput::PopNextPacket(int32 InPacketSize)
	{
		// Not implemented.
		audio_ensure(false);
		return MakeArrayView<const uint8>(nullptr,0);
	}

	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

