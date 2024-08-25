// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/ManifestData.h"
#include "Misc/Compression.h"
#include "Misc/EnumClassFlags.h"
#include "Serialization/Archive.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Algo/Accumulate.h"
#include "Algo/Sort.h"
#include "Algo/Transform.h"
#include "Data/ManifestUObject.h"
#include "BuildPatchManifest.h"
#include "BuildPatchUtil.h"

DECLARE_LOG_CATEGORY_EXTERN(LogManifestData, Log, All);
DEFINE_LOG_CATEGORY(LogManifestData);

// The manifest header magic codeword, for quick checking that the opened file is probably a manifest file.
#define MANIFEST_HEADER_MAGIC 0x44BEC00C

namespace BuildPatchServices
{

	// The constant minimum sizes for each version of a header struct. Must be updated.
	// If new member variables are added the version MUST be bumped and handled properly here,
	// and these values must never change.
	static const uint32 ManifestHeaderVersionSizes[(int32)EFeatureLevel::LatestPlusOne] =
	{
		// EFeatureLevel::Original is 37B (32b Magic, 32b HeaderSize, 32b DataSizeUncompressed, 32b DataSizeCompressed, 160b SHA1, 8b StoredAs)
		// This remained the same all up to including EFeatureLevel::StoresPrerequisiteIds.
		37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
		// EFeatureLevel::StoredAsBinaryData is 41B, (296b Original, 32b Version).
		// This remained the same all up to including EFeatureLevel::UsesBuildTimeGeneratedBuildId.
		41, 41, 41, 41, 41
	};
	static_assert((int32)EFeatureLevel::Latest == 18, "Please adjust ManifestHeaderVersionSizes values accordingly.");

	/**
	 * Enum which describes the FManifestMeta data version.
	 */
	enum class EManifestMetaVersion : uint8
	{
		Original = 0,
		SerialisesBuildId,

		// Always after the latest version, signifies the latest version plus 1 to allow initialization simplicity.
		LatestPlusOne,
		Latest = (LatestPlusOne - 1)
	};

	/**
	 * Enum which describes the FChunkDataList data version.
	 */
	enum class EChunkDataListVersion : uint8
	{
		Original = 0,

		// Always after the latest version, signifies the latest version plus 1 to allow initialization simplicity.
		LatestPlusOne,
		Latest = (LatestPlusOne - 1)
	};

	/**
	 * Enum which describes the FFileManifestList data version.
	 */
	enum class EFileManifestListVersion : uint8
	{
		Original = 0,

		// Always after the latest version, signifies the latest version plus 1 to allow initialization simplicity.
		LatestPlusOne,
		Latest = (LatestPlusOne - 1)
	};

	namespace ManifestVersionHelpers
	{
		const TCHAR* GetChunkSubdir(EFeatureLevel FeatureLevel)
		{
			return FeatureLevel < EFeatureLevel::DataFileRenames ? TEXT("Chunks")
				: FeatureLevel < EFeatureLevel::ChunkCompressionSupport ? TEXT("ChunksV2")
				: FeatureLevel < EFeatureLevel::VariableSizeChunksWithoutWindowSizeChunkInfo ? TEXT("ChunksV3")
				: TEXT("ChunksV4");
		}

		const TCHAR* GetFileSubdir(EFeatureLevel FeatureLevel)
		{
			return FeatureLevel < EFeatureLevel::DataFileRenames ? TEXT("Files")
				: FeatureLevel < EFeatureLevel::StoresChunkDataShaHashes ? TEXT("FilesV2")
				: TEXT("FilesV3");
		}

