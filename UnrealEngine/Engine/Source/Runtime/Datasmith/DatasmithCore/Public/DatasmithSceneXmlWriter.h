// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

class FArchive;
class IDatasmithScene;

class DATASMITHCORE_API FDatasmithSceneXmlWriter
{
public:
	void Serialize( TSharedRef< IDatasmithScene > DatasmithScene, FArchive& Archive );
};