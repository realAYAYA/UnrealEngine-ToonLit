// Copyright Epic Games, Inc. All Rights Reserved.

#include "MLDeformerAsset.h"
#include "MLDeformerObjectVersion.h"

void UMLDeformerAsset::Serialize(FArchive& Archive)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UMLDeformerAsset::Serialize)

	Archive.UsingCustomVersion(UE::MLDeformer::FMLDeformerObjectVersion::GUID);
	Super::Serialize(Archive);
}
