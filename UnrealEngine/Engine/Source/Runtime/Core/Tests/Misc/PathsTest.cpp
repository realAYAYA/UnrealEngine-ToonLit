// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_TESTS

#include "PathTests.h"
#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Containers/UnrealString.h"
#include "Misc/Paths.h"
#include "Tests/TestHarnessAdapter.h"
#if WITH_LOW_LEVEL_TESTS
#include "TestCommon/Expectations.h"
#endif

const FStringView PathTest::BaseDir = TEXTVIEW("/root");

const PathTest::FTestPair PathTest::ExpectedRelativeToAbsolutePaths[10] =
{
	{ TEXTVIEW(""),					TEXTVIEW("/root/") },
	{ TEXTVIEW("dir"),				TEXTVIEW("/root/dir") },
	{ TEXTVIEW("/groot"),			TEXTVIEW("/groot") },
	{ TEXTVIEW("/groot/"),			TEXTVIEW("/groot/") },
	{ TEXTVIEW("/r/dir"),			TEXTVIEW("/r/dir") },
	{ TEXTVIEW("/r/dir"),			TEXTVIEW("/r/dir") },
	{ TEXTVIEW("C:\\"),				TEXTVIEW("C:/") },
	{ TEXTVIEW("C:\\A\\B"),			TEXTVIEW("C:/A/B") },
	{ TEXTVIEW("a/b/../c"),			TEXTVIEW("/root/a/c") },
	{ TEXTVIEW("/a/b/../c"),		TEXTVIEW("/a/c") },
};

