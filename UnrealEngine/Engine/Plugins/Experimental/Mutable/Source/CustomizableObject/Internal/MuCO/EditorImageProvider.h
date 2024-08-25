// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "MuCO/CustomizableObjectSystem.h"

#include "EditorImageProvider.generated.h"

class UTexture2D;
class UCustomizableObjectInstance;


UCLASS()
class CUSTOMIZABLEOBJECT_API UEditorImageProvider : public UCustomizableSystemImageProvider
{
	GENERATED_BODY()

public:
	// UCustomizableSystemImageProvider interface
	virtual ValueType HasTextureParameterValue(const FName& ID) override;
	virtual UTexture2D* GetTextureParameterValue(const FName& ID) override;
};