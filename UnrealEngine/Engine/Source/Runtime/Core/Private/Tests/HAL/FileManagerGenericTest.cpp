// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManagerGeneric.h"

#include "Containers/UnrealString.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"
#include "Misc/StringBuilder.h"

class FArchiveFileReaderGenericTest : public FAutomationTestBase
{
public:
	using FAutomationTestBase::FAutomationTestBase;
	~FArchiveFileReaderGenericTest();
	void TestInternalPrecache();

private:
	bool CreateTestFile();
	void TestBytesValid(const TCHAR* What, uint8* Data, int64 NumBytes, int64 FileOffset);
	int32 GetExpectedValue(int64 IntOffset);
	void SetPosAndBuffer(TUniquePtr<FArchiveFileReaderGeneric>& Reader, int64 Pos, int64 BufferBase, int64 BufferSize);

	FString TestFileName;
	TUniquePtr<IFileHandle> ReadHandle;
	int64 FileSize;
};

FArchiveFileReaderGenericTest::~FArchiveFileReaderGenericTest()
{
	ReadHandle.Reset();
	if (!TestFileName.IsEmpty())
	{
		IFileManager& FileManager = IFileManager::Get();
		FileManager.Delete(*TestFileName, false /* bRequireExists */, true /* bEvenReadOnly */, true /* Quiet */);
	}
	TestFileName.Empty();
}

