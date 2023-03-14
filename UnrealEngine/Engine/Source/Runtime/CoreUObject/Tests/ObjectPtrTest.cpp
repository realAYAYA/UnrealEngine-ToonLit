// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/ObjectPtr.h"

#include "ObjectRefTrackingTestBase.h"

#include "Concepts/EqualityComparable.h"
#include "Serialization/ArchiveCountMem.h"
#include "Templates/Models.h"
#include "UObject/Interface.h"
#include "UObject/MetaData.h"
#include "UObject/ObjectRedirector.h"
#include "UObject/SoftObjectPath.h"

#include <type_traits>

namespace ObjectPtrTest
{
using FMutableObjectPtr = TObjectPtr<UObject>;
using FMutableInterfacePtr = TObjectPtr<UInterface>;
using FMutablePackagePtr = TObjectPtr<UPackage>;
using FConstObjectPtr = TObjectPtr<const UObject>;
using FConstInterfacePtr = TObjectPtr<const UInterface>;
using FConstPackagePtr = TObjectPtr<const UPackage>;

class UForwardDeclaredObjDerived;
class FForwardDeclaredNotObjDerived;

using UTestDummyObject = UMetaData;

static_assert(sizeof(FObjectPtr) == sizeof(FObjectHandle), "FObjectPtr type must always compile to something equivalent to an FObjectHandle size.");
static_assert(sizeof(FObjectPtr) == sizeof(void*), "FObjectPtr type must always compile to something equivalent to a pointer size.");
static_assert(sizeof(TObjectPtr<UObject>) == sizeof(void*), "TObjectPtr<UObject> type must always compile to something equivalent to a pointer size.");

// Ensure that a TObjectPtr is trivially copyable, (copy/move) constructible, (copy/move) assignable, and destructible
static_assert(std::is_trivially_copyable<FMutableObjectPtr>::value, "TObjectPtr must be trivially copyable");
static_assert(std::is_trivially_copy_constructible<FMutableObjectPtr>::value, "TObjectPtr must be trivially copy constructible");
static_assert(std::is_trivially_move_constructible<FMutableObjectPtr>::value, "TObjectPtr must be trivially move constructible");
static_assert(std::is_trivially_copy_assignable<FMutableObjectPtr>::value, "TObjectPtr must be trivially copy assignable");
static_assert(std::is_trivially_move_assignable<FMutableObjectPtr>::value, "TObjectPtr must be trivially move assignable");
static_assert(std::is_trivially_destructible<FMutableObjectPtr>::value, "TObjectPtr must be trivially destructible");

// Ensure that raw pointers can be used to construct wrapped object pointers and that const-ness isn't stripped when constructing or converting with raw pointers
static_assert(std::is_constructible<FMutableObjectPtr, UObject*>::value, "TObjectPtr<UObject> must be constructible from a raw UObject*");
static_assert(!std::is_constructible<FMutableObjectPtr, const UObject*>::value, "TObjectPtr<UObject> must not be constructible from a const raw UObject*");
static_assert(std::is_convertible<FMutableObjectPtr, UObject*>::value, "TObjectPtr<UObject> must be convertible to a raw UObject*");
static_assert(std::is_convertible<FMutableObjectPtr, const UObject*>::value, "TObjectPtr<UObject> must be convertible to a const raw UObject*");

static_assert(std::is_constructible<FConstObjectPtr, UObject*>::value, "TObjectPtr<const UObject> must be constructible from a raw UObject*");
static_assert(std::is_constructible<FConstObjectPtr, const UObject*>::value, "TObjectPtr<const UObject> must be constructible from a const raw UObject*");
static_assert(!std::is_convertible<FConstObjectPtr, UObject*>::value, "TObjectPtr<const UObject> must not be convertible to a raw UObject*");
static_assert(std::is_convertible<FConstObjectPtr, const UObject*>::value, "TObjectPtr<const UObject> must be convertible to a const raw UObject*");

// Ensure that a TObjectPtr<const UObject> is constructible and assignable from a TObjectPtr<UObject> but not vice versa
static_assert(std::is_constructible<FConstObjectPtr, const FMutableObjectPtr&>::value, "Missing constructor (TObjectPtr<const UObject> from TObjectPtr<UObject>)");
static_assert(!std::is_constructible<FMutableObjectPtr, const FConstObjectPtr&>::value, "Invalid constructor (TObjectPtr<UObject> from TObjectPtr<const UObject>)");
static_assert(std::is_assignable<FConstObjectPtr, const FMutableObjectPtr&>::value, "Missing assignment (TObjectPtr<const UObject> from TObjectPtr<UObject>)");
static_assert(!std::is_assignable<FMutableObjectPtr, const FConstObjectPtr&>::value, "Invalid assignment (TObjectPtr<UObject> from TObjectPtr<const UObject>)");

static_assert(std::is_constructible<FConstObjectPtr, const FConstObjectPtr&>::value, "Missing constructor (TObjectPtr<const UObject> from TObjectPtr<const UObject>)");
static_assert(std::is_assignable<FConstObjectPtr, const FConstObjectPtr&>::value, "Missing assignment (TObjectPtr<const UObject> from TObjectPtr<const UObject>)");

// Ensure that a TObjectPtr<UObject> is constructible and assignable from a TObjectPtr<UInterface> but not vice versa
static_assert(std::is_constructible<FMutableObjectPtr, const FMutableInterfacePtr&>::value, "Missing constructor (TObjectPtr<UObject> from TObjectPtr<UInterface>)");
static_assert(!std::is_constructible<FMutableInterfacePtr, const FMutableObjectPtr&>::value, "Invalid constructor (TObjectPtr<UInterface> from TObjectPtr<UObject>)");
static_assert(std::is_constructible<FConstObjectPtr, const FConstInterfacePtr&>::value, "Missing constructor (TObjectPtr<const UObject> from TObjectPtr<const UInterface>)");
static_assert(std::is_constructible<FConstObjectPtr, const FMutableInterfacePtr&>::value, "Missing constructor (TObjectPtr<const UObject> from TObjectPtr<UInterface>)");
static_assert(!std::is_constructible<FConstInterfacePtr, const FConstObjectPtr&>::value, "Invalid constructor (TObjectPtr<const UInterface> from TObjectPtr<const UObject>)");
static_assert(!std::is_constructible<FConstInterfacePtr, const FMutableObjectPtr&>::value, "Invalid constructor (TObjectPtr<const UInterface> from TObjectPtr<UObject>)");

static_assert(std::is_assignable<FMutableObjectPtr, const FMutableInterfacePtr&>::value, "Missing assignment (TObjectPtr<UObject> from TObjectPtr<UInterface>)");
static_assert(std::is_assignable<FConstObjectPtr, const FMutableInterfacePtr&>::value, "Missing assignment (TObjectPtr<const UObject> from TObjectPtr<UInterface>)");
static_assert(std::is_assignable<FConstObjectPtr, const FConstInterfacePtr&>::value, "Missing assignment (TObjectPtr<const UObject> from TObjectPtr<const UInterface>)");
static_assert(!std::is_assignable<FMutableInterfacePtr, const FMutableObjectPtr&>::value, "Invalid assignment (TObjectPtr<UInterface> from TObjectPtr<UObject>)");
static_assert(!std::is_assignable<FConstInterfacePtr, const FMutableObjectPtr&>::value, "Invalid assignment (TObjectPtr<const UInterface> from TObjectPtr<UObject>)");
static_assert(!std::is_assignable<FConstInterfacePtr, const FConstObjectPtr&>::value, "Invalid assignment (TObjectPtr<const UInterface> from TObjectPtr<const UObject>)");

// Ensure that TObjectPtr<[const] UObject> is comparable with another TObjectPtr<[const] UObject> regardless of constness
static_assert(TModels<CEqualityComparableWith, FConstObjectPtr, FConstObjectPtr>::Value, "Must be able to compare equality and inequality bidirectionally between TObjectPtr<const UObject> and TObjectPtr<const UObject>");
static_assert(TModels<CEqualityComparableWith, FMutableObjectPtr, FConstObjectPtr>::Value, "Must be able to compare equality and inequality bidirectionally between TObjectPtr<UObject> and TObjectPtr<const UObject>");

// Ensure that TObjectPtr<[const] UObject> is comparable with another TObjectPtr<[const] UInterface> regardless of constness
static_assert(TModels<CEqualityComparableWith, FConstObjectPtr, FConstInterfacePtr>::Value, "Must be able to compare equality and inequality bidirectionally between TObjectPtr<const UObject> and TObjectPtr<const UInterface>");
static_assert(TModels<CEqualityComparableWith, FMutableObjectPtr, FConstInterfacePtr>::Value, "Must be able to compare equality and inequality bidirectionally between TObjectPtr<UObject> and TObjectPtr<const UInterface>");
static_assert(TModels<CEqualityComparableWith, FConstObjectPtr, FMutableInterfacePtr>::Value, "Must be able to compare equality and inequality bidirectionally between TObjectPtr<const UObject> and TObjectPtr<UInterface>");
static_assert(TModels<CEqualityComparableWith, FMutableObjectPtr, FMutableInterfacePtr>::Value, "Must be able to compare equality and inequality bidirectionally between TObjectPtr<UObject> and TObjectPtr<UInterface>");

// Ensure that TObjectPtr<[const] UPackage> is not comparable with a TObjectPtr<[const] UInterface> regardless of constness
// TODO: This only ensures that at least one of the A==B,B==A,A!=B,B!=A operations fail, not that they all fail.
#if !(PLATFORM_MICROSOFT) || !defined(_MSC_EXTENSIONS) // MSVC static analyzer is run in non-conformance mode, and that causes these checks to fail.
static_assert(!TModels<CEqualityComparableWith, FConstPackagePtr, FConstInterfacePtr>::Value, "Must not be able to compare equality and inequality bidirectionally between TObjectPtr<const UPackage> and TObjectPtr<const UInterface>");
static_assert(!TModels<CEqualityComparableWith, FMutablePackagePtr, FConstInterfacePtr>::Value, "Must not be able to compare equality and inequality bidirectionally between TObjectPtr<UPackage> and TObjectPtr<const UInterface>");
static_assert(!TModels<CEqualityComparableWith, FConstPackagePtr, FMutableInterfacePtr>::Value, "Must not be able to compare equality and inequality bidirectionally between TObjectPtr<const UPackage> and TObjectPtr<UInterface>");
static_assert(!TModels<CEqualityComparableWith, FMutablePackagePtr, FMutableInterfacePtr>::Value, "Must not be able to compare equality and inequality bidirectionally between TObjectPtr<UPackage> and TObjectPtr<UInterface>");
#endif // #if !(PLATFORM_MICROSOFT) || !defined(_MSC_EXTENSIONS)

// Ensure that TObjectPtr<[const] UObject> is comparable with a raw pointer of the same referenced type regardless of constness
static_assert(TModels<CEqualityComparableWith, FConstObjectPtr, const UObject*>::Value, "Must be able to compare equality and inequality bidirectionally between TObjectPtr<const UObject> and const UObject*");
static_assert(TModels<CEqualityComparableWith, FMutableObjectPtr, const UObject*>::Value, "Must be able to compare equality and inequality bidirectionally between TObjectPtr<UObject> and const UObject*");
static_assert(TModels<CEqualityComparableWith, FConstObjectPtr, UObject*>::Value, "Must be able to compare equality and inequality bidirectionally between TObjectPtr<const UObject> and UObject*");
static_assert(TModels<CEqualityComparableWith, FMutableObjectPtr, UObject*>::Value, "Must be able to compare equality and inequality bidirectionally between TObjectPtr<UObject> and UObject*");

// Ensure that TObjectPtr<[const] UObject> is comparable with a UInterface raw pointer regardless of constness
static_assert(TModels<CEqualityComparableWith, FConstObjectPtr, const UInterface*>::Value, "Must be able to compare equality and inequality bidirectionally between TObjectPtr<const UObject> and const UInterface*");
static_assert(TModels<CEqualityComparableWith, FMutableObjectPtr, const UInterface*>::Value, "Must be able to compare equality and inequality bidirectionally between TObjectPtr<UObject> and const UInterface*");
static_assert(TModels<CEqualityComparableWith, FConstObjectPtr, UInterface*>::Value, "Must be able to compare equality and inequality bidirectionally between TObjectPtr<const UObject> and UInterface*");
static_assert(TModels<CEqualityComparableWith, FMutableObjectPtr, UInterface*>::Value, "Must be able to compare equality and inequality bidirectionally between TObjectPtr<UObject> and UInterface*");

// Ensure that TObjectPtr<[const] UInterface> is comparable with a UObject raw pointer regardless of constness
static_assert(TModels<CEqualityComparableWith, FConstInterfacePtr, const UObject*>::Value, "Must be able to compare equality and inequality bidirectionally between TObjectPtr<const UInterface> and const UObject*");
static_assert(TModels<CEqualityComparableWith, FMutableInterfacePtr, const UObject*>::Value, "Must be able to compare equality and inequality bidirectionally between TObjectPtr<UInterface> and const UObject*");
static_assert(TModels<CEqualityComparableWith, FConstInterfacePtr, UObject*>::Value, "Must be able to compare equality and inequality bidirectionally between TObjectPtr<const UInterface> and UObject*");
static_assert(TModels<CEqualityComparableWith, FMutableInterfacePtr, UObject*>::Value, "Must be able to compare equality and inequality bidirectionally between TObjectPtr<UInterface> and UObject*");

// Ensure that TObjectPtr<[const] UInterface> is not comparable with a UPackage raw pointer regardless of constness
// TODO: This only ensures that at least one of the A==B,B==A,A!=B,B!=A operations fail, not that they all fail.
static_assert(!TModels<CEqualityComparableWith, FConstInterfacePtr, const UPackage*>::Value, "Must not be able to compare equality and inequality bidirectionally between TObjectPtr<const UInterface> and const UPackage*");
static_assert(!TModels<CEqualityComparableWith, FMutableInterfacePtr, const UPackage*>::Value, "Must not be able to compare equality and inequality bidirectionally between TObjectPtr<UInterface> and const UPackage*");
static_assert(!TModels<CEqualityComparableWith, FConstInterfacePtr, UPackage*>::Value, "Must not be able to compare equality and inequality bidirectionally between TObjectPtr<const UInterface> and UPackage*");
static_assert(!TModels<CEqualityComparableWith, FMutableInterfacePtr, UPackage*>::Value, "Must not be able to compare equality and inequality bidirectionally between TObjectPtr<UInterface> and UPackage*");

// Ensure that TObjectPtr<[const] UInterface> is not comparable with a char raw pointer regardless of constness
// TODO: This only ensures that at least one of the A==B,B==A,A!=B,B!=A operations fail, not that they all fail.
static_assert(!TModels<CEqualityComparableWith, FConstObjectPtr, const char*>::Value, "Must not be able to compare equality and inequality bidirectionally between TObjectPtr<const UObject> and const UObject*");
static_assert(!TModels<CEqualityComparableWith, FMutableObjectPtr, const char*>::Value, "Must not be able to compare equality and inequality bidirectionally between TObjectPtr<UObject> and const UObject*");
static_assert(!TModels<CEqualityComparableWith, FConstObjectPtr, char*>::Value, "Must not be able to compare equality and inequality bidirectionally between TObjectPtr<const UObject> and UObject*");
static_assert(!TModels<CEqualityComparableWith, FMutableObjectPtr, char*>::Value, "Must not be able to compare equality and inequality bidirectionally between TObjectPtr<UObject> and UObject*");

// Ensure that TObjectPtr<[const] UObject> is comparable with nullptr regardless of constness
static_assert(TModels<CEqualityComparableWith, FConstObjectPtr, TYPE_OF_NULLPTR>::Value, "Must be able to compare equality and inequality bidirectionally between TObjectPtr<const UObject> and nullptr");
static_assert(TModels<CEqualityComparableWith, FMutableObjectPtr, TYPE_OF_NULLPTR>::Value, "Must be able to compare equality and inequality bidirectionally between TObjectPtr<UObject> and nullptr");

#if !UE_OBJECT_PTR_NONCONFORMANCE_SUPPORT // Specialized NULL support causes these checks to fail.
static_assert(!TModels<CEqualityComparableWith, FConstObjectPtr, long>::Value, "Should not be able to compare equality and inequality bidirectionally between TObjectPtr<const UObject> and long");
static_assert(!TModels<CEqualityComparableWith, FMutableObjectPtr, long>::Value, "Should not be able to compare equality and inequality bidirectionally between TObjectPtr<UObject> and long");
#endif // #if !UE_OBJECT_PTR_NONCONFORMANCE_SUPPORT

#if WITH_LOW_LEVEL_TESTS

// Ensure that the use of incomplete types doesn't provide a means to bypass type safety on TObjectPtr
// NOTE: This is disabled because we're permitting this operation with a deprecation warning.
//static_assert(!std::is_assignable<TObjectPtr<UForwardDeclaredObjDerived>, UForwardDeclaredObjDerived*>::value, "Should not be able to assign raw pointer of incomplete type that descends from UObject to exactly this type of TObjectPtr");
//static_assert(!std::is_assignable<TObjectPtr<FForwardDeclaredNotObjDerived>, FForwardDeclaredNotObjDerived*>::value, "Should not be able to assign raw pointer of incomplete type that does not descend from UObject to exactly this type of TObjectPtr");

class FObjectPtrTestBase : public FObjectRefTrackingTestBase
{
public:

protected:
};

TEST_CASE_METHOD(FObjectPtrTestBase, "CoreUObject::TObjectPtr::Null Behavior", "[CoreUObject][ObjectPtr]")
{
	TObjectPtr<UObject> NullObjectPtr(nullptr);
	TEST_TRUE(TEXT("Nullptr should equal a null object pointer"), nullptr == NullObjectPtr);
	TEST_TRUE(TEXT("A null object pointer should equal nullptr"), NullObjectPtr == nullptr);
	TEST_FALSE(TEXT("A null object pointer should evaluate to false"), !!NullObjectPtr);
	TEST_TRUE(TEXT("Negation of a null object pointer should evaluate to true"), !NullObjectPtr);
}

TEST_CASE_METHOD(FObjectPtrTestBase, "CoreUObject::TObjectPtr::Default Serialize", "[CoreUObject][ObjectPtr]")
{
	FSnapshotObjectRefMetrics ObjectRefMetrics(*this);

	const FName TestPackageName(TEXT("/Engine/Test/ObjectPtrDefaultSerialize/Transient"));
	UPackage* TestPackage = NewObject<UPackage>(nullptr, TestPackageName, RF_Transient);
	TestPackage->AddToRoot();
	UObject* TestSoftObject = NewObject<UTestDummyObject>(TestPackage, TEXT("DefaultSerializeObject"));

	FObjectPtr DefaultSerializeObjectPtr(FObjectRef {FName("/Engine/Test/ObjectPtrDefaultSerialize/Transient"), NAME_None, NAME_None, FObjectPathId("DefaultSerializeObject")});

	ObjectRefMetrics.TestNumResolves(TEXT("Unexpected resolve count after initializing an FObjectPtr"), UE_WITH_OBJECT_HANDLE_LATE_RESOLVE ? 0 : 1);
	ObjectRefMetrics.TestNumFailedResolves(TEXT("Unexpected resolve failure after initializing an FObjectPtr"), 0);
	ObjectRefMetrics.TestNumReads(TEXT("NumReads should not change when initializing an FObjectPtr"), 0);

	FArchiveUObject Writer;
	Writer << DefaultSerializeObjectPtr;

	ObjectRefMetrics.TestNumResolves(TEXT("Serializing an FObjectPtr should force it to resolve"), 1);
	ObjectRefMetrics.TestNumFailedResolves(TEXT("Unexpected resolve failure after serializing an FObjectPtr"), 0);
	ObjectRefMetrics.TestNumReads(TEXT("NumReads should increase after serializing an FObjectPtr"), 1);

	Writer << DefaultSerializeObjectPtr;

	ObjectRefMetrics.TestNumResolves(TEXT("Serializing an FObjectPtr twice should only require it to resolve once"), 1);
	ObjectRefMetrics.TestNumFailedResolves(TEXT("Unexpected resolve failure after serializing an FObjectPtr"), 0);
	ObjectRefMetrics.TestNumReads(TEXT("NumReads should increase after serializing an FObjectPtr"), 2);

	TestPackage->RemoveFromRoot();
}

TEST_CASE_METHOD(FObjectPtrTestBase, "CoreUObject::TObjectPtr::Soft Object Path", "[CoreUObject][ObjectPtr]")
{
	FSnapshotObjectRefMetrics ObjectRefMetrics(*this);

	const FName TestPackageName(TEXT("/Engine/Test/ObjectPtrSoftObjectPath/Transient"));
	UPackage* TestPackage = NewObject<UPackage>(nullptr, TestPackageName, RF_Transient);
	TestPackage->AddToRoot();
	UObject* TestSoftObject = NewObject<UTestDummyObject>(TestPackage, TEXT("TestSoftObject"));

	FObjectPtr DefaultSoftObjPtr(FObjectRef {FName("/Engine/Test/ObjectPtrSoftObjectPath/Transient"), NAME_None, NAME_None, FObjectPathId("TestSoftObject")});

	ObjectRefMetrics.TestNumResolves(TEXT("Unexpected resolve count after initializing an FObjectPtr"), UE_WITH_OBJECT_HANDLE_LATE_RESOLVE ? 0 : 1);
	ObjectRefMetrics.TestNumFailedResolves(TEXT("Unexpected resolve failure after initializing an FObjectPtr"), 0);
	ObjectRefMetrics.TestNumReads(TEXT("NumReads should not change when initializing an FObjectPtr"), 0);

	// Initializing a soft object path from a TObjectPtr that's unresolved should stay unresolved.
	FSoftObjectPath DefaultSoftObjPath(DefaultSoftObjPtr);
	ObjectRefMetrics.TestNumResolves(TEXT("Unexpected resolve count after initializing an FSoftObjectPath from an FObjectPtr"), 0);

	TEST_EQUAL_STR(TEXT("Soft object path constructed from an FObjectPtr does not have the expected path value"), TEXT("/Engine/Test/ObjectPtrSoftObjectPath/Transient.TestSoftObject"), *DefaultSoftObjPath.ToString());

	TestPackage->RemoveFromRoot();
}

TEST_CASE_METHOD(FObjectPtrTestBase, "CoreUObject::TObjectPtr::Forward Declared", "[CoreUObject][ObjectPtr]")
{
	UForwardDeclaredObjDerived* PtrFwd = nullptr;
	TObjectPtr<UForwardDeclaredObjDerived> ObjPtrFwd(MakeObjectPtrUnsafe<UForwardDeclaredObjDerived>(reinterpret_cast<UObject*>(PtrFwd)));
	TEST_TRUE(TEXT("Null forward declared pointer used to construct a TObjectPtr should result in a null TObjectPtr"), !ObjPtrFwd);
}

TEST_CASE_METHOD(FObjectPtrTestBase, "CoreUObject::TObjectPtr::Hash Consistency", "[CoreUObject][ObjectPtr]")
{
	const FName TestPackage1Name(TEXT("/Engine/Test/ObjectPtrHashConsistency1/Transient"));
	UPackage* TestPackage1 = NewObject<UPackage>(nullptr, TestPackage1Name, RF_Transient);
	TestPackage1->AddToRoot();
	UObject* TestOuter1 = NewObject<UTestDummyObject>(TestPackage1, TEXT("TestOuter1"));
	UObject* TestOuter2 = NewObject<UTestDummyObject>(TestPackage1, TEXT("TestOuter2"));
	UObject* TestOuter3 = NewObject<UTestDummyObject>(TestPackage1, TEXT("TestOuter3"));
	UObject* TestOuter4 = NewObject<UTestDummyObject>(TestPackage1, TEXT("TestOuter4"));

	UObject* TestPublicObject = NewObject<UTestDummyObject>(TestOuter1, TEXT("TestPublicObject"), RF_Public);
	UObjectRedirector* TestRedirectorToPublicObject1 = NewObject<UObjectRedirector>(TestOuter3, TEXT("TestPublicRedirector1"), RF_Public);
	TestRedirectorToPublicObject1->DestinationObject = TestPublicObject;
	UObjectRedirector* TestRedirectorToPublicObject2 = NewObject<UObjectRedirector>(TestOuter4, TEXT("TestPublicRedirector2"), RF_Public);
	TestRedirectorToPublicObject2->DestinationObject = TestPublicObject;

	// Perform hash consistency checks on public object reference
	{
		// Check that unresolved/resolved pointers produce the same hash
		FSnapshotObjectRefMetrics ObjectRefMetrics(*this);

		FObjectPtr TestPublicWrappedObjectPtr(FObjectRef{TestPackage1Name, NAME_None, NAME_None, FObjectPathId("TestOuter1.TestPublicObject")});
		ObjectRefMetrics.TestNumResolves(TEXT("Unexpected resolve count after initializing an FObjectPtr"), UE_WITH_OBJECT_HANDLE_LATE_RESOLVE ? 0 : 1);
		ObjectRefMetrics.TestNumFailedResolves(TEXT("Unexpected resolve failure after initializing an FObjectPtr"), 0);

		uint32 HashWrapped = GetTypeHash(TestPublicWrappedObjectPtr);
		ObjectRefMetrics.TestNumResolves(TEXT("Unexpected resolve count after hashing an FObjectPtr"), 1);
		ObjectRefMetrics.TestNumFailedResolves(TEXT("Unexpected resolve failure after hashing an FObjectPtr"), 0);

		TestPublicWrappedObjectPtr.Get();

		ObjectRefMetrics.TestNumResolves(TEXT("Unexpected resolve count after resolving an FObjectPtr"), 1);
		ObjectRefMetrics.TestNumFailedResolves(TEXT("Unexpected resolve failure after resolving an FObjectPtr"), 0);

		uint32 HashRaw = GetTypeHash(TestPublicObject);
		TEST_EQUAL(TEXT("Hash of raw public FObjectPtr should equal hash of wrapped public FObjectPtr"), HashRaw, HashWrapped);

		FObjectPtr TestPublicWrappedRedir1(FObjectRef{TestPackage1Name, NAME_None, NAME_None, FObjectPathId("TestOuter3.TestPublicRedirector1")});
		ObjectRefMetrics.TestNumResolves(TEXT("Unexpected resolve count after initializing an FObjectPtr"), UE_WITH_OBJECT_HANDLE_LATE_RESOLVE ? 1 : 2);
		ObjectRefMetrics.TestNumFailedResolves(TEXT("Unexpected resolve failure after initializing an FObjectPtr"), 0);

		uint32 HashWrappedRedir1 = GetTypeHash(TestPublicWrappedRedir1);
		ObjectRefMetrics.TestNumResolves(TEXT("Unexpected resolve count after hashing an FObjectPtr"), 2);
		ObjectRefMetrics.TestNumFailedResolves(TEXT("Unexpected resolve failure after hashing an FObjectPtr"), 0);

		TEST_EQUAL(TEXT("Hash of first wrapped public redirector should equal hash of wrapped public FObjectPtr it references"), HashWrapped, HashWrappedRedir1);

		FObjectPtr TestPublicWrappedRedir2(FObjectRef{TestPackage1Name, NAME_None, NAME_None, FObjectPathId("TestOuter4.TestPublicRedirector2")});
		ObjectRefMetrics.TestNumResolves(TEXT("Unexpected resolve count after initializing an FObjectPtr"), UE_WITH_OBJECT_HANDLE_LATE_RESOLVE ? 2 : 3);
		ObjectRefMetrics.TestNumFailedResolves(TEXT("Unexpected resolve failure after initializing an FObjectPtr"), 0);

		uint32 HashWrappedRedir2 = GetTypeHash(TestPublicWrappedRedir2);
		ObjectRefMetrics.TestNumResolves(TEXT("Unexpected resolve count after hashing an FObjectPtr"), 3);
		ObjectRefMetrics.TestNumFailedResolves(TEXT("Unexpected resolve failure after hashing an FObjectPtr"), 0);

		TEST_EQUAL(TEXT("Hash of first wrapped public redirector should equal hash of wrapped public FObjectPtr it references"), HashWrapped, HashWrappedRedir1);

		//Check that renaming an object doesn't change its hash
		TestPublicObject->Rename(TEXT("TestPublicObjectRenamed"));
		uint32 HashWrappedAfterRename = GetTypeHash(TestPublicWrappedObjectPtr);
		TEST_EQUAL(TEXT("Hash of resolved public FObjectPtr before rename should equal hash of resolved public FObjectPtr after rename"), HashWrappedAfterRename, HashWrapped);

		//Check that reparenting an object doesn't change its hash
		TestPublicObject->Rename(nullptr, TestOuter2);
		uint32 HashWrappedAfterReparent = GetTypeHash(TestPublicWrappedObjectPtr);
		TEST_EQUAL(TEXT("Hash of resolved public FObjectPtr before reparenting should equal hash of resolved public FObjectPtr after reparenting"), HashWrappedAfterReparent, HashWrapped);
	}

	TestPackage1->RemoveFromRoot();
}


TEST_CASE_METHOD(FObjectPtrTestBase, "CoreUObject::TObjectPtr::Long Path", "[CoreUObject][ObjectPtr]")
{
	const FName TestPackage1Name(TEXT("/Engine/Test/FObjectPtrTestLongPath/Transient"));
	UPackage* TestPackage1 = NewObject<UPackage>(nullptr, TestPackage1Name, RF_Transient);
	TestPackage1->AddToRoot();

	ON_SCOPE_EXIT{
		TestPackage1->RemoveFromRoot();
	};

	UObject* TestObject1 = NewObject<UTestDummyObject>(TestPackage1, TEXT("TestObject1"));
	UObject* TestObject2 = NewObject<UTestDummyObject>(TestObject1, TEXT("TestObject2"));
	UObject* TestObject3 = NewObject<UTestDummyObject>(TestObject2, TEXT("TestObject3"));
	UObject* TestObject4 = NewObject<UTestDummyObject>(TestObject3, TEXT("TestObject4"));

	FObjectPathId LongPath(TestObject4);
	FObjectPathId::ResolvedNameContainerType ResolvedNames;
	LongPath.Resolve(ResolvedNames);

	TEST_EQUAL(TEXT("Resolved path from FObjectPathId should have 4 elements"), ResolvedNames.Num(), 4);
	TEST_EQUAL(TEXT("Resolved path from FObjectPathId should have TestObject1 at element 0"), ResolvedNames[0], TestObject1->GetFName());
	TEST_EQUAL(TEXT("Resolved path from FObjectPathId should have TestObject2 at element 1"), ResolvedNames[1], TestObject2->GetFName());
	TEST_EQUAL(TEXT("Resolved path from FObjectPathId should have TestObject3 at element 2"), ResolvedNames[2], TestObject3->GetFName());
	TEST_EQUAL(TEXT("Resolved path from FObjectPathId should have TestObject4 at element 3"), ResolvedNames[3], TestObject4->GetFName());
}

// @TODO: OBJPTR: We should have a test that ensures that lazy loading of an object with an external package is handled correctly.
//				  This should also include external packages in the outer chain of the target object.
// IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FObjectPtrTestExternalPackages, FObjectPtrTestBase, TEXT(TEST_NAME_ROOT ".ExternalPackages"), ObjectPtrTestFlags)
// bool FObjectPtrTestExternalPackages::RunTest(const FString& Parameters)
// {
// 	const FName TestExternalPackage1Name(TEXT("/Engine/Test/ObjectPtrExternalPackages1/Transient"));
// 	UPackage* TestExternalPackage1 = NewObject<UPackage>(nullptr, TestExternalPackage1Name, RF_Transient);
// 	TestExternalPackage1->SetPackageFlags(PKG_EditorOnly | PKG_ContainsMapData);
// 	TestExternalPackage1->AddToRoot();

// 	const FName TestPackage1Name(TEXT("/Engine/Test/ObjectPtrExternalPackages1/Transient"));
// 	UPackage* TestPackage1 = NewObject<UPackage>(nullptr, TestPackage1Name, RF_Transient);
// 	TestPackage1->AddToRoot();
// 	UObject* TestOuter1 = NewObject<UTestDummyObject>(TestPackage1, TEXT("TestOuter1"));
// 	UObject* TestOuter2 = NewObject<UTestDummyObject>(TestPackage1, TEXT("TestOuter2"));
// 	UObject* TestOuter3 = NewObject<UTestDummyObject>(TestPackage1, TEXT("TestOuter3"));
// 	UObject* TestOuter4 = NewObject<UTestDummyObject>(TestPackage1, TEXT("TestOuter4"));

// 	UObject* TestPublicObject = NewObject<UTestDummyObject>(TestOuter4, TEXT("TestPublicObject"), RF_Public);
// 	TestPublicObject->SetExternalPackage(TestExternalPackage1);

// 	TestPackage1->RemoveFromRoot();
// 	TestExternalPackage1->RemoveFromRoot();
// 	return true;
// }

// @TODO: OBJPTR: We should have a test that ensures that we can (de)serialize an FObjectPtr to FLinkerSave/FLinkerLoad and that upon load the object
//			pointer is not resolved if we are in a configuration that supports lazy load.  This is proving difficult due to the restrictions around how
//			FLinkerSave/FLinkerLoad is used.
// IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FObjectPtrTestLinkerSerializeBehavior, FObjectPtrTestBase, TEXT(TEST_NAME_ROOT ".LinkerSerialize"), ObjectPtrTestFlags)
// bool FObjectPtrTestLinkerSerializeBehavior::RunTest(const FString& Parameters)
// {
// 	FSnapshotObjectRefMetrics ObjectRefMetrics(*this);
// 	FObjectPtr DefaultTexturePtr(FObjectRef {FName("/Engine/EngineResources/DefaultTexture"), NAME_None, NAME_None, FObjectPathId("DefaultTexture")});

// 	TUniquePtr<FLinkerSave> Linker = TUniquePtr<FLinkerSave>(new FLinkerSave(nullptr /*InOuter*/, false /*bForceByteSwapping*/, true /*bSaveUnversioned*/));
// 	return true;
// }


#endif

class UForwardDeclaredObjDerived: public UObject {};
class FForwardDeclaredNotObjDerived {};

}

