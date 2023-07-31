// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include "SoundFileIOEnums.h"


namespace Audio
{
	using SoundFileCount = int64;

	/**
 * Specifies a sound file description.
 */
	struct FSoundFileDescription
	{
		/** The number of frames (interleaved samples) in the sound file. */
		int64 NumFrames;

		/** The sample rate of the sound file. */
		int32 SampleRate;

		/** The number of channels of the sound file. */
		int32 NumChannels;

		/** The format flags of the sound file. */
		int32 FormatFlags;

		/** The number of sections of the sound file. */
		int32 NumSections;

		/** Whether or not the sound file is seekable. */
		int32 bIsSeekable;
	};

	struct FSoundFileConvertFormat
	{
		/** Desired convert format. */
		int32 Format;

		/** Desired convert sample rate. */
		uint32 SampleRate;

		/** For compression-type target formats that used an encoding quality (0.0 = low, 1.0 = high). */
		double EncodingQuality;

		/** Whether or not to peak-normalize the audio file during import. */
		bool bPerformPeakNormalization;

		/** Creates audio engine's default source format */
		static FSoundFileConvertFormat CreateDefault()
		{
			FSoundFileConvertFormat Default = FSoundFileConvertFormat();
			Default.Format = Audio::ESoundFileFormat::WAV | Audio::ESoundFileFormat::PCM_SIGNED_16;
			Default.SampleRate = 48000;
			Default.EncodingQuality = 1.0;
			Default.bPerformPeakNormalization = false;

			return MoveTemp(Default);
		}
	};

	/**
	 * ISoundFile
	 */
	class ISoundFile
	{
	public:
		virtual ~ISoundFile() {}
		virtual ESoundFileError::Type GetState(ESoundFileState::Type& OutState) const = 0;
		virtual ESoundFileError::Type GetError() const = 0;
		virtual ESoundFileError::Type GetId(uint32& OutId) const = 0;
		virtual ESoundFileError::Type GetPath(FName& OutPath) const = 0;
		virtual ESoundFileError::Type GetBulkData(TArray<uint8>** OutData) const = 0;
		virtual ESoundFileError::Type GetDataSize(int32& DataSize) const = 0;
		virtual ESoundFileError::Type GetDescription(FSoundFileDescription& OutDescription) const = 0;
		virtual ESoundFileError::Type GetChannelMap(TArray<ESoundFileChannelMap::Type>& OutChannelMap) const = 0;
		virtual ESoundFileError::Type IsStreamed(bool& bOutIsStreamed) const = 0;
	};

	class ISoundFileReader
	{
	public:
		virtual ~ISoundFileReader() {}

		virtual ESoundFileError::Type Init(TSharedPtr<ISoundFile> InSoundFileData, bool bIsStreamed) = 0;
		virtual ESoundFileError::Type Init(const TArray<uint8>* InData) = 0;
		virtual ESoundFileError::Type Release() = 0;
		virtual ESoundFileError::Type SeekFrames(SoundFileCount Offset, ESoundFileSeekMode::Type SeekMode, SoundFileCount& OutOffset) = 0;
		virtual ESoundFileError::Type ReadFrames(float* DataPtr, SoundFileCount NumFrames, SoundFileCount& OutNumFramesRead) = 0;
		virtual ESoundFileError::Type ReadFrames(double* DataPtr, SoundFileCount NumFrames, SoundFileCount& OutNumFramesRead) = 0;
		virtual ESoundFileError::Type ReadSamples(float* DataPtr, SoundFileCount NumSamples, SoundFileCount& OutNumSamplesRead) = 0;
		virtual ESoundFileError::Type ReadSamples(double* DataPtr, SoundFileCount NumSamples, SoundFileCount& OutNumSamplesRead) = 0;
		virtual ESoundFileError::Type GetDescription(FSoundFileDescription& OutputDescription, TArray<ESoundFileChannelMap::Type>& OutChannelMap) = 0;
	};

	class ISoundFileWriter
	{
	public:
		virtual ~ISoundFileWriter() {}

		virtual ESoundFileError::Type Init(const FSoundFileDescription& FileDescription, const TArray<ESoundFileChannelMap::Type>& InChannelMap, double EncodingQuality) = 0;
		virtual ESoundFileError::Type Release() = 0;
		virtual ESoundFileError::Type SeekFrames(SoundFileCount Offset, ESoundFileSeekMode::Type SeekMode, SoundFileCount& OutOffset) = 0;
		virtual ESoundFileError::Type WriteFrames(const float* Data, SoundFileCount NumFrames, SoundFileCount& OutNumFramesWritten) = 0;
		virtual ESoundFileError::Type WriteFrames(const double* Data, SoundFileCount NumFrames, SoundFileCount& OutNumFramesWritten) = 0;
		virtual ESoundFileError::Type WriteSamples(const float* DataPtr, SoundFileCount NumSamples, SoundFileCount& OutNumSampleWritten) = 0;
		virtual ESoundFileError::Type WriteSamples(const double* DataPtr, SoundFileCount NumSamples, SoundFileCount& OutNumSampleWritten) = 0;
		virtual ESoundFileError::Type GetData(TArray<uint8>** OutData) = 0;
	};
} // namespace Audio