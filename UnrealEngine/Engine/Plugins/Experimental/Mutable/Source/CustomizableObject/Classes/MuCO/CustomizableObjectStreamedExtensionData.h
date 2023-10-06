// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCO/CustomizableObjectExtensionData.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPtr.h"

#include "CustomizableObjectStreamedExtensionData.generated.h"

/** A simple container that's used to store the FCustomizableObjectStreamedExtensionData in a package */
UCLASS()
class CUSTOMIZABLEOBJECT_API UCustomizableObjectExtensionDataContainer : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FCustomizableObjectExtensionData Data;
};

/**
 * Used to store extension data that can be streamed in and out from an external package.
 *
 * This allows you to load large extension data on demand, instead of having it always loaded.
 */
USTRUCT()
struct CUSTOMIZABLEOBJECT_API FCustomizableObjectStreamedExtensionData
{
	GENERATED_BODY()

public:
	FCustomizableObjectStreamedExtensionData() = default;

#if WITH_EDITOR
	/** Streamed data can only be constructed in the editor */
	FCustomizableObjectStreamedExtensionData(UCustomizableObjectExtensionDataContainer* InContainer);

	/**
	 * Clears the hard reference to the container, so that it's not forced to load as soon as this
	 * struct is loaded, and replaces the soft reference with the provided one.
	 * 
	 * Note that the container may not have been moved to its new path yet, so don't try to load
	 * the container from the given path here.
	 */
	void ConvertToSoftReferenceForCooking(const TSoftObjectPtr<UCustomizableObjectExtensionDataContainer>& NewContainerPath);
#endif // WITH_EDITOR

	/**
	 * If this returns true, GetLoadedData can safely be called.
	 *
	 * The data will stay loaded until Unload is called.
	 */
	bool IsLoaded() const;

	/**
	 * Returns the data, as long as it's loaded.
	 *
	 * If the data isn't loaded, an assertion will fail.
	 */
	const FCustomizableObjectExtensionData& GetLoadedData() const;

	/**
	 * Release this struct's hard reference to the loaded data.
	 *
	 * This doesn't immediately remove it from memory, but will allow it to be deleted by the
	 * garbage collector if there are no other references.
	 *
	 * Note that in editor builds streamed Extension Data must always be loaded due to the package
	 * structure, and therefore this function has no effect in editor builds.
	 */
	void Unload();

	/**
	 * To stream this data in, load the object at the path returned by GetPath and then pass it
	 * into NotifyLoaded.
	 */
	const TSoftObjectPtr<UCustomizableObjectExtensionDataContainer>& GetPath() const { return ContainerPath; }
	void NotifyLoaded(const UCustomizableObjectExtensionDataContainer* LoadedContainer);

private:
	UPROPERTY()
	TSoftObjectPtr<UCustomizableObjectExtensionDataContainer> ContainerPath;

	UPROPERTY()
	TObjectPtr<const UCustomizableObjectExtensionDataContainer> Container;
};
