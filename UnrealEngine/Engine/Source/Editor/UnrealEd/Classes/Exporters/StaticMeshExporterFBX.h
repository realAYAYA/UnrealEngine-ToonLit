// Copyright Epic Games, Inc. All Rights Reserved.

//~=============================================================================
// StaticMeshExporterFBX
//~=============================================================================

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Exporters/Exporter.h"
#include "ExporterFbx.h"
#include "StaticMeshExporterFBX.generated.h"

UCLASS()
class UStaticMeshExporterFBX : public UExporterFBX
{
	GENERATED_UCLASS_BODY()


	//~ Begin UExporter Interface
	virtual bool ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, int32 FileIndex = 0, uint32 PortFlags=0 ) override;
	//~ End UExporter Interface
};



