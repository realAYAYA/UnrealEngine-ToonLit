// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/CoreNet.h"
#include "IrisObjectReferencePackageMap.generated.h"

class FNetworkGUID;

/**
 * Custom packagemap implementation used to be able to capture UObject* references from external serialization.
 * Any object references written when using this packagemap will be added to the References array and serialized as an index.
 * When reading using this packagemap references will be read as an index and resolved by picking the corresponding entry from the provided array containing the references.
 */
UCLASS(transient, MinimalAPI)
class UIrisObjectReferencePackageMap : public UPackageMap
{
public:
	GENERATED_BODY()

	typedef TArray<TObjectPtr<UObject>, TInlineAllocator<4>> FObjectReferenceArray;

	// We override SerialzierObject in order to be able to capture object references
	virtual bool SerializeObject(FArchive& Ar, UClass* InClass, UObject*& Obj, FNetworkGUID* OutNetGUID) override;

	// Init for read, we need a reference array to be able to resolve references
	IRISCORE_API void InitForRead(const FObjectReferenceArray* InReferences);

	// Init for write, all written references will be added to the references array which will be resetted by this call
	IRISCORE_API void InitForWrite(FObjectReferenceArray* InReferences);

protected:
	FObjectReferenceArray* References = nullptr;
};

