// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_LOW_LEVEL_TESTS

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"

#include "Tests/TestHarnessAdapter.h"

#include <catch2/generators/catch_generators.hpp>

TEST_CASE("CommandLine::Filtering::FilterCLIInplace", "[Smoke]")
{
	TArray<FString> AllowedList{ { "cmd_a", "-cmd_b" } };

	SECTION("Filtering CLI examples")
	{
		auto [Input, Expected] = GENERATE_COPY(table<const TCHAR*, const TCHAR*>(
		{
			{ TEXT(""), TEXT("")},
			{ TEXT("not_on_this_list"), TEXT("") },
			{ TEXT("-cmd_a --cmd_b"), TEXT("-cmd_a --cmd_b") },
			{ TEXT("-cmd_a --cmd_b not_on_this_list"), TEXT("-cmd_a --cmd_b") },
			{ TEXT("-cmd_a not_on_this_list --cmd_b"), TEXT("-cmd_a --cmd_b") },
			{ TEXT("-cmd_a -cmd_a -cmd_a"), TEXT("-cmd_a -cmd_a -cmd_a") },
			{ TEXT("-cmd_a --cmd_b \"-cmd_a --cmd_b not_on_this_list\""), TEXT("-cmd_a --cmd_b -cmd_a --cmd_b") },
			{ TEXT("-cmd_a=1 not_on_this_list=2 --cmd_b=true "), TEXT("-cmd_a=1 --cmd_b=true") },
			{ TEXT("-cmd_a=  not_on_this_list=2 --cmd_b=true "), TEXT("-cmd_a=not_on_this_list=2 --cmd_b=true") },
			{ TEXT("-cmd_a=  -not_on_this_list=2 --cmd_b=true "), TEXT("-cmd_a=-not_on_this_list=2 --cmd_b=true") },
		}));
		TCHAR Result[256]{};

		CHECK(FCommandLine::FilterCLIUsingGrammarBasedParser(Result, UE_ARRAY_COUNT(Result), Input, AllowedList));
		CHECK(FStringView{ Result } == FStringView{ Expected });
	}

	SECTION("Filtering applies to key values in quotes.. FORT-602120")
	{
		auto [Input, Expected] = GENERATE_COPY(table<const TCHAR*, const TCHAR*>(
			{
				{ TEXT("\"-cmd_a --cmd_b not_on_this_list\""), TEXT("-cmd_a --cmd_b") },
				{ TEXT("\"-cmd_a not_on_this_list --cmd_b\""), TEXT("-cmd_a --cmd_b") },
				{ TEXT("-cmd_a \"not_on_this_list --cmd_b\""), TEXT("-cmd_a --cmd_b") },
			}));
		TCHAR Result[256]{};

		CHECK(FCommandLine::FilterCLIUsingGrammarBasedParser(Result, UE_ARRAY_COUNT(Result), Input, AllowedList));
		CHECK(FStringView{ Result } == FStringView{ Expected });
	}


	SECTION("Filtering CLI examples, Using 1 buffer as In and Out")
	{
		auto [Input, Expected] = GENERATE_COPY(table<const TCHAR*, const TCHAR*>(
			{
				{ TEXT(""), TEXT("")},
				{ TEXT("not_on_this_list"), TEXT("") },
				{ TEXT("-cmd_a --cmd_b"), TEXT("-cmd_a --cmd_b") },
				{ TEXT("-cmd_a --cmd_b not_on_this_list"), TEXT("-cmd_a --cmd_b") },
				{ TEXT("-cmd_a not_on_this_list --cmd_b"), TEXT("-cmd_a --cmd_b") },
				{ TEXT("-cmd_a -cmd_a -cmd_a"), TEXT("-cmd_a -cmd_a -cmd_a") },
				{ TEXT("-cmd_a --cmd_b \"-cmd_a --cmd_b not_on_this_list\""), TEXT("-cmd_a --cmd_b -cmd_a --cmd_b") },
				{ TEXT("-cmd_a=1 not_on_this_list=2 --cmd_b=true "), TEXT("-cmd_a=1 --cmd_b=true") },
				{ TEXT("-cmd_a=  not_on_this_list=2 --cmd_b=true "), TEXT("-cmd_a=not_on_this_list=2 --cmd_b=true") },
				{ TEXT("-cmd_a=  -not_on_this_list=2 --cmd_b=true "), TEXT("-cmd_a=-not_on_this_list=2 --cmd_b=true") },
				{ TEXT("\"-cmd_a --cmd_b not_on_this_list\""), TEXT("-cmd_a --cmd_b") },
				{ TEXT("\"-cmd_a not_on_this_list --cmd_b\""), TEXT("-cmd_a --cmd_b") },
				{ TEXT("-cmd_a \"not_on_this_list --cmd_b\""), TEXT("-cmd_a --cmd_b") },
			}));
		TCHAR SourceAndResult[256]{};
		
		FCString::Strcpy(SourceAndResult, Input);
		CHECK(FCommandLine::FilterCLIUsingGrammarBasedParser(SourceAndResult, UE_ARRAY_COUNT(SourceAndResult), SourceAndResult, AllowedList));
		CHECK(FStringView{ SourceAndResult } == FStringView{ Expected });
	}

	SECTION("Filtering with an empty AllowedList, returns an empty string")
	{
		const TCHAR* Input = TEXT("-cmd_a --cmd_b");
		TCHAR Result[256] = TEXT("Not Empty");

		CHECK(FCommandLine::FilterCLIUsingGrammarBasedParser(Result, UE_ARRAY_COUNT(Result), Input, {}));
		CHECK(Result[0] == TCHAR(0));
	}

	SECTION("Fail for to small a result buffer")
	{
		const TCHAR* Input = TEXT("-cmd_a --cmd_b");
		TCHAR Result[5]{};

		CHECK(false == FCommandLine::FilterCLIUsingGrammarBasedParser(Result, UE_ARRAY_COUNT(Result), Input, AllowedList));
		CHECK(Result[0] == TCHAR(0));
	}

	SECTION("End to end as it is currently used")
	{
		TArray<FString> Ignored;
		TArray<FString> ApprovedArgs;
		FCommandLine::Parse(TEXT("-cmd_a --cmd_b /cmd_c"), ApprovedArgs, Ignored);

		auto [Input, Expected] = GENERATE_COPY(table<const TCHAR*, const TCHAR*>(
			{
				{ TEXT(""), TEXT("")},
				{ TEXT("not_on_this_list"), TEXT("") },
				{ TEXT("cmd_a"), TEXT("cmd_a") },
				{ TEXT("-cmd_a"), TEXT("-cmd_a") },
				{ TEXT("--cmd_b"), TEXT("--cmd_b") },
				{ TEXT("/cmd_c"), TEXT("/cmd_c") },

				{ TEXT("cmd_a --cmd_b"), TEXT("cmd_a --cmd_b") },
				{ TEXT("-cmd_a --cmd_b"), TEXT("-cmd_a --cmd_b") },
				{ TEXT("-cmd_a --cmd_b /cmd_c"), TEXT("-cmd_a --cmd_b /cmd_c") },
				{ TEXT("-cmd_a --cmd_b not_on_this_list"), TEXT("-cmd_a --cmd_b") },
				{ TEXT("-cmd_a not_on_this_list --cmd_b"), TEXT("-cmd_a --cmd_b") },
				{ TEXT("cmd_a -cmd_a -cmd_a"), TEXT("cmd_a -cmd_a -cmd_a") },
				{ TEXT("-cmd_a --cmd_b \"-cmd_a --cmd_b not_on_this_list\""), TEXT("-cmd_a --cmd_b -cmd_a --cmd_b") },
				{ TEXT("-cmd_a=1 not_on_this_list=2 --cmd_b=true "), TEXT("-cmd_a=1 --cmd_b=true") },
				{ TEXT("-cmd_a=  not_on_this_list=2 --cmd_b=true "), TEXT("-cmd_a=not_on_this_list=2 --cmd_b=true") },
				{ TEXT("-cmd_a=  -not_on_this_list=2 --cmd_b=true "), TEXT("-cmd_a=-not_on_this_list=2 --cmd_b=true") },
				{ TEXT("\"-cmd_a --cmd_b not_on_this_list\""), TEXT("-cmd_a --cmd_b") },
				{ TEXT("\"-cmd_a not_on_this_list --cmd_b\""), TEXT("-cmd_a --cmd_b") },
				{ TEXT("-cmd_a \"not_on_this_list --cmd_b\""), TEXT("-cmd_a --cmd_b") },
				{ TEXT("-run=../../risky.exe -cmd_a=/mnt/horde/FN+NC+PF/good.bin --cmd_b=c:\\log.txt"), TEXT("-cmd_a=/mnt/horde/FN+NC+PF/good.bin --cmd_b=c:\\log.txt") },
			}));
		TCHAR SourceAndResult[256]{};

		FCString::Strcpy(SourceAndResult, Input);
		CHECK(FCommandLine::FilterCLIUsingGrammarBasedParser(SourceAndResult, UE_ARRAY_COUNT(SourceAndResult), SourceAndResult, ApprovedArgs));
		CHECK(FStringView{ SourceAndResult } == FStringView{ Expected });
	}
}


