// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "GeometryCacheCodecBase.h"

#include "GeometryCacheCodecRaw.generated.h"

class UGeometryCacheTrackStreamable;

struct FGeometryCacheCodecRenderStateRaw : FGeometryCacheCodecRenderStateBase
{
	FGeometryCacheCodecRenderStateRaw(const TArray<int32> &SetTopologyRanges) : FGeometryCacheCodecRenderStateBase(SetTopologyRanges) {}
	virtual bool DecodeSingleFrame(FGeometryCacheCodecDecodeArguments &Args) override;
};

UCLASS(hidecategories = Object)
class GEOMETRYCACHE_API UGeometryCacheCodecRaw : public UGeometryCacheCodecBase
{
	GENERATED_UCLASS_BODY()

	virtual bool DecodeSingleFrame(FGeometryCacheCodecDecodeArguments &Args) override;
	virtual FGeometryCacheCodecRenderStateBase *CreateRenderState() override;

#if WITH_EDITORONLY_DATA
	virtual void BeginCoding(TArray<FStreamedGeometryCacheChunk> &AppendChunksTo) override;
	virtual void EndCoding() override;
	virtual void CodeFrame(const FGeometryCacheCodecEncodeArguments& Args) override;
#endif

private:
	UPROPERTY(VisibleAnywhere, Category = GeometryCache)
	int32 DummyProperty;

#if WITH_EDITORONLY_DATA
	struct FEncoderData
	{
		int32 CurrentChunkId; // Index in the AppendChunksTo list of the chunk we are working on
		TArray<FStreamedGeometryCacheChunk> *AppendChunksTo; // Any chunks this codec creates will be appended to this list
	};
	FEncoderData EncoderData;
#endif
};

