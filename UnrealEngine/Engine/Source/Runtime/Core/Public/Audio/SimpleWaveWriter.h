// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"

namespace Audio
{
	class FSimpleWaveWriter
	{
		// Local definition so we don't depend on platform includes.
		enum EFormatType { IEEE_FLOAT = 0x3 }; // WAVE_FORMAT_IEEE_FLOAT
		struct FWaveFormatEx
		{
			uint16	FormatTag;
			uint16	NumChannels;
			uint32	NumSamplesPerSec;
			uint32	AverageBytesPerSec;
			uint16	BlockAlign;
			uint16	NumBitsPerSample;
			uint16	Size;
		};

	public:
		CORE_API FSimpleWaveWriter(TUniquePtr<FArchive>&& InOutputStream, int32 InSampleRate, int32 InNumChannels, bool bInUpdateHeaderAfterEveryWrite);
		CORE_API ~FSimpleWaveWriter();

		CORE_API void Write(TArrayView<const float> InBuffer);

	private:
		void WriteHeader(int32 InSampleRate, int32 InNumChannels);
		void UpdateHeader();

		TUniquePtr<FArchive> OutputStream;
		int32 RiffSizePos = 0;
		int32 DataSizePos = 0;
		bool bUpdateHeaderAfterEveryWrite = false;
	};
}