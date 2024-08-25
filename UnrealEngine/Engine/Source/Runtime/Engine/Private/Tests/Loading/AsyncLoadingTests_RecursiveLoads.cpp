// Copyright Epic Games, Inc. All Rights Reserved.

#include "AsyncLoadingTests_Shared.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "UObject/CoreRedirects.h"
#include "UObject/SavePackage.h"

#if WITH_DEV_AUTOMATION_TESTS

// All RecursiveLoads tests should run on zenloader only as the other loaders are not compliant.
typedef FLoadingTests_ZenLoaderOnly_Base FLoadingTests_RecursiveLoads_Base;

/**
 * This test validates loading an object synchronously during serialize.
 */
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FLoadingTests_RecursiveLoads_FromSerialize, 
	FLoadingTests_RecursiveLoads_Base,
	TEXT("System.Engine.Loading.RecursiveLoads.FromSerialize"),
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)
bool FLoadingTests_RecursiveLoads_FromSerialize::RunTest(const FString& Parameters)
{
	FLoadingTestsScope LoadingTestScope(this);

	UAsyncLoadingTests_Shared::OnSerialize.BindLambda(
		[this](FArchive& Ar, UAsyncLoadingTests_Shared* Object)
		{
			if (Ar.IsLoading())
			{
				if (UObject* Obj = Object->SoftReference.LoadSynchronous())
				{
					TestTrue(TEXT("Recursive loads in serialize should be deserialized"), !Obj->HasAnyFlags(RF_NeedLoad));
					TestTrue(TEXT("Recursive loads in serialize should have their postload delayed until the caller is also ready to postload"), Obj->HasAnyFlags(RF_NeedPostLoad));
				}
			}
		}
	);

	LoadingTestScope.LoadObjects();

	return true;
}

/**
 * This test validates loading an object with a thread-safe postload synchronously inside a thread-safe postload.
 */
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FLoadingTests_RecursiveLoads_FromPostLoad_ThreadSafe,
	FLoadingTests_RecursiveLoads_Base,
	TEXT("System.Engine.Loading.RecursiveLoads.FromPostLoad_ThreadSafe"),
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)
bool FLoadingTests_RecursiveLoads_FromPostLoad_ThreadSafe::RunTest(const FString& Parameters)
{
	FLoadingTestsScope LoadingTestScope(this);

	// Set both object for thread-safe postloads so we get called earlier.
	UAsyncLoadingTests_Shared::OnIsPostLoadThreadSafe.BindLambda(
		[](const UAsyncLoadingTests_Shared* Object)
		{
			return true;
		}
	);

	// When running with async loading thread, this should be called on ALT.
	UAsyncLoadingTests_Shared::OnPostLoad.BindLambda(
		[this](UAsyncLoadingTests_Shared* Object)
		{
			// We expect objects that are thread-safe to postload, to have been postloaded before returning from their sync load.
			if (UObject* Obj = Object->SoftReference.LoadSynchronous())
			{
				TestFalse(TEXT("Sync loads inside thread-safe postload of objects that are thread-safe to postload should be fully loaded"), Obj->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad));
			}
		}
	);

	LoadingTestScope.LoadObjects();

	return true;
}

/**
 * This test validates loading an object synchronously inside a thread-safe (non-deferred) postload.
 */
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FLoadingTests_RecursiveLoads_FromPostLoad_ThreadUnsafe,
	FLoadingTests_RecursiveLoads_Base,
	TEXT("System.Engine.Loading.RecursiveLoads.FromPostLoad_ThreadUnsafe"),
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)
bool FLoadingTests_RecursiveLoads_FromPostLoad_ThreadUnsafe::RunTest(const FString& Parameters)
{
	FLoadingTestsScope LoadingTestScope(this);

	// Make the first postloads as thread-safe
	UAsyncLoadingTests_Shared::OnIsPostLoadThreadSafe.BindLambda(
		[](const UAsyncLoadingTests_Shared* Object)
		{
			return Object->GetPathName() == FLoadingTestsScope::ObjectPath1;
		}
	);

	UAsyncLoadingTests_Shared::OnPostLoad.BindLambda(
		[this](UAsyncLoadingTests_Shared* Object)
		{
			if (Object->GetPathName() == FLoadingTestsScope::ObjectPath1)
			{
				if (IsAsyncLoadingMultithreaded())
				{
					TestFalse(TEXT("Thread-safe postloads should get called from the async loading thread when it is active"), IsInGameThread());
				}
			
				if (UObject* Obj = Object->SoftReference.LoadSynchronous())
				{
					TestTrue(TEXT("Sync loads of non thread-safe objects from thread-safe postload should be deserialized"), !Obj->HasAnyFlags(RF_NeedLoad));

					// Since the Object returned will continue to postload on the game-thread, we can't verify the RF_NeedPostLoad flag as we could race
					// trying to look at the value depending on its state on the game-thread.
					
					// What we can do is verify below that postload is called from the game-thread on the object
				}
			}
			else
			{
				TestTrue(TEXT("Sync loads of non thread-safe objects from thread-safe postload should have their postload deferred on the game-thread"), IsInGameThread());
			}
		}
	);

	LoadingTestScope.LoadObjects();

	return true;
}

