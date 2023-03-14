// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_LOW_LEVEL_TESTS

#include "ObjectRefTrackingTestBase.h"

#if UE_WITH_OBJECT_HANDLE_TRACKING
FDelegateHandle FObjectRefTrackingTestBase::ResolvedCallbackHandle;
FDelegateHandle FObjectRefTrackingTestBase::HandleReadCallbackHandle;
#endif

thread_local uint32 FObjectRefTrackingTestBase::NumResolves = 0;
thread_local uint32 FObjectRefTrackingTestBase::NumFailedResolves = 0;
thread_local uint32 FObjectRefTrackingTestBase::NumReads = 0;

#endif