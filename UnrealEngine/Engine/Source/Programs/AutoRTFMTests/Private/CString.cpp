// Copyright Epic Games, Inc. All Rights Reserved.

#pragma autortfm

#include "Catch2Includes.h"
#include <AutoRTFM/AutoRTFM.h>
#include <cstring>

TEST_CASE("CString.memcpy")
{
	constexpr unsigned Size = 128;
	const char* const From = "Hello, world!";
	char To[Size];
	memset(To, 0, Size);
	AutoRTFM::Commit([&]
	{
		memcpy(To, From, 5);
	});

	REQUIRE(0 == strcmp("Hello", To));
}

TEST_CASE("CString.memmove")
{
	constexpr unsigned Size = 128;
	char To[Size] = "Hello, world!";
	AutoRTFM::Commit([&]
	{
		memmove(To + 7, To, 5);
	});

	REQUIRE(0 == strcmp("Hello, Hello!", To));
}

TEST_CASE("CString.strcpy")
{
	constexpr unsigned Size = 128;
	const char* const From = "Hello, world!";
	char To[Size];

#pragma clang optimize off
	AutoRTFM::Commit([&]
		{
			strcpy(To, From);
		});
#pragma clang optimize on

	REQUIRE(0 == strcmp(From, To));
}

TEST_CASE("CString.strncpy")
{
	constexpr unsigned Size = 128;
	const char* const From = "Hello, world!";
	char To[Size];
	memset(To, 0, Size);

#pragma clang optimize off
	AutoRTFM::Commit([&]
		{
			strncpy(To, From, 5);
		});
#pragma clang optimize on

	REQUIRE(0 == strcmp("Hello", To));
}

TEST_CASE("CString.strcat")
{
	constexpr unsigned Size = 128;
	char To[Size] = "Hello";

#pragma clang optimize off
	AutoRTFM::Commit([&]
		{
			strcat(To, ", world!");
		});
#pragma clang optimize on

	REQUIRE(0 == strcmp("Hello, world!", To));
}

TEST_CASE("CString.strncat")
{
	constexpr unsigned Size = 128;
	char To[Size] = "Hello";

#pragma clang optimize off
	AutoRTFM::Commit([&]
		{
			strncat(To, ", world! Not this!", 8);
		});
#pragma clang optimize on

	REQUIRE(0 == strcmp("Hello, world!", To));
}

TEST_CASE("CString.memcmp")
{
	constexpr unsigned Size = 128;
	char A[Size] = "This";

	int Compare;
#pragma clang optimize off
	AutoRTFM::Commit([&]
		{
			Compare = memcmp(A, "That", 4);
		});
#pragma clang optimize on

	REQUIRE(0 < Compare);
}

TEST_CASE("CString.strcmp")
{
	constexpr unsigned Size = 128;
	char A[Size] = "This";

	int Compare;
#pragma clang optimize off
	AutoRTFM::Commit([&]
		{
			Compare = strcmp(A, "That");
		});
#pragma clang optimize on

	REQUIRE(0 < Compare);
}

TEST_CASE("CString.strncmp")
{
	constexpr unsigned Size = 128;
	char A[Size] = "This";

	int Compare;
#pragma clang optimize off
	AutoRTFM::Commit([&]
		{
			Compare = strncmp(A, "That", 3);
		});
#pragma clang optimize on

	REQUIRE(0 < Compare);
}

TEST_CASE("CString.strchr")
{
	constexpr unsigned Size = 128;
	char A[Size] = "This";

	const char* Result;
#pragma clang optimize off
	AutoRTFM::Commit([&]
		{
			Result = strchr(A, 'i');
		});
#pragma clang optimize on

	REQUIRE((A + 2) == Result);
}

TEST_CASE("CString.strrchr")
{
	constexpr unsigned Size = 128;
	char A[Size] = "This";

	const char* Result;
#pragma clang optimize off
	AutoRTFM::Commit([&]
		{
			Result = strrchr(A, 'i');
		});
#pragma clang optimize on

	REQUIRE((A + 2) == Result);
}

TEST_CASE("CString.strstr")
{
	constexpr unsigned Size = 128;
	char A[Size] = "This";

	const char* Result;
#pragma clang optimize off
	AutoRTFM::Commit([&]
		{
			Result = strstr(A, "is");
		});
#pragma clang optimize on

	REQUIRE((A + 2) == Result);
}

TEST_CASE("CString.strlen")
{
	constexpr unsigned Size = 128;
	char A[Size] = "This";

	size_t Result;
#pragma clang optimize off
	AutoRTFM::Commit([&]
		{
			Result = strlen(A);
		});
#pragma clang optimize on

	REQUIRE(4 == Result);
}
