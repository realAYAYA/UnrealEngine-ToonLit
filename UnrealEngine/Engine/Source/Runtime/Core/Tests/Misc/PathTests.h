// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/StringView.h"
#include "Tests/TestHarnessAdapter.h"
#if WITH_LOW_LEVEL_TESTS
#include "TestCommon/Expectations.h"
#endif

#if WITH_TESTS

template<class PathType, class StringType>
void TestCollapseRelativeDirectories()
{
	auto Run = [](const TCHAR* Path, const TCHAR* Expected)
	{
		// Run test
		StringType Actual;
		Actual += Path;
		const bool bValid = PathType::CollapseRelativeDirectories(Actual);

		if (Expected)
		{
			// If we're looking for a result, make sure it was returned correctly
			if (!bValid || FCString::Strcmp(*Actual, Expected) != 0)
			{
				FAIL_CHECK(FString::Printf(TEXT("Path '%s' failed to collapse correctly (got '%s', expected '%s')."), Path, *Actual, Expected));
			}
		}
		else
		{
			// Otherwise, make sure it failed
			if (bValid)
			{
				FAIL_CHECK(FString::Printf(TEXT("Path '%s' collapsed unexpectedly."), Path));
			}
		}
	};

	Run(TEXT(".."),                                                   nullptr);
	Run(TEXT("/.."),                                                  nullptr);
	Run(TEXT("./"),                                                   TEXT(""));
	Run(TEXT("./file.txt"),                                           TEXT("file.txt"));
	Run(TEXT("/."),                                                   TEXT("/."));
	Run(TEXT("Folder"),                                               TEXT("Folder"));
	Run(TEXT("/Folder"),                                              TEXT("/Folder"));
	Run(TEXT("C:/Folder"),                                            TEXT("C:/Folder"));
	Run(TEXT("C:/Folder/.."),                                         TEXT("C:"));
	Run(TEXT("C:/Folder/../"),                                        TEXT("C:/"));
	Run(TEXT("C:/Folder/../file.txt"),                                TEXT("C:/file.txt"));
	Run(TEXT("Folder/.."),                                            TEXT(""));
	Run(TEXT("Folder/../"),                                           TEXT("/"));
	Run(TEXT("Folder/../file.txt"),                                   TEXT("/file.txt"));
	Run(TEXT("/Folder/.."),                                           TEXT(""));
	Run(TEXT("/Folder/../"),                                          TEXT("/"));
	Run(TEXT("/Folder/../file.txt"),                                  TEXT("/file.txt"));
	Run(TEXT("Folder/../.."),                                         nullptr);
	Run(TEXT("Folder/../../"),                                        nullptr);
	Run(TEXT("Folder/../../file.txt"),                                nullptr);
	Run(TEXT("C:/.."),                                                nullptr);
	Run(TEXT("C:/."),                                                 TEXT("C:/."));
	Run(TEXT("C:/./"),                                                TEXT("C:/"));
	Run(TEXT("C:/./file.txt"),                                        TEXT("C:/file.txt"));
	Run(TEXT("C:/Folder1/../Folder2"),                                TEXT("C:/Folder2"));
	Run(TEXT("C:/Folder1/../Folder2/"),                               TEXT("C:/Folder2/"));
	Run(TEXT("C:/Folder1/../Folder2/file.txt"),                       TEXT("C:/Folder2/file.txt"));
	Run(TEXT("C:/Folder1/../Folder2/../.."),                          nullptr);
	Run(TEXT("C:/Folder1/../Folder2/../Folder3"),                     TEXT("C:/Folder3"));
	Run(TEXT("C:/Folder1/../Folder2/../Folder3/"),                    TEXT("C:/Folder3/"));
	Run(TEXT("C:/Folder1/../Folder2/../Folder3/file.txt"),            TEXT("C:/Folder3/file.txt"));
	Run(TEXT("C:/Folder1/Folder2/../../Folder3"),                     TEXT("C:/Folder3"));
	Run(TEXT("C:/Folder1/Folder2/../../Folder3/"),                    TEXT("C:/Folder3/"));
	Run(TEXT("C:/Folder1/Folder2/../../Folder3/file.txt"),            TEXT("C:/Folder3/file.txt"));
	Run(TEXT("C:/Folder1/Folder2/../../Folder3/../Folder4"),          TEXT("C:/Folder4"));
	Run(TEXT("C:/Folder1/Folder2/../../Folder3/../Folder4/"),         TEXT("C:/Folder4/"));
	Run(TEXT("C:/Folder1/Folder2/../../Folder3/../Folder4/file.txt"), TEXT("C:/Folder4/file.txt"));
	Run(TEXT("C:/Folder1/Folder2/../Folder3/../../Folder4"),          TEXT("C:/Folder4"));
	Run(TEXT("C:/Folder1/Folder2/../Folder3/../../Folder4/"),         TEXT("C:/Folder4/"));
	Run(TEXT("C:/Folder1/Folder2/../Folder3/../../Folder4/file.txt"), TEXT("C:/Folder4/file.txt"));
	Run(TEXT("C:/Folder1/Folder2/.././../Folder4"),                   TEXT("C:/Folder4"));
	Run(TEXT("C:/Folder1/Folder2/.././../Folder4/"),                  TEXT("C:/Folder4/"));
	Run(TEXT("C:/Folder1/Folder2/.././../Folder4/file.txt"),          TEXT("C:/Folder4/file.txt"));
	Run(TEXT("C:/A/B/.././../C"),                                     TEXT("C:/C"));
	Run(TEXT("C:/A/B/.././../C/"),                                    TEXT("C:/C/"));
	Run(TEXT("C:/A/B/.././../C/file.txt"),                            TEXT("C:/C/file.txt"));
	Run(TEXT(".svn"),                                                 TEXT(".svn"));
	Run(TEXT("/.svn"),                                                TEXT("/.svn"));
	Run(TEXT("./Folder/.svn"),                                        TEXT("Folder/.svn"));
	Run(TEXT("./.svn/../.svn"),                                       TEXT(".svn"));
	Run(TEXT(".svn/./.svn/.././../.svn"),                             TEXT("/.svn"));
	Run(TEXT("C:/Folder1/./Folder2/..Folder3"),						 TEXT("C:/Folder1/Folder2/..Folder3"));
	Run(TEXT("C:/Folder1/./Folder2/..Folder3/Folder4"),				 TEXT("C:/Folder1/Folder2/..Folder3/Folder4"));
	Run(TEXT("C:/Folder1/./Folder2/..Folder3/..Folder4"),			 TEXT("C:/Folder1/Folder2/..Folder3/..Folder4"));
	Run(TEXT("C:/Folder1/./Folder2/..Folder3/Folder4/../Folder5"),	 TEXT("C:/Folder1/Folder2/..Folder3/Folder5"));
	Run(TEXT("C:/Folder1/..Folder2/Folder3/..Folder4/../Folder5"),	 TEXT("C:/Folder1/..Folder2/Folder3/Folder5"));
}

