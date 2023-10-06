// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "Templates/SharedPointer.h"

class FArchive;
class IDatasmithLevelSequenceElement;

#define DATASMITH_ANIMATION_EXTENSION TEXT(".udsanim")

class DATASMITHCORE_API FDatasmithAnimationSerializer
{
public:
	bool Serialize(const TSharedRef<IDatasmithLevelSequenceElement>& LevelSequence, const TCHAR* FilePath, bool bDebugFormat = false);
	bool Deserialize(const TSharedRef<IDatasmithLevelSequenceElement>& LevelSequence, const TCHAR* FilePath);
};