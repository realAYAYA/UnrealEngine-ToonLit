// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_LOW_LEVEL_TESTS

#include "TestHarness.h"
#include "TestMacros/Assertions.h"
#include "ObjectPtrTestClass.h"
#include "UObject/Package.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"
#include "UObject/LinkerPlaceholderExportObject.h"
#include "UObject/LinkerPlaceholderClass.h"
#include "UObject/ObjectHandleTracking.h"
#include "LowLevelTestsRunner/WarnFilterScope.h"

TEST_CASE("UE::CoreUObject::FObjectProperty::CheckValidAddress")
{
	bool bAllowRead = false;
#if UE_WITH_OBJECT_HANDLE_TRACKING
	auto CallbackHandle = UE::CoreUObject::AddObjectHandleReadCallback([&bAllowRead](TArrayView<const UObject* const> Objects)
		{
			if (!bAllowRead)
				FAIL("Unexpected read during CheckValidObject");
		});
	ON_SCOPE_EXIT
	{
		UE::CoreUObject::RemoveObjectHandleReadCallback(CallbackHandle);
	};
#endif
	UClass* Class = UObjectPtrTestClassWithRef::StaticClass();
	FObjectProperty* Property = CastField<FObjectProperty>(Class->FindPropertyByName(TEXT("ObjectPtr")));
	REQUIRE(Property != nullptr);

	UPackage* TestPackage = NewObject<UPackage>(nullptr, TEXT("TestPackageName"), RF_Transient);
	TestPackage->AddToRoot();
	ON_SCOPE_EXIT
	{
		TestPackage->RemoveFromRoot();
	};
	UObjectPtrTestClassWithRef* Obj = NewObject<UObjectPtrTestClassWithRef>(TestPackage, TEXT("Object"));
	UObjectPtrTestClass* Other = NewObject<UObjectPtrTestClass>(Obj, TEXT("Other"));

	//verify nothing happens by default
	CHECK(!Obj->ObjectPtr);
	Property->CheckValidObject(&Obj->ObjectPtr, nullptr);
	CHECK(!Obj->ObjectPtr);

	//valid assignment
	Obj->ObjectPtr = Other;
	Property->CheckValidObject(&Obj->ObjectPtr, nullptr);
	CHECK(Obj->ObjectPtr == Other);

	bAllowRead = true; //has to read the fullpath from the object to make the error message
	//assign a bad value to the pointer
	Obj->ObjectPtr = reinterpret_cast<UObjectPtrTestClass*>(Obj);
	CHECK(Obj->ObjectPtr != nullptr);

	UE::Testing::FWarnFilterScope _([](const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category)
		{
			if (Category == TEXT("LogProperty") && FCString::Strstr(Message, TEXT("Reference will be nullptred")) && Verbosity == ELogVerbosity::Type::Warning)
			{
				return true;
			}
			return false;
		});
	Property->CheckValidObject(&Obj->ObjectPtr, Other);
	CHECK(!Obj->ObjectPtr); //value should be nulled since the type was not compatible	
}

TEST_CASE("UE::CoreUObject::FObjectProperty::CheckValidAddressNonNullable")
{
	bool bAllowRead = false;
#if UE_WITH_OBJECT_HANDLE_TRACKING
	auto CallbackHandle = UE::CoreUObject::AddObjectHandleReadCallback([&bAllowRead](TArrayView<const UObject* const> Objects)
		{
			if (!bAllowRead)
			{
				FAIL("Unexpected read during CheckValidObject");
			}
		});
	ON_SCOPE_EXIT
	{
		UE::CoreUObject::RemoveObjectHandleReadCallback(CallbackHandle);
	};
#endif
	UClass* Class = UObjectPtrTestClassWithRef::StaticClass();
	FObjectProperty* Property = CastField<FObjectProperty>(Class->FindPropertyByName(TEXT("ObjectPtrNonNullable")));
	REQUIRE(Property != nullptr);

	UPackage* TestPackage = NewObject<UPackage>(nullptr, TEXT("TestPackageName"), RF_Transient);
	TestPackage->AddToRoot();
	ON_SCOPE_EXIT
	{
		TestPackage->RemoveFromRoot();
	};
	UObjectPtrTestClassWithRef* Obj = NewObject<UObjectPtrTestClassWithRef>(TestPackage, TEXT("Object"));
	UObjectPtrTestClass* Other = NewObject<UObjectPtrTestClass>(Obj, TEXT("Other"));

	//property is already null should stay null
	CHECK(!Obj->ObjectPtrNonNullable);
	Property->CheckValidObject(&Obj->ObjectPtrNonNullable, Other);
	CHECK(!Obj->ObjectPtrNonNullable);

	//valid assignment
	Obj->ObjectPtrNonNullable = Other;
	Property->CheckValidObject(&Obj->ObjectPtrNonNullable, nullptr);
	CHECK(Obj->ObjectPtrNonNullable == Other);

	bAllowRead = true; //has to read the fullpath from the object to make the error message
	//assign a bad value to the pointer
	Obj->ObjectPtrNonNullable = reinterpret_cast<UObjectPtrTestClass*>(Obj);
	CHECK(Obj->ObjectPtrNonNullable != nullptr);

	UE::Testing::FWarnFilterScope _([](const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category)
		{
			if (Category == TEXT("LogProperty") && FCString::Strstr(Message, TEXT("Reference will be reverted back to")) && Verbosity == ELogVerbosity::Type::Warning)
			{
				return true;
			}
			return false;
		});
	Property->CheckValidObject(&Obj->ObjectPtrNonNullable, Other);
	CHECK(Obj->ObjectPtrNonNullable == Other); //non nullable properties should be assigned the old value

	//assign a bad value to the pointer
	Obj->ObjectPtrNonNullable = reinterpret_cast<UObjectPtrTestClass*>(Obj);
	CHECK(Obj->ObjectPtrNonNullable != nullptr);

	//old value is required for non nullable properties
	REQUIRE_CHECK(Property->CheckValidObject(&Obj->ObjectPtrNonNullable, nullptr));
}


#endif
