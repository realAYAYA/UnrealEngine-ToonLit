// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Array.h"
#include "Templates/SharedPointer.h"

struct FWebMFrame;

struct WEBMMEDIA_API FWebMAudioTrackInfo
{
	const char* CodecName;
	const uint8* CodecPrivateData;
	size_t CodecPrivateDataSize;
	int32 SampleRate;
	int32 NumOfChannels;
	bool bIsValid;
};

struct WEBMMEDIA_API FWebMVideoTrackInfo
{
	const char* CodecName;
	bool bIsValid;
};

class FMkvFileReader;

class WEBMMEDIA_API FWebMContainer
{
public:
	FWebMContainer();
	virtual ~FWebMContainer();

	bool Open(const FString& File);
	void ReadFrames(FTimespan AmountOfTimeToRead, TArray<TSharedPtr<FWebMFrame>>& AudioFrames, TArray<TSharedPtr<FWebMFrame>>& VideoFrames);
	FWebMAudioTrackInfo GetCurrentAudioTrackInfo() const;
	FWebMVideoTrackInfo GetCurrentVideoTrackInfo() const;

private:
	struct FMkvFileState;
	TUniquePtr<FMkvFileReader> MkvReader;
	TUniquePtr<FMkvFileState> MkvFile;
	FTimespan CurrentTime;
	int32 SelectedAudioTrack;
	int32 SelectedVideoTrack;
	bool bNoMoreToRead;

	void SeekToNextValidBlock();
};
