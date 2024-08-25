// Copyright Epic Games, Inc. All Rights Reserved.

#pragma autortfm

#include "MyAutoRTFMTestObject.h"
#include "Catch2Includes.h"
#include <AutoRTFM/AutoRTFM.h>
#include "UObject/GCObject.h"
#include "UObject/ReachabilityAnalysis.h"

TEST_CASE("UObject.Create")
{
	UMyAutoRTFMTestObject* Object = nullptr;

	AutoRTFM::Commit([&]
	{
		Object = NewObject<UMyAutoRTFMTestObject>();
	});

	REQUIRE(nullptr != Object);
	REQUIRE(42 == Object->Value);
}

TEST_CASE("UObject.Abort")
{
	UMyAutoRTFMTestObject* Object = nullptr;

	REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == AutoRTFM::Transact([&]
	{
		Object = NewObject<UMyAutoRTFMTestObject>();
		AutoRTFM::AbortTransaction();
	}));

	REQUIRE(nullptr == Object);
}

// This is a copy of the helper function in TestGarbageCollector.cpp.
int32 PerformGarbageCollectionWithIncrementalReachabilityAnalysis(TFunctionRef<bool(int32)> ReachabilityIterationCallback)
{
	int32 ReachabilityIterationIndex = 0;

	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, false);

	while (IsIncrementalReachabilityAnalysisPending())
	{
		if (ReachabilityIterationCallback(ReachabilityIterationIndex))
		{
			break;
		}

		// Re-check if incremental rachability is still pending because the callback above could've triggered GC which would complete all iterations
		if (IsIncrementalReachabilityAnalysisPending())
		{
			PerformIncrementalReachabilityAnalysis(GetReachabilityAnalysisTimeLimit());
			ReachabilityIterationIndex++;
		}
	}

	if (IsIncrementalPurgePending())
	{
		IncrementalPurgeGarbage(false);
	}
	check(IsIncrementalPurgePending() == false);

	return ReachabilityIterationIndex + 1;
}

TEST_CASE("UObject.MarkAsReachable")
{
	// We need incremental reachability to be on.
	SetIncrementalReachabilityAnalysisEnabled(true);

	// Cache the original time limit.
	const float Original = GetReachabilityAnalysisTimeLimit();

	// And we need a super small time limit s that reachability analysis will definitely have started.
	SetReachabilityAnalysisTimeLimit(FLT_MIN);
	
	// We need to be sure we've done the static GC initialization before we start doing a garbage
	// collection.
	FGCObject::StaticInit();

	UMyAutoRTFMTestObject* const Object = NewObject<UMyAutoRTFMTestObject>();

	PerformGarbageCollectionWithIncrementalReachabilityAnalysis([Object](int32 index)
	{
		if (0 != index)
		{
			return true;
		}

		AutoRTFM::Commit([&]
		{
			Object->MarkAsReachable();
		});

		return false;
	});

	// Reset it back just incase another test required the original time limit.
	SetReachabilityAnalysisTimeLimit(Original);
}
