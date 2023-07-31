// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ElectraSubtitleDecoder.h"
#include "ElectraSubtitleDecoderFactory.h"

/**
 * WebVTT subtitle decoder (https://www.w3.org/TR/webvtt1/)
 */
class FElectraSubtitleDecoderWVTT : public IElectraSubtitleDecoder
{
public:
	static void RegisterCodecs(IElectraSubtitleDecoderFactoryRegistry& InRegistry);

	FElectraSubtitleDecoderWVTT();

	virtual ~FElectraSubtitleDecoderWVTT();

	//-------------------------------------------------------------------------
	// Methods from IElectraSubtitleDecoder
	//
	virtual bool InitializeStreamWithCSD(const TArray<uint8>& InCSD, const Electra::FParamDict& InAdditionalInfo) override;
	virtual IElectraSubtitleDecoder::FOnSubtitleReceivedDelegate& GetParsedSubtitleReceiveDelegate() override;
	virtual Electra::FTimeValue GetStreamedDeliveryTimeOffset() override;
	virtual void AddStreamedSubtitleData(const TArray<uint8>& InData, Electra::FTimeValue InAbsoluteTimestamp, Electra::FTimeValue InDuration, const Electra::FParamDict& InAdditionalInfo) override;
	virtual void SignalStreamedSubtitleEOD() override;
	virtual void Flush() override;
	virtual void Start() override;
	virtual void Stop() override;
	virtual void UpdatePlaybackPosition(Electra::FTimeValue InAbsolutePosition, Electra::FTimeValue InLocalPosition) override;

private:
	struct FSubtitleEntry
	{
		FString Text;
		TOptional<int32> SourceID;
		TOptional<FString> CurrentTime;
		TOptional<FString> ID;
		TOptional<FString> Settings;
		bool bIsAdditionalCue = false;
	};

	FString RemoveAllTags(const FString& InString);

	FOnSubtitleReceivedDelegate ParsedSubtitleDelegate;
	uint32 NextID = 0;

	uint16				DataReferenceIndex = 0;
	int32				Width = 0;
	int32				Height = 0;
	int32				TranslationX = 0;
	int32				TranslationY = 0;
	uint32				Timescale = 0;

	FString				Configuration;
	FString				Label;
};
