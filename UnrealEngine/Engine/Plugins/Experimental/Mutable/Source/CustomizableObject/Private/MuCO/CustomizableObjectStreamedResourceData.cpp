// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/CustomizableObjectStreamedResourceData.h"

#include "ExternalPackageHelper.h"
#include "MuCO/CustomizableObject.h"
#include "UObject/Package.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CustomizableObjectStreamedResourceData)

#if WITH_EDITOR
FCustomizableObjectStreamedResourceData::FCustomizableObjectStreamedResourceData(
	UCustomizableObjectResourceDataContainer* InContainer)
{
	check(IsInGameThread());
	check(InContainer);

	Container = InContainer;
	ContainerPath = InContainer;
}

void FCustomizableObjectStreamedResourceData::ConvertToSoftReferenceForCooking(
	const TSoftObjectPtr<UCustomizableObjectResourceDataContainer>& NewContainerPath)
{
	check(IsInGameThread());

	// Remove the hard reference to the container, so that it can be unloaded
	Container = nullptr;

	ContainerPath = NewContainerPath;
}
#endif // WITH_EDITOR

bool FCustomizableObjectStreamedResourceData::IsLoaded() const
{
	check(IsInGameThread());

	return Container != nullptr;
}

const FCustomizableObjectResourceData& FCustomizableObjectStreamedResourceData::GetLoadedData() const
{
	check(IsInGameThread());

	check(Container);
	return Container->Data;
}

void FCustomizableObjectStreamedResourceData::Unload()
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

void FCustomizableObjectStreamedResourceData::NotifyLoaded(const UCustomizableObjectResourceDataContainer* LoadedContainer)
{
	check(IsInGameThread());
	check(ContainerPath.Get() == LoadedContainer);

	Container = LoadedContainer;
}