template<class PathType, class StringType>
void TestRemoveDuplicateSlashes()
{
	auto Run = [&](const TCHAR* Path, const TCHAR* Expected)
	{
		StringType Actual;
		Actual += Path;	
		PathType::RemoveDuplicateSlashes(Actual);
		CHECK_EQUALS(TEXT("RemoveDuplicateSlashes"), *Actual, Expected);
	};

	Run(TEXT(""), TEXT(""));
	Run(TEXT("C:/Folder/File.txt"), TEXT("C:/Folder/File.txt"));
	Run(TEXT("C:/Folder/File/"), TEXT("C:/Folder/File/"));
	Run(TEXT("/"), TEXT("/"));
	Run(TEXT("//"), TEXT("/"));
	Run(TEXT("////"), TEXT("/"));
	Run(TEXT("/Folder/File"), TEXT("/Folder/File"));
	Run(TEXT("//Folder/File"), TEXT("/Folder/File")); // Don't use on //UNC paths; it will be stripped!
	Run(TEXT("/////Folder//////File/////"), TEXT("/Folder/File/"));
	Run(TEXT("\\\\Folder\\\\File\\\\"), TEXT("\\\\Folder\\\\File\\\\")); // It doesn't strip backslash, and we rely on that in some places
	Run(TEXT("//\\\\//Folder//\\\\//File//\\\\//"), TEXT("/\\\\/Folder/\\\\/File/\\\\/"));
}

namespace PathTest
{

struct FTestPair
{
	FStringView Input;
	FStringView Expected;
};

extern const FStringView BaseDir;

extern const FTestPair ExpectedRelativeToAbsolutePaths[10];

}

#endif //WITH_TESTS