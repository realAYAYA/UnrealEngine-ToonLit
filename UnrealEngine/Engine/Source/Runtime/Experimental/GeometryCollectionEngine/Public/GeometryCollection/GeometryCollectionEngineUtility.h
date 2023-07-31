// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FGeometryCollection;
class UGeometryCollectionCache;

namespace GeometryCollectionEngineUtility
{
	void GEOMETRYCOLLECTIONENGINE_API PrintDetailedStatistics(const FGeometryCollection* GeometryCollection, const UGeometryCollectionCache* InCache);

	void GEOMETRYCOLLECTIONENGINE_API PrintDetailedStatisticsSummary(const TArray<const FGeometryCollection*> GeometryCollectionArray);

	void GEOMETRYCOLLECTIONENGINE_API ComputeNormals(FGeometryCollection* GeometryCollection);

	void GEOMETRYCOLLECTIONENGINE_API ComputeTangents(FGeometryCollection* GeometryCollection);

}