		EManifestMetaVersion FeatureLevelToManifestMetaVersion(EFeatureLevel FeatureLevel)
		{
			switch (FeatureLevel)
			{
			case BuildPatchServices::EFeatureLevel::Original:
			case BuildPatchServices::EFeatureLevel::CustomFields:
			case BuildPatchServices::EFeatureLevel::StartStoringVersion:
			case BuildPatchServices::EFeatureLevel::DataFileRenames:
			case BuildPatchServices::EFeatureLevel::StoresIfChunkOrFileData:
			case BuildPatchServices::EFeatureLevel::StoresDataGroupNumbers:
			case BuildPatchServices::EFeatureLevel::ChunkCompressionSupport:
			case BuildPatchServices::EFeatureLevel::StoresPrerequisitesInfo:
			case BuildPatchServices::EFeatureLevel::StoresChunkFileSizes:
			case BuildPatchServices::EFeatureLevel::StoredAsCompressedUClass:
			case BuildPatchServices::EFeatureLevel::UNUSED_0:
			case BuildPatchServices::EFeatureLevel::UNUSED_1:
			case BuildPatchServices::EFeatureLevel::StoresChunkDataShaHashes:
			case BuildPatchServices::EFeatureLevel::StoresPrerequisiteIds:
			case BuildPatchServices::EFeatureLevel::StoredAsBinaryData:
			case BuildPatchServices::EFeatureLevel::VariableSizeChunksWithoutWindowSizeChunkInfo:
			case BuildPatchServices::EFeatureLevel::VariableSizeChunks:
			case BuildPatchServices::EFeatureLevel::UsesRuntimeGeneratedBuildId:
				return EManifestMetaVersion::Original;
			case BuildPatchServices::EFeatureLevel::UsesBuildTimeGeneratedBuildId:
				return EManifestMetaVersion::SerialisesBuildId;
			}
			checkf(false, TEXT("Unhandled FeatureLevel %s"), FeatureLevelToString(FeatureLevel));
			return EManifestMetaVersion::Latest;
		}
	}
	static_assert((uint32)EFeatureLevel::Latest == 18, "Please adjust ManifestVersionHelpers::FeatureLevelToManifestMetaVersion for new feature levels.");

	namespace ManifestDataHelpers
	{
		uint32 GetFullDataSize(const FManifestHeader& Header)
		{
			const bool bIsCompressed = EnumHasAllFlags(Header.StoredAs, EManifestStorageFlags::Compressed);
			return Header.HeaderSize + (bIsCompressed ? Header.DataSizeCompressed : Header.DataSizeUncompressed);
		}

		TUniquePtr<FArchive> CreateMemoryArchive(bool bIsLoading, TArray<uint8>& Memory)
		{
			if (bIsLoading)
			{
				return TUniquePtr<FArchive>(new FMemoryReader(Memory));
			}
			else
			{
				return TUniquePtr<FArchive>(new FMemoryWriter(Memory));
			}
		}
	}

	/* FManifestHeader - The header for a compressed/encoded manifest file.
	*****************************************************************************/

	FManifestHeader::FManifestHeader()
		: Version(EFeatureLevel::Latest)
		, HeaderSize(0)
		, DataSizeCompressed(0)
		, DataSizeUncompressed(0)
		, StoredAs(EManifestStorageFlags::None)
		, SHAHash()
	{
	}

	FArchive& operator<<(FArchive& Ar, FManifestHeader& Header)
	{
		if (Ar.IsError())
		{
			return Ar;
		}
		// Calculate how much space left in the archive for reading data ( will be 0 when writing ).
		const int64 StartPos = Ar.Tell();
		const int64 ArchiveSizeLeft = Ar.TotalSize() - StartPos;
		uint32 ExpectedSerializedBytes = 0;
		// Make sure the archive has enough data to read from, or we are saving instead.
		bool bSuccess = Ar.IsSaving() || (ArchiveSizeLeft >= ManifestHeaderVersionSizes[(int32)EFeatureLevel::Original]);
		if (bSuccess)
		{
			// Start by loading the first version we had.
			uint32 Magic = MANIFEST_HEADER_MAGIC;
			uint8 StoredAs = (uint8)Header.StoredAs;
			Header.HeaderSize = ManifestHeaderVersionSizes[(int32)Header.Version];
			Ar << Magic;
			Ar << Header.HeaderSize;
			Ar << Header.DataSizeUncompressed;
			Ar << Header.DataSizeCompressed;
			Ar.Serialize(Header.SHAHash.Hash, FSHA1::DigestSize);
			Ar << StoredAs;
			Header.StoredAs = (EManifestStorageFlags)StoredAs;
			bSuccess = Magic == MANIFEST_HEADER_MAGIC && !Ar.IsError();
			ExpectedSerializedBytes = ManifestHeaderVersionSizes[(int32)EFeatureLevel::Original];

			// After the Original with no specific version serialized, the header size increased and we had a version to load.
			if (bSuccess && Header.HeaderSize > ManifestHeaderVersionSizes[(int32)EFeatureLevel::Original])
			{
				int32 Version = (int32)Header.Version;
				Ar << Version;
				Header.Version = (EFeatureLevel)Version;
				bSuccess = !Ar.IsError();
				ExpectedSerializedBytes = ManifestHeaderVersionSizes[(int32)EFeatureLevel::StoredAsBinaryData];
			}
			// Otherwise, this header was at the version for a UObject class before this code refactor.
			else if (bSuccess && Ar.IsLoading())
			{
				Header.Version = EFeatureLevel::StoredAsCompressedUClass;
			}
		}

		// Make sure the expected number of bytes were serialized. In practice this will catch errors where type
		// serialization operators changed their format and that will need investigating.
		bSuccess = bSuccess && (Ar.Tell() - StartPos) == ExpectedSerializedBytes;

		if (bSuccess)
		{
			// Make sure the archive now points to data location.
			Ar.Seek(StartPos + Header.HeaderSize);
		}
		else
		{
			// If we had a serialization error when loading, zero out the header values.
			if (Ar.IsLoading())
			{
				FMemory::Memzero(Header);
			}
			Ar.SetError();
		}

		return Ar;
	}

