// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeFileFormatRaw.h"

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformCrt.h"
#include "HAL/UnrealMemory.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Math/Vector.h"
#include "Misc/FileHelper.h"
#include "Templates/UnrealTemplate.h"

#define LOCTEXT_NAMESPACE "LandscapeEditor.NewLandscape"

TArray<FLandscapeFileResolution> CalculatePossibleRawResolutions(int64 FileSize)
{
	TArray<FLandscapeFileResolution> PossibleResolutions;

	// Find all possible heightmap sizes, between 8 and 8192 width/height
	const int32 MinWidth = FMath::Max(8, (int32)FMath::DivideAndRoundUp(FileSize, (int64)8192));
	const int32 MaxWidth = FMath::TruncToInt(FMath::Sqrt(static_cast<double>(FileSize)));
	for (int32 Width = MinWidth; Width <= MaxWidth; Width++)
	{
		if (FileSize % Width == 0)
		{
			FLandscapeFileResolution ImportResolution;
			ImportResolution.Width = Width;
			ImportResolution.Height = FileSize / Width;
			PossibleResolutions.Add(ImportResolution);
		}
	}

	for (int32 i = PossibleResolutions.Num() - 1; i >= 0; --i)
	{
		FLandscapeFileResolution ImportResolution = PossibleResolutions[i];
		if (ImportResolution.Width != ImportResolution.Height)
		{
			Swap(ImportResolution.Width, ImportResolution.Height);
			PossibleResolutions.Add(ImportResolution);
		}
	}

	return PossibleResolutions;
}

FLandscapeHeightmapFileFormat_Raw::FLandscapeHeightmapFileFormat_Raw()
{
	FileTypeInfo.Description = LOCTEXT("FileFormatRaw_HeightmapDesc", "Heightmap .r16/.raw files");
	FileTypeInfo.Extensions.Add(".r16");
	FileTypeInfo.Extensions.Add(".raw");
	FileTypeInfo.bSupportsExport = true;
}

FLandscapeFileInfo FLandscapeHeightmapFileFormat_Raw::Validate(const TCHAR* HeightmapFilename, FName LayerName) const
{
	FLandscapeFileInfo Result;

	int64 ImportFileSize = IFileManager::Get().FileSize(HeightmapFilename);

	if (ImportFileSize < 0)
	{
		Result.ResultCode = ELandscapeImportResult::Error;
		Result.ErrorMessage = LOCTEXT("Import_HeightmapFileReadError", "Error reading heightmap file");
	}
	else if (ImportFileSize == 0 || ImportFileSize % 2 != 0)
	{
		Result.ResultCode = ELandscapeImportResult::Error;
		Result.ErrorMessage = LOCTEXT("Import_HeightmapFileInvalidSize", "The heightmap file has an invalid size (possibly not 16-bit?)");
	}
	else
	{
		Result.PossibleResolutions = CalculatePossibleRawResolutions(ImportFileSize / 2);

		if (Result.PossibleResolutions.Num() == 0)
		{
			Result.ResultCode = ELandscapeImportResult::Error;
			Result.ErrorMessage = LOCTEXT("Import_HeightmapFileInvalidSize", "The heightmap file has an invalid size (possibly not 16-bit?)");
		}
	}

	return Result;
}

FLandscapeImportData<uint16> FLandscapeHeightmapFileFormat_Raw::Import(const TCHAR* HeightmapFilename, FName LayerName, FLandscapeFileResolution ExpectedResolution) const
{
	FLandscapeImportData<uint16> Result;

	TArray<uint8> TempData;
	if (!FFileHelper::LoadFileToArray(TempData, HeightmapFilename, FILEREAD_Silent))
	{
		Result.ResultCode = ELandscapeImportResult::Error;
		Result.ErrorMessage = LOCTEXT("Import_HeightmapFileReadError", "Error reading heightmap file");
	}
	else if (TempData.Num() != (ExpectedResolution.Width * ExpectedResolution.Height * 2))
	{
		Result.ResultCode = ELandscapeImportResult::Error;
		Result.ErrorMessage = LOCTEXT("Import_HeightmapResolutionMismatch", "The heightmap file's resolution does not match the requested resolution");
	}
	else
	{
		Result.Data.Empty(ExpectedResolution.Width * ExpectedResolution.Height);
		Result.Data.AddUninitialized(ExpectedResolution.Width * ExpectedResolution.Height);
		FMemory::Memcpy(Result.Data.GetData(), TempData.GetData(), ExpectedResolution.Width * ExpectedResolution.Height * 2);
	}

	return Result;
}

