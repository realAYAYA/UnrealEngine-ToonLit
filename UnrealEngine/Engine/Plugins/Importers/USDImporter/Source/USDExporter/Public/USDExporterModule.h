// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class IUsdExporterModule : public IModuleInterface
{
public:
	static void HashEditorSelection( FSHA1& HashToUpdate );
};
