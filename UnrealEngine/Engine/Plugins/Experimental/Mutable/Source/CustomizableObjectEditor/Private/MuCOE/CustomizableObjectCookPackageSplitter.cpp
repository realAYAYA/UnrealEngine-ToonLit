// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/CustomizableObjectCookPackageSplitter.h"

#include "Algo/Find.h"
#include "MuCO/CustomizableObject.h"
#include "UObject/NameTypes.h"
#include "UObject/Package.h"

REGISTER_COOKPACKAGE_SPLITTER(FCustomizableObjectCookPackageSplitter, UCustomizableObject);

namespace
{
// Look up a streamed Extension Data constant by name on a Customizable Object.
//
// Returns nullptr if not found.
FCustomizableObjectStreamedExtensionData* FindStreamedExtensionData(
	UCustomizableObject* Object,
	const FString& ContainerName)
{
	return Algo::FindByPredicate(
		Object->StreamedExtensionData,
			[&ContainerName](const FCustomizableObjectStreamedExtensionData& StreamedData)
			{
				const FSoftObjectPath& Path = StreamedData.GetPath().ToSoftObjectPath();
				
				// ContainerName should match the last element of the path, which could be the
				// sub-path string or the asset name.

				if (Path.GetSubPathString().Len() > 0)
				{
					return Path.GetSubPathString() == ContainerName;
				}

				return Path.GetAssetName() == ContainerName;
			});
}

// Finds a streamed Extension Data container by name and moves it to the given Outer
bool MoveContainerToNewOuter(
	UCustomizableObject* Object,
	const FString& ContainerName,
	UObject* NewOuter,
	UCustomizableObjectExtensionDataContainer*& OutContainer)
{
	OutContainer = nullptr;

	const FCustomizableObjectStreamedExtensionData* FoundData = FindStreamedExtensionData(Object, ContainerName);
	if (!FoundData)
	{
		UE_LOG(LogMutable, Error, TEXT("Couldn't find streamed Extension Data container with name %s in array of %d entries"),
			*ContainerName, Object->StreamedExtensionData.Num());

		return false;
	}

	UCustomizableObjectExtensionDataContainer* Container = FoundData->GetPath().LoadSynchronous();
	if (!Container)
	{
		UE_LOG(LogMutable, Error, TEXT("Failed to load streamed Extension Data container %s"),
			*FoundData->GetPath().ToString());

		return false;
	}

	// Ensure the target object doesn't exist
	check(!FindObject<UObject>(NewOuter, *Container->GetName()));

	// The Rename function moves the object into the given package
	if (!Container->Rename(nullptr, NewOuter, REN_DontCreateRedirectors))
	{
		UE_LOG(LogMutable, Error, TEXT("Failed to move streamed Extension Data container %s into Outer %s"),
			*Container->GetPathName(), *NewOuter->GetPathName());

		return false;
	}

	OutContainer = Container;
	return true;
}
}

bool FCustomizableObjectCookPackageSplitter::ShouldSplit(UObject* SplitData)
{
	const UCustomizableObject* Object = CastChecked<UCustomizableObject>(SplitData);

	return Object->StreamedExtensionData.Num() > 0;
}

TArray<ICookPackageSplitter::FGeneratedPackage> FCustomizableObjectCookPackageSplitter::GetGenerateList(
	const UPackage* OwnerPackage,
	const UObject* OwnerObject)
{
	const UCustomizableObject* Object = CastChecked<UCustomizableObject>(OwnerObject);

	TArray<ICookPackageSplitter::FGeneratedPackage> Result;

	// Generate a new package for each streamed Extension Data
	for (const FCustomizableObjectStreamedExtensionData& StreamedData : Object->StreamedExtensionData)
	{
		// The StreamedData container path should be of the form
		// OwnerPackageName.OwnerObjectName:ContainerName
		const FSoftObjectPath& StreamedDataPath = StreamedData.GetPath().ToSoftObjectPath();
		
		// Check that the StreamedData container has the OwnerObject as its Outer
		check(StreamedDataPath.GetWithoutSubPath() == FSoftObjectPath(OwnerObject));

		// Check that the ContainerName is valid and that there isn't another Outer level between
		// the OwnerObject and the container.
		check(StreamedDataPath.GetSubPathString().Len() > 0);
		check(!StreamedDataPath.GetSubPathString().Contains(SUBOBJECT_DELIMITER));

		ICookPackageSplitter::FGeneratedPackage& Package = Result.AddDefaulted_GetRef();
		// Because of the checks above, the container name must be unique within this Customizable
		// Object, so it's safe to use as a package path.
		Package.RelativePath = StreamedDataPath.GetSubPathString();
		Package.SetCreateAsMap(false);

		// To support iterative cooking, GenerationHash should only change when OwnerPackage
		// changes.
		//
		// The simplest and fastest way to do this is to set it to OwnerPackage's GUID.
		{
			// Zero the hash, as we won't be writing all bytes of it below
			Package.GenerationHash.Reset();
			
			// Ensure there's no padding within FGuid, as the padding could be uninitialized
			static_assert(sizeof(FGuid) == sizeof(uint32) * 4);
			// Ensure an FGuid will fit within the hash buffer
			static_assert(sizeof(FGuid) <= sizeof(FBlake3Hash::ByteArray));

			// Create a copy that we can get the address of
			//
			// Note that UPackage::GetGuid is deprecated but there's no replacement yet. The engine
			// team has advised me to use it anyway, so it's necessary to suppress the deprecation
			// warning here.
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			const FGuid OwnerGuid = OwnerPackage->GetGuid();
			PRAGMA_ENABLE_DEPRECATION_WARNINGS

			FMemory::Memcpy(Package.GenerationHash.GetBytes(), &OwnerGuid, sizeof(FGuid));

			// If OwnerGuid is non-zero, the hash should now be non-zero
			check(!OwnerGuid.IsValid() || !Package.GenerationHash.IsZero());
		}
	}

	return Result;
}

