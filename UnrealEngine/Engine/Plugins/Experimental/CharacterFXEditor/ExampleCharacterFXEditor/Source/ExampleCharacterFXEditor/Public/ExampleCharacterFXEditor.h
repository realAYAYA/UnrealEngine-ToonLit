// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BaseCharacterFXEditor.h"
#include "ExampleCharacterFXEditor.generated.h"

UCLASS()
class UExampleCharacterFXEditor : public UBaseCharacterFXEditor
{
	GENERATED_BODY()
public:
	
	virtual TSharedPtr<FBaseAssetToolkit> CreateToolkit() override;
};

