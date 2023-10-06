// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AVConfig.h"
#include "Video/VideoPacket.h"

#include "SimpleAV.h"

#include "SimpleVideo.generated.h"

UENUM(BlueprintType)
enum class ESimpleVideoCodec : uint8
{
	H264 = 0,
	H265 = 1,
};

USTRUCT(BlueprintType)
struct FSimpleVideoPacket
{
	GENERATED_BODY()

public:
	FVideoPacket RawPacket;
};

UCLASS(Abstract)
class AVCODECSCORERHI_API USimpleVideoHelper : public USimpleAVHelper
{
	GENERATED_BODY()

public:
	static ESimpleVideoCodec GuessCodec(TSharedRef<FAVInstance> const& Instance);

	UFUNCTION(BlueprintCallable, Category = "Video", meta = (WorldContext = "WorldContext"))
	static void ShareRenderTarget2D(class UTextureRenderTarget2D* RenderTarget);
};