void FArchiveFileReaderGenericTest::TestInternalPrecache()
{
	if (!CreateTestFile())
	{
		return;
	}

	uint32 BufferSize = 1024;
	TArray<uint8> UnusedBytes;
	UnusedBytes.SetNumUninitialized(BufferSize);
	TUniquePtr<FArchiveFileReaderGeneric> Reader(new FArchiveFileReaderGeneric(ReadHandle.Release(), *TestFileName, FileSize, BufferSize));
	TestEqual(TEXT("Initial Pos should be 0"), Reader->Pos, 0LL);
	TestEqual(TEXT("Size should be what was passed in"), Reader->Size, FileSize);
	TestEqual(TEXT("BufferSize should be what was passed in"), Reader->BufferSize, BufferSize);

	// Vanilla InternalPrecache at start of file
	bool Result = Reader->InternalPrecache(0, BufferSize);
	TestEqual(TEXT("Vanilla0 - InternalPrecache should succeed"), Result, true);
	TestEqual(TEXT("Vanilla0 - InternalPrecache should not alter Pos"), Reader->Pos, 0LL);
	TestEqual(TEXT("Vanilla0 - BufferBase should have be set to PrecacheOffset aka Pos"), Reader->BufferBase, 0LL);
	TestEqual(TEXT("Vanilla0 - Bytes precached should be set to BufferSize unless it runs out of room"), Reader->BufferArray.Num(), BufferSize);
	TestBytesValid(TEXT("Vanilla0 - BufferArray should be expected bytes"), Reader->BufferArray.GetData(), Reader->BufferArray.Num(), Reader->BufferBase);

	// InternalPrecache at PrecacheOffset != Pos is ignored, and returns false if the first byte at PrecacheOffset is not cached
	Result = Reader->InternalPrecache(BufferSize  + BufferSize, BufferSize);
	TestEqual(TEXT("PrecacheNotAtPos - Should fail"), Result, false);
	TestEqual(TEXT("PrecacheNotAtPos - BufferBase should not be altered"), Reader->BufferBase, 0LL);
	TestEqual(TEXT("PrecacheNotAtPos - Buffe should not be altered"), Reader->BufferArray.Num(), BufferSize);

	// InternalPrecache at PrecacheOffset != Pos is ignored, but returns true if the first byte at PrecacheOffset is cached
	Result = Reader->InternalPrecache(BufferSize - 1, BufferSize);
	TestEqual(TEXT("PrecacheNotAtPos - Should succeed since the first byte at PrecacheOffset is buffered"), Result, true);
	TestEqual(TEXT("PrecacheNotAtPos - BufferBase should not be altered"), Reader->BufferBase, 0LL);
	TestEqual(TEXT("PrecacheNotAtPos - Buffe should not be altered"), Reader->BufferArray.Num(), BufferSize);

	// InternalPrecache partway through the buffer should allocate the full buffer/not allocate anything, depending on bPrecacheAsSoonAsPossible
	int64 PosStart = BufferSize / 2;
	SetPosAndBuffer(Reader, PosStart, 0, BufferSize);
	Result = Reader->InternalPrecache(PosStart, BufferSize);
	TestEqual(TEXT("PrecachePartwayPosMoreThanBuffer - Should succeed"), Result, true);
	if (FArchiveFileReaderGeneric::bPrecacheAsSoonAsPossible)
	{
		TestEqual(TEXT("PrecachePartwayPosMoreThanBuffer - BufferBase should be updated"), Reader->BufferBase, PosStart);
	}
	else
	{
		TestEqual(TEXT("PrecachePartwayPosMoreThanBuffer - BufferBase should not be updated"), Reader->BufferBase, 0LL);
	}
	TestEqual(TEXT("PrecachePartwayPosMoreThanBuffer - BufferCount should be set to BufferSize"), Reader->BufferArray.Num(), BufferSize);
	TestBytesValid(TEXT("PrecachePartwayPosMoreThanBuffer - BufferBytes should match BufferBase"), Reader->BufferArray.GetData(), Reader->BufferArray.Num(), Reader->BufferBase);

	// InternalPrecache partway through the buffer should allocate the full buffer/not allocate anything, and should handle the case of BufferArray's allocation being smaller than BufferSize
	SetPosAndBuffer(Reader, 3*BufferSize/4, BufferSize/2, BufferSize/2);
	Reader->BufferArray.Shrink();
	Result = Reader->InternalPrecache(3*BufferSize/4, BufferSize);
	TestEqual(TEXT("PrecachePartwayPosMoreThanBufferReallocation - Should succeed"), Result, true);
	if (FArchiveFileReaderGeneric::bPrecacheAsSoonAsPossible)
	{
		TestEqual(TEXT("PrecachePartwayPosMoreThanBufferReallocation - BufferBase should be updated"), Reader->BufferBase, 3*BufferSize/4);
		TestEqual(TEXT("PrecachePartwayPosMoreThanBufferReallocation - BufferCount should be set to BufferSize"), Reader->BufferArray.Num(), BufferSize);
	}
	else
	{
		TestEqual(TEXT("PrecachePartwayPosMoreThanBufferReallocation - BufferBase should not be updated"), Reader->BufferBase, BufferSize/2);
		TestEqual(TEXT("PrecachePartwayPosMoreThanBufferReallocation - BufferCount should not be updated"), Reader->BufferArray.Num(), BufferSize/2);
	}
	TestBytesValid(TEXT("PrecachePartwayPosMoreThanBufferReallocation - BufferBytes should match BufferBase"), Reader->BufferArray.GetData(), Reader->BufferArray.Num(), Reader->BufferBase);

	// InternalPrecache partway through the buffer, when the requested size is within the remaining buffer, should not change the buffer
	PosStart = BufferSize / 2;
	SetPosAndBuffer(Reader, PosStart, 0, BufferSize);
	Result = Reader->InternalPrecache(PosStart, BufferSize / 4);
	TestEqual(TEXT("PrecachePartwayPosLessThanBuffer - Should succeed"), Result, true);
	TestEqual(TEXT("PrecachePartwayPosLessThanBuffer - BufferBase should not be updated"), Reader->BufferBase, 0LL);
	TestEqual(TEXT("PrecachePartwayPosLessThanBuffer - BufferCount should be set to BufferSize"), Reader->BufferArray.Num(), BufferSize);
	TestBytesValid(TEXT("PrecachePartwayPosLessThanBuffer - BufferBytes should match BufferBase"), Reader->BufferArray.GetData(), Reader->BufferArray.Num(), Reader->BufferBase);

	// InternalPrecache at the end of the buffer should allocate the full buffer
	PosStart = BufferSize;
	SetPosAndBuffer(Reader, PosStart, 0, BufferSize);
	Result = Reader->InternalPrecache(PosStart, BufferSize);
	TestEqual(TEXT("PrecachePosEndOfBuffer - Should succeed"), Result, true);
	TestEqual(TEXT("PrecachePosEndOfBuffer - BufferBase should be updated"), Reader->BufferBase, PosStart);
	TestEqual(TEXT("PrecachePosEndOfBuffer - BufferCount should be set to BufferSize"), Reader->BufferArray.Num(), BufferSize);
	TestBytesValid(TEXT("PrecachePosEndOfBuffer - BufferBytes should match BufferBase"), Reader->BufferArray.GetData(), Reader->BufferArray.Num(), Reader->BufferBase);

	// InternalPrecache near the end of the file should clamp BufferSize to Size - Pos
	PosStart = FileSize - 1;
	SetPosAndBuffer(Reader, PosStart, 0, 0);
	Result = Reader->InternalPrecache(PosStart, BufferSize);
	TestEqual(TEXT("PrecacheNearEndOfFile - Should succeed"), Result, true);
	TestEqual(TEXT("PrecacheNearEndOfFile - BufferBase should be updated"), Reader->BufferBase, PosStart);
	TestEqual(TEXT("PrecacheNearEndOfFile - BufferCount should be set Min(BufferSize, Size - Pos)"), Reader->BufferArray.Num(), 1LL);
	TestBytesValid(TEXT("PrecacheNearEndOfFile - BufferBytes should match BufferBase"), Reader->BufferArray.GetData(), Reader->BufferArray.Num(), Reader->BufferBase);

	AddExpectedError(TEXT("ReadFile failed"));
	// InternalPrecache at the end of the file should fail
	SetPosAndBuffer(Reader, FileSize, 0, 16);
	Result = Reader->InternalPrecache(FileSize, BufferSize);
	TestEqual(TEXT("PrecacheEndOfFile - Should fail"), Result, false);
	TestEqual(TEXT("PrecacheEndOfFile - BufferBase should not be updated"), Reader->BufferBase, 0LL);
	TestEqual(TEXT("PrecacheEndOfFile - BufferCount should not be updated"), Reader->BufferArray.Num(), 16LL);

	// If ReadLowLevel fails, InternalPrecache should return true if the first byte of PrecacheOffset is in the buffer
	SetPosAndBuffer(Reader, 8, 0, 16);
	Reader->SeekLowLevel(FileSize);
	Result = Reader->InternalPrecache(8, 16);
	TestEqual(TEXT("PrecacheReadFailsBytesRemain - Should succeed if bytes are left in the buffer"), Result, true);
	if (FArchiveFileReaderGeneric::bPrecacheAsSoonAsPossible)
	{
		TestEqual(TEXT("PrecacheReadFailsBytesRemain - BufferBase should be updated"), Reader->BufferBase, 8LL);
		TestEqual(TEXT("PrecacheReadFailsBytesRemain - BufferCount should be updated"), Reader->BufferArray.Num(), 8LL);
	}
	else
	{
		TestEqual(TEXT("PrecacheReadFailsBytesRemain - BufferBase should not be updated"), Reader->BufferBase, 0LL);
		TestEqual(TEXT("PrecacheReadFailsBytesRemain - BufferCount should not be updated"), Reader->BufferArray.Num(), 16LL);
	}

	// If ReadLowLevel fails, InternalPrecache should return false if the first byte of PrecacheOffset is not in the buffer
	SetPosAndBuffer(Reader, 16, 0, 16);
	Reader->SeekLowLevel(FileSize);
	Result = Reader->InternalPrecache(16, 16);
	TestEqual(TEXT("PrecacheReadFails - Should fail if bytes are not left in the buffer"), Result, false);
	TestEqual(TEXT("PrecacheReadFails - BufferCount should be updated to empty"), Reader->BufferArray.Num(), 0LL);

	// TODO: Call ClearExpectedErrors to clear the AddExpectedError called above when it is implemented. Until then, make sure these failure-testing cases are the last
	// cases tested in the test
	//ClearExpectedErrors();
}

