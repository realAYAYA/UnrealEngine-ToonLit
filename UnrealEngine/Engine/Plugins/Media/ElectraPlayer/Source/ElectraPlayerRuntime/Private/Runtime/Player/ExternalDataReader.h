// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IExternalDataReader.h"
#include "Player/AdaptiveStreamingPlayerResourceRequest.h"


namespace Electra
{

class IPlayerSessionServices;

class FExternalDataReader : public IExternalDataReader
{
public:
	virtual ~FExternalDataReader();
	FExternalDataReader(IPlayerSessionServices* InPlayerSessionService);
	void ReadData(const FReadParams& InReadParam, FElectraExternalDataReadCompleted OutCompletionDelegate) override;
private:
	class FDataRequest : public IAdaptiveStreamingPlayerResourceRequest
	{
	public:
		FDataRequest(const FReadParams& InReadParam, FElectraExternalDataReadCompleted OutCompletionDelegate)
			: ReadParams(InReadParam), CompletionDelegate(MoveTemp(OutCompletionDelegate))
		{}
		virtual ~FDataRequest();
		EPlaybackResourceType GetResourceType() const override
		{ return IAdaptiveStreamingPlayerResourceRequest::EPlaybackResourceType::BinaryData; }
		FString GetResourceURL() const override;
		FBinaryDataParams GetBinaryDataParams() const override;
		void SetPlaybackData(TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> InPlaybackData, int64 InTotalResourceSize) override;
		void SignalDataReady() override;
	private:
		FReadParams ReadParams;
		FElectraExternalDataReadCompleted CompletionDelegate;
		TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> PlaybackData;
		int64 TotalResourceSize = -1;
	};

	IPlayerSessionServices* PlayerSessionService = nullptr;
	FCriticalSection Lock;
	TArray<TSharedPtr<FDataRequest, ESPMode::ThreadSafe>> Requests;
};

} // namespace Electra
