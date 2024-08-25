// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "UObject/ObjectPtr.h"
#include "UObject/SoftObjectPtr.h"

#include "CustomizableObjectResourceDataTypes.generated.h"

class UAssetUserData;

/** Simple struct used to store streamed resources of type AssetUserData */
USTRUCT()
struct FCustomizableObjectAssetUserData
{
	GENERATED_BODY()

public:
	
	// Valid in Cooked builds
	UPROPERTY()
	TObjectPtr<UAssetUserData> AssetUserData;

#if WITH_EDITORONLY_DATA
	// Valid in the Editor
	UPROPERTY(Transient)
	TObjectPtr<UAssetUserData> AssetUserDataEditor;
#endif
};