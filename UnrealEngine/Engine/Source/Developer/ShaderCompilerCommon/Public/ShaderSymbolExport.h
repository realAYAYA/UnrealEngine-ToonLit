// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_ENGINE

#include "Serialization/MemoryReader.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformFileManager.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "FileUtilities/ZipArchiveWriter.h"

DECLARE_LOG_CATEGORY_CLASS(LogShaderSymbolExport, Display, Display);

class FShaderSymbolExport
{
public:
	FShaderSymbolExport(FName InShaderFormat);

	/** Should be called from IShaderFormat::NotifyShaderCompiled implementation.
	*   Template type is the platform specific symbol data structure.
	*/
	template<typename TPlatformShaderSymbolData>
	void NotifyShaderCompiled(const TConstArrayView<uint8>& PlatformSymbolData);

private:
	void Initialize();

	const FName ShaderFormat;

	TUniquePtr<FZipArchiveWriter> ZipWriter;
	TSet<FString> ExportedShaders;
	FString ExportPath;

	uint64 TotalSymbolDataBytes{ 0 };
	uint64 TotalSymbolData{ 0 };
	bool bExportShaderSymbols{ false };
};

inline FShaderSymbolExport::FShaderSymbolExport(FName InShaderFormat)
	: ShaderFormat(InShaderFormat)
{
}

inline void FShaderSymbolExport::Initialize()
{
	const bool bSymbolsEnabled = ShouldWriteShaderSymbols(ShaderFormat);
	const bool bForceSymbols = FParse::Value(FCommandLine::Get(), TEXT("-ShaderSymbolsExport="), ExportPath);

	if (bSymbolsEnabled || bForceSymbols)
	{
		// if no command line path is provided, look to the cvar first
		if (ExportPath.IsEmpty())
		{
			if (GetShaderSymbolPathOverride(ExportPath, ShaderFormat))
			{
				ExportPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*ExportPath);
			}
		}

		// if there was no path set via command line or the cvar, fall back to our default
		if (ExportPath.IsEmpty())
		{
			ExportPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(
				*(FPaths::ProjectSavedDir() / TEXT("ShaderSymbols") / ShaderFormat.ToString()));
		}

		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		bExportShaderSymbols = PlatformFile.CreateDirectoryTree(*ExportPath);

		if (!bExportShaderSymbols)
		{
			UE_LOG(LogShaderSymbolExport, Error, TEXT("Failed to create shader symbols output directory. Shader symbol export will be disabled."));
		}
		else
		{
			// Check if the export mode is to an uncompressed archive or loose files.
			bool bExportAsZip = ShouldWriteShaderSymbolsAsZip(ShaderFormat);
			if (bExportAsZip || FParse::Param(FCommandLine::Get(), TEXT("ShaderSymbolsExportZip")))
			{
				FString SingleFilePath = ExportPath / TEXT("ShaderSymbols.zip");

				IFileHandle* OutputZipFile = PlatformFile.OpenWrite(*SingleFilePath);
				if (!OutputZipFile)
				{
					UE_LOG(LogShaderSymbolExport, Error, TEXT("Failed to create shader symbols output file \"%s\". Shader symbol export will be disabled."), *SingleFilePath);
					bExportShaderSymbols = false;
				}
				else
				{
					ZipWriter = MakeUnique<FZipArchiveWriter>(OutputZipFile);
				}
			}
		}
	}

	if (bExportShaderSymbols)
	{
		UE_LOG(LogShaderSymbolExport, Display, TEXT("Shader symbol export enabled. Output directory: \"%s\""), *ExportPath);
		if (ZipWriter)
		{
			UE_LOG(LogShaderSymbolExport, Display, TEXT("Shader symbol zip mode enabled. Shader symbols will be archived in a single (uncompressed) zip file."));
		}
	}
}

template<typename TPlatformShaderSymbolData>
inline void FShaderSymbolExport::NotifyShaderCompiled(const TConstArrayView<uint8>& PlatformSymbolData)
{
	static bool bFirst = true;
	if (bFirst)
	{
		// If we get called, we know we're compiling. Do one time initialization
		// which will create the output directory / open the open file stream.
		Initialize();
		bFirst = false;
	}

	if (bExportShaderSymbols)
	{
		// Deserialize the platform symbol data
		TPlatformShaderSymbolData FullSymbolData;
		FMemoryReaderView Ar(PlatformSymbolData);
		Ar << FullSymbolData;

		for (const auto& SymbolData : FullSymbolData.GetAllSymbolData())
		{
			if (TConstArrayView<uint8> Contents = SymbolData.GetContents(); !Contents.IsEmpty())
			{
				const FString FileName = SymbolData.GetFilename();

				// Skip this symbol data if we've already exported it before.
				bool bAlreadyInSet = false;
				ExportedShaders.Add(FileName, &bAlreadyInSet);
				if (bAlreadyInSet)
				{
					// We've already exported this shader hash
					continue;
				}

				// Emit periodic log messages detailing the size of the shader symbols output file/directory.
				static uint64 LastReport = 0;
				TotalSymbolDataBytes += Contents.Num();
				TotalSymbolData++;

				if ((TotalSymbolDataBytes - LastReport) >= (64 * 1024 * 1024))
				{
					UE_LOG(LogShaderSymbolExport, Display, TEXT("Shader symbols export size: %.2f MB, count: %llu"),
						double(TotalSymbolDataBytes) / (1024.0 * 1024.0), TotalSymbolData);
					LastReport = TotalSymbolDataBytes;
				}

				if (ZipWriter)
				{
					// Append the platform data to the zip file
					ZipWriter->AddFile(FileName, Contents, FDateTime::Now());
				}
				else
				{
					IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

					// Write the symbols to the export directory
					const FString OutputPath = ExportPath / FileName;
					const FString Directory = FPaths::GetPath(OutputPath);

					// FileName could contain extra folders, so we need to make sure they exist first.
					if (!PlatformFile.CreateDirectoryTree(*Directory))
					{
						UE_LOG(LogShaderSymbolExport, Error, TEXT("Failed to create shader symbol directory \"%s\"."), *Directory);
					}
					else
					{
						IFileHandle* File = PlatformFile.OpenWrite(*OutputPath);
						if (!File || !File->Write(Contents.GetData(), Contents.Num()))
						{
							UE_LOG(LogShaderSymbolExport, Error, TEXT("Failed to export shader symbols \"%s\"."), *OutputPath);
						}

						if (File)
						{
							delete File;
						}
					}
				}
			}
		}
	}
}


#endif // WITH_ENGINE
