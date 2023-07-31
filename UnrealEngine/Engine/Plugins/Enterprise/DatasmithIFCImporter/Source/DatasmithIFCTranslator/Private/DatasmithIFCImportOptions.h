// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DatasmithImportOptions.h"

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "DatasmithIFCImportOptions.generated.h"

UCLASS(config = EditorPerProjectUserSettings, HideCategories = (DebugProperty))
class UDatasmithIFCImportOptions : public UDatasmithOptionsBase
{
	GENERATED_BODY()

	UDatasmithIFCImportOptions();

public:
};
