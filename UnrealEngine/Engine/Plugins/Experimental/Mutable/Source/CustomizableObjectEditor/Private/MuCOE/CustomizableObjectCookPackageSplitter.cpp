// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/CustomizableObjectCookPackageSplitter.h"

#include "Algo/Find.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectPrivate.h"
#include "UObject/NameTypes.h"
#include "UObject/Package.h"

REGISTER_COOKPACKAGE_SPLITTER(FCustomizableObjectCookPackageSplitter, UCustomizableObject);

namespace
{
// Look up a streamed Resource Data constant by name on a Customizable Object.
//
// Returns nullptr if not found.
FCustomizableObjectStreamedResourceData* FindStreamedResourceData(
	TArray<FCustomizableObjectStreamedResourceData>& StreamedResources,
	const FString& ContainerName
	)
{
	return Algo::FindByPredicate(
		StreamedResources,
			[&ContainerName](const FCustomizableObjectStreamedResourceData& StreamedData)
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

enum class EMoveContainerError
{
	None,
	FailedToLoadContainer,
	NameCollision, // Object with that name already exists in the new outer
	RenameFailed,
};

const TCHAR* LexToString(EMoveContainerError Error)
{
	switch (Error)
	{
		case EMoveContainerError::None: return TEXT("None");
		case EMoveContainerError::FailedToLoadContainer: return TEXT("FailedToLoadContainer");
		case EMoveContainerError::NameCollision: return TEXT("NameCollision");
		case EMoveContainerError::RenameFailed: return TEXT("RenameFailed");
		default: return TEXT("Unknown");
	}
}

// Moves the StreamedResourceData's data container to the given Outer.
EMoveContainerError MoveContainerToNewOuter(
	UObject* NewOuter,
	const FCustomizableObjectStreamedResourceData* StreamedResourceData,
	UCustomizableObjectResourceDataContainer*& OutContainer
)
{
	check(StreamedResourceData);
	
	OutContainer = nullptr;

	UCustomizableObjectResourceDataContainer* Container = StreamedResourceData->GetPath().LoadSynchronous();
	if (!Container)
	{
		return EMoveContainerError::FailedToLoadContainer;
	}

	if (Container->GetOuter() != NewOuter)
	{
		// Ensure the target object doesn't exist
		if(FindObject<UObject>(NewOuter, *Container->GetName()))
		{
			return EMoveContainerError::NameCollision;
		}

		// The Rename function moves the object into the given package
		if (!Container->Rename(nullptr, NewOuter, REN_DontCreateRedirectors))
		{
			return EMoveContainerError::RenameFailed;
		}
	}

	OutContainer = Container;
	return EMoveContainerError::None;
}


void GenerateNewPackage(const FCustomizableObjectStreamedResourceData& StreamedData,
	const UPackage* OwnerPackage,
	const UObject* OwnerObject,
	TArray<ICookPackageSplitter::FGeneratedPackage>& Result)
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

}

bool FCustomizableObjectCookPackageSplitter::ShouldSplit(UObject* SplitData)
{
	const UCustomizableObject* Object = CastChecked<UCustomizableObject>(SplitData);

	return Object->GetPrivate()->GetStreamedResourceData().Num() > 0 || Object->GetPrivate()->GetStreamedExtensionData().Num() > 0;
}

TArray<ICookPackageSplitter::FGeneratedPackage> FCustomizableObjectCookPackageSplitter::GetGenerateList(
	const UPackage* OwnerPackage,
	const UObject* OwnerObject)
{
	// Keep a strong reference to the CO.
	StrongObject.Reset(OwnerObject);
	
	const UCustomizableObject* Object = CastChecked<UCustomizableObject>(OwnerObject);

	TArray<ICookPackageSplitter::FGeneratedPackage> Result;

	// Generate a new package for each streamed Resource Data
	for (const FCustomizableObjectStreamedResourceData& StreamedData : Object->GetPrivate()->GetStreamedResourceData())
	{
		GenerateNewPackage(StreamedData, OwnerPackage, OwnerObject, Result);
	}

	// Generate a new package for each streamed Extension Data
	for (const FCustomizableObjectStreamedResourceData& StreamedData : Object->GetPrivate()->GetStreamedExtensionData())
	{
		GenerateNewPackage(StreamedData, OwnerPackage, OwnerObject, Result);
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
	// 1.	Move the streamed Data out of the CO's package, so that it doesn't get saved
	//		into the cooked package.
	// 
	// 2.	Remove hard references to the streamed data, so that it doesn't get loaded as soon as
	//		the CO is loaded
	//
	// 3.	Fix up the soft references to point to the streamed data's new location
	const auto& PreSavePackage = [] (const ICookPackageSplitter::FGeneratedPackageForPreSave& GeneratedPackage,
		TArray<FCustomizableObjectStreamedResourceData>& StreamedResources
		) -> bool
	{
		FCustomizableObjectStreamedResourceData* FoundData = FindStreamedResourceData(StreamedResources, GeneratedPackage.RelativePath);
		if (!FoundData)
		{
			UE_LOG(LogMutable, Error, TEXT("Couldn't find streamed Resource Data container with name %s in array of %d entries"),
				*GeneratedPackage.RelativePath, StreamedResources.Num());

			return false;
		}

		// Move the streamed data to the generated package
		UCustomizableObjectResourceDataContainer* Container = nullptr;
		EMoveContainerError Error = MoveContainerToNewOuter(GeneratedPackage.Package, FoundData, Container);
		if (Error != EMoveContainerError::None)
		{
			UE_LOG(LogMutable, Error, TEXT("Failed to move container %s to new outer %s - %s"), *FoundData->GetPath().ToSoftObjectPath().ToString(), *GetPathNameSafe(GeneratedPackage.Package), LexToString(Error));
			return false;
		}

		// Remove the hard reference and set the soft reference to the streamed data's new location
		FoundData->ConvertToSoftReferenceForCooking(TSoftObjectPtr<UCustomizableObjectResourceDataContainer>(Container));

		return true;
	};

	UCustomizableObject* Object = CastChecked<UCustomizableObject>(OwnerObject);

	// There should be one generated package per streamed Resource Data
	const int32 NumStreamedData = Object->GetPrivate()->GetStreamedResourceData().Num();
	const int32 NumStreamedExtensionData = Object->GetPrivate()->GetStreamedExtensionData().Num();
	
	check(NumStreamedData + NumStreamedExtensionData == PlaceholderPackages.Num());


	// After the CO has been saved, the contract for ICookPackageSplitter states that we need to
	// restore the CO back to how it was before, so we need to save some information to help with
	// this.
	SavedContainerNames.Reset();
	SavedExtensionContainerNames.Reset();

	for (int32 Index = 0; Index < NumStreamedData; ++Index)
	{
		const ICookPackageSplitter::FGeneratedPackageForPreSave& GeneratedPackage = PlaceholderPackages[Index];
		if (!PreSavePackage(GeneratedPackage, Object->GetPrivate()->GetStreamedResourceData()))
		{
			return false;
		}

		SavedContainerNames.Add(GeneratedPackage.RelativePath);
	}

	for (int32 Index = 0; Index < NumStreamedExtensionData; ++Index)
	{
		const ICookPackageSplitter::FGeneratedPackageForPreSave& GeneratedPackage = PlaceholderPackages[NumStreamedData + Index];
		if (!PreSavePackage(GeneratedPackage, Object->GetPrivate()->GetStreamedExtensionData()))
		{
			return false;
		}

		SavedExtensionContainerNames.Add(GeneratedPackage.RelativePath);
	}

	return true;
}

void FCustomizableObjectCookPackageSplitter::PostSaveGeneratorPackage(UPackage* OwnerPackage, UObject* OwnerObject)
{
	// Move the streamed data back into the CO's package and restore the StreamedResourceData and StreamedExtensionData
	// array on the CO to how it was before PreSaveGeneratorPackage.

	UCustomizableObject* Object = CastChecked<UCustomizableObject>(OwnerObject);

	TArray<FCustomizableObjectStreamedResourceData> NewArray;
	NewArray.Reset(SavedContainerNames.Num());

	for (const FString& ContainerName : SavedContainerNames)
	{
		FCustomizableObjectStreamedResourceData* ResourceData = FindStreamedResourceData(Object->GetPrivate()->GetStreamedResourceData(), ContainerName);
		if (!ResourceData)
		{
			UE_LOG(LogMutable, Error, TEXT("Couldn't find streamed Resource Data container with name %s in array of %d entries"),
				*ContainerName, Object->GetPrivate()->GetStreamedResourceData().Num());

			continue;
		}

		UCustomizableObjectResourceDataContainer* Container = nullptr;
		EMoveContainerError Error = MoveContainerToNewOuter(OwnerObject, ResourceData, Container);
		UE_CLOG(Error != EMoveContainerError::None, LogMutable, Warning, TEXT("Failed to move container %s back to %s - %s"), *ContainerName, *GetPathNameSafe(OwnerObject), LexToString(Error));

		NewArray.Emplace(Container);
	}

	Object->GetPrivate()->GetStreamedResourceData() = NewArray;

	NewArray.Reset(SavedExtensionContainerNames.Num());

	for (const FString& ContainerName : SavedExtensionContainerNames)
	{
		FCustomizableObjectStreamedResourceData* ResourceData = FindStreamedResourceData(Object->GetPrivate()->GetStreamedExtensionData(), ContainerName);
		if (!ResourceData)
		{
			UE_LOG(LogMutable, Error, TEXT("Couldn't find streamed Extension Data container with name %s in array of %d entries"),
				*ContainerName, Object->GetPrivate()->GetStreamedExtensionData().Num());

			continue;
		}

		UCustomizableObjectResourceDataContainer* Container = nullptr;
		EMoveContainerError Error = MoveContainerToNewOuter(OwnerObject, ResourceData, Container);
		UE_CLOG(Error != EMoveContainerError::None, LogMutable, Warning, TEXT("Failed to move container %s back to %s - %s"), *ContainerName, *GetPathNameSafe(OwnerObject), LexToString(Error));

		NewArray.Emplace(Container);
	}

	Object->GetPrivate()->GetStreamedExtensionData() = NewArray;
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
	
	FCustomizableObjectStreamedResourceData* ResourceData = FindStreamedResourceData(Object->GetPrivate()->GetStreamedResourceData(), GeneratedPackage.RelativePath);
	if (!ResourceData)
	{
		ResourceData = FindStreamedResourceData(Object->GetPrivate()->GetStreamedExtensionData(), GeneratedPackage.RelativePath);
	}

	if (!ResourceData)
	{
		UE_LOG(LogMutable, Error, TEXT("Couldn't find streamed resource Data container with name %s in arrays of %d and %d entries"),
			*GeneratedPackage.RelativePath, Object->GetPrivate()->GetStreamedResourceData().Num(), Object->GetPrivate()->GetStreamedExtensionData().Num());

		return false;
	}

	// [TEMP] Loading a package referencing the CO before PostSaveGeneratedPackage is called causes a name collision.
	// Duplicate the object with the new outer instead of moving it until it is fixed.
	UObject* Container = ResourceData->GetPath().LoadSynchronous();
	EMoveContainerError Error = Container ? EMoveContainerError::None : EMoveContainerError::FailedToLoadContainer;
	if (Container)
	{
		Container = StaticDuplicateObject(Container, GeneratedPackage.Package);
	}

	//UCustomizableObjectResourceDataContainer* Container = nullptr;
	//EMoveContainerError Error = MoveContainerToNewOuter(GeneratedPackage.Package, ResourceData, Container);

	if (Error != EMoveContainerError::None)
	{
		UE_LOG(LogMutable, Error, TEXT("Failed to move container %s to new outer %s - %s"), *ResourceData->GetPath().ToSoftObjectPath().ToString(), *GetPathNameSafe(GeneratedPackage.Package), LexToString(Error));
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

	FCustomizableObjectStreamedResourceData* ResourceData = FindStreamedResourceData(Object->GetPrivate()->GetStreamedResourceData(), GeneratedPackage.RelativePath);
	if (!ResourceData)
	{
		ResourceData = FindStreamedResourceData(Object->GetPrivate()->GetStreamedExtensionData(), GeneratedPackage.RelativePath);
	}

	if (!ResourceData)
	{
		UE_LOG(LogMutable, Error, TEXT("Couldn't find streamed resource Data container with name %s in arrays of %d and %d entries"),
			*GeneratedPackage.RelativePath, Object->GetPrivate()->GetStreamedResourceData().Num(), Object->GetPrivate()->GetStreamedExtensionData().Num());

		return;
	}

	UCustomizableObjectResourceDataContainer* Container = nullptr;
	EMoveContainerError Error = MoveContainerToNewOuter(OwnerObject, ResourceData, Container);
	UE_CLOG(Error != EMoveContainerError::None, LogMutable, Warning, 
		TEXT("Failed to move container %s back to %s - %s"), *ResourceData->GetPath().ToSoftObjectPath().ToString(), *GetPathNameSafe(OwnerObject), LexToString(Error));
}

void FCustomizableObjectCookPackageSplitter::Teardown(ETeardown Status)
{
	StrongObject.Reset();
}