	/* FManifestMeta - The data implementation for a build meta data.
	*****************************************************************************/

	FManifestMeta::FManifestMeta()
		: FeatureLevel(BuildPatchServices::EFeatureLevel::Invalid)
		, bIsFileData(false)
		, AppID(INDEX_NONE)
		, BuildId(FBuildPatchUtils::GenerateNewBuildId())
	{
	}

	FArchive& operator<<(FArchive& Ar, FManifestMeta& Meta)
	{
		if (Ar.IsError())
		{
			return Ar;
		}

		// Serialise the data header type values.
		const int64 StartPos = Ar.Tell();
		uint32 DataSize = 0;
		EManifestMetaVersion DataVersion = Ar.IsSaving() ? ManifestVersionHelpers::FeatureLevelToManifestMetaVersion(Meta.FeatureLevel) : EManifestMetaVersion::Latest;
		{
			uint8 DataVersionInt = (uint8)DataVersion;
			Ar << DataSize;
			Ar << DataVersionInt;
			DataVersion = (EManifestMetaVersion)DataVersionInt;
		}

		// Serialise the ManifestMetaVersion::Original version variables.
		if (!Ar.IsError() && DataVersion >= EManifestMetaVersion::Original)
		{
			int32 FeatureLevelInt = (int32)Meta.FeatureLevel;
			uint8 IsFileDataInt = Meta.bIsFileData ? 1 : 0;
			Ar << FeatureLevelInt;
			Ar << IsFileDataInt;
			Ar << Meta.AppID;
			Ar << Meta.AppName;
			Ar << Meta.BuildVersion;
			Ar << Meta.LaunchExe;
			Ar << Meta.LaunchCommand;
			Ar << Meta.PrereqIds;
			Ar << Meta.PrereqName;
			Ar << Meta.PrereqPath;
			Ar << Meta.PrereqArgs;
			Meta.FeatureLevel = (EFeatureLevel)FeatureLevelInt;
			Meta.bIsFileData = IsFileDataInt == 1;
		}

		// Serialise the BuildId.
		if (!Ar.IsError() && DataVersion >= EManifestMetaVersion::SerialisesBuildId)
		{
			Ar << Meta.BuildId;
		}
		// Otherwise, initialise with backwards compatible default when loading.
		else if (!Ar.IsError() && Ar.IsLoading())
		{
			Meta.BuildId = FBuildPatchUtils::GetBackwardsCompatibleBuildId(Meta);
		}

		//// Here we would check for later data versions to serialise additional values.
		//if (!Ar.IsError() && DataVersion >= ManifestMetaVersion::SomeShinyNewVersion)
		//{
		//	Ar << Meta.SomeShinyNewVariable;
		//}

		// If saving, we need to go back and set the data size.
		if (!Ar.IsError() && Ar.IsSaving())
		{
			const int64 EndPos = Ar.Tell();
			DataSize = EndPos - StartPos;
			Ar.Seek(StartPos);
			Ar << DataSize;
			Ar.Seek(EndPos);
		}

		// We must always make sure to seek the archive to the correct end location.
		Ar.Seek(StartPos + DataSize);
		return Ar;
	}

