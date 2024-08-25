// Copyright Epic Games, Inc. All Rights Reserved.

#include "AsyncLoadingTests_Shared.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "UObject/MetaData.h"
#include "UObject/SavePackage.h"

UAsyncLoadingTests_Shared::FOnPostLoadDelegate UAsyncLoadingTests_Shared::OnPostLoad;
UAsyncLoadingTests_Shared::FOnSerializeDelegate UAsyncLoadingTests_Shared::OnSerialize;
UAsyncLoadingTests_Shared::FOnIsReadyForAsyncPostLoadDelegate UAsyncLoadingTests_Shared::OnIsReadyForAsyncPostLoad;
UAsyncLoadingTests_Shared::FOnIsPostLoadThreadSafeDelegate UAsyncLoadingTests_Shared::OnIsPostLoadThreadSafe;

#if WITH_DEV_AUTOMATION_TESTS

void FLoadingTestsScope::CreateObjects()
{
	Package1 = CreatePackage();
	Object1 = NewObject<UAsyncLoadingTests_Shared>(Package1, ObjectName, RF_Public | RF_Standalone);

	Package2 = CreatePackage();
	Object2 = NewObject<UAsyncLoadingTests_Shared>(Package2, ObjectName, RF_Public | RF_Standalone);
}

void FLoadingTestsScope::DefaultMutateObjects()
{
	// This is the soft reference that we want to test loading for
	Object1->SoftReference = Object2;
}

void FLoadingTestsScope::SavePackages()
{
	// To avoid an error on save, we need to mark the package as fully loaded.
	for (const FString& PackageName : PackageNames)
	{
		if (UPackage* Package = FindObject<UPackage>(nullptr, *PackageName))
		{
			Package->MarkAsFullyLoaded();
		}
	}

	// Save packages to disk.
	for (const FString& PackageName : PackageNames)
	{
		if (UPackage* Package = FindObject<UPackage>(nullptr, *PackageName))
		{
			check(UPackage::SavePackage(Package, nullptr, *FPackageName::LongPackageNameToFilename(*PackageName, FPackageName::GetAssetPackageExtension()), FSavePackageArgs()));
		}
	}
}

void FLoadingTestsScope::GarbageCollect()
{
	TArray<FString> ObjectPaths;

	// Remove RF_Standalone from package and objects inside it.
	for (const FString& PackageName : PackageNames)
	{
		if (UPackage* Package = FindObject<UPackage>(nullptr, *PackageName))
		{
			Package->GetMetaData()->ClearFlags(RF_Standalone);
			ObjectPaths.Add(PackageName);

			ForEachObjectWithPackage(Package,
				[&ObjectPaths](UObject* Object)
				{
					ObjectPaths.Add(Object->GetPathName());
					Object->ClearFlags(RF_Standalone);
					return true;
				}
			);
		}
	}

	// Make sure everything we gathered can be properly found
	for (const FString& ObjectPath : ObjectPaths)
	{
		checkf(FindObject<UObject>(nullptr, *ObjectPath) != nullptr, TEXT("%s should be present in memory"), *ObjectPath);
	}

	// GC and make sure everything gets cleaned up before loading
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

	// Now make sure everything is gone
	for (const FString& ObjectPath : ObjectPaths)
	{
		checkf(FindObject<UObject>(nullptr, *ObjectPath) == nullptr, TEXT("%s should have been garbage collected"), *ObjectPath);
	}
}

void FLoadingTestsScope::LoadObjects()
{
	LoadPackage(nullptr, PackagePath1, LOAD_None);

	UAsyncLoadingTests_Shared* LocalObject1 = FindObject<UAsyncLoadingTests_Shared>(nullptr, ObjectPath1);
	AutomationTest.TestTrue(TEXT("The object should have been properly loaded recursively"), LocalObject1 != nullptr);
	if (LocalObject1)
	{
		AutomationTest.TestFalse(TEXT("The object should have been properly loaded recursively"), LocalObject1->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad));
	}
}

void FLoadingTestsScope::CleanupObjects()
{
	GarbageCollect();

	UAsyncLoadingTests_Shared::OnPostLoad.Unbind();
	UAsyncLoadingTests_Shared::OnSerialize.Unbind();
	UAsyncLoadingTests_Shared::OnIsPostLoadThreadSafe.Unbind();
	UAsyncLoadingTests_Shared::OnIsReadyForAsyncPostLoad.Unbind();
}

#endif // WITH_DEV_AUTOMATION_TESTS
