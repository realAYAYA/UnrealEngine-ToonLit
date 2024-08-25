// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCO/CustomizableObjectResourceData.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPtr.h"

#include "CustomizableObjectStreamedResourceData.generated.h"



/**
 * Used to store resource data that can be streamed in and out from an external package.
 *
 * This allows you to load large resource data on demand, instead of having it always loaded.
 */
USTRUCT()
struct CUSTOMIZABLEOBJECT_API FCustomizableObjectStreamedResourceData
{
	GENERATED_BODY()

public:
	FCustomizableObjectStreamedResourceData() = default;

#if WITH_EDITOR
	/** Streamed data can only be constructed in the editor */
	FCustomizableObjectStreamedResourceData(UCustomizableObjectResourceDataContainer* InContainer);

	/**
	 * Clears the hard reference to the container, so that it's not forced to load as soon as this
	 * struct is loaded, and replaces the soft reference with the provided one.
	 * 
	 * Note that the container may not have been moved to its new path yet, so don't try to load
	 * the container from the given path here.
	 */
	void ConvertToSoftReferenceForCooking(const TSoftObjectPtr<UCustomizableObjectResourceDataContainer>& NewContainerPath);
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
	const FCustomizableObjectResourceData& GetLoadedData() const;

	/**
	 * Release this struct's hard reference to the loaded data.
	 *
	 * This doesn't immediately remove it from memory, but will allow it to be deleted by the
	 * garbage collector if there are no other references.
	 *
	 * Note that in editor builds streamed Resource Data must always be loaded due to the package
	 * structure, and therefore this function has no effect in editor builds.
	 */
	void Unload();

	/**
	 * To stream this data in, load the object at the path returned by GetPath and then pass it
	 * into NotifyLoaded.
	 */
	const TSoftObjectPtr<UCustomizableObjectResourceDataContainer>& GetPath() const { return ContainerPath; }
	void NotifyLoaded(const UCustomizableObjectResourceDataContainer* LoadedContainer);

private:
	UPROPERTY()
	TSoftObjectPtr<UCustomizableObjectResourceDataContainer> ContainerPath;

	UPROPERTY()
	TObjectPtr<const UCustomizableObjectResourceDataContainer> Container;
};