	/* FChunkDataList - The data implementation for a list of referenced chunk data.
	*****************************************************************************/

	FChunkDataList::FChunkDataList()
	{
	}

	FArchive& operator<<(FArchive& Ar, FChunkDataList& ChunkDataList)
	{
		if (Ar.IsError())
		{
			return Ar;
		}

		// Serialise the data header type values.
		const int64 StartPos = Ar.Tell();
		uint32 DataSize = 0;
		EChunkDataListVersion DataVersion = EChunkDataListVersion::Latest;
		int32 ElementCount = ChunkDataList.ChunkList.Num();
		{
			uint8 DataVersionInt = (uint8)DataVersion;
			Ar << DataSize;
			Ar << DataVersionInt;
			Ar << ElementCount;
			DataVersion = (EChunkDataListVersion)DataVersionInt;
		}

		// Make sure we have the right number of defaulted structs.
		ChunkDataList.ChunkList.AddDefaulted(ElementCount - ChunkDataList.ChunkList.Num());
		checkf(ElementCount == ChunkDataList.ChunkList.Num(), TEXT("Programmer error with count and array initialisation sync up."));

		// For a struct list type of data, we serialise every variable as it's own flat list.
		// This makes it very simple to handle or skip, extra variables added to the struct later.

		// Serialise the ManifestMetaVersion::Original version variables.
		if (!Ar.IsError() && DataVersion >= EChunkDataListVersion::Original)
		{
			for (FChunkInfo& ChunkInfo : ChunkDataList.ChunkList) { Ar << ChunkInfo.Guid; }
			for (FChunkInfo& ChunkInfo : ChunkDataList.ChunkList) { Ar << ChunkInfo.Hash; }
			for (FChunkInfo& ChunkInfo : ChunkDataList.ChunkList) { Ar << ChunkInfo.ShaHash; }
			for (FChunkInfo& ChunkInfo : ChunkDataList.ChunkList) { Ar << ChunkInfo.GroupNumber; }
			for (FChunkInfo& ChunkInfo : ChunkDataList.ChunkList) { Ar << ChunkInfo.WindowSize; }
			for (FChunkInfo& ChunkInfo : ChunkDataList.ChunkList) { Ar << ChunkInfo.FileSize; }
		}

		//// Here we would check for later data versions to serialise additional values.
		//if (!Ar.IsError() && DataVersion >= EChunkDataListVersion::SomeShinyNewVersion)
		//{
		//	for (FChunkInfo& ChunkInfo : ChunkDataList.ChunkList) { Ar << ChunkInfo.SomeShinyNewVariable; }
		//}

		// If saving, we need to go back and set the data size.
		if (!Ar.IsError() && Ar.IsSaving())
		{
			const int64 EndPos = Ar.Tell();
			DataSize = EndPos - StartPos;
			Ar.Seek(StartPos);
			Ar << DataSize;
			Ar.Seek(EndPos);
		}

		// We must always make sure to seek the archive to the correct end location.
		Ar.Seek(StartPos + DataSize);
		return Ar;
	}

	/* FFileManifests - The data implementation for a list of file manifests.
	*****************************************************************************/

	FFileManifest::FFileManifest()
		: FileMetaFlags(EFileMetaFlags::None)
		, FileSize(0)
	{
	}

	/* FFileManifestList - The data implementation for a list of referenced files.
	*****************************************************************************/

	FFileManifestList::FFileManifestList()
	{
	}

	void FFileManifestList::OnPostLoad()
	{
		Algo::SortBy(FileList, [](const FFileManifest& FileManifest) -> const FString& { return FileManifest.Filename; }, TLess<FString>());

		for (FFileManifest& FileManifest : FileList)
		{
			FileManifest.FileSize = Algo::Accumulate<int64>(FileManifest.ChunkParts, 0, [](int64 Count, const FChunkPart& ChunkPart){ return Count + ChunkPart.Size; });
		}
	}

