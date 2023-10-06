// Copyright Epic Games, Inc. All Rights Reserved.

#include "FileDecoder.h"
#include "OpusAudioInfo.h"
#include "VorbisAudioInfo.h"
#include "HAL/PlatformFileManager.h"

FAudioFileReader::FAudioFileReader(const FString& InPath)
{
	QualityInfo = { 0 };

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	FileHandle.Reset(PlatformFile.OpenRead(*InPath));
	if (FileHandle.IsValid())
	{
		int64 FileSize = FileHandle->Size();
		CompressedFile.Reset();
		CompressedFile.AddUninitialized(FileSize);
		FileHandle->Read(CompressedFile.GetData(), FileSize);
		
		Decompressor.Reset(GetNewDecompressorForFile(InPath));

		if (Decompressor.IsValid())
		{
			Decompressor->ReadCompressedInfo(CompressedFile.GetData(), FileSize, &QualityInfo);
		}
		else
		{
			QualityInfo.NumChannels = 0;
			UE_LOG(LogTemp, Error, TEXT("Invalid file extension!"));
		}
	}
	else
	{
		QualityInfo.NumChannels = 0;
		UE_LOG(LogTemp, Error, TEXT("Invalid file %s!"), *InPath);
	}
}

void FAudioFileReader::GetFileInfo(FSoundQualityInfo& OutInfo)
{
	OutInfo = QualityInfo;
}

bool FAudioFileReader::PopAudio(float* OutAudio, int32 NumSamples)
{
	check(FileHandle.IsValid());
	check(Decompressor.IsValid());

	DecompressionBuffer.Reset();
	DecompressionBuffer.AddUninitialized(NumSamples);

	bool bIsFinished = Decompressor->ReadCompressedData((uint8*) DecompressionBuffer.GetData(), false, NumSamples * sizeof(Audio::DefaultUSoundWaveSampleType));

	// Convert to float:
	for (int32 Index = 0; Index < NumSamples; Index++)
	{
		OutAudio[Index] = ((float)DecompressionBuffer[Index]) / 32768.0f;
	}

	return bIsFinished;
}

ICompressedAudioInfo* FAudioFileReader::GetNewDecompressorForFile(const FString& InPath)
{
	FString Extension = GetExtensionForFile(InPath);

	static const FString OpusExtension = TEXT("opus");
	static const FString OggExtension = TEXT("ogg");

#if PLATFORM_SUPPORTS_OPUS_CODEC 
	if (Extension.Equals(OpusExtension))
	{
		return new FOpusAudioInfo();
	}
#endif
#if PLATFORM_SUPPORTS_VORBIS_CODEC 
	if (Extension.Equals(OggExtension))
	{
		return new FVorbisAudioInfo();
	}
#endif

	UE_LOG(LogTemp, Error, TEXT("Unknown extension '%s' for the FAudioFileReader formats supported on this platform."), *Extension);
	return nullptr;
}

FString FAudioFileReader::GetExtensionForFile(const FString& InPath)
{
	int32 Index = INDEX_NONE;
	if (InPath.FindLastChar(TCHAR('.'), Index))
	{
		return InPath.RightChop(Index + 1);
	}
	else
	{
		return FString();
	}
}