bool FCustomizableObjectCookPackageSplitter::PreSaveGeneratorPackage(
	UPackage* OwnerPackage,
	UObject* OwnerObject,
	const TArray<ICookPackageSplitter::FGeneratedPackageForPreSave>& PlaceholderPackages,
	TArray<UPackage*>& OutKeepReferencedPackages)
{
	// The CO is just about to be saved (i.e. produce the cooked version of the asset), so this
	// function needs to:
	// 
	// 1.	Move the streamed Extension Data out of the CO's package, so that it doesn't get saved
	//		into the cooked package.
	// 
	// 2.	Remove hard references to the streamed data, so that it doesn't get loaded as soon as
	//		the CO is loaded
	//
	// 3.	Fix up the soft references to point to the streamed data's new location

	UCustomizableObject* Object = CastChecked<UCustomizableObject>(OwnerObject);

	// There should be one generated package per streamed Extension Data
	check(Object->StreamedExtensionData.Num() == PlaceholderPackages.Num());

	// After the CO has been saved, the contract for ICookPackageSplitter states that we need to
	// restore the CO back to how it was before, so we need to save some information to help with
	// this.
	SavedContainerNames.Reset();

	for (const ICookPackageSplitter::FGeneratedPackageForPreSave& GeneratedPackage : PlaceholderPackages)
	{
		// Find the streamed data based on the name of the generated package
		FCustomizableObjectStreamedExtensionData* FoundData = FindStreamedExtensionData(Object, GeneratedPackage.RelativePath);
		if (!FoundData)
		{
			UE_LOG(LogMutable, Error, TEXT("Couldn't find streamed Extension Data container with name %s in array of %d entries"),
				*GeneratedPackage.RelativePath, Object->StreamedExtensionData.Num());

			return false;
		}

		// Move the streamed data to the generated package
		UCustomizableObjectExtensionDataContainer* Container = nullptr;
		if (!MoveContainerToNewOuter(Object, GeneratedPackage.RelativePath, GeneratedPackage.Package, Container))
		{
			return false;
		}

		SavedContainerNames.Add(GeneratedPackage.RelativePath);

		// Remove the hard reference and set the soft reference to the streamed data's new location
		FoundData->ConvertToSoftReferenceForCooking(TSoftObjectPtr<UCustomizableObjectExtensionDataContainer>(Container));
	}

	return true;
}

void FCustomizableObjectCookPackageSplitter::PostSaveGeneratorPackage(UPackage* OwnerPackage, UObject* OwnerObject)
{
	// Move the streamed data back into the CO's package and restore the StreamedExtensionData
	// array on the CO to how it was before PreSaveGeneratorPackage.

	UCustomizableObject* Object = CastChecked<UCustomizableObject>(OwnerObject);

	TArray<FCustomizableObjectStreamedExtensionData> NewArray;
	NewArray.Reserve(SavedContainerNames.Num());

	for (const FString& ContainerName : SavedContainerNames)
	{
		UCustomizableObjectExtensionDataContainer* Container = nullptr;
		MoveContainerToNewOuter(Object, ContainerName, OwnerObject, Container);

		NewArray.Emplace(Container);
	}

	Object->StreamedExtensionData = NewArray;
}

bool FCustomizableObjectCookPackageSplitter::PopulateGeneratedPackage(
	UPackage* OwnerPackage,
	UObject* OwnerObject,
	const FGeneratedPackageForPopulate& GeneratedPackage,
	TArray<UObject*>& OutObjectsToMove,
	TArray<UPackage*>& OutKeepReferencedPackages)
{
	// Move the container into its newly generated package

	UCustomizableObject* Object = CastChecked<UCustomizableObject>(OwnerObject);

	UCustomizableObjectExtensionDataContainer* Container = nullptr;
	if (!MoveContainerToNewOuter(Object, GeneratedPackage.RelativePath, GeneratedPackage.Package, Container))
	{
		return false;
	}

	OutObjectsToMove.Add(Container);

	return true;
}

void FCustomizableObjectCookPackageSplitter::PostSaveGeneratedPackage(
	UPackage* OwnerPackage,
	UObject* OwnerObject,
	const FGeneratedPackageForPopulate& GeneratedPackage)
{
	// Now that the generated package has been saved/cooked, move the container back to the CO, so
	// that everything is the same as it was before cooking.

	UCustomizableObject* Object = CastChecked<UCustomizableObject>(OwnerObject);

	UCustomizableObjectExtensionDataContainer* Container = nullptr;
	MoveContainerToNewOuter(Object, GeneratedPackage.RelativePath, OwnerObject, Container);
}
