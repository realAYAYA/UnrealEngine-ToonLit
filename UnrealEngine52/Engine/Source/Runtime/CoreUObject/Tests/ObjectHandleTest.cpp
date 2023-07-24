// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_LOW_LEVEL_TESTS

#include "ObjectPtrTestClass.h"
#include "UObject/ObjectHandle.h"
#include "UObject/ObjectPtr.h"
#include "UObject/Package.h"
#include "UObject/ObjectResource.h"
#include "UObject/MetaData.h"
#include "HAL/PlatformProperties.h"
#include "ObjectRefTrackingTestBase.h"
#include "IO/IoDispatcher.h"
#include "TestHarness.h"
#include "UObject/ObjectRef.h"
#include "UObject/ObjectPathId.h"

static_assert(sizeof(FObjectHandle) == sizeof(void*), "FObjectHandle type must always compile to something equivalent to a pointer size.");

class FObjectHandleTestBase : public FObjectRefTrackingTestBase
{
public:
	
protected:
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
	void TestResolveFailure(UE::CoreUObject::Private::FPackedObjectRef PackedRef)
	{
		FSnapshotObjectRefMetrics ObjectRefMetrics(*this);
		FObjectHandle TargetHandle = { PackedRef.EncodedRef };
		UObject* ResolvedObject = FObjectPtr(TargetHandle).Get();
		ObjectRefMetrics.TestNumResolves(TEXT("NumResolves should be incremented by one after a resolve attempt"), 1);
		ObjectRefMetrics.TestNumReads(TEXT("NumReads should be incremented by one after a resolve attempt"), 1);

		CHECK(ResolvedObject == nullptr);
		ObjectRefMetrics.TestNumFailedResolves(TEXT("NumFailedResolves should be incremented by one after a failed resolve attempt"), 1);
	}
#endif

#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE || UE_WITH_OBJECT_HANDLE_TRACKING
	void TestResolvableNonNull(const ANSICHAR* PackageName, const ANSICHAR* ObjectName, bool bExpectSubRefReads)
	{

		FSnapshotObjectRefMetrics ObjectRefMetrics(*this);
		FObjectRef TargetRef(FName(PackageName), NAME_None, NAME_None, UE::CoreUObject::Private::FObjectPathId(ObjectName));
		UObject* ResolvedObject = TargetRef.Resolve();
		FObjectPtr Ptr(ResolvedObject);
		Ptr.Get();
		TEST_TRUE(TEXT("expected not null"), ResolvedObject != nullptr);
		ObjectRefMetrics.TestNumResolves(TEXT("NumResolves should be incremented by one after a resolve attempt"), 1);
		ObjectRefMetrics.TestNumReads(TEXT("NumReads should be incremented by one after a resolve attempt"), 1, bExpectSubRefReads /*bAllowAdditionalReads*/);
		ObjectRefMetrics.TestNumFailedResolves(TEXT("NumFailedResolves should not change after a successful resolve attempt"), 0);
	}

	void TestResolveFailure(const ANSICHAR* PackageName, const ANSICHAR* ObjectName)
	{
		FSnapshotObjectRefMetrics ObjectRefMetrics(*this);
		FObjectRef TargetRef(FName(PackageName), NAME_None, NAME_None, UE::CoreUObject::Private::FObjectPathId(ObjectName));
		const UObject* ResolvedObject = TargetRef.Resolve();
		ObjectRefMetrics.TestNumResolves(TEXT("NumResolves should be incremented by one after a resolve attempt"), 1);
		ObjectRefMetrics.TestNumReads(TEXT("NumReads should be incremented by one after a resolve attempt"), 1);
		CHECK(ResolvedObject == nullptr);
		ObjectRefMetrics.TestNumFailedResolves(TEXT("NumFailedResolves should be incremented by one after a failed resolve attempt"), 1);
	}
#endif
};

