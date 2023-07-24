// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Exporters/Exporter.h"

#include "SkeletalMeshExporterUSD.generated.h"

UCLASS()
class USkeletalMeshExporterUsd : public UExporter
{
	GENERATED_BODY()

public:
	USkeletalMeshExporterUsd();

	//~ Begin UExporter Interface
	virtual bool ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, int32 FileIndex = 0, uint32 PortFlags=0 ) override;
	//~ End UExporter Interface
};