TEST_CASE_NAMED(FPathTests, "System::Core::Misc::Paths", "[ApplicationContextMask][SmokeFilter]")
{
	TestCollapseRelativeDirectories<FPaths, FString>();

	// Extension texts
	{
		auto RunGetExtensionTest = [](const TCHAR* InPath, const TCHAR* InExpectedExt)
		{
			// Run test
			const FString Ext = FPaths::GetExtension(FString(InPath));
			if (Ext != InExpectedExt)
			{
				FAIL_CHECK(FString::Printf(TEXT("Path '%s' failed to get the extension (got '%s', expected '%s')."), InPath, *Ext, InExpectedExt));
			}
		};

		RunGetExtensionTest(TEXT("file"),									TEXT(""));
		RunGetExtensionTest(TEXT("file.txt"),								TEXT("txt"));
		RunGetExtensionTest(TEXT("file.tar.gz"),							TEXT("gz"));
		RunGetExtensionTest(TEXT("C:/Folder/file"),							TEXT(""));
		RunGetExtensionTest(TEXT("C:/Folder/file.txt"),						TEXT("txt"));
		RunGetExtensionTest(TEXT("C:/Folder/file.tar.gz"),					TEXT("gz"));
		RunGetExtensionTest(TEXT("C:/Folder/First.Last/file"),				TEXT(""));
		RunGetExtensionTest(TEXT("C:/Folder/First.Last/file.txt"),			TEXT("txt"));
		RunGetExtensionTest(TEXT("C:/Folder/First.Last/file.tar.gz"),		TEXT("gz"));

		auto RunSetExtensionTest = [](const TCHAR* InPath, const TCHAR* InNewExt, const FString& InExpectedPath)
		{
			// Run test
			const FString NewPath = FPaths::SetExtension(FString(InPath), FString(InNewExt));
			if (NewPath != InExpectedPath)
			{
				FAIL_CHECK(FString::Printf(TEXT("Path '%s' failed to set the extension (got '%s', expected '%s')."), InPath, *NewPath, *InExpectedPath));
			}
		};

		RunSetExtensionTest(TEXT("file"),									TEXT("log"),	TEXT("file.log"));
		RunSetExtensionTest(TEXT("file.txt"),								TEXT("log"),	TEXT("file.log"));
		RunSetExtensionTest(TEXT("file.tar.gz"),							TEXT("gz2"),	TEXT("file.tar.gz2"));
		RunSetExtensionTest(TEXT("C:/Folder/file"),							TEXT("log"),	TEXT("C:/Folder/file.log"));
		RunSetExtensionTest(TEXT("C:/Folder/file.txt"),						TEXT("log"),	TEXT("C:/Folder/file.log"));
		RunSetExtensionTest(TEXT("C:/Folder/file.tar.gz"),					TEXT("gz2"),	TEXT("C:/Folder/file.tar.gz2"));
		RunSetExtensionTest(TEXT("C:/Folder/First.Last/file"),				TEXT("log"),	TEXT("C:/Folder/First.Last/file.log"));
		RunSetExtensionTest(TEXT("C:/Folder/First.Last/file.txt"),			TEXT("log"),	TEXT("C:/Folder/First.Last/file.log"));
		RunSetExtensionTest(TEXT("C:/Folder/First.Last/file.tar.gz"),		TEXT("gz2"),	TEXT("C:/Folder/First.Last/file.tar.gz2"));

		auto RunChangeExtensionTest = [](const TCHAR* InPath, const TCHAR* InNewExt, const FString& InExpectedPath)
		{
			// Run test
			const FString NewPath = FPaths::ChangeExtension(FString(InPath), FString(InNewExt));
			if (NewPath != InExpectedPath)
			{
				FAIL_CHECK(FString::Printf(TEXT("Path '%s' failed to change the extension (got '%s', expected '%s')."), InPath, *NewPath, *InExpectedPath));
			}
		};

		RunChangeExtensionTest(TEXT("file"),								TEXT("log"),	TEXT("file"));
		RunChangeExtensionTest(TEXT("file.txt"),							TEXT("log"),	TEXT("file.log"));
		RunChangeExtensionTest(TEXT("file.tar.gz"),							TEXT("gz2"),	TEXT("file.tar.gz2"));
		RunChangeExtensionTest(TEXT("C:/Folder/file"),						TEXT("log"),	TEXT("C:/Folder/file"));
		RunChangeExtensionTest(TEXT("C:/Folder/file.txt"),					TEXT("log"),	TEXT("C:/Folder/file.log"));
		RunChangeExtensionTest(TEXT("C:/Folder/file.tar.gz"),				TEXT("gz2"),	TEXT("C:/Folder/file.tar.gz2"));
		RunChangeExtensionTest(TEXT("C:/Folder/First.Last/file"),			TEXT("log"),	TEXT("C:/Folder/First.Last/file"));
		RunChangeExtensionTest(TEXT("C:/Folder/First.Last/file.txt"),		TEXT("log"),	TEXT("C:/Folder/First.Last/file.log"));
		RunChangeExtensionTest(TEXT("C:/Folder/First.Last/file.tar.gz"),	TEXT("gz2"),	TEXT("C:/Folder/First.Last/file.tar.gz2"));
	}

	// IsUnderDirectory
	{
		auto RunIsUnderDirectoryTest = [](const TCHAR* InPath1, const TCHAR* InPath2, bool ExpectedResult)
		{
			// Run test
			bool Result = FPaths::IsUnderDirectory(FString(InPath1), FString(InPath2));
			if (Result != ExpectedResult)
			{
				FAIL_CHECK(FString::Printf(TEXT("FPaths::IsUnderDirectory('%s', '%s') != %s."), InPath1, InPath2, ExpectedResult ? TEXT("true") : TEXT("false")));
			}
		};

		RunIsUnderDirectoryTest(TEXT("C:/Folder"),			TEXT("C:/FolderN"), false);
		RunIsUnderDirectoryTest(TEXT("C:/Folder1"),			TEXT("C:/Folder2"), false);
		RunIsUnderDirectoryTest(TEXT("C:/Folder"),			TEXT("C:/Folder/SubDir"), false);

		RunIsUnderDirectoryTest(TEXT("C:/Folder"),			TEXT("C:/Folder"), true);
		RunIsUnderDirectoryTest(TEXT("C:/Folder/File"),		TEXT("C:/Folder"), true);
		RunIsUnderDirectoryTest(TEXT("C:/Folder/File"),		TEXT("C:/Folder/"), true);
		RunIsUnderDirectoryTest(TEXT("C:/Folder/"),			TEXT("C:/Folder"), true);
		RunIsUnderDirectoryTest(TEXT("C:/Folder/"),			TEXT("C:/Folder/"), true);
		RunIsUnderDirectoryTest(TEXT("C:/Folder/Subdir/"),	TEXT("C:/Folder"), true);
		RunIsUnderDirectoryTest(TEXT("C:/Folder/Subdir/"),	TEXT("C:/Folder/"), true);
	}

	TestRemoveDuplicateSlashes<FPaths, FString>();

	// ConvertRelativePathToFull
	{
		using namespace PathTest;

		for (FTestPair Pair : ExpectedRelativeToAbsolutePaths)
		{
			FString Actual = FPaths::ConvertRelativePathToFull(FString(BaseDir), FString(Pair.Input));
			CHECK_EQUALS(TEXT("ConvertRelativePathToFull"), FStringView(Actual), Pair.Expected);
		}
	}

	// Split
	auto RunSplitTest = [](const TCHAR* InPath, const TCHAR* InExpectedPath, const TCHAR* InExpectedName, const TCHAR* InExpectedExt)
		{
			FString SplitPath, SplitName, SplitExt;
			FPaths::Split(InPath, SplitPath, SplitName, SplitExt);
			if (SplitPath != InExpectedPath || SplitName != InExpectedName || SplitExt != InExpectedExt)
			{
				FAIL_CHECK(FString::Printf(TEXT("Failed to split path '%s' (got ('%s', '%s', '%s'), expected ('%s', '%s', '%s'))."), InPath,
					*SplitPath, *SplitName, *SplitExt, InExpectedPath, InExpectedName, InExpectedExt));
			}
		};

	RunSplitTest(TEXT(""), TEXT(""), TEXT(""), TEXT(""));
	RunSplitTest(TEXT(".txt"), TEXT(""), TEXT(""), TEXT("txt"));
	RunSplitTest(TEXT(".tar.gz"), TEXT(""), TEXT(".tar"), TEXT("gz"));
	RunSplitTest(TEXT(".tar.gz/"), TEXT(".tar.gz"), TEXT(""), TEXT(""));
	RunSplitTest(TEXT(".tar.gz\\"), TEXT(".tar.gz"), TEXT(""), TEXT(""));
	// TEXT(".") is ambiguous; we currently treat it as an extension separator but we don't guarantee that in our contract
	//RunSplitTest(TEXT("."), TEXT(""), TEXT(""), TEXT(""));
	RunSplitTest(TEXT(".."), TEXT(""), TEXT(".."), TEXT(""));
	RunSplitTest(TEXT("File"), TEXT(""), TEXT("File"), TEXT(""));
	RunSplitTest(TEXT("File.txt"), TEXT(""), TEXT("File"), TEXT("txt"));
	RunSplitTest(TEXT("File.tar.gz"), TEXT(""), TEXT("File.tar"), TEXT("gz"));
	RunSplitTest(TEXT("File.tar.gz/"), TEXT("File.tar.gz"), TEXT(""), TEXT(""));
	RunSplitTest(TEXT("File.tar.gz\\"), TEXT("File.tar.gz"), TEXT(""), TEXT(""));
	RunSplitTest(TEXT("C:/Folder/"), TEXT("C:/Folder"), TEXT(""), TEXT(""));
	RunSplitTest(TEXT("C:/Folder/File"), TEXT("C:/Folder"), TEXT("File"), TEXT(""));
	RunSplitTest(TEXT("C:/Folder/File.txt"), TEXT("C:/Folder"), TEXT("File"), TEXT("txt"));
	RunSplitTest(TEXT("C:/Folder/File.tar.gz"), TEXT("C:/Folder"), TEXT("File.tar"), TEXT("gz"));
	RunSplitTest(TEXT("C:/Folder/First.Last/File"), TEXT("C:/Folder/First.Last"), TEXT("File"), TEXT(""));
	RunSplitTest(TEXT("C:/Folder/First.Last/File.txt"), TEXT("C:/Folder/First.Last"), TEXT("File"), TEXT("txt"));
	RunSplitTest(TEXT("C:/Folder/First.Last/File.tar.gz"), TEXT("C:/Folder/First.Last"), TEXT("File.tar"), TEXT("gz"));
	RunSplitTest(TEXT("C:\\Folder\\"), TEXT("C:\\Folder"), TEXT(""), TEXT(""));
	RunSplitTest(TEXT("C:\\Folder\\File"), TEXT("C:\\Folder"), TEXT("File"), TEXT(""));
	RunSplitTest(TEXT("C:\\Folder\\First.Last\\"), TEXT("C:\\Folder\\First.Last"), TEXT(""), TEXT(""));
	RunSplitTest(TEXT("C:\\Folder\\First.Last\\File"), TEXT("C:\\Folder\\First.Last"), TEXT("File"), TEXT(""));
	RunSplitTest(TEXT("C:\\Folder\\First.Last\\File.txt"), TEXT("C:\\Folder\\First.Last"), TEXT("File"), TEXT("txt"));
	RunSplitTest(TEXT("C:\\Folder\\First.Last\\File.tar.gz"), TEXT("C:\\Folder\\First.Last"), TEXT("File.tar"), TEXT("gz"));

}

#endif //WITH_TESTS
