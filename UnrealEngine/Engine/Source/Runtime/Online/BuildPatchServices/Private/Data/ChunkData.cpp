// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/ChunkData.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Misc/CommandLine.h"
#include "Misc/Compression.h"
#include "Common/FileSystem.h"
#include "BuildPatchHash.h"
#include "BuildPatchManifest.h"

// The chunk header magic codeword, for quick checking that the opened file is a chunk file.
#define CHUNK_HEADER_MAGIC      0xB1FE3AA2

// The chunkdb header magic codeword, for quick checking that the opened file is a chunkdb file.
#define CHUNKDB_HEADER_MAGIC    0xB1FE3AA3

namespace BuildPatchServices
{
	const TCHAR* ToString(const EChunkLoadResult& ChunkLoadResult)
	{
		switch(ChunkLoadResult)
		{
			case EChunkLoadResult::Success:
				return TEXT("Success");
			case EChunkLoadResult::OpenFileFail:
				return TEXT("OpenFileFail");
			case EChunkLoadResult::BadArchive:
				return TEXT("BadArchive");
			case EChunkLoadResult::CorruptHeader:
				return TEXT("CorruptHeader");
			case EChunkLoadResult::IncorrectFileSize:
				return TEXT("IncorrectFileSize");
			case EChunkLoadResult::UnsupportedStorage:
				return TEXT("UnsupportedStorage");
			case EChunkLoadResult::MissingHashInfo:
				return TEXT("MissingHashInfo");
			case EChunkLoadResult::SerializationError:
				return TEXT("SerializationError");
			case EChunkLoadResult::DecompressFailure:
				return TEXT("DecompressFailure");
			case EChunkLoadResult::HashCheckFailed:
				return TEXT("HashCheckFailed");
			case EChunkLoadResult::Aborted:
				return TEXT("Aborted");
			default:
				return TEXT("Unknown");
		}
	}

	const TCHAR* ToString(const EChunkSaveResult& ChunkSaveResult)
	{
		switch(ChunkSaveResult)
		{
			case EChunkSaveResult::Success:
				return TEXT("Success");
			case EChunkSaveResult::FileCreateFail:
				return TEXT("FileCreateFail");
			case EChunkSaveResult::BadArchive:
				return TEXT("BadArchive");
			case EChunkSaveResult::SerializationError:
				return TEXT("SerializationError");
			default:
				return TEXT("Unknown");
		}
	}

	/**
	 * Enum which describes the chunk header version.
	 */
	enum class EChunkVersion : uint32
	{
		Invalid = 0,
		Original,
		StoresShaAndHashType,
		StoresDataSizeUncompressed,

		// Always after the latest version, signifies the latest version plus 1 to allow initialization simplicity.
		LatestPlusOne,
		Latest = (LatestPlusOne - 1)
	};

	// The constant minimum sizes for each version of a header struct. Must be updated.
	// If new member variables are added the version MUST be bumped and handled properly here,
	// and these values must never change.
	static const uint32 ChunkHeaderVersionSizes[(uint32)EChunkVersion::LatestPlusOne] =
	{
		// Dummy for indexing.
		0,
		// Original is 41 bytes (32b Magic, 32b Version, 32b HeaderSize, 32b DataSizeCompressed, 4x32b GUID, 64b Hash, 8b StoredAs).
		41,
		// StoresShaAndHashType is 62 bytes (328b Original, 160b SHA1, 8b HashType).
		62,
		// StoresDataSizeUncompressed is 66 bytes (496b StoresShaAndHashType, 32b DataSizeUncompressed).
		66
	};
	static_assert((uint32)EChunkVersion::LatestPlusOne == 4, "Please adjust ChunkHeaderVersionSizes values accordingly.");

	namespace HeaderHelpers
	{
		void ZeroHeader(FChunkHeader& Header)
		{
			FMemory::Memzero(Header);
		}

		void ZeroHeader(FChunkDatabaseHeader& Header)
		{
			Header.Version = 0;
			Header.HeaderSize = 0;
			Header.DataSize = 0;
			Header.Contents.Empty();
		}