#if WITH_LOW_LEVEL_TESTS

//bunch of class to reproduce multiple inheritance issues.
//need to live outside of the namespace be IMPLEMENT_CORE_INTRINSIC_CLASS don't like namespaces

class FTestBaseClass
{
public:
	virtual ~FTestBaseClass() = default;
	virtual void VirtFunc() { };
};

class UMiddleClass : public UObject, public FTestBaseClass
{
	DECLARE_CLASS_INTRINSIC(UMiddleClass, UObject, CLASS_MatchedSerializers, TEXT("/Script/CoreUObject"))

public:
	virtual void VirtFunc() override { };
};


class FAnotherBaseClass
{
public:
	virtual ~FAnotherBaseClass() = default;
	virtual void AnotherVirtFunc() { };
};


class UDerrivedClass : public UMiddleClass, public FAnotherBaseClass
{
	DECLARE_CLASS_INTRINSIC(UDerrivedClass, UMiddleClass, CLASS_MatchedSerializers, TEXT("/Script/CoreUObject"))
public:
	virtual void AnotherVirtFunc() override { };
};

IMPLEMENT_CORE_INTRINSIC_CLASS(UMiddleClass, UObject,
	{
	}
);

IMPLEMENT_CORE_INTRINSIC_CLASS(UDerrivedClass, UMiddleClass,
	{
	}
);

TEST_CASE("CoreUObject::TObjectPtr::TestEquals")
{
	const FName TestPackageName(TEXT("/Engine/Test/TestEquals/Transient"));
	UPackage* TestPackage = NewObject<UPackage>(nullptr, TestPackageName, RF_Transient);
	TestPackage->AddToRoot();
	UDerrivedClass* Obj = NewObject<UDerrivedClass>(TestPackage, TEXT("DefaultSerializeObject"));

	FTestBaseClass* BasePtr = Obj;
	TObjectPtr<UDerrivedClass> ObjPtr(Obj);

	CHECK(BasePtr == ObjPtr);
}

#endif 
