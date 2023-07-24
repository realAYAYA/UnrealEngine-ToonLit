// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModelProto.h"

class MODELPROTOFILEREADER_API FModelProtoFileReader
{
public:
	/**
	 * It will return nullptr if it could not initialize it.
	 */
	static bool ReadModelProtoFromFile(FModelProto& OutModelProto, const FString& InFilePath);
	static bool ReadModelProtoFromArray(FModelProto& OutModelProto, const TArray<uint8>& InModelReadFromFileInBytes);

	/**
	 * As long as InBinaryWeightsByteSize is bigger than the file size, it won't crash.
	 */
	static bool ReadWeightsFromOtxtBinaryFile(char* OutBinaryWeights, const int32 InBinaryWeightsByteSize, const FString& InFilePath);
};
