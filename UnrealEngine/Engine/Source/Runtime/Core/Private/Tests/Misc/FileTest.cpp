// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "HAL/PlatformFile.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"
#include "Misc/ScopeExit.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// These file tests are designed to ensure expected file writing behavior, as well as cross-platform consistency

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFileTruncateTest, "System.Core.Misc.FileTruncate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CriticalPriority | EAutomationTestFlags::EngineFilter)
bool FFileTruncateTest::RunTest(const FString& Parameters)
{
	const FString TempFilename = FPaths::CreateTempFilename(*FPaths::ProjectIntermediateDir());
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	ON_SCOPE_EXIT
	{
		// Delete temp file
		PlatformFile.DeleteFile(*TempFilename);
	};

	// Open a test file
	if (TUniquePtr<IFileHandle> TestFile = TUniquePtr<IFileHandle>(PlatformFile.OpenWrite(*TempFilename, /*bAppend*/false, /*bAllowRead*/true)))
	{
		// Append 4 int32 values of incrementing value to this file
		int32 Val = 1;
		TestFile->Write((const uint8*)&Val, sizeof(Val));

		++Val;
		TestFile->Write((const uint8*)&Val, sizeof(Val));

		// Tell here, so we can move back and truncate after writing
		const int64 ExpectedTruncatePos = TestFile->Tell();
		++Val;
		TestFile->Write((const uint8*)&Val, sizeof(Val));

		// Tell here, so we can attempt to read here after truncation
		const int64 TestReadPos = TestFile->Tell();
		++Val;
		TestFile->Write((const uint8*)&Val, sizeof(Val));

		// Validate that the Tell position is at the end of the file, and that the size is reported correctly
		{
			const int64 ActualEOFPos = TestFile->Tell();
			const int64 ExpectedEOFPos = (sizeof(int32) * 4);
			if (ActualEOFPos != ExpectedEOFPos)
			{
				AddError(FString::Printf(TEXT("File was not the expected size (got %d, expected %d): %s"), ActualEOFPos, ExpectedEOFPos, *TempFilename));
				return false;
			}

			const int64 ActualFileSize = TestFile->Size();
			if (ActualFileSize != ExpectedEOFPos)
			{
				AddError(FString::Printf(TEXT("File was not the expected size (got %d, expected %d): %s"), ActualFileSize, ExpectedEOFPos, *TempFilename));
				return false;
			}
		}

		// Truncate the file at our test pos
		if (!TestFile->Truncate(ExpectedTruncatePos))
		{
			AddError(FString::Printf(TEXT("File truncation request failed: %s"), *TempFilename));
			return false;
		}

		// Validate that the size is reported correctly
		{
			const int64 ActualFileSize = TestFile->Size();
			if (ActualFileSize != ExpectedTruncatePos)
			{
				AddError(FString::Printf(TEXT("File was not the expected size after truncation (got %d, expected %d): %s"), ActualFileSize, ExpectedTruncatePos, *TempFilename));
				return false;
			}
		}

		// Validate that we can't read past the truncation point
		{
			int32 Dummy = 0;
			if (TestFile->Seek(TestReadPos) && TestFile->Read((uint8*)&Dummy, sizeof(Dummy)))
			{
				AddError(FString::Printf(TEXT("File read seek outside the truncated range: %s"), *TempFilename));
				return false;
			}
		}
	}
	else
	{
		AddError(FString::Printf(TEXT("File failed to open: %s"), *TempFilename));
		return false;
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFileAppendTest, "System.Core.Misc.FileAppend", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CriticalPriority | EAutomationTestFlags::EngineFilter)
bool FFileAppendTest::RunTest( const FString& Parameters )
{
	const FString TempFilename = FPaths::CreateTempFilename(*FPaths::ProjectIntermediateDir());
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	ON_SCOPE_EXIT
	{
		// Delete temp file
		PlatformFile.DeleteFile(*TempFilename);
	};

	// Scratch data for testing
	uint8 One = 1;
	TArray<uint8> TestData;

	// Check a new file can be created
	if (TUniquePtr<IFileHandle> TestFile = TUniquePtr<IFileHandle>(PlatformFile.OpenWrite(*TempFilename, /*bAppend*/false, /*bAllowRead*/true)))
	{
		TestData.AddZeroed(64);

		TestFile->Write(TestData.GetData(), TestData.Num());
	}
	else
	{
		AddError(FString::Printf(TEXT("File failed to open when new: %s"), *TempFilename));
		return false;
	}

	// Confirm same data
	{
		TArray<uint8> ReadData;
		if (!FFileHelper::LoadFileToArray(ReadData, *TempFilename))
		{
			AddError(FString::Printf(TEXT("File failed to load after writing: %s"), *TempFilename));
			return false;
		}

		if (ReadData != TestData)
		{
			AddError(FString::Printf(TEXT("File data was incorrect after writing: %s"), *TempFilename));
			return false;
		}
	}

	// Using append flag should open the file, and writing data immediately should append to the end.
	// We should also be capable of seeking writing.
	if (TUniquePtr<IFileHandle> TestFile = TUniquePtr<IFileHandle>(PlatformFile.OpenWrite(*TempFilename, /*bAppend*/true, /*bAllowRead*/true)))
	{
		// Validate the file actually opened in append mode correctly
		{
			const int64 ActualEOFPos = TestFile->Tell();
			const int64 ExpectedEOFPos = TestFile->Size();
			if (ActualEOFPos != ExpectedEOFPos)
			{
				AddError(FString::Printf(TEXT("File did not seek to the end when opening (got %d, expected %d): %s"), ActualEOFPos, ExpectedEOFPos, *TempFilename));
				return false;
			}
		}

		TestData.Add(One);
		TestData[10] = One;

		TestFile->Write(&One, 1);
		TestFile->Seek(10);
		TestFile->Write(&One, 1);
	}
	else
	{
		AddError(FString::Printf(TEXT("File failed to open when appending: %s"), *TempFilename));
		return false;
	}

	// Confirm same data
	{
		TArray<uint8> ReadData;
		if (!FFileHelper::LoadFileToArray(ReadData, *TempFilename))
		{
			AddError(FString::Printf(TEXT("File failed to load after appending: %s"), *TempFilename));
			return false;
		}

		if (ReadData != TestData)
		{
			AddError(FString::Printf(TEXT("File data was incorrect after appending: %s"), *TempFilename));
			return false;
		}
	}

	// No append should clobber existing file
	if (TUniquePtr<IFileHandle> TestFile = TUniquePtr<IFileHandle>(PlatformFile.OpenWrite(*TempFilename, /*bAppend*/false, /*bAllowRead*/true)))
	{
		TestData.Reset();
		TestData.Add(One);

		TestFile->Write(&One, 1);
	}
	else
	{
		AddError(FString::Printf(TEXT("File failed to open when clobbering: %s"), *TempFilename));
		return false;
	}

	// Confirm same data
	{
		TArray<uint8> ReadData;
		if (!FFileHelper::LoadFileToArray(ReadData, *TempFilename))
		{
			AddError(FString::Printf(TEXT("File failed to load after clobbering: %s"), *TempFilename));
			return false;
		}

		if (ReadData != TestData)
		{
			AddError(FString::Printf(TEXT("File data was incorrect after clobbering: %s"), *TempFilename));
			return false;
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFileShrinkBuffersTest, "System.Core.Misc.FileShrinkBuffers", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CriticalPriority | EAutomationTestFlags::EngineFilter)
bool FFileShrinkBuffersTest::RunTest( const FString& Parameters )
{
	const FString TempFilename = FPaths::CreateTempFilename(*FPaths::ProjectIntermediateDir());
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	ON_SCOPE_EXIT
	{
		// Delete temp file
		PlatformFile.DeleteFile(*TempFilename);
	};

	// Scratch data for testing
	uint8 One = 1;
	TArray<uint8> TestData;

	// Check a new file can be created
	if (TUniquePtr<IFileHandle> TestFile = TUniquePtr<IFileHandle>(PlatformFile.OpenWrite(*TempFilename, /*bAppend*/false, /*bAllowRead*/true)))
	{
		for (uint8 i = 0; i < 64; ++i)
		{
			TestData.Add(i);
		}

		TestFile->Write(TestData.GetData(), TestData.Num());
	}
	else
	{
		AddError(FString::Printf(TEXT("File failed to open when new: %s"), *TempFilename));
		return false;
	}

	// Confirm same data
	{
		TArray<uint8> ReadData;
		if (!FFileHelper::LoadFileToArray(ReadData, *TempFilename))
		{
			AddError(FString::Printf(TEXT("File failed to load after writing: %s"), *TempFilename));
			return false;
		}

		if (ReadData != TestData)
		{
			AddError(FString::Printf(TEXT("File data was incorrect after writing: %s"), *TempFilename));
			return false;
		}
	}

	// Using ShrinkBuffers should not disrupt our read position in the file
	if (TUniquePtr<IFileHandle> TestFile = TUniquePtr<IFileHandle>(PlatformFile.OpenRead(*TempFilename, /*bAllowWrite*/false)))
	{
		// Validate the file actually opened and is of the right size
		TestEqual(TEXT("File not of expected size at time of ShrinkBuffers read test"), TestFile->Size(), static_cast<decltype(TestFile->Size())>(TestData.Num()));

		const int32 FirstHalfSize = TestData.Num() / 2;
		const int32 SecondHalfSize = TestData.Num() - FirstHalfSize;

		TArray<uint8> FirstHalfReadData;
		FirstHalfReadData.AddUninitialized(FirstHalfSize);
		TestTrue(TEXT("Failed to read first half of test file"), TestFile->Read(FirstHalfReadData.GetData(), FirstHalfReadData.Num()));
		
		for (int32 i = 0; i < FirstHalfSize; ++i)
		{
			TestEqual(TEXT("Mismatch in data before ShrinkBuffers was called"), FirstHalfReadData[i], TestData[i]);
		}

		TestFile->ShrinkBuffers();

		TArray<uint8> SecondHalfReadData;
		SecondHalfReadData.AddUninitialized(SecondHalfSize);
		TestTrue(TEXT("Failed to read second half of test file"), TestFile->Read(SecondHalfReadData.GetData(), SecondHalfReadData.Num()));

		for (int32 i = 0; i < SecondHalfSize; ++i)
		{
			TestEqual(TEXT("Mismatch in data after ShrinkBuffers was called"), SecondHalfReadData[i], TestData[FirstHalfSize + i]);
		}
	}
	else
	{
		AddError(FString::Printf(TEXT("File failed to open file for reading: %s"), *TempFilename));
		return false;
	}

	return true;
}

#endif //WITH_DEV_AUTOMATION_TESTS
