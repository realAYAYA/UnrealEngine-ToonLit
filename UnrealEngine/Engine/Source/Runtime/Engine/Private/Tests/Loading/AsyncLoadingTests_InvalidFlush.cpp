// Copyright Epic Games, Inc. All Rights Reserved.

#include "AsyncLoadingTests_Shared.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Async/ManualResetEvent.h"
#include "UObject/CoreRedirects.h"
#include "UObject/SavePackage.h"
#include "Tasks/Task.h"

#if WITH_DEV_AUTOMATION_TESTS

// All InvalidFlush tests should run on zenloader only as the other loaders are not compliant.
typedef FLoadingTests_ZenLoaderOnly_Base FLoadingTests_InvalidFlush_Base;

/**
 * This test validates loading an object synchronously during serialize.
 */
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FLoadingTests_InvalidFlush_FromWorker,
	FLoadingTests_InvalidFlush_Base,
	TEXT("System.Engine.Loading.InvalidFlush.FromWorker"),
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)
bool FLoadingTests_InvalidFlush_FromWorker::RunTest(const FString& Parameters)
{
	AddExpectedError(TEXT("is unable to FlushAsyncLoading from the current thread"), EAutomationExpectedErrorFlags::Contains);
	AddExpectedError(TEXT("[Callstack]"), EAutomationExpectedErrorFlags::Contains, 0 /* At least 1 occurrence */);
	
	FLoadingTestsScope LoadingTestScope(this);

	UAsyncLoadingTests_Shared::OnSerialize.BindLambda(
		[this](FArchive& Ar, UAsyncLoadingTests_Shared* Object)
		{
			if (Ar.IsLoading())
			{
				// Use event instead of waiting on the task to prevent retraction as we really want that task
				// to execute on a worker thread instead of being retracted in the serialize thread.
				UE::FManualResetEvent Event;
				UE::Tasks::Launch(TEXT("FlushAsyncLoading"), [&Event]() { FlushAsyncLoading(); Event.Notify(); });
				Event.Wait();
			}
		}
	);

	LoadingTestScope.LoadObjects();

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