void FLandscapeHeightmapFileFormat_Raw::Export(const TCHAR* HeightmapFilename, FName LayerName, TArrayView<const uint16> Data, FLandscapeFileResolution DataResolution, FVector Scale) const
{
	TArray<uint8> TempData;
	TempData.Empty(DataResolution.Width * DataResolution.Height * 2);
	TempData.AddUninitialized(DataResolution.Width * DataResolution.Height * 2);
	FMemory::Memcpy(TempData.GetData(), Data.GetData(), DataResolution.Width * DataResolution.Height * 2);

	FFileHelper::SaveArrayToFile(TempData, HeightmapFilename);
}

//////////////////////////////////////////////////////////////////////////

FLandscapeWeightmapFileFormat_Raw::FLandscapeWeightmapFileFormat_Raw()
{
	FileTypeInfo.Description = LOCTEXT("FileFormatRaw_WeightmapDesc", "Layer .r8/.raw files");
	FileTypeInfo.Extensions.Add(".r8");
	FileTypeInfo.Extensions.Add(".raw");
	FileTypeInfo.bSupportsExport = true;
}

FLandscapeFileInfo FLandscapeWeightmapFileFormat_Raw::Validate(const TCHAR* WeightmapFilename, FName LayerName) const
{
	FLandscapeFileInfo Result;

	int64 ImportFileSize = IFileManager::Get().FileSize(WeightmapFilename);

	if (ImportFileSize < 0)
	{
		Result.ResultCode = ELandscapeImportResult::Error;
		Result.ErrorMessage = LOCTEXT("Import_LayerFileReadError", "Error reading layer file");
	}
	else
	{
		Result.PossibleResolutions = CalculatePossibleRawResolutions(ImportFileSize);

		if (Result.PossibleResolutions.Num() == 0)
		{
			Result.ResultCode = ELandscapeImportResult::Error;
			Result.ErrorMessage = LOCTEXT("Import_WeightmapFileInvalidSize", "The layer file has an invalid size");
		}
	}

	return Result;
}

FLandscapeImportData<uint8> FLandscapeWeightmapFileFormat_Raw::Import(const TCHAR* WeightmapFilename, FName LayerName, FLandscapeFileResolution ExpectedResolution) const
{
	FLandscapeImportData<uint8> Result;

	TArray<uint8> TempData;
	if (!FFileHelper::LoadFileToArray(TempData, WeightmapFilename, FILEREAD_Silent))
	{
		Result.ResultCode = ELandscapeImportResult::Error;
		Result.ErrorMessage = LOCTEXT("Import_LayerFileReadError", "Error reading layer file");
	}
	else if (TempData.Num() != (ExpectedResolution.Width * ExpectedResolution.Height))
	{
		Result.ResultCode = ELandscapeImportResult::Error;
		Result.ErrorMessage = LOCTEXT("Import_LayerResolutionMismatch", "The layer file's resolution does not match the requested resolution");
	}
	else
	{
		Result.Data = MoveTemp(TempData);
	}

	return Result;
}

void FLandscapeWeightmapFileFormat_Raw::Export(const TCHAR* WeightmapFilename, FName LayerName, TArrayView<const uint8> Data, FLandscapeFileResolution DataResolution, FVector Scale) const
{
	FFileHelper::SaveArrayToFile(Data, WeightmapFilename);
}

#undef LOCTEXT_NAMESPACE
