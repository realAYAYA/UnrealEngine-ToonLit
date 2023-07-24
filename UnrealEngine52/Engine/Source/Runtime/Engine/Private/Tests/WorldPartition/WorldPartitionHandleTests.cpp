// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "WorldPartition/ActorDescContainer.h"
#include "Engine/World.h"

#if WITH_EDITOR
#include "PackageTools.h"
#endif

#if WITH_DEV_AUTOMATION_TESTS

#define TEST_NAME_ROOT "System.Engine.WorldPartition"

#if WITH_EDITOR
struct FWorldPartitionActorDescUnitTestAcccessor
{
	inline static uint32 GetSoftRefCount(const FWorldPartitionActorDesc* ActorDesc)
	{
		return ActorDesc->GetSoftRefCount();
	}

	inline static uint32 GetHardRefCount(const FWorldPartitionActorDesc* ActorDesc)
	{
		return ActorDesc->GetHardRefCount();
	}
};
#endif

namespace WorldPartitionTests
{
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldPartitionSoftRefTest, TEST_NAME_ROOT ".Handle", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

#if WITH_EDITOR
	template <class LoadingContextType>
	void PerformTests(FWorldPartitionSoftRefTest* Test)
	{
		auto TestTrue = [Test](const TCHAR* What, bool Value) -> bool { return Test->TestTrue(What, Value); };
		auto TestFalse = [Test](const TCHAR* What, bool Value) -> bool { return Test->TestFalse(What, Value); };

		UActorDescContainer* ActorDescContainer = NewObject<UActorDescContainer>(GetTransientPackage());
		ActorDescContainer->Initialize({ nullptr, TEXT("/Engine/WorldPartition/WorldPartitionUnitTest") });

		FWorldPartitionHandle Handle = FWorldPartitionHandle(ActorDescContainer, FGuid(TEXT("5D9F93BA407A811AFDDDAAB4F1CECC6A")));

		{
			FWorldPartitionHandle ActorWithChildActorComponentHandle = FWorldPartitionHandle(ActorDescContainer, FGuid(TEXT("538856174ECD465948488F9441AE9251")));
			FWorldPartitionReference ActorWithChildActorComponentReference = ActorWithChildActorComponentHandle.ToReference();
		}		

		FWorldPartitionReference Reference;
		{
			LoadingContextType LoadingContext;
			Reference = FWorldPartitionReference(ActorDescContainer, FGuid(TEXT("0D2B04D240BE5DE58FE437A8D2DBF5C9")));
		}

		TestTrue(TEXT("Handle container"), Handle->GetContainer() == ActorDescContainer);
		TestTrue(TEXT("Reference container"), Reference->GetContainer() == ActorDescContainer);

		TestTrue(TEXT("Handle soft refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetSoftRefCount(Handle.Get()) == 1);
		TestTrue(TEXT("Handle hard refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetHardRefCount(Handle.Get()) == 0);
		TestTrue(TEXT("Reference soft refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetSoftRefCount(Reference.Get()) == 0);
		TestTrue(TEXT("Reference hard refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetHardRefCount(Reference.Get()) == 1);

		// Pin handle scope test
		{
			FWorldPartitionHandlePinRefScope PinRefScopeHandle(Handle);
			TestTrue(TEXT("Pin to Handle soft refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetSoftRefCount(Handle.Get()) == 1);
			TestTrue(TEXT("Pin to Handle hard refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetHardRefCount(Handle.Get()) == 0);
		}

		// Pin reference scope test
		{
			FWorldPartitionHandlePinRefScope PinRefScopeReference(Reference);
			TestTrue(TEXT("Pin to Reference soft refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetSoftRefCount(Reference.Get()) == 0);
			TestTrue(TEXT("Pin to Reference hard refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetHardRefCount(Reference.Get()) == 2);
		}

		TestTrue(TEXT("Handle/Reference equality"), Handle != Reference);
		TestTrue(TEXT("Reference/Handle equality"), Reference != Handle);
		
		TestTrue(TEXT("Handle soft refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetSoftRefCount(Handle.Get()) == 1);
		TestTrue(TEXT("Handle hard refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetHardRefCount(Handle.Get()) == 0);
		TestTrue(TEXT("Reference soft refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetSoftRefCount(Reference.Get()) == 0);
		TestTrue(TEXT("Reference hard refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetHardRefCount(Reference.Get()) == 1);

		// Conversions
		{
			FWorldPartitionHandle HandleToReference = Reference.ToHandle();
			TestTrue(TEXT("Handle/Reference equality"), HandleToReference == Reference);
			TestTrue(TEXT("Reference/Handle equality"), Reference == HandleToReference);
			TestTrue(TEXT("Reference soft refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetSoftRefCount(Reference.Get()) == 1);
			TestTrue(TEXT("Reference hard refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetHardRefCount(Reference.Get()) == 1);

			FWorldPartitionReference ReferenceToHandle = Handle.ToReference();
			TestTrue(TEXT("Handle/Reference equality"), ReferenceToHandle == Handle);
			TestTrue(TEXT("Handle/Reference equality"), Handle == ReferenceToHandle);
			TestTrue(TEXT("Reference soft refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetSoftRefCount(Reference.Get()) == 1);
			TestTrue(TEXT("Reference hard refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetHardRefCount(Reference.Get()) == 1);
		}

		TestTrue(TEXT("Handle soft refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetSoftRefCount(Handle.Get()) == 1);
		TestTrue(TEXT("Handle hard refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetHardRefCount(Handle.Get()) == 0);
		TestTrue(TEXT("Reference soft refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetSoftRefCount(Reference.Get()) == 0);
		TestTrue(TEXT("Reference hard refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetHardRefCount(Reference.Get()) == 1);

		// inplace new test
		{
			uint8 Buffer[sizeof(FWorldPartitionHandle)];
			FWorldPartitionHandle* HandlePtr = new (Buffer) FWorldPartitionHandle(Reference.ToHandle());

			TestTrue(TEXT("Handle array soft refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetSoftRefCount(Reference.Get()) == 1);
			TestTrue(TEXT("Handle array hard refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetHardRefCount(Reference.Get()) == 1);

			HandlePtr->~FWorldPartitionHandle();

			TestTrue(TEXT("Handle array soft refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetSoftRefCount(Reference.Get()) == 0);
			TestTrue(TEXT("Handle array hard refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetHardRefCount(Reference.Get()) == 1);
		}

		// TArray test
		{
			TArray<FWorldPartitionHandle> HandleList;
			HandleList.Add(Handle);

			TestTrue(TEXT("Handle array soft refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetSoftRefCount(Handle.Get()) == 2);
			TestTrue(TEXT("Handle array hard refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetHardRefCount(Handle.Get()) == 0);

			FWorldPartitionReference ReferenceToHandle = Handle.ToReference();
			TestTrue(TEXT("Handle/Reference equality"), ReferenceToHandle == Handle);
			TestTrue(TEXT("Handle/Reference equality"), Handle == ReferenceToHandle);
			TestTrue(TEXT("Handle soft refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetSoftRefCount(Handle.Get()) == 2);
			TestTrue(TEXT("Handle hard refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetHardRefCount(Handle.Get()) == 1);

			TestTrue(TEXT("Handle array contains handle"), HandleList.Contains(Handle));
			TestTrue(TEXT("Handle array contains reference"), HandleList.Contains(ReferenceToHandle));

			HandleList.Add(Reference.ToHandle());
			
			TestTrue(TEXT("Handle array contains reference"), HandleList.Contains(Reference));
			TestTrue(TEXT("Handle array soft refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetSoftRefCount(Reference.Get()) == 1);
			TestTrue(TEXT("Handle array hard refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetHardRefCount(Reference.Get()) == 1);
			
			HandleList.Remove(Handle);
			TestTrue(TEXT("Handle array soft refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetSoftRefCount(Handle.Get()) == 1);
			TestTrue(TEXT("Handle array hard refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetHardRefCount(Handle.Get()) == 1);

			HandleList.Remove(Reference.ToHandle());
			TestTrue(TEXT("Handle array soft refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetSoftRefCount(Reference.Get()) == 0);
			TestTrue(TEXT("Handle array hard refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetHardRefCount(Reference.Get()) == 1);
		}

		TestTrue(TEXT("Handle soft refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetSoftRefCount(Handle.Get()) == 1);
		TestTrue(TEXT("Handle hard refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetHardRefCount(Handle.Get()) == 0);
		TestTrue(TEXT("Reference soft refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetSoftRefCount(Reference.Get()) == 0);
		TestTrue(TEXT("Reference hard refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetHardRefCount(Reference.Get()) == 1);

		// TSet test
		{
			TSet<FWorldPartitionHandle> HandleSet;
			HandleSet.Add(Handle);

			TestTrue(TEXT("Handle set soft refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetSoftRefCount(Handle.Get()) == 2);
			TestTrue(TEXT("Handle set hard refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetHardRefCount(Handle.Get()) == 0);

			FWorldPartitionReference ReferenceToHandle = Handle.ToReference();
			TestTrue(TEXT("Handle/Reference equality"), ReferenceToHandle == Handle);
			TestTrue(TEXT("Handle/Reference equality"), Handle == ReferenceToHandle);
			TestTrue(TEXT("Handle soft refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetSoftRefCount(Handle.Get()) == 2);
			TestTrue(TEXT("Handle hard refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetHardRefCount(Handle.Get()) == 1);

			TestTrue(TEXT("Handle set contains handle"), HandleSet.Contains(Handle));
			TestTrue(TEXT("Handle set contains reference"), HandleSet.Contains(ReferenceToHandle.ToHandle()));

			HandleSet.Add(Reference.ToHandle());
			TestTrue(TEXT("Handle set contains reference"), HandleSet.Contains(Reference.ToHandle()));

			TestTrue(TEXT("Reference soft refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetSoftRefCount(Reference.Get()) == 1);
			TestTrue(TEXT("Reference hard refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetHardRefCount(Reference.Get()) == 1);
		}

		TestTrue(TEXT("Handle soft refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetSoftRefCount(Handle.Get()) == 1);
		TestTrue(TEXT("Handle hard refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetHardRefCount(Handle.Get()) == 0);
		TestTrue(TEXT("Reference soft refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetSoftRefCount(Reference.Get()) == 0);
		TestTrue(TEXT("Reference hard refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetHardRefCount(Reference.Get()) == 1);

		// Move tests
		{
			// Handle move
			TestTrue(TEXT("Handle soft refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetSoftRefCount(Handle.Get()) == 1);
			TestTrue(TEXT("Handle hard refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetHardRefCount(Handle.Get()) == 0);
			{
				FWorldPartitionHandle HandleCopy(MoveTemp(Handle));
				TestTrue(TEXT("Handle move src not valid"), !Handle.IsValid());
				TestTrue(TEXT("Handle move dst valid"), HandleCopy.IsValid());
				TestTrue(TEXT("Handle soft refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetSoftRefCount(HandleCopy.Get()) == 1);
				TestTrue(TEXT("Handle hard refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetHardRefCount(HandleCopy.Get()) == 0);

				Handle = MoveTemp(HandleCopy);
				TestTrue(TEXT("Handle move src not valid"), !HandleCopy.IsValid());
				TestTrue(TEXT("Handle move dst valid"), Handle.IsValid());
				TestTrue(TEXT("Handle soft refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetSoftRefCount(Handle.Get()) == 1);
				TestTrue(TEXT("Handle hard refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetHardRefCount(Handle.Get()) == 0);
			}

			// Reference move
			TestTrue(TEXT("Reference soft refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetSoftRefCount(Reference.Get()) == 0);
			TestTrue(TEXT("Reference hard refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetHardRefCount(Reference.Get()) == 1);
			{
				FWorldPartitionReference ReferenceCopy(MoveTemp(Reference));
				TestTrue(TEXT("Reference move src not valid"), !Reference.IsValid());
				TestTrue(TEXT("Reference move dst valid"), ReferenceCopy.IsValid());
				TestTrue(TEXT("Reference soft refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetSoftRefCount(ReferenceCopy.Get()) == 0);
				TestTrue(TEXT("Reference hard refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetHardRefCount(ReferenceCopy.Get()) == 1);

				Reference = MoveTemp(ReferenceCopy);
				TestTrue(TEXT("Reference move src not valid"), !ReferenceCopy.IsValid());
				TestTrue(TEXT("Reference move dst valid"), Reference.IsValid());
				TestTrue(TEXT("Reference soft refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetSoftRefCount(Reference.Get()) == 0);
				TestTrue(TEXT("Reference hard refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetHardRefCount(Reference.Get()) == 1);
			}

			// Handle reference move
			{
				FWorldPartitionHandle HandleFromReference(MoveTemp(Reference));
				TestTrue(TEXT("Handle move src not valid"), !Reference.IsValid());
				TestTrue(TEXT("Handle move dst valid"), HandleFromReference.IsValid());
				TestTrue(TEXT("Handle soft refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetSoftRefCount(HandleFromReference.Get()) == 1);
				TestTrue(TEXT("Handle hard refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetHardRefCount(HandleFromReference.Get()) == 0);

				Reference = MoveTemp(HandleFromReference);
				TestTrue(TEXT("Handle move src not valid"), !HandleFromReference.IsValid());
				TestTrue(TEXT("Reference move dst valid"), Reference.IsValid());
				TestTrue(TEXT("Reference soft refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetSoftRefCount(Reference.Get()) == 0);
				TestTrue(TEXT("Reference hard refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetHardRefCount(Reference.Get()) == 1);
			}

			// Reference handle move
			{
				FWorldPartitionReference ReferenceFromHandle(MoveTemp(Handle));
				TestTrue(TEXT("Handle move src not valid"), !Handle.IsValid());
				TestTrue(TEXT("Reference move dst valid"), ReferenceFromHandle.IsValid());
				TestTrue(TEXT("Reference soft refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetSoftRefCount(ReferenceFromHandle.Get()) == 0);
				TestTrue(TEXT("Reference hard refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetHardRefCount(ReferenceFromHandle.Get()) == 1);

				Handle = MoveTemp(ReferenceFromHandle);
				TestTrue(TEXT("Reference move src not valid"), !ReferenceFromHandle.IsValid());
				TestTrue(TEXT("Handle move dst valid"), Handle.IsValid());
				TestTrue(TEXT("Handle soft refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetSoftRefCount(Handle.Get()) == 1);
				TestTrue(TEXT("Handle hard refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetHardRefCount(Handle.Get()) == 0);
			}

			// Reset
			{
				FWorldPartitionReference ReferenceFromHandle(Handle.ToReference());
				TestTrue(TEXT("Reference valid"), ReferenceFromHandle.IsValid());
				TestTrue(TEXT("Reference soft refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetSoftRefCount(ReferenceFromHandle.Get()) == 1);
				TestTrue(TEXT("Reference hard refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetHardRefCount(ReferenceFromHandle.Get()) == 1);

				ReferenceFromHandle.Reset();
				TestTrue(TEXT("Reference not valid"), !ReferenceFromHandle.IsValid());
				TestTrue(TEXT("Handle soft refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetSoftRefCount(Handle.Get()) == 1);
				TestTrue(TEXT("Handle hard refcount"), FWorldPartitionActorDescUnitTestAcccessor::GetHardRefCount(Handle.Get()) == 0);
			}
		}

		// Make sure the container gets destroyed so we can test destructing dangling handles
		{
			TestTrue(TEXT("Invalid container test"), Handle.IsValid());
			TestTrue(TEXT("Invalid container test"), Reference.IsValid());

			// Make sure to cleanup world before collecting garbage so it gets uninitialized
			UPackage* Package = FindPackage(NULL, *ActorDescContainer->GetContainerPackage().ToString());
			check(Package);
			UWorld* World = UWorld::FindWorldInPackage(Package);
			check(World->IsInitialized());
			World->CleanupWorld();
			
			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

			// Unload Package so test can run twice without issues.
			UPackageTools::UnloadPackages({ Package });

			TestFalse(TEXT("Invalid container test"), Handle.IsValid());
			TestFalse(TEXT("Invalid container test"), Reference.IsValid());
		}
	};
#endif

	bool FWorldPartitionSoftRefTest::RunTest(const FString& Parameters)
	{
#if WITH_EDITOR
		// Handle tests
		WorldPartitionTests::PerformTests<FWorldPartitionLoadingContext::FImmediate>(this);
		WorldPartitionTests::PerformTests<FWorldPartitionLoadingContext::FDeferred>(this);

		// Serialization tests
		UActorDescContainer* ActorDescContainer = NewObject<UActorDescContainer>(GetTransientPackage());
		ActorDescContainer->Initialize({ nullptr, TEXT("/Engine/WorldPartition/WorldPartitionUnitTest") });

		for (FActorDescList::TIterator<> ActorDescIterator(ActorDescContainer); ActorDescIterator; ++ActorDescIterator)
		{
			UClass* ActorNativeClass = UClass::TryFindTypeSlow<UClass>(ActorDescIterator->GetNativeClass().ToString(), EFindFirstObjectOptions::ExactClass);
			TestFalse(TEXT("Actor Descriptor Serialization"), !ActorNativeClass);

			if (ActorNativeClass)
			{
				TUniquePtr<FWorldPartitionActorDesc> NewActorDesc(AActor::StaticCreateClassActorDesc(ActorNativeClass));

				FWorldPartitionActorDescInitData ActorDescInitData;
				ActorDescInitData.NativeClass = ActorNativeClass;
				ActorDescInitData.PackageName = ActorDescIterator->GetActorPackage();
				ActorDescInitData.ActorPath = ActorDescIterator->GetActorSoftPath();
			
				ActorDescIterator->SerializeTo(ActorDescInitData.SerializedData);
				NewActorDesc->Init(ActorDescInitData);

				TestTrue(TEXT("Actor Descriptor Serialization"), NewActorDesc->Equals(*ActorDescIterator));
			}
		}
#endif
		return true;
	}
}

#undef TEST_NAME_ROOT

#endif 
