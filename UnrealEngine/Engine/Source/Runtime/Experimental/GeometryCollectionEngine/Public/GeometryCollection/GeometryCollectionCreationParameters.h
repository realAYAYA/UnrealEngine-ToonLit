// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"


class FGeometryCollection;

class GEOMETRYCOLLECTIONENGINE_API FGeometryCollectionCreationParameters
{
public:
	FGeometryCollectionCreationParameters(FGeometryCollection& GeometryCollection, bool ReCalculateNormalsIn = false, bool ReCalculateTangetsIn = false);
	~FGeometryCollectionCreationParameters();

private:
	bool ReCalculateNormals;
	bool ReCalculateTangents;
	FGeometryCollection& GeometryCollection;

};