bool FArchiveFileReaderGenericTest::CreateTestFile()
{
	IFileManager& FileManager = IFileManager::Get();
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();;
	TestFileName = FPaths::CreateTempFilename(FPlatformProcess::UserTempDir(), TEXT("ArchiveFileReaderGenericTest"));
	TUniquePtr<IFileHandle> WriteHandle(PlatformFile.OpenWrite(*TestFileName, false /* bAppend */, false /* bAllowRead */));
	if (!WriteHandle)
	{
		AddError(FString::Printf(TEXT("Could not create test file %s."), *TestFileName));
		return false;
	}
	TArray<int32> Builder;
	int32 NumInts = 1024 * 128 + 17; // Make sure the file size is not a multiple of buffersize; we need to test that behavior
	Builder.SetNumUninitialized(NumInts);
	int32* Data = Builder.GetData();
	for (int n = 0; n < Builder.Num(); ++n)
	{
		Data[n] = GetExpectedValue(n);
	}
	FileSize = NumInts * sizeof(int32);
	WriteHandle->Write(reinterpret_cast<uint8*>(Data), FileSize);
	WriteHandle.Reset();

	ReadHandle.Reset(PlatformFile.OpenRead(*TestFileName, false /* bAllowWrite */));
	if (!ReadHandle)
	{
		AddError(FString::Printf(TEXT("Could not open test file %s."), *TestFileName));
		return false;
	}
	if (ReadHandle->Size() != FileSize)
	{
		AddError(FString::Printf(TEXT("Received incorrect file size from test file %s. Expected = %lld. Actual = %lld."), *TestFileName, FileSize, ReadHandle->Size()));
		return false;
	}

	return true;
}