/**
 * This test validates loading an object synchronously during postload.
 */
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FLoadingTests_RecursiveLoads_FromDeferredPostLoad,
	FLoadingTests_RecursiveLoads_Base,
	TEXT("System.Engine.Loading.RecursiveLoads.FromDeferredPostLoad"),
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)
bool FLoadingTests_RecursiveLoads_FromDeferredPostLoad::RunTest(const FString& Parameters)
{
	FLoadingTestsScope LoadingTestScope(this);

	UAsyncLoadingTests_Shared::OnPostLoad.BindLambda(
		[this](UAsyncLoadingTests_Shared* Object)
		{
			if (UObject* Obj = Object->SoftReference.LoadSynchronous())
			{
				TestFalse(TEXT("Recursive loads in postload should be fully loaded"), Obj->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad));
			}
		}
	);

	LoadingTestScope.LoadObjects();

	return true;
}

/**
 * This test validates an error is emitted when flushing a requestid that is not a partial load from inside a recursive function.
 */
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FLoadingTests_RecursiveLoads_FullFlushFrom_Serialize,
	FLoadingTests_RecursiveLoads_Base,
	TEXT("System.Engine.Loading.RecursiveLoads.FullFlushFrom.Serialize"),
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)
bool FLoadingTests_RecursiveLoads_FullFlushFrom_Serialize::RunTest(const FString& Parameters)
{
	FLoadingTestsScope LoadingTestScope(this);

	AddExpectedMessage(TEXT("will result in a partially loaded package to avoid a deadlock."), EAutomationExpectedErrorFlags::Contains);

	int32 RequestId = 0;
	UAsyncLoadingTests_Shared::OnSerialize.BindLambda(
		[this, &RequestId](FArchive& Ar, UAsyncLoadingTests_Shared* Object)
		{
			// Do not try to flush ourself as this would lead to a fatal error :)
			// Just flush Package2 when we're in Package1
			if (Ar.IsLoading() && Object->GetPathName() == FLoadingTestsScope::ObjectPath1)
			{
				// Flush the requestId that has been created outside of the recursive load. This request
				// should be a full request and flushing it should result in an error being reported.
				FlushAsyncLoading(RequestId);

				UAsyncLoadingTests_Shared* Object2 = FindObject<UAsyncLoadingTests_Shared>(nullptr, FLoadingTestsScope::ObjectPath2);
				TestFalse(TEXT("The object should be serialized"), Object2->HasAnyFlags(RF_NeedLoad));
				TestTrue(TEXT("The object should not have been postloaded"), Object2->HasAnyFlags(RF_NeedPostLoad));
			}
		}
	);

	// Create a request before starting the loading test so we get a request that is not tagged as partial.
	RequestId = LoadPackageAsync(FLoadingTestsScope::PackagePath2);

	LoadingTestScope.LoadObjects();

	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FLoadingTests_RecursiveLoads_FullFlushFrom_PostLoad,
	FLoadingTests_RecursiveLoads_Base,
	TEXT("System.Engine.Loading.RecursiveLoads.FullFlushFrom.Postload"),
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)
bool FLoadingTests_RecursiveLoads_FullFlushFrom_PostLoad::RunTest(const FString& Parameters)
{
	FLoadingTestsScope LoadingTestScope(this);

	int32 RequestId = 0;
	UAsyncLoadingTests_Shared::OnPostLoad.BindLambda(
		[this, &RequestId](UAsyncLoadingTests_Shared* Object)
		{
			// Do not try to flush ourself as this would lead to a fatal error :)
			// Just flush Package2 when we're in Package1
			if (Object->GetPathName() == FLoadingTestsScope::ObjectPath1)
			{
				// Flush the requestId that has been created outside of the recursive load. This request
				// should be a full request and flushing it should result in an error being reported.
				FlushAsyncLoading(RequestId);

				UAsyncLoadingTests_Shared* Object2 = FindObject<UAsyncLoadingTests_Shared>(nullptr, FLoadingTestsScope::ObjectPath2);
				TestFalse(TEXT("The object should be serialized and postloaded"), Object2->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad));
			}
		}
	);

	// Create a request before starting the loading test so we get a request that is not tagged as partial.
	RequestId = LoadPackageAsync(FLoadingTestsScope::PackagePath2);

	LoadingTestScope.LoadObjects();

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
