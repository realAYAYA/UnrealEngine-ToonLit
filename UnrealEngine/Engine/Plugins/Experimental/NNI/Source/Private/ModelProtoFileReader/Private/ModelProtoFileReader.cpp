// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModelProtoFileReader.h"
#include "ModelProtoConverter.h"
#include "ModelProtoFileReaderUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include <fstream>



/* FModelProtoFileReader public functions
 *****************************************************************************/

bool FModelProtoFileReader::ReadModelProtoFromFile(FModelProto& OutModelProto, const FString& InFilePath)
{
	// Sanity check - File exists?
	if (!FPaths::FileExists(InFilePath))
	{
		UE_LOG(LogModelProtoFileReader, Warning, TEXT("FModelProtoFileReader::ReadModelProtoFromFile(): Input file path not found: %s."), *InFilePath);
		return false;
	}

	// Read if ONNX or OTXT file
	const FString Extension = FPaths::GetExtension(InFilePath, /*bIncludeDot*/ false);
	if (Extension.Equals(TEXT("onnx"), ESearchCase::IgnoreCase))
	{
		// Protobuf code
		std::ifstream Ifstream(TCHAR_TO_UTF8(*InFilePath), std::ios_base::binary);
		if (Ifstream.is_open())
		{
			const bool bWasLoaded = FModelProtoConverter::ConvertFromONNXProto3Ifstream(OutModelProto, Ifstream);
			// Close Ifstream and return
			Ifstream.close();
			return bWasLoaded;
		}
		else
		{
			UE_LOG(LogModelProtoFileReader, Warning, TEXT("FModelProtoFileReader::ReadModelProtoFromFile(): Input file path was found but could not be opened: %s."), *InFilePath);
		}
	}
	else if (Extension.Equals(TEXT("otxt"), ESearchCase::IgnoreCase))
	{
		UE_LOG(LogModelProtoFileReader, Display, TEXT("OTXT files are deprecated and they should never be used. Use the original ONNX files instead."));
		// Read file
		FString ModelProtoString;
		FFileHelper::LoadFileToString(ModelProtoString, *InFilePath);
		// Empty file?
		if (ModelProtoString.Len() > 0)
		{
			// Create and fill ModelProto
			const bool bWasProperlyLoaded = OutModelProto.LoadFromString(ModelProtoString);
			if (!bWasProperlyLoaded)
			{
				OutModelProto = FModelProto();
			}
			// Verbose - Display ModelProto
			UE_LOG(LogModelProtoFileReader, Display, TEXT("FModelProto properly loaded? %d"), bWasProperlyLoaded);
			return true;
		}
		else
		{
			UE_LOG(LogModelProtoFileReader, Warning, TEXT("FModelProtoFileReader::ReadModelProtoFromFile(): Input file path was found but empty: %s."), *InFilePath);
		}
	}
	else
	{
		UE_LOG(LogModelProtoFileReader, Warning, TEXT("FModelProtoFileReader::ReadModelProtoFromFile(): Unexpected extension used: \"*.%s\" instead of \"*.otxt\"/\"*.onnx\"."), *Extension);
	}
	return false;
}

bool FModelProtoFileReader::ReadModelProtoFromArray(FModelProto& OutModelProto, const TArray<uint8>& InModelReadFromFileInBytes)
{
	return FModelProtoConverter::ConvertFromONNXProto3Array(OutModelProto, InModelReadFromFileInBytes);
}

bool FModelProtoFileReader::ReadWeightsFromOtxtBinaryFile(char* OutBinaryWeights, const int32 InBinaryWeightsByteSize, const FString& InFilePath)
{
	// Note:
	// This function is not the most efficient because LoadFileToArray requires a TArray<uint8>. The most efficient code would be re-using
	// the code from LoadFileToArray() itself but applied to our custom char pointer.
	// However, all of this code will eventually go away and be replaced by a real ONNX file reader, so no need for that.

	// Sanity check
	if (InFilePath.IsEmpty() || !FPaths::FileExists(InFilePath))
	{
		UE_LOG(LogModelProtoFileReader, Warning, TEXT("FModelProtoFileReader::ReadModelProtoFromFile(): Input file path could not be found or loaded: %s"), *InFilePath);
		return false;
	}
	// Open file from disk
	TArray<uint8> BinaryWeights;
	FFileHelper::LoadFileToArray(BinaryWeights, *InFilePath);
	if (BinaryWeights.Num() > 0)
	{
		if (InBinaryWeightsByteSize != BinaryWeights.Num())
		{
			UE_LOG(LogModelProtoFileReader, Warning, TEXT("FModelProtoFileReader::ReadModelProtoFromFile(): Expected size different to actual file size (%d vs. %d). File: %s."),
				InBinaryWeightsByteSize, BinaryWeights.Num(), *InFilePath);
			return false;
		}
		FMemory::Memcpy(OutBinaryWeights, BinaryWeights.GetData(), BinaryWeights.Num());
		return true;
	}
	return false;
}
