// Copyright Epic Games, Inc. All Rights Reserved.

//~=============================================================================
// AnimSequenceExporterFBX
//~=============================================================================

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Exporters/Exporter.h"
#include "ExporterFbx.h"
#include "AnimSequenceExporterFBX.generated.h"

UCLASS()
class UAnimSequenceExporterFBX : public UExporterFBX
{
	GENERATED_UCLASS_BODY()


	//~ Begin UExporter Interface
	virtual bool ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, int32 FileIndex = 0, uint32 PortFlags=0 ) override;
	//~ End UExporter Interface
};