	FArchive& operator<<(FArchive& Ar, FFileManifestList& FileDataList)
	{
		if (Ar.IsError())
		{
			return Ar;
		}

		// Serialise the data header type values.
		const int64 StartPos = Ar.Tell();
		uint32 DataSize = 0;
		EFileManifestListVersion DataVersion = EFileManifestListVersion::Latest;
		int32 ElementCount = FileDataList.FileList.Num();
		{
			uint8 DataVersionInt = (uint8)DataVersion;
			Ar << DataSize;
			Ar << DataVersionInt;
			Ar << ElementCount;
			DataVersion = (EFileManifestListVersion)DataVersionInt;
		}

		// Make sure we have the right number of defaulted structs.
		FileDataList.FileList.AddDefaulted(ElementCount - FileDataList.FileList.Num());
		checkf(ElementCount == FileDataList.FileList.Num(), TEXT("Programmer error with count and array initialisation sync up."));

		// Serialise the ManifestMetaVersion::Original version variables.
		if (!Ar.IsError() && DataVersion >= EFileManifestListVersion::Original)
		{
			for (FFileManifest& FileManifest : FileDataList.FileList) { Ar << FileManifest.Filename; }
			for (FFileManifest& FileManifest : FileDataList.FileList) { Ar << FileManifest.SymlinkTarget; }
			for (FFileManifest& FileManifest : FileDataList.FileList) { Ar << FileManifest.FileHash; }
			for (FFileManifest& FileManifest : FileDataList.FileList)
			{
				uint8 FileMetaFlagsInt = (uint8)FileManifest.FileMetaFlags;
				Ar << FileMetaFlagsInt;
				FileManifest.FileMetaFlags = (EFileMetaFlags)FileMetaFlagsInt;
			}
			for (FFileManifest& FileManifest : FileDataList.FileList) { Ar << FileManifest.InstallTags; }
			for (FFileManifest& FileManifest : FileDataList.FileList) { Ar << FileManifest.ChunkParts; }
		}

		//// Here we would check for later data versions to serialise additional values.
		//if (!Ar.IsError() && DataVersion >= EFileManifestListVersion::SomeShinyNewVersion)
		//{
		//	for (FFileManifest& FileManifest : FileDataList.FileList) { Ar << FileManifest.SomeShinyNewVariable; }
		//}

		// If saving, we need to go back and set the data size.
		if (!Ar.IsError() && Ar.IsSaving())
		{
			const int64 EndPos = Ar.Tell();
			DataSize = EndPos - StartPos;
			Ar.Seek(StartPos);
			Ar << DataSize;
			Ar.Seek(EndPos);
		}

		// If loading call OnPostLoad to setup calculated values.
		if (!Ar.IsError() && Ar.IsLoading())
		{
			FileDataList.OnPostLoad();
		}

		// We must always make sure to seek the archive to the correct end location.
		Ar.Seek(StartPos + DataSize);
		return Ar;
	}

	/* FCustomFields - The data implementation for a list of custom fields.
	*****************************************************************************/

	FCustomFields::FCustomFields()
	{
	}

