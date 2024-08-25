// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "UObject/FieldPath.h"
#include "UObject/ObjectMacros.h"

/** 
 * Data collected during SavePackage that modifies the EPropertyFlags for a single FProperty on a single object instance when that object is serialized by SavePackage.
 * The specified changes apply during both the harvesting phase (discovery of referenced imports and exports) and the serialization to disk phase.
 * @note currently only support marking a property transient
 */
struct FPropertySaveOverride
{
	FFieldPath PropertyPath;
	bool bMarkTransient;
};

/** Data to specify an override to apply to an object during save without mutating the object itself. */
struct FObjectSaveOverride
{
	TArray<FPropertySaveOverride> PropOverrides;
};