// Copyright Epic Games, Inc. All Rights Reserved.

#include "ExternalDataReader.h"
#include "Player/PlayerSessionServices.h"

namespace Electra
{

FExternalDataReader::FDataRequest::~FDataRequest()
{
}

FString FExternalDataReader::FDataRequest::GetResourceURL() const
{
	return ReadParams.URI;
}

IAdaptiveStreamingPlayerResourceRequest::FBinaryDataParams FExternalDataReader::FDataRequest::GetBinaryDataParams() const
{
	IAdaptiveStreamingPlayerResourceRequest::FBinaryDataParams p;
	p.AbsoluteFileOffset = ReadParams.AbsoluteFileOffset;
	p.NumBytesToRead = ReadParams.NumBytesToRead;
	return p;
}

void FExternalDataReader::FDataRequest::SetPlaybackData(TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> InPlaybackData, int64 InTotalResourceSize)
{
	PlaybackData = MoveTemp(InPlaybackData);
	TotalResourceSize = InTotalResourceSize;
}

void FExternalDataReader::FDataRequest::SignalDataReady()
{
	CompletionDelegate.ExecuteIfBound(MoveTemp(PlaybackData), TotalResourceSize, ReadParams);
}


FExternalDataReader::FExternalDataReader(IPlayerSessionServices* InPlayerSessionService)
	: PlayerSessionService(InPlayerSessionService)
{
}

FExternalDataReader::~FExternalDataReader()
{
	FScopeLock lock(&Lock);
}

void FExternalDataReader::ReadData(const FReadParams& InReadParam, FElectraExternalDataReadCompleted OutCompletionDelegate)
{
	if (!PlayerSessionService)
	{
		return;
	}
	TSharedPtr<IAdaptiveStreamingPlayerResourceProvider, ESPMode::ThreadSafe> rp = PlayerSessionService->GetStaticResourceProvider();
	if (!rp.IsValid())
	{
		return;
	}

	TSharedPtr<FDataRequest, ESPMode::ThreadSafe> dr = MakeShared<FDataRequest, ESPMode::ThreadSafe>(InReadParam, OutCompletionDelegate);
	Lock.Lock();
	Requests.Emplace(dr);
	Lock.Unlock();
	rp->ProvideStaticPlaybackDataForURL(dr);
}

} // namespace Electra
