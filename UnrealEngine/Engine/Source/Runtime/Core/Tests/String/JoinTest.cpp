// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_TESTS

#include "String/Join.h"

#include "Containers/StringView.h"
#include "Containers/UnrealString.h"
#include "Misc/StringBuilder.h"
#include "Tests/TestHarnessAdapter.h"

namespace UE::String
{

TEST_CASE_NAMED(FStringJoinTest, "System::Core::String::Join", "[Core][String][SmokeFilter]")
{
	SECTION("Join")
	{
		CHECK(WriteToString<128>(Join(MakeArrayView<FStringView>({TEXTVIEW("ABC"), TEXTVIEW("DEF")}), TEXT(", "))) == TEXTVIEW("ABC, DEF"));
		CHECK(WriteToString<128>(Join(MakeArrayView<FString>({FString(TEXT("ABC")), FString(TEXT("DEF"))}), TEXT(", "))) == TEXTVIEW("ABC, DEF"));
	}

	SECTION("JoinBy")
	{
		CHECK(WriteToString<128>(JoinBy(MakeArrayView<FString>({FString(TEXT("ABC")), FString(TEXT("DEF"))}), UE_PROJECTION_MEMBER(FString, ToLower), TEXT(", "))) == TEXTVIEW("abc, def"));
	}

	SECTION("JoinQuoted")
	{
		CHECK(WriteToString<128>(JoinQuoted(MakeArrayView<FStringView>({TEXTVIEW("ABC"), TEXTVIEW("DEF")}), TEXT(", "), TEXT("|"))) == TEXTVIEW("|ABC|, |DEF|"));
		CHECK(WriteToString<128>(JoinQuoted(MakeArrayView<FString>({FString(TEXT("ABC")), FString(TEXT("DEF"))}), TEXT(", "), TEXT("|"))) == TEXTVIEW("|ABC|, |DEF|"));
	}

	SECTION("JoinQuotedBy")
	{
		CHECK(WriteToString<128>(JoinQuotedBy(MakeArrayView<FString>({FString(TEXT("ABC")), FString(TEXT("DEF"))}), UE_PROJECTION_MEMBER(FString, ToLower), TEXT(", "), TEXT("|"))) == TEXTVIEW("|abc|, |def|"));
	}
}

} // UE::String

#endif //WITH_TESTS