TEST_CASE_METHOD(FObjectHandleTestBase, "CoreUObject::FObjectHandle::Null Behavior", "[CoreUObject][ObjectHandle]")
{
	FObjectHandle TargetHandle = UE::CoreUObject::Private::MakeObjectHandle(nullptr);

	TEST_TRUE(TEXT("Handle to target is null"), IsObjectHandleNull(TargetHandle));
	TEST_TRUE(TEXT("Handle to target is resolved"), IsObjectHandleResolved(TargetHandle));

	FSnapshotObjectRefMetrics ObjectRefMetrics(*this);
	UObject* ResolvedObject = UE::CoreUObject::Private::ResolveObjectHandle(TargetHandle);

	TEST_EQUAL(TEXT("Resolved object is equal to original object"), (UObject*)nullptr, ResolvedObject);

	ObjectRefMetrics.TestNumFailedResolves(TEXT("NumFailedResolves should not change after a resolve attempt on a null handle"), 0);
	ObjectRefMetrics.TestNumResolves(TEXT("NumResolves should not change after a resolve attempt on a null handle"), 0);
	ObjectRefMetrics.TestNumReads(TEXT("NumReads should be incremented by one after a resolve attempt on a null handle"), 1);
}

TEST_CASE_METHOD(FObjectHandleTestBase, "CoreUObject::FObjectHandle::Pointer Behavior", "[CoreUObject][ObjectHandle]")
{
	FObjectHandle TargetHandle = UE::CoreUObject::Private::MakeObjectHandle((UObject*)0x0042);

	TEST_FALSE(TEXT("Handle to target is null"), IsObjectHandleNull(TargetHandle));
	TEST_TRUE(TEXT("Handle to target is resolved"), IsObjectHandleResolved(TargetHandle));

	FSnapshotObjectRefMetrics ObjectRefMetrics(*this);
	UObject* ResolvedObject = UE::CoreUObject::Private::ResolveObjectHandle(TargetHandle);

	TEST_EQUAL(TEXT("Resolved object is equal to original object"), (UObject*)0x0042, ResolvedObject);

	ObjectRefMetrics.TestNumResolves(TEXT("NumResolves should not change after a resolve attempt on a pointer handle"), 0);
	ObjectRefMetrics.TestNumFailedResolves(TEXT("NumFailedResolves should not change after a resolve attempt on a pointer handle"), 0);
	ObjectRefMetrics.TestNumReads(TEXT("NumReads should be incremented by one after a resolve attempt on a pointer handle"),1);
}

#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
TEST_CASE_METHOD(FObjectHandleTestBase, "CoreUObject::FObjectHandle::Resolve Engine Content Target", "[CoreUObject][ObjectHandle]")
{
	const FName TestPackageName(TEXT("/Engine/Test/ObjectPtrDefaultSerialize/Transient"));
	UPackage* TestPackage = NewObject<UPackage>(nullptr, TestPackageName, RF_Transient);
	TestPackage->AddToRoot();
	UObject* TestSoftObject = NewObject<UObjectPtrTestClass>(TestPackage, TEXT("DefaultSerializeObject"));
	UObject* TestSubObject = NewObject<UObjectPtrTestClass>(TestSoftObject, TEXT("SubObject"));
	ON_SCOPE_EXIT{
		TestPackage->RemoveFromRoot();
	};

	TestResolvableNonNull("/Engine/Test/ObjectPtrDefaultSerialize/Transient", "DefaultSerializeObject.SubObject", true);
	TestResolvableNonNull("/Engine/Test/ObjectPtrDefaultSerialize/Transient", "DefaultSerializeObject", false);
}


// TODO: Disabled until warnings and errors related to loading a non-existent package have been fixed.
DISABLED_TEST_CASE_METHOD(FObjectHandleTestBase, "CoreUObject::FObjectHandle::Resolve Non Existent Target", "[CoreUObject][ObjectHandle]")
{
	// Confirm we don't successfully resolve an incorrect reference to engine content
	TestResolveFailure("/Engine/EngineResources/NonExistentPackageName_0", "DefaultTexture");

	const FName TestPackageName(TEXT("/Engine/Test/ObjectPtrDefaultSerialize/Transient"));
	UPackage* TestPackage = NewObject<UPackage>(nullptr, TestPackageName, RF_Transient);
	TestPackage->AddToRoot();
	UObject* TestSoftObject = NewObject<UObjectPtrTestClass>(TestPackage, TEXT("DefaultSerializeObject"));
	ON_SCOPE_EXIT{
		TestPackage->RemoveFromRoot();
	};

	TestResolveFailure("/Engine/Test/ObjectPtrDefaultSerialize/Transient", "DefaultSerializeObject_DoesNotExist");
}