		EChunkVersion FeatureLevelToChunkVersion(EFeatureLevel FeatureLevel)
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
					return EChunkVersion::Original;
				case BuildPatchServices::EFeatureLevel::StoresChunkDataShaHashes:
				case BuildPatchServices::EFeatureLevel::StoresPrerequisiteIds:
					return EChunkVersion::StoresShaAndHashType;
				case BuildPatchServices::EFeatureLevel::StoredAsBinaryData:
				case BuildPatchServices::EFeatureLevel::VariableSizeChunksWithoutWindowSizeChunkInfo:
				case BuildPatchServices::EFeatureLevel::VariableSizeChunks:
				case BuildPatchServices::EFeatureLevel::UsesRuntimeGeneratedBuildId:
				case BuildPatchServices::EFeatureLevel::UsesBuildTimeGeneratedBuildId:
					return EChunkVersion::StoresDataSizeUncompressed;
			}
			checkf(false, TEXT("Unhandled FeatureLevel %s"), FeatureLevelToString(FeatureLevel));
			return EChunkVersion::Invalid;
		}
	}
	static_assert((uint32)EFeatureLevel::Latest == 18, "Please adjust HeaderHelpers::FeatureLevelToChunkVersion for new feature levels.");

	FChunkHeader::FChunkHeader()
		: Version((uint32)EChunkVersion::Latest)
		, HeaderSize(ChunkHeaderVersionSizes[(uint32)EChunkVersion::Latest])
		, DataSizeCompressed(0)
		, DataSizeUncompressed(1024 * 1024)
		, StoredAs(EChunkStorageFlags::None)
		, HashType(EChunkHashFlags::RollingPoly64)
		, RollingHash(0)
	{
	}

	FArchive& operator<<(FArchive& Ar, FChunkHeader& Header)
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
		bool bSuccess = Ar.IsSaving() || (ArchiveSizeLeft >= ChunkHeaderVersionSizes[(uint32)EChunkVersion::Original]);
		if (bSuccess)
		{
			Header.HeaderSize = ChunkHeaderVersionSizes[Header.Version];
			uint32 Magic = CHUNK_HEADER_MAGIC;
			uint8 StoredAs = (uint8)Header.StoredAs;
			Ar << Magic
			   << Header.Version
			   << Header.HeaderSize
			   << Header.DataSizeCompressed
			   << Header.Guid
			   << Header.RollingHash
			   << StoredAs;
			Header.StoredAs = (EChunkStorageFlags)StoredAs;
			bSuccess = Magic == CHUNK_HEADER_MAGIC && !Ar.IsError();
			ExpectedSerializedBytes = ChunkHeaderVersionSizes[(uint32)EChunkVersion::Original];

			// From version 2, we have a hash type choice. Previous versions default as only rolling.
			if (bSuccess && Header.Version >= (uint32)EChunkVersion::StoresShaAndHashType)
			{
				bSuccess = Ar.IsSaving() || (ArchiveSizeLeft >= ChunkHeaderVersionSizes[(uint32)EChunkVersion::StoresShaAndHashType]);
				if (bSuccess)
				{
					uint8 HashType = (uint8)Header.HashType;
					Ar.Serialize(Header.SHAHash.Hash, FSHA1::DigestSize);
					Ar << HashType;
					Header.HashType = (EChunkHashFlags)HashType;
					bSuccess = !Ar.IsError();
				}
				ExpectedSerializedBytes = ChunkHeaderVersionSizes[(uint32)EChunkVersion::StoresShaAndHashType];
			}

			// From version 3, we have an uncompressed data size. Previous versions default to 1 MiB (1048576 B).
			if (bSuccess && Header.Version >= (uint32)EChunkVersion::StoresDataSizeUncompressed)
			{
				bSuccess = Ar.IsSaving() || (ArchiveSizeLeft >= ChunkHeaderVersionSizes[(uint32)EChunkVersion::StoresDataSizeUncompressed]);
				if (bSuccess)
				{
					Ar << Header.DataSizeUncompressed;
					bSuccess = !Ar.IsError();
				}
				ExpectedSerializedBytes = ChunkHeaderVersionSizes[(uint32)EChunkVersion::StoresDataSizeUncompressed];
			}
		}

		// Make sure the expected number of bytes were serialized. In practice this will catch errors where type
		// serialization operators changed their format and that will need investigating.
		bSuccess = bSuccess && (Ar.Tell() - StartPos) == ExpectedSerializedBytes;

		if (bSuccess)
		{
			// Make sure the archive now points to data location. Only seek if we must, to avoid a flush.
			const int64 DataLocation = StartPos + Header.HeaderSize;
			if (Ar.Tell() != DataLocation)
			{
				Ar.Seek(DataLocation);
			}
		}
		else
		{
			// If we had a serialization error when loading, zero out the header values.
			if (Ar.IsLoading())
			{
				HeaderHelpers::ZeroHeader(Header);
			}
			Ar.SetError();
		}

		return Ar;
	}

	/* FChunkInfo implementation
	*****************************************************************************/
	FChunkInfo::FChunkInfo()
		: Guid()
		, Hash(0)
		, ShaHash()
		, GroupNumber(0)
		, WindowSize(1048576)
		, FileSize(0)
	{
	}

	/* FChunkPart implementation
	*****************************************************************************/
	FChunkPart::FChunkPart()
		: Guid()
		, Offset(0)
		, Size(0)
	{
	}

	FChunkPart::FChunkPart(const FGuid& InGuid, const uint32 InOffset, const uint32 InSize)
		: Guid(InGuid)
		, Offset(InOffset)
		, Size(InSize)
	{
	}

	FArchive& operator<<(FArchive& Ar, FChunkPart& ChunkPart)
	{
		if (Ar.IsError())
		{
			return Ar;
		}

		const int64 StartPos = Ar.Tell();
		uint32 DataSize = 0;

		Ar << DataSize;
		Ar << ChunkPart.Guid;
		Ar << ChunkPart.Offset;
		Ar << ChunkPart.Size;

		// If saving, we need to go back and set the data size.
		if (!Ar.IsError() && Ar.IsSaving())
		{
			const int64 EndPos = Ar.Tell();
			DataSize = EndPos - StartPos;
			Ar.Seek(StartPos);
			Ar << DataSize;
			Ar.Seek(EndPos);
		}

		// We must always make sure to seek the archive to the correct end location. Only seek if we must, to avoid a flush.
		const int64 DataLocation = StartPos + DataSize;
		if (Ar.Tell() != DataLocation)
		{
			Ar.Seek(DataLocation);
		}
		return Ar;
	}

	/* FFileChunkPart implementation
	*****************************************************************************/
	FFileChunkPart::FFileChunkPart()
		: Filename()
		, FileOffset(0)
		, ChunkPart()
	{
	}

	/**
	 * Enum which describes the chunk database header version.
	 */
	enum class EChunkDatabaseVersion : uint32
	{
		Invalid = 0,
		Original,

		// Always after the latest version, signifies the latest version plus 1 to allow initialization simplicity
		LatestPlusOne,
		Latest = (LatestPlusOne - 1)
	};

	// The constant minimum sizes for each version of a header struct. Must be updated.
	// If new member variables are added the version MUST be bumped and handled properly here,
	// and these values must never change.
	static const uint32 ChunkDatabaseHeaderVersionSizes[(uint32)EChunkDatabaseVersion::LatestPlusOne] =
	{
		// Dummy for indexing.
		0,
		// Version 1 is 24 bytes (32b Magic, 32b Version, 32b HeaderSize, 64b DataSize, 32b ChunkCount).
		24
	};

	FChunkDatabaseHeader::FChunkDatabaseHeader()
		: Version((uint32)EChunkDatabaseVersion::Latest)
		, HeaderSize(ChunkDatabaseHeaderVersionSizes[(uint32)EChunkDatabaseVersion::Latest])
		, Contents()
	{
	}

	FArchive& operator<<(FArchive& Ar, FChunkDatabaseHeader& Header)
	{
		if (Ar.IsError())
		{
			return Ar;
		}
		// Calculate how much space left in the archive for reading data (will be 0 when writing).
		const int64 StartPos = Ar.Tell();
		const int64 ArchiveSizeLeft = Ar.TotalSize() - StartPos;
		uint32 ExpectedSerializedBytes = 0;
		// Make sure the archive has enough data to read from, or we are saving instead.
		bool bSuccess = Ar.IsSaving() || (ArchiveSizeLeft >= ChunkDatabaseHeaderVersionSizes[(uint32)EChunkDatabaseVersion::Original]);
		if (bSuccess)
		{
			uint32 Magic = CHUNKDB_HEADER_MAGIC;
			// Chunk entry is 28 bytes (4x32b GUID, 64b FileStart, 32b FileSize).
			static const uint32 ChunkEntrySize = 28;
			int32 ChunkCount = Header.Contents.Num();
			Header.HeaderSize = ChunkDatabaseHeaderVersionSizes[(uint32)EChunkDatabaseVersion::Original] + (ChunkCount * ChunkEntrySize);
			Ar << Magic
			   << Header.Version
			   << Header.HeaderSize
			   << Header.DataSize
			   << ChunkCount;
			bSuccess = Magic == CHUNKDB_HEADER_MAGIC && !Ar.IsError();
			ExpectedSerializedBytes = ChunkDatabaseHeaderVersionSizes[(uint32)EChunkDatabaseVersion::Original];

			// Serialize all chunk info.
			if (bSuccess)
			{
				Header.Contents.SetNumZeroed(ChunkCount);
				for (int32 ChunkIdx = 0; ChunkIdx < ChunkCount; ++ChunkIdx)
				{
					FChunkLocation& Location = Header.Contents[ChunkIdx];
					Ar << Location.ChunkId
					   << Location.ByteStart
					   << Location.ByteSize;
				}
				ExpectedSerializedBytes += ChunkCount * ChunkEntrySize;
			}
		}

		// Make sure the expected number of bytes were serialized. In practice this will catch errors where type
		// serialization operators changed their format and that will need investigating.
		bSuccess = bSuccess && (Ar.Tell() - StartPos) == ExpectedSerializedBytes;

		if (bSuccess)
		{
			// Make sure the archive now points to data location. Only seek if we must, to avoid a flush.
			const int64 DataLocation = StartPos + Header.HeaderSize;
			if (Ar.Tell() != DataLocation)
			{
				Ar.Seek(DataLocation);
			}
		}
		else
		{
			// If we had a serialization error, zero out the header values.
			if (Ar.IsLoading())
			{
				HeaderHelpers::ZeroHeader(Header);
			}
		}

		return Ar;
	}


	FScopeLockedChunkData::FScopeLockedChunkData(IChunkDataAccess* InChunkDataAccess)
		: ChunkDataAccess(InChunkDataAccess)
	{
		ChunkDataAccess->GetDataLock(&ChunkData, &ChunkHeader);
	}

	FScopeLockedChunkData::~FScopeLockedChunkData()
	{
		ChunkDataAccess->ReleaseDataLock();
	}

	BuildPatchServices::FChunkHeader* FScopeLockedChunkData::GetHeader() const
	{
		return ChunkHeader;
	}

	uint8* FScopeLockedChunkData::GetData() const
	{
		return ChunkData;
	}


	class FChunkDataAccess : public IChunkDataAccess
	{
	public:
		FChunkDataAccess(uint32 DataSize)
		{
			ChunkData.Reserve(DataSize);
			ChunkData.SetNumUninitialized(DataSize);
		}

		~FChunkDataAccess()
		{
		}

		// IChunkDataAccess interface begin.
		virtual void GetDataLock(uint8** OutChunkData, FChunkHeader** OutChunkHeader) override
		{
			ThreadLock.Lock();
			if (OutChunkData)
			{
				(*OutChunkData) = ChunkData.GetData();
			}
			if (OutChunkHeader)
			{
				(*OutChunkHeader) = &ChunkHeader;
			}
		}
		virtual void GetDataLock(const uint8** OutChunkData, const FChunkHeader** OutChunkHeader) const override
		{
			ThreadLock.Lock();
			if (OutChunkData)
			{
				(*OutChunkData) = ChunkData.GetData();
			}
			if (OutChunkHeader)
			{
				(*OutChunkHeader) = &ChunkHeader;
			}
		}
		virtual void ReleaseDataLock() const override
		{
			ThreadLock.Unlock();
		}
		// IChunkDataAccess interface end.

	private:
		FChunkHeader ChunkHeader;
		TArray<uint8> ChunkData;
		mutable FCriticalSection ThreadLock;
	};

	IChunkDataAccess* FChunkDataAccessFactory::Create(uint32 DataSize)
	{
		return new FChunkDataAccess(DataSize);
	}


	class FChunkDataSerialization : public IChunkDataSerialization
	{
	public:
		FChunkDataSerialization(IFileSystem* InFileSystem, EFeatureLevel InFeatureLevel)
			: FileSystem(InFileSystem)
			, FeatureLevel(InFeatureLevel)
		{
		}

		~FChunkDataSerialization()
		{
		}

		// IChunkDataAccess interface begin.
		virtual IChunkDataAccess* LoadFromFile(const FString& Filename, EChunkLoadResult& OutLoadResult) const override
		{
			IChunkDataAccess* ChunkData = nullptr;
			// Read the chunk file.
			TUniquePtr<FArchive> FileReader(FileSystem->CreateFileReader(*Filename));
			bool bSuccess = FileReader.IsValid();
			if (bSuccess)
			{
				ChunkData = Load(*FileReader, OutLoadResult);
				// Close the file.
				FileReader->Close();
			}
			else
			{
				OutLoadResult = EChunkLoadResult::OpenFileFail;
			}

			return ChunkData;
		}

		virtual IChunkDataAccess* LoadFromMemory(const TArray<uint8>& Memory, EChunkLoadResult& OutLoadResult) const override
		{
			FMemoryReader MemoryReader(Memory);
			return Load(MemoryReader, OutLoadResult);
		}

		virtual IChunkDataAccess* LoadFromArchive(FArchive& Archive, EChunkLoadResult& OutLoadResult) const override
		{
			if (Archive.IsLoading())
			{
				return Load(Archive, OutLoadResult);
			}
			else
			{
				OutLoadResult = EChunkLoadResult::BadArchive;
				return nullptr;
			}
		}

		virtual EChunkSaveResult SaveToFile(const FString& Filename, const IChunkDataAccess* ChunkDataAccess) const override
		{
			EChunkSaveResult SaveResult;
			TUniquePtr<FArchive> FileOut(FileSystem->CreateFileWriter(*Filename));
			if (FileOut.IsValid())
			{
				SaveResult = SaveToArchive(*FileOut, ChunkDataAccess);
			}
			else
			{
				SaveResult = EChunkSaveResult::FileCreateFail;
			}
			return SaveResult;
		}

		virtual EChunkSaveResult SaveToMemory(TArray<uint8>& Memory, const IChunkDataAccess* ChunkDataAccess) const override
		{
			FMemoryWriter MemoryWriter(Memory);
			return Save(MemoryWriter, ChunkDataAccess);
		}

		virtual EChunkSaveResult SaveToArchive(FArchive& Archive, const IChunkDataAccess* ChunkDataAccess) const override
		{
			if (Archive.IsSaving())
			{
				return Save(Archive, ChunkDataAccess);
			}
			else
			{
				return EChunkSaveResult::BadArchive;
			}
		}

		virtual void InjectShaToChunkData(TArray<uint8>& Memory, const FSHAHash& ShaHashData) const override
		{
			const uint32 StoresShaAndHashTypeUint = (uint32)EChunkVersion::StoresShaAndHashType;
			const uint32 StoresShaAndHashTypeHeaderSize = ChunkHeaderVersionSizes[StoresShaAndHashTypeUint];
			FMemoryReader MemoryReader(Memory);
			FMemoryWriter MemoryWriter(Memory);

			FChunkHeader Header;
			MemoryReader << Header;
			Header.HashType |= BuildPatchServices::EChunkHashFlags::Sha1;
			Header.SHAHash = ShaHashData;
			if (Header.Version < StoresShaAndHashTypeUint)
			{
				check(Header.HeaderSize <= StoresShaAndHashTypeHeaderSize);
				Header.Version = StoresShaAndHashTypeUint;
				Memory.InsertZeroed(0, StoresShaAndHashTypeHeaderSize - Header.HeaderSize);
			}
			MemoryWriter << Header;
		}

		// IChunkDataAccess interface end.

	private:
		IChunkDataAccess* Load(FArchive& Reader, EChunkLoadResult& OutLoadResult) const
		{
			IChunkDataAccess* ChunkData = nullptr;

			// Begin of read pos.
			const int64 StartPos = Reader.Tell();

			// Available read size.
			const int64 AvailableSize = Reader.TotalSize() - StartPos;

			// Read and check the header.
			FChunkHeader HeaderCheck;
			Reader << HeaderCheck;

			// Get file size.
			const int64 FileSize = HeaderCheck.HeaderSize + HeaderCheck.DataSizeCompressed;

			if (HeaderCheck.Guid.IsValid())
			{
				if (HeaderCheck.HashType != EChunkHashFlags::None)
				{
					if ((HeaderCheck.HeaderSize + HeaderCheck.DataSizeCompressed) <= AvailableSize)
					{
						if ((HeaderCheck.StoredAs & EChunkStorageFlags::Encrypted) == EChunkStorageFlags::None)
						{
							// Create the data.
							ChunkData = FChunkDataAccessFactory::Create(FileSize);

							// Lock data.
							FChunkHeader* Header;
							uint8* Data;
							ChunkData->GetDataLock(&Data, &Header);
							*Header = HeaderCheck;

							// Read the data.
							Reader.Serialize(Data, Header->DataSizeCompressed);
							if (Reader.IsError() == false)
							{
								OutLoadResult = EChunkLoadResult::Success;
								// Decompress.
								if ((Header->StoredAs & EChunkStorageFlags::Compressed) != EChunkStorageFlags::None)
								{
									// Create a new data instance.
									IChunkDataAccess* NewChunkData = FChunkDataAccessFactory::Create(Header->DataSizeUncompressed);
									// Lock data.
									FChunkHeader* NewHeader;
									uint8* NewData;
									NewChunkData->GetDataLock(&NewData, &NewHeader);
									// Uncompress the memory.
									bool bSuccess = FCompression::UncompressMemory(
										NAME_Zlib,
										NewData,
										Header->DataSizeUncompressed,
										Data,
										Header->DataSizeCompressed);
									// If successful, switch over to new data.
									if (bSuccess)
									{
										FMemory::Memcpy(*NewHeader, *Header);
										NewHeader->StoredAs = EChunkStorageFlags::None;
										NewHeader->DataSizeCompressed = Header->DataSizeUncompressed;
										ChunkData->ReleaseDataLock();
										delete ChunkData;
										ChunkData = NewChunkData;
										Header = NewHeader;
										Data = NewData;
									}
									// Otherwise delete new data.
									else
									{
										OutLoadResult = EChunkLoadResult::DecompressFailure;
										NewChunkData->ReleaseDataLock();
										delete NewChunkData;
										NewChunkData = nullptr;
									}
								}
								// Verify.
								if (OutLoadResult == EChunkLoadResult::Success && (Header->HashType & EChunkHashFlags::RollingPoly64) != EChunkHashFlags::None)
								{
									if (Header->DataSizeCompressed != Header->DataSizeUncompressed || Header->RollingHash != FRollingHash::GetHashForDataSet(Data, Header->DataSizeUncompressed))
									{
										OutLoadResult = EChunkLoadResult::HashCheckFailed;
									}
								}
								FSHAHash ShaHashCheck;
								if (OutLoadResult == EChunkLoadResult::Success && (Header->HashType & EChunkHashFlags::Sha1) != EChunkHashFlags::None)
								{
									FSHA1::HashBuffer(Data, Header->DataSizeUncompressed, ShaHashCheck.Hash);
									if (!(ShaHashCheck == Header->SHAHash))
									{
										OutLoadResult = EChunkLoadResult::HashCheckFailed;
									}
								}
							}
							else
							{
								OutLoadResult = EChunkLoadResult::SerializationError;
							}
						}
						else
						{
							OutLoadResult = EChunkLoadResult::UnsupportedStorage;
						}
					}
					else
					{
						OutLoadResult = EChunkLoadResult::IncorrectFileSize;
					}
				}
				else
				{
					OutLoadResult = EChunkLoadResult::MissingHashInfo;
				}
			}
			else
			{
				OutLoadResult = EChunkLoadResult::CorruptHeader;
			}

			if (ChunkData != nullptr)
			{
				// Release data.
				ChunkData->ReleaseDataLock();

				// Delete data if failed
				if (OutLoadResult != EChunkLoadResult::Success)
				{
					delete ChunkData;
					ChunkData = nullptr;
				}
			}

			return ChunkData;
		}

		EChunkSaveResult Save(FArchive& Writer, const IChunkDataAccess* ChunkDataAccess) const
		{
			EChunkSaveResult SaveResult;
			const uint8* ChunkDataSource;
			const FChunkHeader* ChunkAccessHeader;
			ChunkDataAccess->GetDataLock(&ChunkDataSource, &ChunkAccessHeader);
			// Setup to handle compression.
			bool bDataIsCompressed = false;
			TArray<uint8> TempCompressedData;
			int32 CompressedSize = ChunkAccessHeader->DataSizeUncompressed;
			if (FeatureLevel >= EFeatureLevel::ChunkCompressionSupport)
			{
				TempCompressedData.Empty(ChunkAccessHeader->DataSizeUncompressed);
				TempCompressedData.AddUninitialized(ChunkAccessHeader->DataSizeUncompressed);
				// Compression can increase data size, too. This call will return false in that case.
				bDataIsCompressed = FCompression::CompressMemory(
					NAME_Zlib,
					TempCompressedData.GetData(),
					CompressedSize,
					ChunkDataSource,
					ChunkAccessHeader->DataSizeUncompressed,
					COMPRESS_BiasMemory);
			}

			// If compression succeeded, set data vars.
			if (bDataIsCompressed)
			{
				ChunkDataSource = TempCompressedData.GetData();
			}
			else
			{
				CompressedSize = ChunkAccessHeader->DataSizeUncompressed;
			}

			// Setup Header.
			FChunkHeader Header;
			FMemory::Memcpy(Header, *ChunkAccessHeader);
			Header.Version = (uint32)HeaderHelpers::FeatureLevelToChunkVersion(FeatureLevel);
			const int64 StartPos = Writer.Tell();
			Writer << Header;
			Header.HeaderSize = Writer.Tell() - StartPos;
			Header.StoredAs = bDataIsCompressed ? EChunkStorageFlags::Compressed : EChunkStorageFlags::None;
			Header.DataSizeCompressed = CompressedSize;
			Header.DataSizeUncompressed = ChunkAccessHeader->DataSizeUncompressed;
			// Make sure we at least have rolling hash.
			if ((Header.HashType & EChunkHashFlags::RollingPoly64) == EChunkHashFlags::None)
			{
				Header.HashType = EChunkHashFlags::RollingPoly64;
			}

			// Write out files.
			Writer.Seek(StartPos);
			Writer << Header;
			// We must use const_cast here due to FArchive::Serialize supporting both read and write. We created a writer.
			Writer.Serialize(const_cast<uint8*>(ChunkDataSource), CompressedSize);
			if (!Writer.IsError())
			{
				SaveResult = EChunkSaveResult::Success;
			}
			else
			{
				SaveResult = EChunkSaveResult::SerializationError;
			}
			ChunkDataAccess->ReleaseDataLock();
			return SaveResult;
		}

	private:
		IFileSystem* const FileSystem;
		const EFeatureLevel FeatureLevel;
	};

	IChunkDataSerialization* FChunkDataSerializationFactory::Create(IFileSystem* FileSystem, EFeatureLevel FeatureLevel)
	{
		check(FileSystem != nullptr);
		return new FChunkDataSerialization(FileSystem, FeatureLevel);
	}
}