// This code was lifted from CommandLine.cpp
// SO we can prove the new Grammar based parse does an equivalent job.
// It can be removed along with the version in CommandLine.cpp once we are happy with FilterCLIUsingGrammarBasedParser 
namespace OldFilterMethod
{
	TArray<FString> ApprovedArgs;

	TArray<FString> FilterCommandLine(const TCHAR* CommandLine)
	{
		TArray<FString> Ignored;
		TArray<FString> ParsedList;
		// Parse the command line list
		FCommandLine::Parse(CommandLine, ParsedList, Ignored);
		// Remove any that are not in our approved list
		for (int32 Index = 0; Index < ParsedList.Num(); Index++)
		{
			bool bFound = false;
			for (auto ApprovedArg : ApprovedArgs)
			{
				if (ParsedList[Index].StartsWith(ApprovedArg))
				{
					bFound = true;
					break;
				}
			}
			if (!bFound)
			{
				ParsedList.RemoveAt(Index);
				Index--;
		}
	}
		return ParsedList;
}

	void BuildCommandLineAllowList(TCHAR* CommandLine, uint32 ArrayCount, const TArray<FString>& FilteredArgs)
	{
		check(ArrayCount > 0);
		// Zero the whole string
		FMemory::Memzero(CommandLine, sizeof(TCHAR) * ArrayCount);

		uint32 StartIndex = 0;
		for (auto Arg : FilteredArgs)
		{
			if ((StartIndex + Arg.Len() + 2) < ArrayCount)
			{
				if (StartIndex != 0)
				{
					CommandLine[StartIndex++] = TEXT(' ');
				}
				CommandLine[StartIndex++] = TEXT('-');
				FCString::Strncpy(&CommandLine[StartIndex], *Arg, ArrayCount - StartIndex);
				StartIndex += Arg.Len();
			}
		}
	}

