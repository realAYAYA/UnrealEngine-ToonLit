// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_LOW_LEVEL_TESTS

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Misc/Parse.h"

#include "Tests/TestHarnessAdapter.h"

#include <catch2/generators/catch_generators.hpp>

TEST_CASE("Parse::Value::ToBuffer", "[Parse][Smoke]")
{
	TCHAR Buffer[256];

	SECTION("Basic Usage") 
	{
		const TCHAR* Line = TEXT("a=a1 b=b2 c=c3");

		CHECK(FParse::Value(Line, TEXT("a="), Buffer, 256));
		CHECK(FCString::Strcmp(Buffer, TEXT("a1")) == 0);

		CHECK(FParse::Value(Line, TEXT("b="), Buffer, 256));
		CHECK(FCString::Strcmp(Buffer, TEXT("b2")) == 0);

		CHECK(FParse::Value(Line, TEXT("c="), Buffer, 256));
		CHECK(FCString::Strcmp(Buffer, TEXT("c3")) == 0);

		CHECK(false == FParse::Value(Line, TEXT("not_there="), Buffer, 256));
		CHECK(Buffer[0] == TCHAR(0));
	}

	SECTION("Quoted Values")
	{
		CHECK(FParse::Value(TEXT("a=a1 b=\"value with a space, and commas\" c=c3"), TEXT("b="), Buffer, 256));
		CHECK(FCString::Strcmp(Buffer, TEXT("value with a space, and commas")) == 0);
	}

	SECTION("Value may (not)? have a delimiter")
	{
		const TCHAR* Line = TEXT("a=a1,a2");

		CHECK(FParse::Value(Line, TEXT("a="), Buffer, 256, true));
		CHECK(FCString::Strcmp(Buffer, TEXT("a1")) == 0);

		CHECK(FParse::Value(Line, TEXT("a="), Buffer, 256, false)); // false = don't stop on , or )
		CHECK(FCString::Strcmp(Buffer, TEXT("a1,a2")) == 0);
	}

	SECTION("Value may have spaces on its left")
	{
		CHECK(FParse::Value(TEXT("a=   value"), TEXT("a="), Buffer, 256));
		CHECK(FCString::Strcmp(Buffer, TEXT("value")) == 0);
	}

	SECTION("Value could be a key value pair")
	{
		CHECK(FParse::Value(TEXT("a=  b=value"), TEXT("a="), Buffer, 256));
		CHECK(FCString::Strcmp(Buffer, TEXT("b=value")) == 0);

		CHECK(FParse::Value(TEXT("a=  b=  value"), TEXT("a="), Buffer, 256));
		CHECK(FCString::Strcmp(Buffer, TEXT("b=")) == 0);
		CHECK(FParse::Value(TEXT("a=  b=  value"), TEXT("b="), Buffer, 256));
		CHECK(FCString::Strcmp(Buffer, TEXT("value")) == 0);
	}

	SECTION("Key may appear mutiple times")
	{
		const TCHAR* Line = TEXT("rep=a1 rep=b2 rep=c3");
		const TCHAR* ExpectedResults[] = { TEXT("a1"), TEXT("b2"), TEXT("c3") };

		const TCHAR* Cursor = Line;
		for (int Loop = 0; Loop < 4; ++Loop)
		{
			CHECK(Cursor != nullptr);

			bool bFound = FParse::Value(Cursor, TEXT("rep="), Buffer, 256, true, &Cursor);

			if (Loop < 3) 
			{
				CHECK(bFound);
				CHECK(FCString::Strcmp(Buffer, ExpectedResults[Loop]) == 0);
			}
			else
			{
				CHECK(!bFound);
				CHECK(Buffer[0] == TCHAR(0));
				CHECK(Cursor == nullptr);
			}
		}
	}
	
	SECTION("Key may have no value, It is found but Value is empty")
	{
		CHECK(FParse::Value(TEXT("a=   "), TEXT("a="), Buffer, 256));
		CHECK(Buffer[0] == TCHAR(0));
	}

	SECTION("Key with unbalanced quote, It is found but Value is empty")
	{
		for (TCHAR& C : Buffer)
		{
			C = TCHAR('*');
		}
		CHECK(FParse::Value(TEXT("a=\"   "), TEXT("a="), Buffer, 256));
		CHECK(FCString::Strchr(Buffer, TCHAR('*')) == nullptr);
	}

	SECTION("Key may have no value, It is found but Value is empty")
	{
		CHECK(FParse::Value(TEXT("a=   "), TEXT("a="), Buffer, 256));
		CHECK(Buffer[0] == TCHAR(0));
	}

	SECTION("Output var sanity")
	{
		CHECK(false == FParse::Value(TEXT("a=   "), TEXT("a="), Buffer, 0));
	}
}


