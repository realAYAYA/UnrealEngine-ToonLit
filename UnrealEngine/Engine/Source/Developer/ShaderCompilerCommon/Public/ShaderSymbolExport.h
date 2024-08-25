// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// HEADER_UNIT_SKIP - Bad includes (ZipArchiveWriter.h)

#if WITH_ENGINE

#include "CoreTypes.h"
#include "Containers/ContainersFwd.h"
#include "Containers/Set.h"
#include "Templates/UniquePtr.h"
#include "Serialization/MemoryReader.h"

class FZipArchiveWriter;

class SHADERCOMPILERCOMMON_API FShaderSymbolExport
{
public:
	FShaderSymbolExport() = delete;
	FShaderSymbolExport(FName InShaderFormat);
	~FShaderSymbolExport();

	/** Should be called from IShaderFormat::NotifyShaderCompiled implementation.
	*   Template type is the platform specific symbol data structure.
	*/
	template<typename TPlatformShaderSymbolData>
	void NotifyShaderCompiled(const TConstArrayView<uint8>& PlatformSymbolData);

	/** Called at the end of a cook to free resources and finalize artifacts created during the cook. */
	void NotifyShaderCompilersShutdown();

private:
	void Initialize();
	void WriteSymbolData(const FString& Filename, const FString& DebugInfo, TConstArrayView<uint8> Contents);

	const FName ShaderFormat;

	TUniquePtr<FZipArchiveWriter> ZipWriter;
	TSet<FString> ExportedShaders;
	FString ExportPath;
	FString InfoFilePath;

	uint64 TotalSymbolDataBytes{ 0 };
	uint64 TotalSymbolData{ 0 };
	bool bExportShaderSymbols{ false };

	struct FShaderInfo
	{
		FString Hash;
		FString Data;
	};
	TArray<FShaderInfo> ShaderInfos;

	/**
	 * If true, the current process is the first process in a multiprocess group, or is not in a group,
	 * and should combine artifacts produced by the other processes. Will also be false if no combination
	 * is necessary for given settings.
	 */
	bool bMultiprocessOwner{ false };
};

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
			const FString FileName = SymbolData.GetFilename();
			const FString DebugInfo = SymbolData.GetDebugInfo();
			TConstArrayView<uint8> Contents = SymbolData.GetContents();

			WriteSymbolData(FileName, DebugInfo, Contents);
		}
	}
}

#endif // WITH_ENGINE
