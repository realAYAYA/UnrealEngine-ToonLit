// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartitionVirtualHeightfieldMeshBuilder.h"

#include "HeightfieldMinMaxTexture.h"
#include "HeightfieldMinMaxTextureBuild.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectIterator.h"
#include "VirtualHeightfieldMeshComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WorldPartitionVirtualHeightfieldMeshBuilder)

DEFINE_LOG_CATEGORY_STATIC(LogWorldPartitionVirtualHeightfieldMeshBuilder, All, All);

UWorldPartitionVirtualHeightfieldMeshBuilder::UWorldPartitionVirtualHeightfieldMeshBuilder(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UWorldPartitionVirtualHeightfieldMeshBuilder::RunInternal(UWorld* World, const FCellInfo& InCellInfo, FPackageSourceControlHelper& PackageHelper)
{
	// Find UVirtualHeightfieldMeshComponent and update the associated UHeightfieldMinMaxTexture assets.
	// todo[vhm]: Convert builder to sequentially load and build sections of the world.
	TSet<UObject*> ModifiedObjects;
	for (TObjectIterator<UVirtualHeightfieldMeshComponent> It(RF_ClassDefaultObject, false, EInternalObjectFlags::Garbage); It; ++It)
	{
		if (It->GetWorld() == World)
		{
			if (VirtualHeightfieldMesh::HasMinMaxHeightTexture(*It) && VirtualHeightfieldMesh::BuildMinMaxHeightTexture(*It))
			{
				ModifiedObjects.Add(It->GetMinMaxTexture());
			}
		}
	}

	// Checkout and save any modified packages.
	for (UObject* ModifiedObject : ModifiedObjects)
	{
		UObject* Outer = ModifiedObject->GetOuter();
		UPackage* Package = Cast<UPackage>(Outer);

		if (Package != nullptr && Package->IsDirty())
		{
			if (!PackageHelper.Checkout(Package))
			{
				UE_LOG(LogWorldPartitionVirtualHeightfieldMeshBuilder, Error, TEXT("Error checking out package %s."), *Package->GetName());
				return false;
			}

			FString PackageFileName = SourceControlHelpers::PackageFilename(Package);
			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Standalone;
			SaveArgs.SaveFlags = SAVE_Async;
			if (!UPackage::SavePackage(Package, nullptr, *PackageFileName, SaveArgs))
			{
				UE_LOG(LogWorldPartitionVirtualHeightfieldMeshBuilder, Error, TEXT("Error saving package %s."), *Package->GetName());
				return false;
			}

			if (!PackageHelper.AddToSourceControl(Package))
			{
				UE_LOG(LogWorldPartitionVirtualHeightfieldMeshBuilder, Error, TEXT("Error adding package %s to source control."), *Package->GetName());
				return false;
			}
		}
	}

	UPackage::WaitForAsyncFileWrites();

	return true;
}

