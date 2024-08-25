// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "HairCardGeneratorEditorSettings.generated.h"

enum class EHairAtlasTextureType : uint8;


UCLASS(config = Editor)
class UHairCardGeneratorEditorSettings : public UObject
{
	GENERATED_BODY()

public:
	UHairCardGeneratorEditorSettings();

	UPROPERTY(Config)
	FString HairCardAssetNameFormat;

	UPROPERTY(Config)
	FString HairCardAssetPathFormat;
};