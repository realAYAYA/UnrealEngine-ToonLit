// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/NameTypes.h"

// Metadata used by StructUtils
namespace UE::StructUtils::Metadata
{
	// Metadata usable in UPROPERTY for customizing the behavior when displaying the property in a property panel or graph node

	/// FInstancedPropertyBag

	/// [PropertyMetadata] FixedLayout: Indicates that the instanced property bag has a fixed layout and it is not possible to add/remove properties.
	extern STRUCTUTILS_API const FName FixedLayoutName;

	/// [PropertyMetadata] DefaultType: Default property type when adding a new Property. Should be taken from enum EPropertyBagPropertyType
	extern STRUCTUTILS_API const FName DefaultTypeName;

	/// [PropertyMetadata] AllowArrays: By default it is always allowed to create arrays for properties. Use AllowArrays=false to disable Array support.
	extern STRUCTUTILS_API const FName AllowArraysName;

	/// [PropertyMetadata] IsPinTypeAccepted: Name of a UFunction with signature bool(FEdGraphPinType). Returns false if the type should be discarded.
	extern STRUCTUTILS_API const FName IsPinTypeAcceptedName;

	/// [PropertyMetadata] CanRemoveProperty: Name of a UFunction with signature bool(FGuid, FName). ID and name of the property that will be removed. Returns false if the property should not be removed.
	extern STRUCTUTILS_API const FName CanRemovePropertyName;
}