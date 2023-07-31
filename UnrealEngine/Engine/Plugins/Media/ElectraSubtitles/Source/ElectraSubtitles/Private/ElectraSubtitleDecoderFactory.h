// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"


class IElectraSubtitleDecoderFactory
{
public:
	virtual ~IElectraSubtitleDecoderFactory() = default;
	virtual TSharedPtr<IElectraSubtitleDecoder, ESPMode::ThreadSafe> CreateDecoder(const FString& SubtitleCodecName) = 0;
};


class IElectraSubtitleDecoderFactoryRegistry
{
public:
	virtual ~IElectraSubtitleDecoderFactoryRegistry() = default;

	struct FCodecInfo
	{
		FString	CodecName;
		int32 Priority = 0;
	};

	virtual void AddDecoderFactory(const TArray<FCodecInfo>& InCodecInformation, IElectraSubtitleDecoderFactory* InDecoderFactory) = 0;

};

