// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Templates/SharedPointer.h"

struct FDatasmithImportContext;
class IDatasmithLevelVariantSetsElement;
class ULevelVariantSets;

class FDatasmithLevelVariantSetsImporter
{
public:
	static ULevelVariantSets* ImportLevelVariantSets(const TSharedRef<IDatasmithLevelVariantSetsElement>& LevelVariantSets, FDatasmithImportContext& InImportContext, ULevelVariantSets* ExistingLevelVariantSets);
};