void FArchiveFileReaderGenericTest::TestBytesValid(const TCHAR* What, uint8* InData, int64 InNumBytes, int64 InFileOffset)
{
	uint8* Data = InData;
	int64 NumBytes = InNumBytes;
	int64 FileOffset = InFileOffset;

	int64 StartByte = FileOffset % sizeof(int32);
	if (StartByte != 0)
	{
		int64 FloorIndex = (FileOffset - StartByte)/sizeof(int32);
		int64 ExpectedValue = GetExpectedValue(FloorIndex);
		for (int n = 0; n < NumBytes && n < sizeof(int32) - StartByte; ++n)
		{
			if (Data[n] != reinterpret_cast<uint8*>(&ExpectedValue)[StartByte + n])
			{
				TestEqual(What, Data[n], reinterpret_cast<uint8*>(&ExpectedValue)[StartByte + n]);
				return;
			}
		}
		if (NumBytes < static_cast<int64>(sizeof(int32) - StartByte))
		{
			return;
		}
		int32 BytesRead = sizeof(int32) - StartByte;
		Data += BytesRead;
		FileOffset += BytesRead;
		NumBytes -= BytesRead;
	}
	int32* IntData = reinterpret_cast<int32*>(Data);
	int64 IntOffset = FileOffset / sizeof(int32);
	int64 NumInts = NumBytes / sizeof(int32);
	for (int n = 0; n < NumInts; ++n)
	{
		if (IntData[n] != GetExpectedValue(IntOffset + n))
		{
			TestEqual(What, IntData[n], GetExpectedValue(IntOffset + n));
			return;
		}
	}

	int64 NumEndBytes = NumBytes - NumInts * sizeof(int32);
	if (NumEndBytes != 0)
	{
		uint8* DataStart = Data + NumInts * sizeof(int32);
		int32 ExpectedValue = GetExpectedValue(IntOffset + NumInts);
		for (int n = 0; n < NumEndBytes; ++n)
		{
			if (DataStart[n] != reinterpret_cast<uint8*>(&ExpectedValue)[n])
			{
				TestEqual(What, DataStart[n], reinterpret_cast<uint8*>(&ExpectedValue)[n]);
				return;
			}
		}
	}
}

void FArchiveFileReaderGenericTest::SetPosAndBuffer(TUniquePtr<FArchiveFileReaderGeneric>& Reader, int64 Pos, int64 BufferBase, int64 BufferSize)
{
	Reader->Pos = Pos;
	Reader->BufferBase = BufferBase;
	Reader->BufferArray.SetNumUninitialized(BufferSize, false /* bAllowShrink */);
	if (BufferSize)
	{
		Reader->SeekLowLevel(BufferBase);
		int64 BytesRead;
		Reader->ReadLowLevel(Reader->BufferArray.GetData(), BufferSize, BytesRead);
		TestEqual(TEXT("SetPosAndBuffer - ReadLowLevel read the requested bytes"), BytesRead, BufferSize);
	}
	// See the contract for Buffer window and LowLevel Pos in the variable comment on BufferArray
	bool bPosWithinBuffer = BufferBase <= Pos && Pos < BufferBase + BufferSize;
	if (!bPosWithinBuffer)
	{
		Reader->SeekLowLevel(Pos);
	}
	TestEqual(TEXT("SetPosAndBuffer - Pos set correctly"), Reader->Pos, Pos);
	TestEqual(TEXT("SetPosAndBuffer - BufferBase set correctly"), Reader->BufferBase, BufferBase);
	TestEqual(TEXT("SetPosAndBuffer - BufferSize set correctly"), Reader->BufferArray.Num(), BufferSize);
	TestBytesValid(TEXT("SetPosAndBuffer - BufferBytes should match BufferBase"), Reader->BufferArray.GetData(), Reader->BufferArray.Num(), Reader->BufferBase);
}

int32 FArchiveFileReaderGenericTest::GetExpectedValue(int64 IntOffset)
{
	return 0xbe000000 + static_cast<int32>(IntOffset);
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FArchiveFileReaderGenericTestRunner, FArchiveFileReaderGenericTest, "System.Core.HAL.FArchiveFileReaderGeneric", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FArchiveFileReaderGenericTestRunner::RunTest(const FString& Parameters)
{
	TestInternalPrecache();
	return !HasAnyErrors();
}

#endif
