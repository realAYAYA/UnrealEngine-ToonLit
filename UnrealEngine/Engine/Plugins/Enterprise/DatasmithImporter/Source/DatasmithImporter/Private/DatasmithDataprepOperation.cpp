// Copyright Epic Games, Inc. All Rights Reserved.

#include "DatasmithDataprepOperation.h"

#include "DatasmithBlueprintLibrary.h"
#include "Utility/DatasmithImporterUtils.h"

#define LOCTEXT_NAMESPACE "DatasmithDataprepOperation"

void UDataprepSetupStaticLightingOperation::OnExecution_Implementation(const FDataprepContext& InContext)
{
#ifdef LOG_TIME
	DataprepOperationTime::FTimeLogger TimeLogger(TEXT("SetupStaticLighting"), [&](FText Text) { this->LogInfo(Text); });
#endif

	// Execute operation
	UDatasmithStaticMeshBlueprintLibrary::SetupStaticLighting(InContext.Objects, false, bEnableLightmapUVGeneration, LightmapResolutionIdealRatio);
}

#undef LOCTEXT_NAMESPACE
