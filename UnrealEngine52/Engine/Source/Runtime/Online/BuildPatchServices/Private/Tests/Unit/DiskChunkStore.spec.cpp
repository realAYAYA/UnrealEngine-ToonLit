// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Tests/TestHelpers.h"
#include "Tests/Fake/FileSystem.fake.h"
#include "Tests/Fake/ChunkDataAccess.fake.h"
#include "Tests/Mock/ChunkDataSerialization.mock.h"
#include "Tests/Mock/DiskChunkStoreStat.mock.h"
#include "Installer/DiskChunkStore.h"
#include "BuildPatchHash.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FDiskChunkStoreSpec, "BuildPatchServices.Unit", EAutomationTestFlags::ProductFilter | EAutomationTestFlags::ApplicationContextMask)
// Unit
TUniquePtr<BuildPatchServices::IDiskChunkStore> DiskChunkStore;
// Mock
TUniquePtr<BuildPatchServices::FFakeFileSystem> FakeFileSystem;
TUniquePtr<BuildPatchServices::FMockChunkDataSerialization> MockChunkDataSerialization;
TUniquePtr<BuildPatchServices::FMockDiskChunkStoreStat> MockDiskChunkStoreStat;
TUniquePtr<BuildPatchServices::FFakeChunkDataAccess> FakeChunkDataAccessOne;
TUniquePtr<BuildPatchServices::FFakeChunkDataAccess> FakeChunkDataAccessTwo;
// Data
FString StoreRootPath;
FGuid SomeChunk;
bool bChunkOneWasDeleted;
bool bChunkTwoWasDeleted;
TArray<uint8> SomeData;
// Helpers
void MakeChunkData();
void MakeUnit();
int32 DiskDataNum();
END_DEFINE_SPEC(FDiskChunkStoreSpec)

#define DISK_STORE_TEST_TIMEOUT 1.0

