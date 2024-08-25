// Copyright Epic Games, Inc. All Rights Reserved.

#include "SoundFileIO/SoundFileIO.h"

#include "CoreMinimal.h"

#include "Async/AsyncWork.h"
#include "AudioMixerDevice.h"
#include "Engine/Engine.h"
#include "EngineGlobals.h"
#include "Misc/FileHelper.h"
#include "Modules/ModuleManager.h"
#include "SoundFileIOManager.h"
#include "SoundFile.h"
#include "SoundFileIOEnums.h"
#include "Stats/Stats.h"


namespace Audio::SoundFileUtils
{
	static void CopyOptionalWavChunks(TSharedPtr<ISoundFileReader>& InSoundDataReader, const int32 InInputFormat, TSharedPtr<ISoundFileWriter>& InSoundFileWriter, const int32 InOutputFormat);

	bool AUDIOMIXER_API InitSoundFileIOManager()
	{
		return Audio::SoundFileIOManagerInit();
	}

	bool AUDIOMIXER_API ShutdownSoundFileIOManager()
	{
		return Audio::SoundFileIOManagerShutdown();
	}

	uint32 AUDIOMIXER_API GetNumSamples(const TArray<uint8>& InAudioData)
	{
		FSoundFileIOManager SoundIOManager;
		TSharedPtr<ISoundFileReader> InputSoundDataReader = SoundIOManager.CreateSoundDataReader();

		ESoundFileError::Type Error = InputSoundDataReader->Init(&InAudioData);
		if (Error != ESoundFileError::Type::NONE)
		{
			return 0;
		}

		TArray<ESoundFileChannelMap::Type> ChannelMap;

		FSoundFileDescription InputDescription;
		InputSoundDataReader->GetDescription(InputDescription, ChannelMap);
		InputSoundDataReader->Release();

		return InputDescription.NumFrames * InputDescription.NumChannels;
	}

