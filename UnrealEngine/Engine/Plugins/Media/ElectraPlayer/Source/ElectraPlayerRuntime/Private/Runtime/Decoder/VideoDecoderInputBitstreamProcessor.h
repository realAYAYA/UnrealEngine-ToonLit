// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"
#include "Containers/Map.h"
#include "Misc/Variant.h"

class IElectraDecoderVideoOutput;

namespace Electra
{

struct FAccessUnit;
class FParamDict;

class IVideoDecoderInputBitstreamProcessor
{
public:
	class ICodecSpecificMessages
	{
	public:
		virtual ~ICodecSpecificMessages() = default;
	};

	struct FBitstreamInfo
	{
		bool bIsSyncFrame = false;
		bool bIsDiscardable = false;
		TSharedPtr<ICodecSpecificMessages> CodecSpecificMessages;
	};

	enum class EProcessResult
	{
		None,
		CSDChanged
	};

	static TSharedPtr<IVideoDecoderInputBitstreamProcessor, ESPMode::ThreadSafe> Create(const FString& InCodec, const TMap<FString, FVariant>& InDecoderConfigOptions);

	virtual ~IVideoDecoderInputBitstreamProcessor() = default;

	virtual void Clear() = 0;
	virtual EProcessResult ProcessAccessUnitForDecoding(FBitstreamInfo& OutBSI, FAccessUnit* InOutAccessUnit) = 0;

	virtual void SetPropertiesOnOutput(TSharedPtr<IElectraDecoderVideoOutput, ESPMode::ThreadSafe> InDecoderOutput, FParamDict* InOutProperties, const FBitstreamInfo& InFromBSI) = 0;

};

} // namespace Electra