TEST_CASE("Parse::GrammaredCLIParse::Callback", "[Smoke]")
{
	struct StringKeyValue {
		const TCHAR* Key;
		const TCHAR* Value;
	};

	SECTION("ExpectedPass")
	{
		auto [Input, ExpectedResults] = GENERATE_COPY(table<const TCHAR*, std::vector<StringKeyValue>>(
			{
				{ TEXT("basic_ident"),	{ {TEXT("basic_ident"), nullptr} } },
				{ TEXT("-one_dash"),	{ {TEXT("-one_dash"), nullptr} } },
				{ TEXT("--two_dash"),	{ {TEXT("--two_dash"), nullptr} } },
				{ TEXT("/slash"),		{ {TEXT("/slash"), nullptr} } },
				{ TEXT("key=value"),	{ {TEXT("key"), TEXT("value")} } },
				{ TEXT("key.with.dots=value"),	{ {TEXT("key.with.dots"), TEXT("value")} } },
				{ TEXT("-key=value"),	{ {TEXT("-key"), TEXT("value")} } },
				{ TEXT("-key=\"value\""), { {TEXT("-key"), TEXT("\"value\"")} } },
				{ TEXT("-key=111"),		{ {TEXT("-key"), TEXT("111")} } },
				{ TEXT("-key=111."),	{ {TEXT("-key"), TEXT("111.")} } },
				{ TEXT("-key=111.222"),	{ {TEXT("-key"), TEXT("111.222")} } },
				{ TEXT("-key=-111"),	{ {TEXT("-key"), TEXT("-111")} } },
				{ TEXT("-key=-111.22"),	{ {TEXT("-key"), TEXT("-111.22")} } },
				{ TEXT("-key=../../some+dir\\text-file.txt"),	{ {TEXT("-key"), TEXT("../../some+dir\\text-file.txt")} } },
				{ TEXT("-key=c:\\log.txt"),	{ {TEXT("-key"), TEXT("c:\\log.txt")} } },
				{ TEXT("-token=00aabbcc99"),	{ {TEXT("-token"), TEXT("00aabbcc99")} } },
				{ TEXT("-token=\"00aab bcc99\""),	{ {TEXT("-token"), TEXT("\"00aab bcc99\"")} } },
				{ TEXT("a -b --c d=e"),	{ {TEXT("a"), nullptr},
										  {TEXT("-b"), nullptr},
										  {TEXT("--c"), nullptr},
										  {TEXT("d"), TEXT("e")} } },
				{ TEXT("a \"-b --c\" d=e"),	{ {TEXT("a"), nullptr},
											  {TEXT("-b"), nullptr},
											  {TEXT("--c"), nullptr},
											  {TEXT("d"), TEXT("e")} } },
				{ TEXT("\"a -b --c d=e\""),	{ {TEXT("a"), nullptr},
											  {TEXT("-b"), nullptr},
											  {TEXT("--c"), nullptr},
											  {TEXT("d"), TEXT("e")} } },
				{ TEXT("    leading_space"), { {TEXT("leading_space"), nullptr} } },
				{ TEXT("trailing_space   "), { {TEXT("trailing_space"), nullptr} } },
			}));
		size_t CallbackCalledCount = 0;
		// NOTE: I'm making a Ref to a Structured binding var because CLANG
		// has issues when trying to use them in lambdas.
		// https://github.com/llvm/llvm-project/issues/48582
		const std::vector<StringKeyValue>& ExpectedResultsRef = ExpectedResults;
		auto CallBack = [&CallbackCalledCount, &ExpectedResultsRef](FStringView Key, FStringView Value)
		{
			REQUIRE(CallbackCalledCount < ExpectedResultsRef.size());
			CHECK(Key == FStringView{ ExpectedResultsRef[CallbackCalledCount].Key });
			CHECK(Value == FStringView{ ExpectedResultsRef[CallbackCalledCount].Value });
			++CallbackCalledCount;
		};

		INFO("ExpectedPass " << FStringView{ Input });
		FParse::FGrammarBasedParseResult Result = FParse::GrammarBasedCLIParse(Input, CallBack);

		CHECK(CallbackCalledCount == ExpectedResults.size());
		CHECK(Result.ErrorCode == FParse::EGrammarBasedParseErrorCode::Succeeded);
	}

	SECTION("Quoted commands may be dissallowed, if so gives an error code.")
	{
		auto [Input, ExpectedErrorCode, ExpectedErrorAt, ExpectedResults] = GENERATE_COPY(table<const TCHAR*, FParse::EGrammarBasedParseErrorCode, size_t, std::vector<StringKeyValue>>(
			{
				{ TEXT("a \"-b --c\" d=e"), FParse::EGrammarBasedParseErrorCode::DisallowedQuotedCommand, 2, { {TEXT("a"), nullptr} } },
			}));
		size_t CallbackCalledCount = 0;
		const std::vector<StringKeyValue>& ExpectedResultsRef = ExpectedResults;
		auto CallBack = [&CallbackCalledCount, &ExpectedResultsRef](FStringView Key, FStringView Value)
		{
			REQUIRE(CallbackCalledCount < ExpectedResultsRef.size());
			CHECK(Key == FStringView{ ExpectedResultsRef[CallbackCalledCount].Key });
			CHECK(Value == FStringView{ ExpectedResultsRef[CallbackCalledCount].Value });
			++CallbackCalledCount;
		};

		FParse::FGrammarBasedParseResult Result = FParse::GrammarBasedCLIParse(Input, CallBack, FParse::EGrammarBasedParseFlags::None);

		CHECK(CallbackCalledCount == ExpectedResults.size());
		CHECK(Result.ErrorCode == ExpectedErrorCode);
		CHECK(Result.At == Input + ExpectedErrorAt);
	}

	SECTION("Expected Fail cases")
	{
		auto [Input, ExpectedErrorCode, ExpectedErrorAt, ExpectedResults] = GENERATE_COPY(table<const TCHAR*, FParse::EGrammarBasedParseErrorCode, size_t, std::vector<StringKeyValue>>(
			{
					{ TEXT("-a \"-b"), FParse::EGrammarBasedParseErrorCode::UnBalancedQuote, 3, { {TEXT("-a"), nullptr},
																								{TEXT("-b"), nullptr}} },
					{ TEXT("-a=\"unbalanced_quote_value"), FParse::EGrammarBasedParseErrorCode::UnBalancedQuote, 3, { } },
			}));
		size_t CallbackCalledCount = 0;
		const std::vector<StringKeyValue>& ExpectedResultsRef = ExpectedResults;
		auto CallBack = [&CallbackCalledCount, &ExpectedResultsRef](FStringView Key, FStringView Value)
		{
			REQUIRE(CallbackCalledCount < ExpectedResultsRef.size());
			CHECK(Key == FStringView{ ExpectedResultsRef[CallbackCalledCount].Key });
			CHECK(Value == FStringView{ ExpectedResultsRef[CallbackCalledCount].Value });
			++CallbackCalledCount;
		};

		FParse::FGrammarBasedParseResult Result = FParse::GrammarBasedCLIParse(Input, CallBack);

		CHECK(CallbackCalledCount == ExpectedResults.size());
		CHECK(Result.ErrorCode == ExpectedErrorCode);
		CHECK(Result.At == Input + ExpectedErrorAt);
	}
}

#endif
	