	bool AUDIOMIXER_API ConvertAudioToWav(const TArray<uint8>& InAudioData, TArray<uint8>& OutWaveData)
	{
		const FSoundFileConvertFormat ConvertFormat = FSoundFileConvertFormat::CreateDefault();

		FSoundFileIOManager SoundIOManager;
		TSharedPtr<ISoundFileReader> InputSoundDataReader = SoundIOManager.CreateSoundDataReader();
		
		ESoundFileError::Type Error = InputSoundDataReader->Init(&InAudioData);
		if (Error != ESoundFileError::Type::NONE)
		{
			return false;
		}

		TArray<ESoundFileChannelMap::Type> ChannelMap;
		
		FSoundFileDescription InputDescription;
		InputSoundDataReader->GetDescription(InputDescription, ChannelMap);

		FSoundFileDescription NewSoundFileDescription;
		NewSoundFileDescription.NumChannels = InputDescription.NumChannels;
		NewSoundFileDescription.NumFrames = InputDescription.NumFrames;
		NewSoundFileDescription.FormatFlags = ConvertFormat.Format;
		NewSoundFileDescription.SampleRate = InputDescription.SampleRate;
		NewSoundFileDescription.NumSections = InputDescription.NumSections;
		NewSoundFileDescription.bIsSeekable = InputDescription.bIsSeekable;

		TSharedPtr<ISoundFileWriter> SoundFileWriter = SoundIOManager.CreateSoundFileWriter();
		Error = SoundFileWriter->Init(NewSoundFileDescription, ChannelMap, ConvertFormat.EncodingQuality);
		if (Error != ESoundFileError::Type::NONE)
		{
			return false;
		}

		// Copy optional chunks before writing data chunk which libsndfile assumes will be the last chunk
		CopyOptionalWavChunks(InputSoundDataReader, InputDescription.FormatFlags, SoundFileWriter, NewSoundFileDescription.FormatFlags);

		// Create a buffer to do the processing 
		SoundFileCount ProcessBufferSamples = static_cast<SoundFileCount>(1024) * NewSoundFileDescription.NumChannels;
		TArray<float> ProcessBuffer;
		ProcessBuffer.Init(0.0f, ProcessBufferSamples);

		// Find the max value if we've been told to do peak normalization on import
		float MaxValue = 0.0f;
		SoundFileCount InputSamplesRead = 0;
		bool bPerformPeakNormalization = ConvertFormat.bPerformPeakNormalization;
		if (bPerformPeakNormalization)
		{
			Error = InputSoundDataReader->ReadSamples(ProcessBuffer.GetData(), ProcessBufferSamples, InputSamplesRead);
			check(Error == ESoundFileError::Type::NONE);

			while (InputSamplesRead)
			{
				for (SoundFileCount Sample = 0; Sample < InputSamplesRead; ++Sample)
				{
					if (ProcessBuffer[Sample] > FMath::Abs(MaxValue))
					{
						MaxValue = ProcessBuffer[Sample];
					}
				}

				Error = InputSoundDataReader->ReadSamples(ProcessBuffer.GetData(), ProcessBufferSamples, InputSamplesRead);
				check(Error == ESoundFileError::Type::NONE);
			}

			// If this happens, it means we have a totally silent file
			if (MaxValue == 0.0)
			{
				bPerformPeakNormalization = false;
			}

			// Seek the file back to the beginning
			SoundFileCount OutOffset;
			InputSoundDataReader->SeekFrames(0, ESoundFileSeekMode::FROM_START, OutOffset);
		}

		bool SamplesProcessed = true;

		// Read the first block of samples
		Error = InputSoundDataReader->ReadSamples(ProcessBuffer.GetData(), ProcessBufferSamples, InputSamplesRead);
		check(Error == ESoundFileError::Type::NONE);

		// Normalize and clamp the input decoded audio
		if (bPerformPeakNormalization)
		{
			for (int32 Sample = 0; Sample < InputSamplesRead; ++Sample)
			{
				ProcessBuffer[Sample] = FMath::Clamp(ProcessBuffer[Sample] / MaxValue, -1.0f, 1.0f);
			}
		}
		else
		{
			for (int32 Sample = 0; Sample < InputSamplesRead; ++Sample)
			{
				ProcessBuffer[Sample] = FMath::Clamp(ProcessBuffer[Sample], -1.0f, 1.0f);
			}
		}

		SoundFileCount SamplesWritten = 0;

		while (InputSamplesRead == ProcessBuffer.Num())
		{
			Error = SoundFileWriter->WriteSamples((const float*)ProcessBuffer.GetData(), InputSamplesRead, SamplesWritten);
			check(Error == ESoundFileError::Type::NONE);
			check(SamplesWritten == InputSamplesRead);

			// read more samples
			Error = InputSoundDataReader->ReadSamples(ProcessBuffer.GetData(), ProcessBufferSamples, InputSamplesRead);
			check(Error == ESoundFileError::Type::NONE);

			// Normalize and clamp the samples
			if (bPerformPeakNormalization)
			{
				for (int32 Sample = 0; Sample < InputSamplesRead; ++Sample)
				{
					ProcessBuffer[Sample] = FMath::Clamp(ProcessBuffer[Sample] / MaxValue, -1.0f, 1.0f);
				}
			}
			else
			{
				for (int32 Sample = 0; Sample < InputSamplesRead; ++Sample)
				{
					ProcessBuffer[Sample] = FMath::Clamp(ProcessBuffer[Sample], -1.0f, 1.0f);
				}
			}
		}

		// Write final block of samples
		Error = SoundFileWriter->WriteSamples((const float*)ProcessBuffer.GetData(), InputSamplesRead, SamplesWritten);
		check(Error == ESoundFileError::Type::NONE);

		// Release the sound file handles as soon as we finished converting the file
		InputSoundDataReader->Release();
		SoundFileWriter->Release();

		// Get the raw binary data.....
		TArray<uint8>* Data = nullptr;
		SoundFileWriter->GetData(&Data);

		OutWaveData.Init(0, Data->Num());
		FMemory::Memcpy(OutWaveData.GetData(), (const void*)&(*Data)[0], OutWaveData.Num());

		return true;
	}

	void CopyOptionalWavChunks(TSharedPtr<ISoundFileReader>& InSoundDataReader, const int32 InInputFormat, TSharedPtr<ISoundFileWriter>& InSoundFileWriter, const int32 InOutputFormat)
	{
		// libsndfile only supports chunk operations with wave file formats
		if ((InInputFormat & ESoundFileFormat::WAV) && (InOutputFormat & ESoundFileFormat::WAV))
		{
			// Get the optional chunks from the input data
			FSoundFileChunkArray OptionalChunks;
			ESoundFileError::Type Error = InSoundDataReader->GetOptionalChunks(OptionalChunks);
			if (Error != ESoundFileError::Type::NONE)
			{
				UE_LOG(LogAudioMixer, Error, TEXT("Error encountered while reading optional chunk data...skipping"));
			}
			else
			{
				// Copy any chunks found over to the output file
				Error = InSoundFileWriter->WriteOptionalChunks(OptionalChunks);
				if (Error != ESoundFileError::Type::NONE)
				{
					UE_LOG(LogAudioMixer, Error, TEXT("Error encountered while writing optional chunk data...skipping"));
				}
			}
		}
	}
}
