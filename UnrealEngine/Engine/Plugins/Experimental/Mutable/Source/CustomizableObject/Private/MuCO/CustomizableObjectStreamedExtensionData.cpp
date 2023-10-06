// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/CustomizableObjectStreamedExtensionData.h"

#include "ExternalPackageHelper.h"
#include "MuCO/CustomizableObject.h"
#include "UObject/Package.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CustomizableObjectStreamedExtensionData)

#if WITH_EDITOR
FCustomizableObjectStreamedExtensionData::FCustomizableObjectStreamedExtensionData(
	UCustomizableObjectExtensionDataContainer* InContainer)
{
	check(IsInGameThread());
	check(InContainer);

	Container = InContainer;
	ContainerPath = InContainer;
}

void FCustomizableObjectStreamedExtensionData::ConvertToSoftReferenceForCooking(
	const TSoftObjectPtr<UCustomizableObjectExtensionDataContainer>& NewContainerPath)
{
	check(IsInGameThread());

	// Remove the hard reference to the container, so that it can be unloaded
	Container = nullptr;

	ContainerPath = NewContainerPath;
}
#endif // WITH_EDITOR

bool FCustomizableObjectStreamedExtensionData::IsLoaded() const
{
	check(IsInGameThread());

	return Container != nullptr;
}

const FCustomizableObjectExtensionData& FCustomizableObjectStreamedExtensionData::GetLoadedData() const
{
	check(IsInGameThread());

	check(Container);
	return Container->Data;
}

void FCustomizableObjectStreamedExtensionData::Unload()
{
	check(IsInGameThread());

	if (!Container)
	{
		// Already unloaded
		return;
	}

	if (FPlatformProperties::RequiresCookedData())
	{
		// The container should have been cooked out to its own package
		check(Container->GetOuter()->IsA<UPackage>());

		Container = nullptr;
	}
	else
	{
		// In an uncooked build the container is stored in the CO package and the CO needs to
		// maintain a hard reference to it to ensure that it isn't discarded on save.
		check(Container->GetOuter()->IsA<UCustomizableObject>());
	}
}

void FCustomizableObjectStreamedExtensionData::NotifyLoaded(const UCustomizableObjectExtensionDataContainer* LoadedContainer)
{
	check(IsInGameThread());
	check(ContainerPath.Get() == LoadedContainer);

	Container = LoadedContainer;
}
