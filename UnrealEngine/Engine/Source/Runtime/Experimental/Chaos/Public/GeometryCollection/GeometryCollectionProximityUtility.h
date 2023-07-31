// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

class FGeometryCollection;

class CHAOS_API FGeometryCollectionProximityUtility
{
public:
	FGeometryCollectionProximityUtility(FGeometryCollection* InCollection);

	void UpdateProximity();

	// maximum separation to still count as 'contact'
	float ProximityThreshold = 0.01f;

private:
	FGeometryCollection* Collection;
};

