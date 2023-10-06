// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeFileFormatPng.h"

#include "Algo/Transform.h"
#include "Containers/Array.h"
#include "Containers/ContainersFwd.h"
#include "Containers/UnrealString.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformCrt.h"
#include "HAL/UnrealMemory.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Math/Vector.h"
#include "Misc/FileHelper.h"
#include "Modules/ModuleManager.h"
#include "Templates/SharedPointer.h"


#define LOCTEXT_NAMESPACE "LandscapeEditor.NewLandscape"


FLandscapeHeightmapFileFormat_Png::FLandscapeHeightmapFileFormat_Png()
{
	FileTypeInfo.Description = LOCTEXT("FileFormatPng_HeightmapDesc", "Heightmap .png files");
	FileTypeInfo.Extensions.Add(".png");
	FileTypeInfo.bSupportsExport = true;
}

FLandscapeFileInfo FLandscapeHeightmapFileFormat_Png::Validate(const TCHAR* HeightmapFilename, FName LayerName) const
{
	FLandscapeFileInfo Result;

	TArray64<uint8> ImportData;
	if (!FFileHelper::LoadFileToArray(ImportData, HeightmapFilename, FILEREAD_Silent))
	{
		Result.ResultCode = ELandscapeImportResult::Error;
		Result.ErrorMessage = LOCTEXT("Import_HeightmapFileReadError", "Error reading heightmap file");
	}
	else
	{
		IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");
		TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

		if (!ImageWrapper->SetCompressed(ImportData.GetData(), ImportData.Num()) 
			|| ImageWrapper->GetWidth() <= 0 
			|| ImageWrapper->GetHeight() <= 0)
		{
			Result.ResultCode = ELandscapeImportResult::Error;
			Result.ErrorMessage = LOCTEXT("Import_HeightmapFileCorruptPng", "The heightmap file cannot be read (corrupt png?)");
		}
		else if ((ImageWrapper->GetWidth() > MAX_int32)
			|| (ImageWrapper->GetHeight() > MAX_int32)
			|| (ImageWrapper->GetWidth() > MAX_int64 / ImageWrapper->GetHeight()))	// the total pixel count should fit in an int64 to avoid overflow issues
		{
			Result.ResultCode = ELandscapeImportResult::Error;
			Result.ErrorMessage = LOCTEXT("Import_HeightmapFileTooLargePng", "The heightmap file is too large to load");
		}
		else
		{
			FLandscapeFileResolution ImportResolution;
			ImportResolution.Width = static_cast<uint32>(ImageWrapper->GetWidth());
			ImportResolution.Height = static_cast<uint32>(ImageWrapper->GetHeight());
			Result.PossibleResolutions.Add(ImportResolution);

			if (ImageWrapper->GetFormat() != ERGBFormat::Gray)
			{
				Result.ResultCode = ELandscapeImportResult::Warning;
				Result.ErrorMessage = LOCTEXT("Import_HeightmapFileColorPng", "The imported layer is not Grayscale. Results in-Editor will not be consistent with the source file.");
			}
			else if (ImageWrapper->GetBitDepth() != 16)
			{
				Result.ResultCode = ELandscapeImportResult::Warning;
				Result.ErrorMessage = LOCTEXT("Import_HeightmapFileLowBitDepth", "The heightmap file appears to be an 8-bit png, 16-bit is preferred. The import *can* continue, but the result may be lower quality than desired.");
			}
			
		}
	}

	// possible todo: support sCAL (XY scale) and pCAL (Z scale) png chunks for filling out Result.DataScale
	// I don't know if any heightmap generation software uses these or not
	// if we support their import we should make the exporter write them too

	return Result;
}