	void FilterCLI(TCHAR* OutLine, int32 MaxLen, const TCHAR* InLine, const TArrayView<FString>& AllowedList)
	{
		ApprovedArgs = AllowedList;
		TArray<FString> OriginalList = FilterCommandLine(InLine);
		BuildCommandLineAllowList(OutLine, MaxLen, OriginalList);
	}
}


TEST_CASE("CommandLine::Filtering::FilterCLI_Comparison", "[Smoke]")
{
	SECTION("Sudo real example")
	{
		TArray<FString> ApprovedArgs;

		{
			const TCHAR* OverrideList = TEXT("-cmd_a -cmd_b -cmd_c");

			TArray<FString> Ignored;
			FCommandLine::Parse(OverrideList, ApprovedArgs, Ignored);
		}

		const TCHAR* Original = TEXT("\"un keyed value should be filtered\" -cmd_a=\"c:/some/path\" -to_be_filtered=be_gone -cmd_b -not_allowed -cmd_c=7890sfd9wjd8jf984mx");

		TCHAR NewMethod[1024];
		TCHAR OldMethod[1024];

		FCString::Strcpy(NewMethod, UE_ARRAY_COUNT(NewMethod), Original);
		FCString::Strcpy(OldMethod, UE_ARRAY_COUNT(NewMethod), Original);

		FCommandLine::FilterCLIUsingGrammarBasedParser(NewMethod, UE_ARRAY_COUNT(NewMethod), NewMethod, ApprovedArgs);
		OldFilterMethod::FilterCLI(OldMethod, UE_ARRAY_COUNT(OldMethod), OldMethod, ApprovedArgs);

		CHECK(NewMethod[0] != TCHAR('\0')); // not empty
		CHECK(FStringView{ NewMethod } == FStringView{ OldMethod }); // same result both methods
		CHECK(FStringView{ NewMethod } == FStringView{ TEXT("-cmd_a=\"c:/some/path\" -cmd_b -cmd_c=7890sfd9wjd8jf984mx") }); // this should be the result
	}
}
#endif

