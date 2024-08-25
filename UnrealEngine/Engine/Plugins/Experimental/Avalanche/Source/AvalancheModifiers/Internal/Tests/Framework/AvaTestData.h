// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Set.h"
#include "UObject/NameTypes.h"

class FAvaTestData
{
public:
	FAvaTestData();
	TSet<FName> ExpectedConversionModifiers;
	TSet<FName> ExpectedGeometryModifiers;
	TSet<FName> ExpectedLayoutModifiersDynamicMesh;
	TSet<FName> ExpectedLayoutModifiersStaticMesh;
	TSet<FName> ExpectedRenderingModifiers;
};