FLandscapeImportData<uint16> FLandscapeHeightmapFileFormat_Png::Import(const TCHAR* HeightmapFilename, FName LayerName, FLandscapeFileResolution ExpectedResolution) const
{
	FLandscapeImportData<uint16> Result;

	TArray<uint8> TempData;
	if (!FFileHelper::LoadFileToArray(TempData, HeightmapFilename, FILEREAD_Silent))
	{
		Result.ResultCode = ELandscapeImportResult::Error;
		Result.ErrorMessage = LOCTEXT("Import_HeightmapFileReadError", "Error reading heightmap file");
	}
	else
	{
		IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");
		TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

		const int64 Width = ExpectedResolution.Width;
		const int64 Height = ExpectedResolution.Height;

		if (!ImageWrapper->SetCompressed(TempData.GetData(), TempData.Num()))
		{
			Result.ResultCode = ELandscapeImportResult::Error;
			Result.ErrorMessage = LOCTEXT("Import_HeightmapFileCorruptPng", "The heightmap file cannot be read (corrupt png?)");
		}
		else if (ImageWrapper->GetWidth() != Width || ImageWrapper->GetHeight() != Height)
		{
			Result.ResultCode = ELandscapeImportResult::Error;
			Result.ErrorMessage = LOCTEXT("Import_HeightmapResolutionMismatch", "The heightmap file's resolution does not match the requested resolution");
		}
		else if (Width > MAX_int64 / Height) // total pixel count must fit in an int64
		{
			Result.ResultCode = ELandscapeImportResult::Error;
			Result.ErrorMessage = LOCTEXT("Import_HeightmapFileTooLargePng", "The heightmap file is too large to load");
		}
		else
		{
			if (ImageWrapper->GetFormat() != ERGBFormat::Gray)
			{
				Result.ResultCode = ELandscapeImportResult::Warning;
				Result.ErrorMessage = LOCTEXT("Import_HeightmapFileColorPng", "The imported layer is not Grayscale. Results in-Editor will not be consistent with the source file.");
			}
			else if (ImageWrapper->GetBitDepth() != 16)
			{
				Result.ResultCode = ELandscapeImportResult::Warning;
				Result.ErrorMessage = LOCTEXT("Import_HeightmapFileLowBitDepth", "The heightmap file appears to be an 8-bit png, 16-bit is preferred. The import *can* continue, but the result may be lower quality than desired.");
			}

			const int64 TotalPixels = Width * Height;

			TArray64<uint8> RawData;
			if (ImageWrapper->GetBitDepth() <= 8)
			{
				if (!ImageWrapper->GetRaw(ERGBFormat::Gray, 8, RawData)
					|| (RawData.Num() != TotalPixels))
				{
					Result.ResultCode = ELandscapeImportResult::Error;
					Result.ErrorMessage = LOCTEXT("Import_HeightmapFileCorruptPng", "The heightmap file cannot be read (corrupt png?)");
				}
				else
				{
					Result.Data.Empty(TotalPixels);
					Algo::Transform(RawData, Result.Data, [](uint8 Value) { return static_cast<uint16>(Value * 0x101); }); // Expand to 16-bit
				}
			}
			else
			{
				if (!ImageWrapper->GetRaw(ERGBFormat::Gray, 16, RawData)
					|| (RawData.Num()/2 != TotalPixels))
				{
					Result.ResultCode = ELandscapeImportResult::Error;
					Result.ErrorMessage = LOCTEXT("Import_HeightmapFileCorruptPng", "The heightmap file cannot be read (corrupt png?)");
				}
				else
				{
					Result.Data.Empty(TotalPixels);
					Result.Data.AddUninitialized(TotalPixels);
					const uint64 TotalBytes = static_cast<uint64>(TotalPixels) * 2;
					FMemory::Memcpy(Result.Data.GetData(), RawData.GetData(), TotalBytes);
				}
			}
		}
	}

	return Result;
}

void FLandscapeHeightmapFileFormat_Png::Export(const TCHAR* HeightmapFilename, FName LayerName, TArrayView<const uint16> Data, FLandscapeFileResolution DataResolution, FVector Scale) const
{
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

	if (ImageWrapper->SetRaw(Data.GetData(), Data.Num() * 2, DataResolution.Width, DataResolution.Height, ERGBFormat::Gray, 16))
	{
		const TArray64<uint8> TempData = ImageWrapper->GetCompressed();
		FFileHelper::SaveArrayToFile(TempData, HeightmapFilename);
	}
}

//////////////////////////////////////////////////////////////////////////

FLandscapeWeightmapFileFormat_Png::FLandscapeWeightmapFileFormat_Png()
{
	FileTypeInfo.Description = LOCTEXT("FileFormatPng_WeightmapDesc", "Layer .png files");
	FileTypeInfo.Extensions.Add(".png");
	FileTypeInfo.bSupportsExport = true;
}