void FDiskChunkStoreSpec::Define()
{
	using namespace BuildPatchServices;

	// Data setup.
	FRollingHashConst::Init();
	StoreRootPath = TEXT("RootPath");
	SomeChunk = FGuid::NewGuid();
	SomeData.AddUninitialized(64);

	// Specs.
	BeforeEach([this]()
	{
		FakeFileSystem.Reset(new FFakeFileSystem());
		MockChunkDataSerialization.Reset(new FMockChunkDataSerialization());
		MockDiskChunkStoreStat.Reset(new FMockDiskChunkStoreStat());
		FakeChunkDataAccessOne.Reset(new FFakeChunkDataAccess());
		FakeChunkDataAccessTwo.Reset(new FFakeChunkDataAccess());
		bChunkOneWasDeleted = false;
		bChunkTwoWasDeleted = false;
		FakeChunkDataAccessOne->OnDeleted = [this]()
		{
			bChunkOneWasDeleted = true;
		};
		FakeChunkDataAccessTwo->OnDeleted = [this]()
		{
			bChunkTwoWasDeleted = true;
		};
		MockChunkDataSerialization->SaveToArchiveFunc = [](FArchive& Ar, const IChunkDataAccess* ChunkPtr)
		{
			const FFakeChunkDataAccess* FakeChunkDataAccess = static_cast<const FFakeChunkDataAccess*>(ChunkPtr);
			Ar << *const_cast<FChunkHeader*>(&FakeChunkDataAccess->ChunkHeader);
			Ar.Serialize(FakeChunkDataAccess->ChunkData, FakeChunkDataAccess->ChunkHeader.DataSizeUncompressed);
			return BuildPatchServices::EChunkSaveResult::Success;
		};
		MakeChunkData();
		MakeUnit();
	});

	Describe("DiskChunkStore", [this]()
	{
		Describe("Construction", [this]()
		{
			It("should create a chunkdump file at provided path.", [this]()
			{
				if(TEST_BECOMES_TRUE(DiskDataNum() == 1, DISK_STORE_TEST_TIMEOUT))
				{
					FScopeLock ScopeLock(&FakeFileSystem->ThreadLock);
					TEST_TRUE(FakeFileSystem->DiskData.CreateConstIterator().Key().StartsWith(StoreRootPath / TEXT("")));
				}
			});

			Describe("when there are errors opening the chunkdump", [this]()
			{
				BeforeEach([this]()
				{
					DiskChunkStore.Reset();
					FakeFileSystem->CreateFileReaderFunc = [this](const TCHAR*, EReadFlags) -> TUniquePtr<FArchive>
					{
						if (FakeFileSystem->RxCreateFileReader.Num() == 10)
						{
							FakeFileSystem->CreateFileReaderFunc = nullptr;
						}
						return nullptr;
					};
					FakeFileSystem->CreateFileWriterFunc = [this](const TCHAR*, EWriteFlags) -> TUniquePtr<FArchive>
					{
						if (FakeFileSystem->RxCreateFileWriter.Num() == 10)
						{
							FakeFileSystem->CreateFileWriterFunc = nullptr;
						}
						return nullptr;
					};
					FakeFileSystem->RxCreateFileReader.Reset();
					FakeFileSystem->RxCreateFileWriter.Reset();
					MakeUnit();
				});

				It("should retry until successful.", [this]()
				{
					TEST_BECOMES_TRUE(FakeFileSystem->RxCreateFileWriter.Num() == 12, DISK_STORE_TEST_TIMEOUT);
					TEST_BECOMES_TRUE(FakeFileSystem->RxCreateFileReader.Num() == 12, DISK_STORE_TEST_TIMEOUT);
				});
			});
		});

		Describe("Destruction", [this]()
		{
			It("should delete the chunkdump file created.", [this]()
			{
				DiskChunkStore.Reset();
				TEST_TRUE(DiskDataNum() == 0);
			});

			Describe("when there are still queued requests", [this]()
			{
				BeforeEach([this]()
				{
					MockChunkDataSerialization->SaveToArchiveFunc = [](FArchive&, const IChunkDataAccess*)
					{
						FPlatformProcess::Sleep(0.5);
						return BuildPatchServices::EChunkSaveResult::Success;
					};
					DiskChunkStore->Put(FGuid::NewGuid(), MoveTemp(FakeChunkDataAccessOne));
					DiskChunkStore->Put(FGuid::NewGuid(), MoveTemp(FakeChunkDataAccessTwo));
				});

				It("should clean up all queued put memory.", [this]()
				{
					DiskChunkStore.Reset();
					TEST_BECOMES_TRUE(bChunkOneWasDeleted, DISK_STORE_TEST_TIMEOUT);
					TEST_BECOMES_TRUE(bChunkTwoWasDeleted, DISK_STORE_TEST_TIMEOUT);
				});
			});
		});

		Describe("Put", [this]()
		{
			It("should release chunk data once saved.", [this]()
			{
				DiskChunkStore->Put(SomeChunk, MoveTemp(FakeChunkDataAccessOne));
				TEST_BECOMES_TRUE(bChunkOneWasDeleted, DISK_STORE_TEST_TIMEOUT);
			});

			It("should save some chunk to the chunkdump.", [this]()
			{
				DiskChunkStore->Put(SomeChunk, MoveTemp(FakeChunkDataAccessOne));
				TEST_BECOMES_TRUE(MockChunkDataSerialization->RxSaveToArchive.Num() == 1, DISK_STORE_TEST_TIMEOUT);
			});

			It("should not save some chunk that was previously saved.", [this]()
			{
				DiskChunkStore->Put(SomeChunk, MoveTemp(FakeChunkDataAccessOne));
				DiskChunkStore->Put(SomeChunk, MoveTemp(FakeChunkDataAccessTwo));
				TEST_BECOMES_TRUE(bChunkOneWasDeleted, DISK_STORE_TEST_TIMEOUT);
				TEST_BECOMES_TRUE(bChunkTwoWasDeleted, DISK_STORE_TEST_TIMEOUT);
				TEST_EQUAL(MockChunkDataSerialization->RxSaveToArchive.Num(), 1);
			});

			It("should cause the reader to be reopened ready for a Get.", [this]()
			{
				DiskChunkStore->Put(SomeChunk, MoveTemp(FakeChunkDataAccessOne));
				TEST_BECOMES_TRUE(FakeFileSystem->RxCreateFileReader.Num() == 2, DISK_STORE_TEST_TIMEOUT);
			});
		});

		Describe("Get", [this]()
		{
			Describe("when some chunk was not previously Put", [this]()
			{
				It("should not attempt to load some chunk.", [this]()
				{
					TEST_NULL(DiskChunkStore->Get(SomeChunk));
					TEST_EQUAL(MockChunkDataSerialization->RxLoadFromArchive.Num(), 0);
				});
			});

			Describe("when some chunk was previously Put", [this]()
			{
				BeforeEach([this]()
				{
					DiskChunkStore->Put(SomeChunk, MoveTemp(FakeChunkDataAccessOne));
				});

				It("should load some chunk from the chunkdump.", [this]()
				{
					DiskChunkStore->Get(SomeChunk);
					TEST_EQUAL(MockChunkDataSerialization->RxLoadFromArchive.Num(), 1);
				});

				It("should enforce the reader to have been reopened.", [this]()
				{
					DiskChunkStore->Get(SomeChunk);
					TEST_EQUAL(FakeFileSystem->RxCreateFileReader.Num(), 2);
				});

				Describe("and LoadFromArchive will be successful", [this]()
				{
					BeforeEach([this]()
					{
						MockChunkDataSerialization->TxLoadFromArchive.Emplace(FakeChunkDataAccessOne.Release(), EChunkLoadResult::Success);
					});

					It("should not load some chunk twice in a row.", [this]()
					{
						TEST_EQUAL(DiskChunkStore->Get(SomeChunk), DiskChunkStore->Get(SomeChunk));
						TEST_EQUAL(MockChunkDataSerialization->RxLoadFromArchive.Num(), 1);
					});
				});

				Describe("and LoadFromArchive will not be successful", [this]()
				{
					BeforeEach([this]()
					{
						MockChunkDataSerialization->TxLoadFromArchive.Emplace(nullptr, EChunkLoadResult::SerializationError);
					});

					It("should return nullptr.", [this]()
					{
						TEST_NULL(DiskChunkStore->Get(SomeChunk));
					});

					It("should only attempt to load some chunk once.", [this]()
					{
						DiskChunkStore->Get(SomeChunk);
						DiskChunkStore->Get(SomeChunk);
						TEST_EQUAL(MockChunkDataSerialization->RxLoadFromArchive.Num(), 1);
					});
				});
			});
		});

		Describe("Remove", [this]()
		{
			Describe("when some chunk was not previously Put", [this]()
			{
				It("should not attempt to load some chunk.", [this]()
				{
					TUniquePtr<IChunkDataAccess> Removed = DiskChunkStore->Remove(SomeChunk);
					TEST_FALSE(Removed.IsValid());
					TEST_EQUAL(MockChunkDataSerialization->RxLoadFromArchive.Num(), 0);
				});
			});

			Describe("when some chunk was previously Put", [this]()
			{
				BeforeEach([this]()
				{
					FFakeChunkDataAccess* FakeSomeChunk = new FFakeChunkDataAccess();
					FakeSomeChunk->ChunkHeader.Guid = SomeChunk;
					DiskChunkStore->Put(SomeChunk, TUniquePtr<IChunkDataAccess>(FakeSomeChunk));
				});

				It("should load some chunk from the chunkdump.", [this]()
				{
					DiskChunkStore->Remove(SomeChunk);
					TEST_EQUAL(MockChunkDataSerialization->RxLoadFromArchive.Num(), 1);
				});

				Describe("and LoadFromArchive will be successful", [this]()
				{
					BeforeEach([this]()
					{
						FakeChunkDataAccessOne->ChunkHeader.Guid = SomeChunk;
						MockChunkDataSerialization->TxLoadFromArchive.Emplace(FakeChunkDataAccessOne.Release(), EChunkLoadResult::Success);
						FakeChunkDataAccessOne.Reset(new FFakeChunkDataAccess());
					});

					Describe("and when some chunk was last used with Get", [this]()
					{
						BeforeEach([this]()
						{
							DiskChunkStore->Get(SomeChunk);
							MockChunkDataSerialization->RxLoadFromArchive.Empty();
						});

						It("should return some chunk without loading it.", [this]()
						{
							TUniquePtr<IChunkDataAccess> Removed = DiskChunkStore->Remove(SomeChunk);
							TEST_TRUE(Removed.IsValid());
							TEST_EQUAL(MockChunkDataSerialization->RxLoadFromArchive.Num(), 0);
						});
					});

					Describe("and when some chunk was last used with Remove", [this]()
					{
						BeforeEach([this]()
						{
							DiskChunkStore->Remove(SomeChunk);
							MockChunkDataSerialization->TxLoadFromArchive.Emplace(FakeChunkDataAccessOne.Release(), EChunkLoadResult::Success);
							FakeChunkDataAccessOne.Reset(new FFakeChunkDataAccess());
							MockChunkDataSerialization->RxLoadFromArchive.Empty();
						});

						It("should need to reload some chunk.", [this]()
						{
							TUniquePtr<IChunkDataAccess> Removed = DiskChunkStore->Remove(SomeChunk);
							TEST_TRUE(Removed.IsValid());
							TEST_EQUAL(MockChunkDataSerialization->RxLoadFromArchive.Num(), 1);
						});
					});
				});

				Describe("and LoadFromArchive will not be successful", [this]()
				{
					BeforeEach([this]()
					{
						MockChunkDataSerialization->TxLoadFromArchive.Emplace(nullptr, EChunkLoadResult::SerializationError);
					});

					It("should return invalid ptr.", [this]()
					{
						TEST_FALSE(DiskChunkStore->Remove(SomeChunk).IsValid());
					});

					It("should only attempt to load some chunk once.", [this]()
					{
						DiskChunkStore->Remove(SomeChunk);
						DiskChunkStore->Remove(SomeChunk);
						TEST_EQUAL(MockChunkDataSerialization->RxLoadFromArchive.Num(), 1);
					});
				});
			});
		});
	});

	AfterEach([this]()
	{
		DiskChunkStore.Reset();
		FakeChunkDataAccessOne.Reset();
		FakeChunkDataAccessTwo.Reset();
		MockChunkDataSerialization.Reset();
		FakeFileSystem.Reset();
	});
}

