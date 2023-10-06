// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

class FText;
struct FAssetData;

/** Used in filtering allowed references between assets. Implement a subclass of this and return it in OnMakeAssetReferenceFilter */
class IAssetReferenceFilter
{
public:
	virtual ~IAssetReferenceFilter() { }
	/** Filter function to pass/fail an asset. Called in some situations that are performance-sensitive so is expected to be fast. */
	virtual bool PassesFilter(const FAssetData& AssetData, FText* OutOptionalFailureReason = nullptr) const = 0;
};