// Copyright Epic Games, Inc. All Rights Reserved.

//~=============================================================================
// ExporterFBX
//~=============================================================================

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "Exporters/Exporter.h"
#include "ExporterFbx.generated.h"

class UFbxExportOption;

UCLASS()
class UExporterFBX : public UExporter
{
public:
	GENERATED_BODY()

	class UFbxExportOption* GetAutomatedExportOptionsFbx();
};