TEST_CASE_METHOD(FObjectHandleTestBase, "CoreUObject::FObjectHandle::Resolve Script Target", "[CoreUObject][ObjectHandle]")
{
	// Confirm we successfully resolve a correct reference to engine content
	TestResolvableNonNull("/Script/CoreUObject", "MetaData", true);
}

#endif

TEST_CASE_METHOD(FObjectHandleTestBase, "CoreUObject::TObjectPtr::HandleNullGetClass", "[CoreUObject][ObjectHandle]")
{
	TObjectPtr<UObject> Ptr = nullptr;
	TEST_TRUE(TEXT("TObjectPtr.GetClass should return null on a null object"), Ptr.GetClass() == nullptr);
}

#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
TEST_CASE("CoreUObject::FObjectHandle::Names")
{
	const FName TestPackageName(TEXT("/Engine/Test/PackageResolve/Transient"));
	UPackage* TestPackage = NewObject<UPackage>(nullptr, TestPackageName, RF_Transient);
	TestPackage->AddToRoot();
	UObject* Obj1 = NewObject<UObjectPtrTestClass>(TestPackage, TEXT("DefaultSerializeObject"));
	ON_SCOPE_EXIT{
		TestPackage->RemoveFromRoot();
	};

	FObjectPtr PackagePtr(MakeUnresolvedHandle(TestPackage));
	FObjectPtr Obj1Ptr(MakeUnresolvedHandle(Obj1));

	CHECK(!PackagePtr.IsResolved());
	CHECK(TestPackage->GetPathName() == PackagePtr.GetPathName());
	CHECK(TestPackage->GetFName() == PackagePtr.GetFName());
	CHECK(TestPackage->GetName() == PackagePtr.GetName());
	//CHECK(TestPackage->GetFullName() == PackagePtr.GetFullName()); TODO always been broken
	CHECK(!PackagePtr.IsResolved());

	CHECK(!Obj1Ptr.IsResolved());
	CHECK(Obj1->GetPathName() == Obj1Ptr.GetPathName());
	CHECK(Obj1->GetFName() == Obj1Ptr.GetFName());
	CHECK(Obj1->GetName() == Obj1Ptr.GetName());
	//CHECK(Obj1->GetFullName() == Obj1Ptr.GetFullName()); TODO always been broken
	CHECK(!Obj1Ptr.IsResolved());
}
#endif

#if UE_WITH_OBJECT_HANDLE_TRACKING || UE_WITH_OBJECT_HANDLE_LATE_RESOLVE

TEST_CASE("CoreUObject::ObjectRef")
{
	const FName TestPackageName(TEXT("/Engine/Test/ObjectRef/Transient"));
	UPackage* TestPackage = NewObject<UPackage>(nullptr, TestPackageName, RF_Transient);
	TestPackage->AddToRoot();
	UObject* Obj1 = NewObject<UObjectPtrTestClass>(TestPackage, TEXT("DefaultSerializeObject"));
	UObject* Inner1 = NewObject<UObjectPtrTestClass>(Obj1, TEXT("Inner"));
	ON_SCOPE_EXIT{
		TestPackage->RemoveFromRoot();
	};

	{
		FObjectImport ObjectImport(Obj1);
		FObjectRef ObjectRef(Obj1);

		CHECK(ObjectImport.ClassPackage == ObjectRef.ClassPackageName);
		CHECK(ObjectImport.ClassName == ObjectRef.ClassName);
		CHECK(TestPackage->GetFName() == ObjectRef.PackageName);
	}

	{
		FObjectImport ObjectImport(Inner1);
		FObjectRef ObjectRef(Inner1);

		CHECK(ObjectImport.ClassPackage == ObjectRef.ClassPackageName);
		CHECK(ObjectImport.ClassName == ObjectRef.ClassName);
		CHECK(TestPackage->GetFName() == ObjectRef.PackageName);
	}
}

#endif

#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE

TEST_CASE_METHOD(FObjectHandleTestBase, "CoreUObject::TObjectPtr::Null Behavior", "[CoreUObject][ObjectHandle]")
{
	TObjectPtr<UObject> Ptr = nullptr;
	UObjectPtrTestClass* TestObject = nullptr;

	uint32 ResolveCount = 0;
	auto ResolveDelegate = [&ResolveCount](const FObjectRef& SourceRef, UPackage* ObjectPackage, UObject* Object)
		{
			++ResolveCount;
		};
	auto Handle = UE::CoreUObject::AddObjectHandleReferenceResolvedCallback(ResolveDelegate);
	ON_SCOPE_EXIT
	{
		UE::CoreUObject::RemoveObjectHandleReferenceResolvedCallback(Handle);
	};
	//compare against all flavours of nullptr, should not try and resolve this pointer
	CHECK(Ptr == nullptr); CHECK(ResolveCount == 0u);
	CHECK(nullptr == Ptr); CHECK(ResolveCount == 0u);
	CHECK_FALSE(Ptr != nullptr); CHECK(ResolveCount == 0u);
	CHECK_FALSE(nullptr != Ptr); CHECK(ResolveCount == 0u);
	CHECK(!Ptr); CHECK(ResolveCount == 0u);

	//using an if otherwise the macros try to convert to a pointer and not use the bool operator
	if (Ptr)
	{
		CHECK(false);
	}
	else
	{
		CHECK(true);
	}
	CHECK(ResolveCount == 0u);

	CHECK(Ptr == TestObject); CHECK(ResolveCount == 0u);
	CHECK(TestObject == Ptr); CHECK(ResolveCount == 0u);
	CHECK_FALSE(Ptr != TestObject); CHECK(ResolveCount == 0u);
	CHECK_FALSE(TestObject != Ptr); CHECK(ResolveCount == 0u);

	FObjectRef TargetRef(FName("SomePackage"), FName("ClassPackageName"), FName("ClassName"), UE::CoreUObject::Private::FObjectPathId("ObjectName"));
	UE::CoreUObject::Private::FPackedObjectRef PackedObjectRef = UE::CoreUObject::Private::MakePackedObjectRef(TargetRef);
	FObjectPtr ObjectPtr({ PackedObjectRef.EncodedRef });
	REQUIRE(!ObjectPtr.IsResolved()); //make sure not resolved

	//an unresolved pointers compared against nullptr should still not resolve
	Ptr = *reinterpret_cast<TObjectPtr<UObject>*>(&ObjectPtr);
	CHECK_FALSE(Ptr == nullptr); CHECK(ResolveCount == 0u);
	CHECK_FALSE(nullptr == Ptr); CHECK(ResolveCount == 0u);
	CHECK(Ptr != nullptr); CHECK(ResolveCount == 0u);
	CHECK(nullptr != Ptr); CHECK(ResolveCount == 0u);
	CHECK_FALSE(!Ptr); CHECK(ResolveCount == 0u);

	//using an if otherwise the macros try to convert to a pointer and not use the bool operator
	if (Ptr)
	{
		CHECK(true);
	}
	else
	{
		CHECK(false);
	}
	CHECK(ResolveCount == 0u);

	//test an unresolve pointer against a null raw pointer
	CHECK_FALSE(Ptr == TestObject); CHECK(ResolveCount == 0u);
	CHECK_FALSE(TestObject == Ptr);	CHECK(ResolveCount == 0u);
	CHECK(Ptr != TestObject); CHECK(ResolveCount == 0u);
	CHECK(TestObject != Ptr); CHECK(ResolveCount == 0u);

	//creating a real object for something that can resolve
	const FName TestPackageName(TEXT("/Engine/Test/ObjectPtrDefaultSerialize/Transient"));
	UPackage* TestPackage = NewObject<UPackage>(nullptr, TestPackageName, RF_Transient);
	TestPackage->AddToRoot();

	const FName TestObjectName(TEXT("MyObject"));
	TestObject = NewObject<UObjectPtrTestClass>(TestPackage, TestObjectName, RF_Transient);
	TObjectPtr<UObject> TestNotLazyObject = NewObject<UObjectPtrNotLazyTestClass>(TestPackage, TEXT("NotLazy"), RF_Transient);

	//compare resolved ptr against nullptr
	TObjectPtr<UObject> ResolvedPtr = TestObject;
	CHECK(ResolvedPtr.IsResolved());
	CHECK(Ptr != ResolvedPtr);  CHECK(ResolveCount == 0u);
	CHECK(ResolvedPtr != Ptr);  CHECK(ResolveCount == 0u);
	CHECK_FALSE(Ptr == ResolvedPtr);  CHECK(ResolveCount == 0u);
	CHECK_FALSE(ResolvedPtr == Ptr);  CHECK(ResolveCount == 0u);

	//compare unresolved against nullptr
	FObjectPtr FPtr(MakeUnresolvedHandle(TestObject));
	TObjectPtr<UObject> UnResolvedPtr = *reinterpret_cast<TObjectPtr<UObject>*>(&FPtr);
	CHECK(!UnResolvedPtr.IsResolved());
	CHECK_FALSE(Ptr == UnResolvedPtr); CHECK(ResolveCount == 0u);
	CHECK_FALSE(UnResolvedPtr == Ptr); CHECK(ResolveCount == 0u);
	CHECK(Ptr != UnResolvedPtr); CHECK(ResolveCount == 0u);
	CHECK(UnResolvedPtr != Ptr); CHECK(ResolveCount == 0u);

	//compare unresolved against resolved not equal
	CHECK_FALSE(TestNotLazyObject == UnResolvedPtr); CHECK(ResolveCount == 0u); 
	CHECK_FALSE(UnResolvedPtr == TestNotLazyObject); CHECK(ResolveCount == 0u);
	CHECK(TestNotLazyObject != UnResolvedPtr); CHECK(ResolveCount == 0u);
	CHECK(UnResolvedPtr != TestNotLazyObject); CHECK(ResolveCount == 0u);

	//compare resolved against naked pointer
	Ptr = TestObject;
	REQUIRE(Ptr.IsResolved());
	CHECK(Ptr == TestObject); CHECK(ResolveCount == 0u);
	CHECK(TestObject == Ptr); CHECK(ResolveCount == 0u);
	CHECK_FALSE(Ptr != TestObject); CHECK(ResolveCount == 0u);
	CHECK_FALSE(TestObject != Ptr); CHECK(ResolveCount == 0u);

	//compare resolved pointer and unresolved of the same object
	CHECK(Ptr == UnResolvedPtr); CHECK(ResolveCount == 0u);
	CHECK(UnResolvedPtr == Ptr); CHECK(ResolveCount == 0u);
	CHECK_FALSE(Ptr != UnResolvedPtr); CHECK(ResolveCount == 0u);
	CHECK_FALSE(UnResolvedPtr != Ptr); CHECK(ResolveCount == 0u);

	TestObject = nullptr;
	CHECK_FALSE(Ptr == TestObject); CHECK(ResolveCount == 0u);
	CHECK_FALSE(TestObject == Ptr); CHECK(ResolveCount == 0u);
	CHECK(Ptr != TestObject); CHECK(ResolveCount == 0u);
	CHECK(TestObject != Ptr); CHECK(ResolveCount == 0u);

	TestObject = static_cast<UObjectPtrTestClass*>(Ptr.Get());
	Ptr = nullptr;
	CHECK_FALSE(Ptr == TestObject); CHECK(ResolveCount == 0u);
	CHECK_FALSE(TestObject == Ptr); CHECK(ResolveCount == 0u);
	CHECK(Ptr != TestObject); CHECK(ResolveCount == 0u);
	CHECK(TestObject != Ptr); CHECK(ResolveCount == 0u);
}

#endif

#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE

TEST_CASE_METHOD(FObjectHandleTestBase, "CoreUObject::FObjectHandle::Resolve Malformed Handle", "[CoreUObject][ObjectHandle]")
{
	TestResolveFailure(UE::CoreUObject::Private::FPackedObjectRef { 0xFFFF'FFFF'FFFF'FFFFull });
	TestResolveFailure(UE::CoreUObject::Private::FPackedObjectRef { 0xEFEF'EFEF'EFEF'EFEFull });
}
#endif // UE_WITH_OBJECT_HANDLE_LATE_RESOLVE

#endif
