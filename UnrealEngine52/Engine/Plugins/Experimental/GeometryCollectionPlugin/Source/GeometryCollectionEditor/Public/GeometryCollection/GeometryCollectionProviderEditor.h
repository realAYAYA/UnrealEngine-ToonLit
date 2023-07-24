// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GeometryCollection/GeometryCollectionCache.h"

class FTargetCacheProviderEditor : public ITargetCacheProvider
{
public:

	virtual UGeometryCollectionCache* GetCacheForCollection(const UGeometryCollection* InCollection) override;

};