FLandscapeFileInfo FLandscapeWeightmapFileFormat_Png::Validate(const TCHAR* WeightmapFilename, FName LayerName) const
{
	FLandscapeFileInfo Result;

	TArray64<uint8> ImportData;
	if (!FFileHelper::LoadFileToArray(ImportData, WeightmapFilename, FILEREAD_Silent))
	{
		Result.ResultCode = ELandscapeImportResult::Error;
		Result.ErrorMessage = LOCTEXT("Import_LayerFileReadError", "Error reading layer file");
	}
	else
	{
		IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");
		TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

		if (!ImageWrapper->SetCompressed(ImportData.GetData(), ImportData.Num()))
		{
			Result.ResultCode = ELandscapeImportResult::Error;
			Result.ErrorMessage = LOCTEXT("Import_LayerCorruptPng", "The layer file cannot be read (corrupt png?)");
		}
		else
		{
			if (ImageWrapper->GetFormat() != ERGBFormat::Gray)
			{
				Result.ResultCode = ELandscapeImportResult::Warning;
				Result.ErrorMessage = LOCTEXT("Import_LayerColorPng", "The imported layer is not Grayscale. Results in-Editor will not be consistent with the source file.");
			}
			FLandscapeFileResolution ImportResolution;
			ImportResolution.Width = ImageWrapper->GetWidth();
			ImportResolution.Height = ImageWrapper->GetHeight();
			Result.PossibleResolutions.Add(ImportResolution);
		}
	}

	return Result;
}

FLandscapeImportData<uint8> FLandscapeWeightmapFileFormat_Png::Import(const TCHAR* WeightmapFilename, FName LayerName, FLandscapeFileResolution ExpectedResolution) const
{
	FLandscapeImportData<uint8> Result;

	TArray64<uint8> TempData;
	if (!FFileHelper::LoadFileToArray(TempData, WeightmapFilename, FILEREAD_Silent))
	{
		Result.ResultCode = ELandscapeImportResult::Error;
		Result.ErrorMessage = LOCTEXT("Import_LayerFileReadError", "Error reading layer file");
	}
	else
	{
		IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");
		TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

		if (!ImageWrapper->SetCompressed(TempData.GetData(), TempData.Num()))
		{
			Result.ResultCode = ELandscapeImportResult::Error;
			Result.ErrorMessage = LOCTEXT("Import_LayerCorruptPng", "The layer file cannot be read (corrupt png?)");
		}
		else if (ImageWrapper->GetWidth() != ExpectedResolution.Width || ImageWrapper->GetHeight() != ExpectedResolution.Height)
		{
			Result.ResultCode = ELandscapeImportResult::Error;
			Result.ErrorMessage = LOCTEXT("Import_LayerResolutionMismatch", "The layer file's resolution does not match the requested resolution");
		}
		else if (!ImageWrapper->GetRaw(ERGBFormat::Gray, 8, Result.Data))
		{
			Result.ResultCode = ELandscapeImportResult::Error;
			Result.ErrorMessage = LOCTEXT("Import_LayerCorruptPng", "The layer file cannot be read (corrupt png?)");
		}
		else
		{
			if (ImageWrapper->GetFormat() != ERGBFormat::Gray)
			{
				Result.ResultCode = ELandscapeImportResult::Warning;
				Result.ErrorMessage = LOCTEXT("Import_LayerColorPng", "The imported layer is not Grayscale. Results in-Editor will not be consistent with the source file.");
			}
		}
	}

	return Result;
}

void FLandscapeWeightmapFileFormat_Png::Export(const TCHAR* WeightmapFilename, FName LayerName, TArrayView<const uint8> Data, FLandscapeFileResolution DataResolution, FVector Scale) const
{
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

	if (ImageWrapper->SetRaw(Data.GetData(), Data.Num(), DataResolution.Width, DataResolution.Height, ERGBFormat::Gray, 8))
	{
		const TArray64<uint8> TempData = ImageWrapper->GetCompressed();
		FFileHelper::SaveArrayToFile(TempData, WeightmapFilename);
	}
}

#undef LOCTEXT_NAMESPACE
