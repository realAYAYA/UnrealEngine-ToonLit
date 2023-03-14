// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DatasmithImporterEditorSettings.generated.h"

UCLASS(config=EditorSettings, MinimalAPI)
class UDatasmithImporterEditorSettings : public UObject
{
	GENERATED_BODY()

public:
	UDatasmithImporterEditorSettings();

	UPROPERTY(config)
	bool bOfflineImporter;

};
