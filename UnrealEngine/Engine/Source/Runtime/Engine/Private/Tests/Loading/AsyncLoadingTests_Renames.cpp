// Copyright Epic Games, Inc. All Rights Reserved.

#include "AsyncLoadingTests_Shared.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "UObject/CoreRedirects.h"
#include "UObject/SavePackage.h"

#if WITH_DEV_AUTOMATION_TESTS

// All Renames tests should run on zenloader only as the other loaders are not compliant.
class FLoadingTests_Renames_Base : public FLoadingTests_ZenLoaderOnly_Base
{
public:
	using FLoadingTests_ZenLoaderOnly_Base::FLoadingTests_ZenLoaderOnly_Base;

protected:
	void DoRenameTest(bool bWithSave, bool bWithGC)
	{
		// Package 1 has a hard ref to package 2.
		auto MutateObjects =
			[](FLoadingTestsScope& Scope)
			{
				Scope.Object1->HardReference = Scope.Object2;
			};

		FLoadingTestsScope LoadingTestScope(this, MutateObjects);

		UAsyncLoadingTests_Shared* Object2 = LoadObject<UAsyncLoadingTests_Shared>(nullptr, FLoadingTestsScope::ObjectPath2);

		// Move the Object in package 2 to a new package 3, leaving a redirector
		// This leaves the zenloader global import store in a bad state since it still thinks object in Package 2 is still an export.
		UPackage* NewPackage = LoadingTestScope.CreatePackage();
		Object2->Rename(nullptr, NewPackage);

		FString MovedObjectName = Object2->GetFullName();

		if (bWithSave)
		{
			LoadingTestScope.SavePackages();
		}

		if (bWithGC)
		{
			LoadingTestScope.GarbageCollect();
		}

		// Load object 1, which should trigger a fixup during import because object2 will be resolved in the wrong package.
		UAsyncLoadingTests_Shared* Object1 = LoadObject<UAsyncLoadingTests_Shared>(nullptr, FLoadingTestsScope::ObjectPath1);

		TestNotEqual<UObject*>("Object1 should have been loaded", Object1, nullptr);
		if (Object1)
		{
			TestNotEqual<UObject*>("Object1's hard reference should have been loaded", Object1->HardReference, nullptr);

			if (Object1->HardReference)
			{
				TestEqual("Object1's hard reference object name should be the moved object", Object1->HardReference.GetFullName(), MovedObjectName);
			}
		}

		// Try to reload package2 for good measure as the previous operation might have caused the loader to become confused.
		UPackage* Package = LoadPackage(nullptr, FLoadingTestsScope::PackagePath2, LOAD_None);

		TestNotEqual<UObject*>("Package should have been loaded", Package, nullptr);
		if (Package)
		{
			TestEqual("Package name is wrong", Package->GetPathName(), FLoadingTestsScope::PackagePath2);
		}
		
		Object2 = LoadObject<UAsyncLoadingTests_Shared>(nullptr, FLoadingTestsScope::ObjectPath2);

		TestNotEqual<UObject*>("Object should have been loaded", Object2, nullptr);
		if (Object2)
		{
			TestEqual("Object name is wrong", Object2->GetFullName(), MovedObjectName);
		}
	}
};

/**
 * This test validates the loader behavior when an object is moved to a different package in memory only.
 */
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FLoadingTests_Renames_ObjectMovedToAnotherPackage_InMemory,
	FLoadingTests_Renames_Base,
	TEXT("System.Engine.Loading.Renames.ObjectMovedToAnotherPackage.InMemory"),
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)
bool FLoadingTests_Renames_ObjectMovedToAnotherPackage_InMemory::RunTest(const FString& Parameters)
{
	static constexpr bool bWithSave = false;
	static constexpr bool bWithGC   = false;
	DoRenameTest(bWithSave, bWithGC);

	return true;
}

/**
 * This test validates the loader behavior when an object is moved to a different package and dirtied packages are written to disk.
 */
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FLoadingTests_Renames_ObjectMovedToAnotherPackage_OnDisk,
	FLoadingTests_Renames_Base,
	TEXT("System.Engine.Loading.Renames.ObjectMovedToAnotherPackage.OnDisk"),
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)
bool FLoadingTests_Renames_ObjectMovedToAnotherPackage_OnDisk::RunTest(const FString& Parameters)
{
	static constexpr bool bWithSave = true;
	static constexpr bool bWithGC = false;
	DoRenameTest(bWithSave, bWithGC);

	return true;
}

/**
 * This test validates the loader behavior when an object is moved to a different package and dirtied packages are written to disk, then reloaded after a full GC.
 */
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FLoadingTests_Renames_ObjectMovedToAnotherPackage_OnDiskReload,
	FLoadingTests_Renames_Base,
	TEXT("System.Engine.Loading.Renames.ObjectMovedToAnotherPackage.OnDiskReload"),
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)
bool FLoadingTests_Renames_ObjectMovedToAnotherPackage_OnDiskReload::RunTest(const FString& Parameters)
{
	static constexpr bool bWithSave = true;
	static constexpr bool bWithGC = true;
	DoRenameTest(bWithSave, bWithGC);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
