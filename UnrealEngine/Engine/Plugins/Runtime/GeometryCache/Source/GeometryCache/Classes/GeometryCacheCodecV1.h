// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "GeometryCacheCodecBase.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/BufferReader.h"

#include "GeometryCacheCodecV1.generated.h"

class UGeometryCacheTrackStreamable;
class ICodecDecoder;
class ICodecEncoder;

struct FGeometryCacheCodecRenderStateV1 : FGeometryCacheCodecRenderStateBase
{
	FGeometryCacheCodecRenderStateV1(const TArray<int32> &SetTopologyRanges); 
	~FGeometryCacheCodecRenderStateV1();	
	
	virtual bool DecodeSingleFrame(FGeometryCacheCodecDecodeArguments &Args) override;
};

UCLASS(hidecategories = Object)
class GEOMETRYCACHE_API UGeometryCacheCodecV1 : public UGeometryCacheCodecBase
{
	GENERATED_UCLASS_BODY()

	virtual ~UGeometryCacheCodecV1();

	virtual bool DecodeSingleFrame(FGeometryCacheCodecDecodeArguments &Args) override;
	virtual bool DecodeBuffer(const uint8* Buffer, uint32 BufferSize, FGeometryCacheMeshData& OutMeshData) override;
	virtual FGeometryCacheCodecRenderStateBase *CreateRenderState() override;

#if WITH_EDITORONLY_DATA
	virtual void InitializeEncoder(float InVertexQuantizationPrecision, int32 InUVQuantizationBitRange);
	virtual void BeginCoding(TArray<FStreamedGeometryCacheChunk> &AppendChunksTo) override;
	virtual void EndCoding() override;
	virtual void CodeFrame(const FGeometryCacheCodecEncodeArguments& Args) override;	
#endif

private:
	ICodecDecoder* Decoder;

#if WITH_EDITORONLY_DATA
	ICodecEncoder* Encoder;

	struct FEncoderData
	{
		int32 CurrentChunkId; // Index in the AppendChunksTo list of the chunk we are working on
		TArray<FStreamedGeometryCacheChunk>* AppendChunksTo; // Any chunks this codec creates will be appended to this list
	};
	FEncoderData EncoderData;
	int32 NextContextId;
#endif
};

