// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModelProto.h"
#include <istream>

class FModelProtoConverter
{
public:
	/**
	 * It creates and fills OutModelProto from the *.onnx file read into InIfstream or TArray<uint8>.
	 * @return False if it could not initialize it, true if successful.
	 */
	static bool ConvertFromONNXProto3Ifstream(FModelProto& OutModelProto, std::istream& InIfstream);
	static bool ConvertFromONNXProto3Array(FModelProto& OutModelProto, const TArray<uint8>& InModelReadFromFileInBytes);

private:
	/**
	 * Auxiliary function to avoid code redundancies.
	 * Exactly one of the 2 input arguments must be a nullptr.
	 */
	static bool ConvertFromONNXProto3(FModelProto& OutModelProto, std::istream* InIfstream, const TArray<uint8>* InModelReadFromFileInBytes);
};