	FArchive& operator<<(FArchive& Ar, FCustomFields& CustomFields)
	{
		if (Ar.IsError())
		{
			return Ar;
		}

		// We have to convert a map to an array.
		TArray<TTuple<FString, FString>> ArrayFields;
		ArrayFields.Reserve(CustomFields.Fields.Num());
		for (TTuple<FString, FString>& Field : CustomFields.Fields)
		{
			ArrayFields.Emplace(MoveTemp(Field.Get<0>()), MoveTemp(Field.Get<1>()));
		}
		CustomFields.Fields.Empty();

		// Serialise the data header type values.
		const int64 StartPos = Ar.Tell();
		uint32 DataSize = 0;
		EChunkDataListVersion DataVersion = EChunkDataListVersion::Latest;
		int32 ElementCount = ArrayFields.Num();
		{
			uint8 DataVersionInt = (uint8)DataVersion;
			Ar << DataSize;
			Ar << DataVersionInt;
			Ar << ElementCount;
			DataVersion = (EChunkDataListVersion)DataVersionInt;
		}
		ArrayFields.AddDefaulted(ElementCount - ArrayFields.Num());
		checkf(ElementCount == ArrayFields.Num(), TEXT("Programmer error with count and array initialisation sync up."));

		// Serialise the ManifestMetaVersion::Original version variables.
		if (!Ar.IsError() && DataVersion >= EChunkDataListVersion::Original)
		{
			for (TTuple<FString, FString>& Field : ArrayFields) { Ar << Field.Get<0>(); }
			for (TTuple<FString, FString>& Field : ArrayFields) { Ar << Field.Get<1>(); }
		}

		//// Here we would check for later data versions to serialise additional values.
		//if (!Ar.IsError() && DataVersion >= EChunkDataListVersion::SomeShinyNewVersion)
		//{
		//	for (TTuple<FString, FString, FShinyNewType>& Field : ArrayFields) { Ar << Field.Get<2>(); }
		//}

		// If saving, we need to go back and set the data size.
		if (!Ar.IsError() && Ar.IsSaving())
		{
			const int64 EndPos = Ar.Tell();
			DataSize = EndPos - StartPos;
			Ar.Seek(StartPos);
			Ar << DataSize;
			Ar.Seek(EndPos);
		}

		// We convert the array back to a map.
		CustomFields.Fields.Empty(ArrayFields.Num());
		for (TTuple<FString, FString>& Field : ArrayFields)
		{
			CustomFields.Fields.Add(MoveTemp(Field.Get<0>()), MoveTemp(Field.Get<1>()));
		}
		ArrayFields.Empty();

		// We must always make sure to seek the archive to the correct end location.
		Ar.Seek(StartPos + DataSize);
		return Ar;
	}

	/* FManifestData - The public interface to load/saving manifest files.
	*****************************************************************************/
	void FManifestData::Init()
	{
#if !BUILDPATCHSERVICES_NOUOBJECT
		FManifestUObject::Init();
#endif // !BUILDPATCHSERVICES_NOUOBJECT

#if DO_CHECK && !UE_BUILD_SHIPPING
		// Run tests to verify entered header sizes, asserting on failure.
		for (EFeatureLevel FeatureLevel : TEnumRange<EFeatureLevel>())
		{
			FManifestHeader Header;
			Header.Version = FeatureLevel;
			TArray<uint8> Data;
			FMemoryWriter Ar(Data);
			Ar << Header;
			check(Header.HeaderSize == Data.Num());
			check(Header.HeaderSize == ManifestHeaderVersionSizes[(int32)FeatureLevel]);
		}
#endif
	}