void FDiskChunkStoreSpec::MakeChunkData()
{
	FakeChunkDataAccessOne->ChunkData = SomeData.GetData();
	FakeChunkDataAccessTwo->ChunkData = SomeData.GetData();
	FakeChunkDataAccessOne->ChunkHeader.DataSizeCompressed = SomeData.Num();
	FakeChunkDataAccessTwo->ChunkHeader.DataSizeCompressed = SomeData.Num();
	FakeChunkDataAccessOne->ChunkHeader.DataSizeUncompressed = SomeData.Num();
	FakeChunkDataAccessTwo->ChunkHeader.DataSizeUncompressed = SomeData.Num();
}

void FDiskChunkStoreSpec::MakeUnit()
{
	using namespace BuildPatchServices;
	FDiskChunkStoreConfig DiskChunkStoreConfig(StoreRootPath);
	DiskChunkStoreConfig.MaxRetryTime = 0.01;
	DiskChunkStore.Reset(FDiskChunkStoreFactory::Create(
		FakeFileSystem.Get(),
		MockChunkDataSerialization.Get(),
		MockDiskChunkStoreStat.Get(),
		MoveTemp(DiskChunkStoreConfig)));
}

int32 FDiskChunkStoreSpec::DiskDataNum()
{
	FScopeLock ScopeLock(&FakeFileSystem->ThreadLock);
	return FakeFileSystem->DiskData.Num();
}

#endif //WITH_DEV_AUTOMATION_TESTS
