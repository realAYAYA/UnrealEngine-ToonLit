// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCO/CustomizableObjectInstanceDescriptor.h"


/** Hash of the Descriptor.
* Can change and is not backwards compatible. Do not serialize. */
class CUSTOMIZABLEOBJECT_API FDescriptorHash
{
public:
	FDescriptorHash() = default;

	explicit FDescriptorHash(const FCustomizableObjectInstanceDescriptor& Descriptor);

	bool operator==(const FDescriptorHash& Other) const;

	bool operator!=(const FDescriptorHash& Other) const;
	
	/** Return true if this Hash is a subset of the other Hash (i.e., this Descriptor is a subset of the other Descriptor). */
	bool IsSubset(const FDescriptorHash& Other) const;

	FString ToString() const;

private:
	uint32 Hash = 0;

public:
	int32 MinLOD = 0;

	// Array of bitmasks that indicate which LODs of each component have been requested
	TArray<uint16> RequestedLODsPerComponent;
};
