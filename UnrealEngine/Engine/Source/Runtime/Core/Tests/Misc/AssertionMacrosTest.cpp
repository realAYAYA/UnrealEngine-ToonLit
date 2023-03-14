// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_LOW_LEVEL_TESTS && DO_ENSURE

#include "Misc/AssertionMacros.h"
#include "GenericPlatform/GenericPlatformMisc.h"

#include "TestHarness.h"
#include "TestMacros/Assertions.h"

static void OneEnsureNonFailed();
static void OneEnsureNonFailedDifferent();
static void OneEnsureFailed();
static void OneEnsureFailedDifferent();
static void OneEnsureFailedDifferentAgain();
static void MultipleEnsuresNonFailed();
static void MultipleEnsuresFailed();
static void MultipleEnsuresFailedDifferent();

/**
 * @brief Ensure test case, tests Catch2-style macros that check or require whether at least one ensure was triggered or not.
 * Note that each different ensure expression is counted once, if the same ensure fails twice it's only counted the first time it fails.
 */
TEST_CASE("Core::Misc::AssertionMacros", "[Core][Misc][AssertionMacros][Ensure]")
{
	TGuardValue<bool> IgnoreDebugger(GIgnoreDebugger, true);
	SECTION("Ensure not triggered with REQUIRE_NOENSURE")
	{
		REQUIRE_NOENSURE(OneEnsureNonFailed());
	}

	/**
	 * Some sections are disabled until assertion messages and/or callstacks are skipped in a thread-local manner.
	*/

	DISABLED_SECTION("Ensure triggered with REQUIRE_ENSURE")
	{
		REQUIRE_ENSURE(OneEnsureFailed());
	};

	SECTION("Ensure not triggered with multiple CHECK_NOENSURE")
	{
		SIZE_T EnsuresBefore = FDebug::GetNumEnsureFailures();
		CHECK_NOENSURE(OneEnsureNonFailed());
		CHECK_NOENSURE(OneEnsureNonFailedDifferent());
		CHECK(FDebug::GetNumEnsureFailures() == EnsuresBefore);
	}

	DISABLED_SECTION("Ensure triggered multiple CHECK_ENSURE")
	{
		SIZE_T EnsuresBefore = FDebug::GetNumEnsureFailures();
		CHECK_ENSURE(OneEnsureFailedDifferent());
		CHECK_ENSURE(OneEnsureFailedDifferentAgain());
		CHECK(FDebug::GetNumEnsureFailures() == EnsuresBefore + 2);
	};

	SECTION("Multiple ensures not triggered with one REQUIRE_NOENSURE")
	{
		REQUIRE_NOENSURE(MultipleEnsuresNonFailed());
	}

	DISABLED_SECTION("At least one of multiple ensures triggered with REQUIRE_ENSURE")
	{
		REQUIRE_ENSURE(MultipleEnsuresFailed());
	};

	SECTION("Multiple ensures not triggered with CHECK_NOENSURE")
	{
		CHECK_NOENSURE(MultipleEnsuresNonFailed());
	}
	
	DISABLED_SECTION("At least one of multiple ensures triggered with CHECK_ENSURE")
	{
		CHECK_ENSURE(MultipleEnsuresFailedDifferent());
	};
}

static void OneEnsureNonFailed()
{
	ensure(true);
}

static void OneEnsureNonFailedDifferent()
{
	ensure(1 == 1);
}

static void OneEnsureFailed()
{
	ensure(false);
}

static void OneEnsureFailedDifferent()
{
	ensure(1 == 2);
}

static void OneEnsureFailedDifferentAgain()
{
	ensure(2 == 3);
}

static void MultipleEnsuresNonFailed()
{
	ensure(2 == 2);
	ensure(3 == 3);
}

static void MultipleEnsuresFailed()
{
	ensure(2 == 2);
	ensure(3 == 4);
	ensure(4 == 4);
}

static void MultipleEnsuresFailedDifferent()
{
	ensure(2 == 2);
	ensure(4 == 5);
	ensure(3 == 3);
}

#endif