	bool FManifestData::Serialize(FArchive& Ar, FBuildPatchAppManifest& AppManifest, BuildPatchServices::EFeatureLevel SaveFormat)
	{
		if (Ar.IsError())
		{
			return false;
		}
		bool bSuccess = true;
		// If we are saving an old format, defer to the old code!
		if (Ar.IsSaving() && SaveFormat < EFeatureLevel::StoredAsBinaryData)
		{
			bSuccess = FManifestUObject::SaveToArchive(Ar, AppManifest);
		}
		else
		{
			const int64 StartPos = Ar.Tell();
			FManifestHeader Header;
			Header.Version = SaveFormat;

			// Load header right away.
			if (Ar.IsLoading())
			{
				Ar << Header;
				bSuccess = !Ar.IsError();
			}

			// If we are loading an old format, defer to the old code!
			if (Ar.IsLoading() && Header.Version < EFeatureLevel::StoredAsBinaryData)
			{
				const uint32 FullDataSize = ManifestDataHelpers::GetFullDataSize(Header);
				TArray<uint8> FullData;
				FullData.AddUninitialized(FullDataSize);
				Ar.Seek(StartPos);
				Ar.Serialize(FullData.GetData(), FullDataSize);
				bSuccess = FManifestUObject::LoadFromMemory(FullData, AppManifest);
				// Mark as should be re-saved, client that stores binary should stop using UObject class.
				AppManifest.bNeedsResaving = true;
			}
			else
			{
				// Compression format selection - we only have one right now.
				const FName CompressionFormat = NAME_Zlib;
				const ECompressionFlags CompressionFlags = ECompressionFlags::COMPRESS_BiasMemory;
				// Yay shiny new format!
				TArray<uint8> ManifestRawData;
				// Fill the array with loaded data.
				if (bSuccess && Ar.IsLoading())
				{
					// DataSizeCompressed always equals the size of the data following the header.
					ManifestRawData.AddUninitialized(Header.DataSizeCompressed);
					Ar.Serialize(ManifestRawData.GetData(), Header.DataSizeCompressed);
					bSuccess = !Ar.IsError();
				}
				// Uncompress from input archive.
				if (bSuccess && Ar.IsLoading() && EnumHasAllFlags(Header.StoredAs, EManifestStorageFlags::Compressed))
				{
					TArray<uint8> CompressedData = MoveTemp(ManifestRawData);
					ManifestRawData.AddUninitialized(Header.DataSizeUncompressed);
					bSuccess = FCompression::UncompressMemory(
						CompressionFormat,
						ManifestRawData.GetData(),
						ManifestRawData.Num(),
						CompressedData.GetData(),
						CompressedData.Num(),
						CompressionFlags);
				}
				// If loading, check the raw data SHA
				if (bSuccess && Ar.IsLoading())
				{
					FSHAHash DataHash;
					FSHA1::HashBuffer(ManifestRawData.GetData(), ManifestRawData.Num(), DataHash.Hash);
					bSuccess = DataHash == Header.SHAHash;
				}
				if (bSuccess)
				{
					// Create the directional interface to the raw data array.
					TUniquePtr<FArchive> RawAr = ManifestDataHelpers::CreateMemoryArchive(Ar.IsLoading(), ManifestRawData);
					// Serialise each of the manifest's data members.
					*RawAr << AppManifest.ManifestMeta;
					*RawAr << AppManifest.ChunkDataList;
					*RawAr << AppManifest.FileManifestList;
					*RawAr << AppManifest.CustomFields;
					bSuccess = !RawAr->IsError();
					//// Here we would check for later header versions to serialise additional structures.
					//if (bSuccess && Header.Version >= EFeatureLevel::SomeShinyNewVersion)
					//{
					//	*RawAr << AppManifest.CustomFields;
					//	bSuccess = !RawAr->IsError();
					//}
				}
				// If saving, calculate the raw data SHA.
				if (bSuccess && Ar.IsSaving())
				{
					FSHAHash DataHash;
					FSHA1::HashBuffer(ManifestRawData.GetData(), ManifestRawData.Num(), DataHash.Hash);
					Header.SHAHash = DataHash;
				}
				// Compress to input archive.
				if (bSuccess && Ar.IsSaving())
				{
					TArray<uint8> TempCompressed;
					Header.DataSizeUncompressed = ManifestRawData.Num();
					Header.DataSizeCompressed = ManifestRawData.Num();
					TempCompressed.AddUninitialized(Header.DataSizeCompressed);
					const bool bDataIsCompressed = FCompression::CompressMemory(
						CompressionFormat,
						TempCompressed.GetData(),
						(int32&)Header.DataSizeCompressed,
						ManifestRawData.GetData(),
						ManifestRawData.Num(),
						CompressionFlags);
					if (bDataIsCompressed)
					{
						TempCompressed.SetNum(Header.DataSizeCompressed, EAllowShrinking::No);
						ManifestRawData = MoveTemp(TempCompressed);
						Header.StoredAs = EManifestStorageFlags::Compressed;
					}
					else
					{
						Header.DataSizeCompressed = ManifestRawData.Num();
						Header.StoredAs = EManifestStorageFlags::None;
					}
				}
				// If saving, go back to fill out header and then write data.
				if (bSuccess && Ar.IsSaving())
				{
					Ar.Seek(StartPos);
					Ar << Header;
					Ar.Serialize(ManifestRawData.GetData(), ManifestRawData.Num());
					Ar.Flush();
					bSuccess = !Ar.IsError();
				}
				// If loading, setup manifest internal tracking.
				if (bSuccess && Ar.IsLoading())
				{
					AppManifest.FileManifestList.OnPostLoad();
					AppManifest.InitLookups();
				}
			}
			// We must always make sure to seek the archive to the correct end location, but only seek if we must, to avoid a flush.
			const int64 DataLocation = StartPos + Header.HeaderSize + Header.DataSizeCompressed;
			if (bSuccess && Ar.Tell() != DataLocation)
			{
				Ar.Seek(DataLocation);
			}
		}
		bSuccess = bSuccess && !Ar.IsError();
		if (!bSuccess)
		{
			Ar.SetError();
		}
		return bSuccess;
	}
}
